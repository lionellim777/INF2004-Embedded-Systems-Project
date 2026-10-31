/* Real lwIP IP->UDP->DHCP->UDP->IP regression; only NIC and time are simulated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip4.h"
#include "lwip/pbuf.h"
#include "lwip/inet_chksum.h"
#include "dhcpserver.h"
static unsigned replies;
static unsigned char response[600];
static size_t response_length;
sys_prot_t sys_arch_protect(void) {return 0;}
void sys_arch_unprotect(sys_prot_t value) {(void)value;}
u32_t sys_now(void) {return 1000;}
uint32_t cyw43_hal_ticks_ms(void) {return sys_now();}
static err_t nic_output(struct netif *netif,struct pbuf *p,const ip4_addr_t *dest)
{
    (void)netif;(void)dest;
    response_length=pbuf_copy_partial(p,response,sizeof(response),0);
    replies++;
    return ERR_OK;
}
static err_t nic_init(struct netif *n)
{
    n->output=nic_output;n->name[0]='t';n->name[1]='0';n->mtu=1500;
    n->flags=NETIF_FLAG_BROADCAST|NETIF_FLAG_LINK_UP;return ERR_OK;
}
static void put16(unsigned char *p,unsigned v){p[0]=(unsigned char)(v>>8);p[1]=(unsigned char)v;}
static unsigned message_type(void)
{
    assert(response_length>=271);
    assert(response[20]==0 && response[21]==67 && response[22]==0 && response[23]==68);
    size_t pos=28+240;
    while(pos<response_length && response[pos]!=255){
        if(response[pos]==0){pos++;continue;}
        assert(pos+1<response_length && pos+2+response[pos+1]<=response_length);
        if(response[pos]==53){assert(response[pos+1]==1);return response[pos+2];}
        pos+=2+response[pos+1];
    }
    assert(0);return 0;
}
static void inject(struct netif *nic,unsigned type,const unsigned char *requested)
{
    unsigned char packet[320]={0};
    unsigned char *dhcp=packet+28;
    dhcp[0]=1;dhcp[1]=1;dhcp[2]=6;dhcp[4]=0x42;dhcp[5]=0x77;
    dhcp[10]=0x80; /* client asks for broadcast replies */
    dhcp[28]=2;dhcp[29]=3;dhcp[30]=4;dhcp[31]=5;dhcp[32]=6;dhcp[33]=7;
    memcpy(dhcp+236,"\x63\x82\x53\x63",4);
    unsigned char *opts=dhcp+240;*opts++=53;*opts++=1;*opts++=(unsigned char)type;
    if(requested){*opts++=50;*opts++=4;memcpy(opts,requested,4);opts+=4;*opts++=54;*opts++=4;memcpy(opts,"\xc0\xa8\x04\x01",4);opts+=4;}
    *opts++=255;
    size_t len=(size_t)(opts-packet);
    packet[0]=0x45;put16(packet+2,(unsigned)len);packet[8]=64;packet[9]=17;
    /* source stays 0.0.0.0: this is the part handler-only mocks missed. */
    memset(packet+16,255,4);
    put16(packet+20,68);put16(packet+22,67);put16(packet+24,(unsigned)len-20);
    u16_t checksum=inet_chksum(packet,20);memcpy(packet+10,&checksum,2);
    struct pbuf *p=pbuf_alloc(PBUF_RAW,(u16_t)len,PBUF_RAM);assert(p);assert(pbuf_take(p,packet,(u16_t)len)==ERR_OK);
    assert(ip4_input(p,nic)==ERR_OK);
}
int main(void)
{
    lwip_init();struct netif nic;
    ip4_addr_t ip,mask,gw;IP4_ADDR(&ip,192,168,4,1);IP4_ADDR(&mask,255,255,255,0);gw=ip;
    assert(netif_add(&nic,&ip,&mask,&gw,NULL,nic_init,ip4_input));netif_set_default(&nic);netif_set_up(&nic);
    dhcp_server_t server;dhcp_server_init(&server,&ip,&mask);assert(server.udp);
    inject(&nic,1,NULL);
#ifdef EXPECT_BROKEN_CONFIG
    assert(replies==0);puts("REPRODUCED v5 failure: actual lwIP drops 0.0.0.0 DHCP discovery before handler.");
#else
    assert(replies==1);assert(message_type()==2);
    unsigned char offered[4];memcpy(offered,response+28+16,4);
    assert(offered[0]==192&&offered[1]==168&&offered[2]==4&&offered[3]>=16&&offered[3]<=23);
    inject(&nic,3,offered);assert(replies==2);assert(message_type()==5);
    assert(memcmp(response+28+16,offered,4)==0);
    puts("PASS actual lwIP: 0.0.0.0 DISCOVER -> OFFER -> REQUEST -> ACK, correct assigned address.");
#endif
    dhcp_server_deinit(&server);return 0;
}
