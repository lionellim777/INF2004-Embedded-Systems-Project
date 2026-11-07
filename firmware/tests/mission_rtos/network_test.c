#include "network_test_sdk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Include the real translation unit so its async state steps can be driven
 * deterministically without replacing the implementation with a test model. */
#include "network_service.c"
cyw43_t cyw43_state;
static mqtt_client_t fake_client;
static int link=CYW43_LINK_UP;
static bool client_connected=true;
static err_t publication_error, subscription_error;
static unsigned stops, transmitted;
static struct { char topic[128]; uint8_t bytes[16]; size_t length; } sent[128];
uint32_t save_and_disable_interrupts(void) { return 0; }
void restore_interrupts(uint32_t s) { (void)s; }
int cyw43_arch_init(void) { return 0; }
void cyw43_arch_enable_sta_mode(void) {}
void cyw43_arch_poll(void) {}
int cyw43_arch_wifi_connect_async(const char *s,const char *p,uint32_t auth) { (void)s;(void)p;(void)auth;return 0; }
int cyw43_tcpip_link_status(cyw43_t *c,int itf) { (void)c;(void)itf;return link; }
mqtt_client_t *mqtt_client_new(void) { return &fake_client; }
void mqtt_disconnect(mqtt_client_t *c) { (void)c;client_connected=false; }
bool mqtt_client_is_connected(mqtt_client_t *c) { (void)c;return client_connected; }
void mqtt_set_inpub_callback(mqtt_client_t *c,void (*a)(void *,const char *,u32_t),void (*b)(void *,const u8_t *,u16_t,u8_t),void *ctx)
{ (void)c;(void)a;(void)b;(void)ctx; }
err_t mqtt_client_connect(mqtt_client_t *c,const ip_addr_t *a,uint16_t port,void (*cb)(mqtt_client_t *,void *,mqtt_connection_status_t),void *ctx,const struct mqtt_connect_client_info_t *i)
{
 (void)a;assert(port==1883);assert(strlen(i->will_msg)==1 && i->will_msg[0]==1);
 assert(strstr(i->will_topic,"/link"));
 client_connected=true;cb(c,ctx,MQTT_CONNECT_ACCEPTED);return ERR_OK;
}
err_t mqtt_subscribe(mqtt_client_t *c,const char *t,uint8_t qos,mqtt_request_cb_t cb,void *ctx)
{ (void)c;(void)t;assert(qos==1);cb(ctx,subscription_error);return subscription_error; }
err_t mqtt_publish(mqtt_client_t *c,const char *t,const void *p,u16_t n,u8_t qos,u8_t retain,mqtt_request_cb_t cb,void *ctx)
{
 (void)c;(void)retain;(void)cb;(void)ctx;assert(qos==0);assert(n<=16);
 if (publication_error) return publication_error;
 assert(transmitted<128);snprintf(sent[transmitted].topic,128,"%s",t);
 memcpy(sent[transmitted].bytes,p,n);sent[transmitted].length=n;++transmitted;return ERR_OK;
}
int ipaddr_aton(const char *s,ip_addr_t *a) { (void)s;a->addr=1;return 1; }
uint32_t get_rand_32(void) { return 1; }
uint64_t get_absolute_time(void) { return 0; }
uint32_t to_ms_since_boot(uint64_t t) { return (uint32_t)(t/1000); }
void mtk_bridge_delay_ms(uint32_t ms) { (void)ms; }
void board_stop(uint8_t why) { assert(why==STOP_BUTTON);++stops; }
void board_read(uint8_t b[MOTION_RECORD_BYTES])
{ motion_record_t r={.valid=true};assert(motion_record_encode(&r,b,MOTION_RECORD_BYTES)); }
static void online(uint32_t ms)
{
 flags=INITIALIZED;policy=200;state(NETWORK_JOINING,ms);link=CYW43_LINK_UP;
 step(ms);assert(network_state()==NETWORK_CONNECTING);
 step(ms+1);assert(network_state()==NETWORK_SUBSCRIBING);
 step(ms+2);assert(network_state()==NETWORK_ONLINE);
}
static void protocols(void)
{
 uint8_t bytes[5];
 for(char c='A';c<='Z';++c) {
  assert(barcode_record_encode(0,c,true,true,bytes));assert(barcode_record_valid(bytes,5));
  assert((bytes[4]&31u)==(uint8_t)(c-'A'+1));
 }
 assert(!barcode_record_encode(0,'z',true,false,bytes));
 assert(!barcode_record_encode(0,'Z',false,false,bytes));
 assert(barcode_record_encode(0,0,false,false,bytes));assert(barcode_record_valid(bytes,5));
 bytes[4]=128;assert(!barcode_record_valid(bytes,5));
 uint8_t s[10];assert(!status_record_encode(0,0,1,2,4,s));
 binary_command_frame_t f;binary_command_t out={.kind=99};uint8_t period[]={0xc8,0};
 assert(binary_command_begin(&f,2,2));assert(!binary_command_append(&f,period,1,false,&out));
 assert(out.kind==99);assert(binary_command_append(&f,period+1,1,true,&out));assert(out.period_ms==200);
 assert(!binary_command_append(&f,period,2,true,&out));
 assert(!binary_command_begin(&f,2,100));
 assert(binary_command_begin(&f,2,2));assert(!binary_command_append(&f,period,1,true,&out));
 assert(binary_command_begin(&f,2,2));assert(!binary_command_append(&f,period,3,true,&out));
 period[0]=0xc7;assert(binary_command_begin(&f,2,2));assert(!binary_command_append(&f,period,2,true,&out));
 puts("PASS binary protocol: A-Z, unavailable/simulated states, signed motion, fragmented and rejected commands");
}
int main(void)
{
 protocols();online(UINT32_MAX-500);step(0);
 bool motion_seen=false;
 for(unsigned i=0;i<transmitted;++i) if(strstr(sent[i].topic,"/motion")) {
  motion_seen=true;assert(sent[i].length==13 && sent[i].bytes[0]==0 && sent[i].bytes[12]==1);
 }
 assert(motion_seen); /* strlen would have sent zero bytes */
 assert(network_barcode(0,'Z',true,false));step(1000);
 bool z_seen=false;
 for(unsigned i=0;i<transmitted;++i) if(strstr(sent[i].topic,"/barcode") && sent[i].bytes[4]==58) z_seen=true;
 assert(z_seen);
 unsigned before=stops;incoming(NULL,stop_command,0);data(NULL,NULL,0,MQTT_DATA_FLAG_LAST);assert(stops==before+1);
 incoming(NULL,RTOS_TOPIC_PREFIX "command/start",0);data(NULL,NULL,0,1);assert(stops==before+1);
 incoming(NULL,period_command,2);uint8_t p[]={0xe8,3};data(NULL,p,1,0);assert((policy&8191)==200);
 data(NULL,p+1,1,1);assert((policy&8191)==1000);
 publication_error=ERR_ARG;uint32_t previous_motion=motion_ms;step(3000);assert(motion_ms==previous_motion);
 publication_error=0;step(3010);assert(motion_ms==3010);
 link=0;step(3020);assert(network_state()==NETWORK_RETRYING && network_error()==4);
 link=CYW43_LINK_UP;step(4019);assert(network_state()==NETWORK_RETRYING);
 step(4020);step(4021);step(4022);assert(network_state()==NETWORK_ONLINE && network_error()==0);
 client_connected=false;step(4030);assert(network_state()==NETWORK_RETRYING);
 online(10000);subscription_error=ERR_ARG;state(NETWORK_CONNECTING,11000);flags|=CONNECTED;
 step(11000);assert(network_state()==NETWORK_RETRYING && network_error()==8);
 puts("PASS actual MQTT service: explicit byte lengths, Z preservation, STOP-only commands, no start topic, period changes, backpressure, reconnect, subscription failure, clock wrap");
 for(unsigned i=0;i<transmitted;++i) {
  printf("PACKET %s ",sent[i].topic);
  for(size_t j=0;j<sent[i].length;++j)printf("%02x",sent[i].bytes[j]);
  puts("");
 }
 return 0;
}
