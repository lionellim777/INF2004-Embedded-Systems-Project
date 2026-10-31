/* Exercise real handlers, fake only network/clock boundary. */
#include <assert.h>
#include <stdlib.h>
#include "../app/robot_firmware/wireless/calibration_wifi.c"
#include "../app/robot_firmware/wireless/dhcpserver.c"
static uint64_t now;
static int aborts, closes, frees;
static bool no_memory, close_fails;
static char wire[40000];static size_t wire_length;
static struct tcp_pcb fake_pcb;
static struct udp_pcb fake_udp;
static struct pbuf udp_packet;
static char udp_payload[600];
static unsigned udp_sent;
uint64_t time_us_64(void){return now;}uint32_t get_rand_32(void){return 123;}
void stdio_set_driver_enabled(stdio_driver_t*d,bool v){(void)d;(void)v;}
int cyw43_arch_init_with_country(int x){(void)x;return 0;}
void cyw43_arch_enable_ap_mode(const char*a,const char*b,int c){(void)a;(void)b;(void)c;}
void cyw43_arch_disable_ap_mode(void){}void cyw43_arch_deinit(void){}void cyw43_arch_poll(void){}
void tcp_arg(struct tcp_pcb*p,void*a){(void)p;(void)a;}
void tcp_sent(struct tcp_pcb*p,err_t(*f)(void*,struct tcp_pcb*,u16_t)){(void)p;(void)f;}
void tcp_recv(struct tcp_pcb*p,err_t(*f)(void*,struct tcp_pcb*,struct pbuf*,err_t)){(void)p;(void)f;}
void tcp_err(struct tcp_pcb*p,void(*f)(void*,err_t)){(void)p;(void)f;}
void tcp_poll(struct tcp_pcb*p,err_t(*f)(void*,struct tcp_pcb*),int i){(void)p;(void)f;(void)i;}
err_t tcp_close(struct tcp_pcb*p){(void)p;++closes;return close_fails?ERR_MEM:ERR_OK;}
void tcp_abort(struct tcp_pcb*p){(void)p;++aborts;}
u16_t tcp_sndbuf(struct tcp_pcb*p){(void)p;return 1024;}
err_t tcp_write(struct tcp_pcb*p,const void*d,u16_t n,int f){(void)p;assert(f==TCP_WRITE_FLAG_COPY);if(no_memory)return ERR_MEM;assert(wire_length+n<sizeof(wire));memcpy(wire+wire_length,d,n);wire_length+=n;wire[wire_length]=0;return ERR_OK;}
err_t tcp_output(struct tcp_pcb*p){(void)p;return ERR_OK;}void tcp_recved(struct tcp_pcb*p,u16_t n){(void)p;(void)n;}
void tcp_nagle_disable(struct tcp_pcb*p){(void)p;}
struct tcp_pcb *tcp_new_ip_type(int t){(void)t;return &fake_pcb;}
err_t tcp_bind(struct tcp_pcb*p,const ip_addr_t*a,u16_t port){(void)p;(void)a;(void)port;return ERR_OK;}
struct tcp_pcb*tcp_listen_with_backlog(struct tcp_pcb*p,int n){(void)n;return p;}
void tcp_accept(struct tcp_pcb*p,err_t(*f)(void*,struct tcp_pcb*,err_t)){(void)p;(void)f;}
u16_t pbuf_copy_partial(const struct pbuf*p,void*d,u16_t n,u16_t off){if(n>p->tot_len-off)n=p->tot_len-off;memcpy(d,(char*)p->payload+off,n);return n;}
void pbuf_free(struct pbuf*p){(void)p;++frees;}
struct udp_pcb*udp_new(void){return &fake_udp;}void udp_recv(struct udp_pcb*p,udp_recv_fn f,void*a){(void)p;(void)f;(void)a;}
void udp_remove(struct udp_pcb*p){(void)p;}err_t udp_bind(struct udp_pcb*p,const ip_addr_t*a,u16_t n){(void)p;(void)a;(void)n;return ERR_OK;}
struct pbuf*pbuf_alloc(int a,u16_t n,int b){(void)a;(void)b;assert(n<=sizeof(udp_payload));udp_packet=(struct pbuf){n,udp_payload};return &udp_packet;}
err_t udp_sendto_if(struct udp_pcb*p,struct pbuf*b,const ip_addr_t*a,u16_t port,struct netif*n){(void)p;(void)b;(void)a;(void)port;(void)n;++udp_sent;return ERR_OK;}
err_t udp_sendto(struct udp_pcb*p,struct pbuf*b,const ip_addr_t*a,u16_t port){return udp_sendto_if(p,b,a,port,NULL);}
struct netif*ip_current_input_netif(void){return NULL;}uint32_t cyw43_hal_ticks_ms(void){return (uint32_t)(now/1000);}
static client_t *open_client(void){memset(clients,0,sizeof(clients));wire_length=0;assert(accept_client(NULL,&fake_pcb,ERR_OK)==ERR_OK);return &clients[0];}
static void feed(client_t*c,const char*text,size_t n){struct pbuf p={(u16_t)n,(void*)text};assert(client_recv(c,c->pcb,&p,ERR_OK)==ERR_OK);}
static void drain(client_t*c){for(unsigned i=0;c->pcb && i<100;i++){size_t queued=c->queued;assert(client_sent(c,c->pcb,(u16_t)(queued-c->acknowledged))==ERR_OK);}assert(c->pcb==NULL);}
static void http_tests(void){
    assert(calibration_wifi_init());
    client_t*c=open_client();core.selection_allowed=true;
    const char *req="POST /command/f HTTP/1.1\r\nHost: 192.168.4.1\r\nX-Calibrate: 1\r\nContent-Length: 0\r\n\r\n";
    /* Every byte arrives separately, as is legal on TCP. */
    for(size_t i=0;i<strlen(req);i++)feed(c,req+i,1);
    assert(core.pending=='f');drain(c);assert(strstr(wire,"202 Accepted"));
    assert(calibration_wifi_getchar()=='f');
    c=open_client();req="GET /command/f HTTP/1.1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"404"));assert(core.pending==0);
    c=open_client();req="POST /command/f HTTP/1.1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"403"));
    core.selection_allowed=false;c=open_client();req="POST /command/r HTTP/1.1\r\nx-calibrate: 1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"409"));
    c=open_client();req="POST /command/x HTTP/1.1\r\nX-Calibrate: 1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"202"));assert(calibration_wifi_getchar()=='x');
    c=open_client();req="GET /logs?since=0 HTTP/1.1\r\n\r\n";capture("test log\n",9);feed(c,req,strlen(req));drain(c);assert(strstr(wire,"test log\n"));assert(strstr(wire,"X-Next: 9\r\n"));
    /* Eight maximum retained lines still fit and survive the HTTP result. */
    char *fields[]={core.stop,core.settled,core.summary,core.timing,core.barcode,core.trace[0],core.trace[1],core.course_result};
    for(unsigned i=0;i<8;i++){memset(fields[i],'A'+i,CAL_LINE_CAPACITY-1);fields[i][CAL_LINE_CAPACITY-2]='\n';fields[i][CAL_LINE_CAPACITY-1]=0;}
    c=open_client();req="GET /result HTTP/1.1\r\n\r\n";feed(c,req,strlen(req));drain(c);
    for(unsigned i=0;i<8;i++)assert(strstr(wire,fields[i]));
    strcpy(core.optical_result,"[OPTICAL RESULT] samples=5000\n");
    c=open_client();req="GET /result HTTP/1.1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,core.optical_result));
    core.selection_allowed=true;c=open_client();req="POST /command/v HTTP/1.1\r\nX-Calibrate: 1\r\n\r\n";
    feed(c,req,strlen(req));drain(c);assert(strstr(wire,"202")&&strstr(wire,"motors stay off"));assert(calibration_wifi_getchar()=='v');
    core.selection_allowed=true;c=open_client();req="POST /command/b HTTP/1.1\r\nX-Calibrate: 1\r\n\r\n";
    feed(c,req,strlen(req));drain(c);assert(strstr(wire,"202"));assert(calibration_wifi_getchar()=='b');
    c=open_client();req="POST /mission/b/l/120 HTTP/1.1\r\nX-Calibrate: 1\r\n\r\n";
    feed(c,req,strlen(req));drain(c);
#ifdef ROBOCAR_STATIONARY_BARCODE
    assert(strstr(wire,"409") && core.pending==0);
#else
    assert(strstr(wire,"202") && calibration_wifi_getchar()=='q');
    course_config_t cfg=calibration_wifi_course_config();assert(cfg.y_action=='b' && cfg.z_action=='l' && cfg.advance_mm==120);
#endif
    c=open_client();req="POST /mission/b/l/120 HTTP/1.1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"403"));
    core.selection_allowed=false;c=open_client();req="POST /mission/l/r/120 HTTP/1.1\r\nX-Calibrate: 1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"409"));
    core.selection_allowed=true;
    c=open_client();req="GET /logs?since=4294967296 HTTP/1.1\r\n\r\n";feed(c,req,strlen(req));drain(c);assert(strstr(wire,"400"));
    c=open_client();req="GET / HTTP/1.1\r\n\r\n";no_memory=true;feed(c,req,strlen(req));assert(c->queued==0);no_memory=false;drain(c);assert(strstr(wire,"<!doctype html>"));assert(strstr(wire,"</html>"));
    c=open_client();char huge[2000];memset(huge,'A',sizeof(huge));feed(c,huge,sizeof(huge));drain(c);assert(strstr(wire,"431"));
    c=open_client();req="GET / HTTP/1.1\r\n\0";feed(c,req,18);drain(c);assert(strstr(wire,"400"));
    c=open_client();now+=CLIENT_TIMEOUT_US+1;assert(client_poll(c,c->pcb)==ERR_OK && !c->pcb);
    c=open_client();int old=closes;client_error(c,ERR_ABRT);assert(!c->pcb && old==closes);
    c=open_client();close_fails=true;assert(close_client(c)==ERR_ABRT);assert(aborts>0);close_fails=false;
    memset(clients,0,sizeof(clients));for(unsigned i=0;i<CLIENT_COUNT;i++)assert(accept_client(NULL,&fake_pcb,ERR_OK)==ERR_OK);
    assert(accept_client(NULL,&fake_pcb,ERR_OK)==ERR_ABRT);
}
static void dhcp_tests(void){
    uint8_t pad[]={0,53,1,1,255};assert(opt_find(pad,sizeof(pad),53,1)==pad+1);
    uint8_t shortopt[]={53,1};assert(!opt_find(shortopt,sizeof(shortopt),53,1));
    uint8_t bad[]={50,255,1,2,3,4};assert(!opt_find(bad,sizeof(bad),50,4));
    uint8_t last[]={53};assert(!opt_find(last,sizeof(last),53,1));
    /* Random malformed datagrams through the actual DHCP handler, under ASan. */
    uint32_t seed=42;char data[548];
    for(unsigned k=0;k<5000;k++){for(size_t i=0;i<sizeof(data);i++){seed=seed*1664525u+1013904223u;data[i]=(char)(seed>>24);}struct pbuf p={(u16_t)(k%sizeof(data)),data};dhcp_server_process(&dhcp,&fake_udp,&p,NULL,68);}
    dhcp_msg_t msg={0};msg.op=1;msg.htype=1;msg.hlen=6;msg.chaddr[0]=2;memcpy(msg.options,"\x63\x82\x53\x63",4);
    msg.options[4]=53;msg.options[5]=1;msg.options[6]=1;msg.options[7]=255;
    struct pbuf p={248,&msg};unsigned before=udp_sent;dhcp_server_process(&dhcp,&fake_udp,&p,NULL,68);assert(udp_sent==before+1);
    dhcp_msg_t *reply=(dhcp_msg_t*)udp_payload;assert(reply->op==2 && reply->yiaddr[0]==192 && reply->yiaddr[3]>=16);
    msg.options[6]=3;msg.options[7]=50;msg.options[8]=4;memcpy(msg.options+9,reply->yiaddr,4);msg.options[13]=255;p.tot_len=254;dhcp_server_process(&dhcp,&fake_udp,&p,NULL,68);assert(udp_sent==before+2);
    /* Standard renewal supplies ciaddr rather than option 50. */
    memcpy(msg.ciaddr,msg.options+9,4);msg.options[7]=255;p.tot_len=248;dhcp_server_process(&dhcp,&fake_udp,&p,NULL,68);assert(udp_sent==before+3);
}
int main(void){http_tests();dhcp_tests();puts("HTTP fragmentation, command rejection, buffer pressure, lifecycle and DHCP bounds/renewal tests passed (radio mocked).");return 0;}
