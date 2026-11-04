/* Real lwIP TCP: how soon is a lost first reply resent? Only NIC and time are simulated.
 * Every page poll is a new connection, so this first-resend delay decides whether
 * one lost packet makes a poll miss the page's timeout. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/inet_chksum.h"
static unsigned char out[1600];
static size_t out_length;
static unsigned data_sends;
sys_prot_t sys_arch_protect(void) {return 0;}
void sys_arch_unprotect(sys_prot_t value) {(void)value;}
u32_t sys_now(void) {return 0;}
static void put16(unsigned char *p,unsigned v){p[0]=(unsigned char)(v>>8);p[1]=(unsigned char)v;}
static void put32(unsigned char *p,u32_t v){put16(p,v>>16);put16(p+2,v&0xffffu);}
static u32_t get32(const unsigned char *p){return (u32_t)p[0]<<24|(u32_t)p[1]<<16|(u32_t)p[2]<<8|p[3];}
static err_t nic_output(struct netif *netif,struct pbuf *p,const ip4_addr_t *dest)
{
    (void)netif;(void)dest;
    out_length=pbuf_copy_partial(p,out,sizeof(out),0);
    assert(out[9]==6);
    size_t ip_header=(size_t)(out[0]&15)*4, tcp_header=(size_t)(out[ip_header+12]>>4)*4;
    if(out_length>ip_header+tcp_header){data_sends++;}
    return ERR_OK;
}
static err_t nic_init(struct netif *n)
{
    n->output=nic_output;n->name[0]='t';n->name[1]='0';n->mtu=1500;
    n->flags=NETIF_FLAG_BROADCAST|NETIF_FLAG_LINK_UP;return ERR_OK;
}
/* Laptop 192.168.4.16:50000 -> car 192.168.4.1:80, no payload. */
static void inject(struct netif *nic,unsigned flags,u32_t seq,u32_t ack)
{
    ip4_addr_t laptop,car;IP4_ADDR(&laptop,192,168,4,16);IP4_ADDR(&car,192,168,4,1);
    unsigned char packet[40]={0};
    unsigned char *tcp=packet+20;
    put16(tcp,50000);put16(tcp+2,80);put32(tcp+4,seq);put32(tcp+8,ack);
    tcp[12]=5<<4;tcp[13]=(unsigned char)flags;put16(tcp+14,8192);
    struct pbuf *segment=pbuf_alloc(PBUF_RAW,20,PBUF_RAM);assert(segment);assert(pbuf_take(segment,tcp,20)==ERR_OK);
    u16_t checksum=inet_chksum_pseudo(segment,IP_PROTO_TCP,20,&laptop,&car);memcpy(tcp+16,&checksum,2);pbuf_free(segment);
    packet[0]=0x45;put16(packet+2,40);packet[8]=64;packet[9]=6;
    memcpy(packet+12,&laptop.addr,4);memcpy(packet+16,&car.addr,4);
    checksum=inet_chksum(packet,20);memcpy(packet+10,&checksum,2);
    struct pbuf *p=pbuf_alloc(PBUF_RAW,40,PBUF_RAM);assert(p);assert(pbuf_take(p,packet,40)==ERR_OK);
    assert(ip4_input(p,nic)==ERR_OK);
}
static err_t accepted(void *arg,struct tcp_pcb *pcb,err_t err)
{
    (void)arg;(void)err;
    static const char reply[]="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    assert(tcp_write(pcb,reply,sizeof(reply)-1u,TCP_WRITE_FLAG_COPY)==ERR_OK);
    assert(tcp_output(pcb)==ERR_OK);
    return ERR_OK;
}
int main(void)
{
    lwip_init();struct netif nic;
    ip4_addr_t ip,mask,gw;IP4_ADDR(&ip,192,168,4,1);IP4_ADDR(&mask,255,255,255,0);gw=ip;
    assert(netif_add(&nic,&ip,&mask,&gw,NULL,nic_init,ip4_input));netif_set_default(&nic);netif_set_up(&nic);
    struct tcp_pcb *pcb=tcp_new_ip_type(IPADDR_TYPE_V4);assert(pcb);
    assert(tcp_bind(pcb,IP_ANY_TYPE,80)==ERR_OK);pcb=tcp_listen(pcb);assert(pcb);tcp_accept(pcb,accepted);
    /* The car has been up a while: lwIP's tick counter is not at zero. */
    for(int i=0;i<8;i++){tcp_tmr();}
    inject(&nic,TCP_SYN,1000u,0u);
    assert((out[33]&(TCP_SYN|TCP_ACK))==(TCP_SYN|TCP_ACK));
    /* Handshake completes at once, like a nearby laptop (~10 ms RTT). */
    inject(&nic,TCP_ACK,1001u,get32(out+24)+1u);
    assert(data_sends==1u);
    /* The reply is lost: never ACK it. Step lwIP's timer until it resends. */
    unsigned waited_ms=0;
    while(data_sends==1u && waited_ms<10000u){tcp_tmr();waited_ms+=TCP_TMR_INTERVAL;}
    assert(data_sends==2u);
    printf("lost first reply resent after %u ms\n",waited_ms);
#ifdef EXPECT_OLD_RTO
    assert(waited_ms>=2000u);puts("REPRODUCED old config: a lost reply waits too long for a 3.5 s page poll.");
#else
    assert(waited_ms<=1500u);puts("PASS actual lwIP: lost first reply resent within 1.5 s.");
#endif
    return 0;
}
