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
static uint16_t mock_scan_run=1;
static mqtt_request_cb_t pending_ack;static void *pending_context;
static bool mock_scan_stopped;
static int init_result,pm_result;
static unsigned sta_calls,pm_calls;
static struct { char topic[128]; uint8_t bytes[SCAN_PACKET_BYTES]; size_t length; } sent[256];
size_t runtime_report_packet(unsigned slot,uint32_t ms,uint8_t out[RUNTIME_PACKET_BYTES])
{
 if(slot>4)return 0;
 if(!slot){runtime_summary_t s={.ms=ms,.run=mock_scan_run,.stop_sequence=1,.requested_us=10,.disabled_us=12,.source=1,.reason=1,.stack_scans=3};return runtime_summary_encode(&s,out);}
 runtime_stack_t stack={4096,4224,3000,(uint8_t)(slot+1u),7};runtime_task_t task={.iterations=10,.gap_us=10000,.work_us=100,.active_gap_us=5000,.active_work_us=50};
 return runtime_task_encode(ms,mock_scan_run,slot-1,&stack,&task,out);
}
uint16_t track_scan_run(void){return mock_scan_run;}
size_t track_scan_state(uint8_t out[SCAN_PACKET_BYTES])
{ scan_state_t s={.run=mock_scan_run,.ms=1234,.ready=true,.sealed=mock_scan_stopped,.recoveries=33};return scan_state_encode(&s,out); }
size_t track_scan_export(uint16_t run,unsigned index,uint8_t out[SCAN_PACKET_BYTES])
{
 if(!mock_scan_stopped || run!=mock_scan_run || index>0)return 0;
 return scan_done_encode(run,0,false,out);
}
uint32_t save_and_disable_interrupts(void) { return 0; }
void restore_interrupts(uint32_t s) { (void)s; }
int cyw43_arch_init(void) { return init_result; }
void cyw43_arch_enable_sta_mode(void) { ++sta_calls; }
int cyw43_wifi_pm(cyw43_t *c,uint32_t pm)
{ assert(c==&cyw43_state && pm==CYW43_NONE_PM && sta_calls>pm_calls);++pm_calls;return pm_result; }
void cyw43_arch_poll(void) {}
int cyw43_arch_wifi_connect_async(const char *s,const char *p,uint32_t auth) { (void)s;(void)p;(void)auth;return 0; }
int cyw43_tcpip_link_status(cyw43_t *c,int itf) { (void)c;(void)itf;return link; }
mqtt_client_t *mqtt_client_new(void) { return &fake_client; }
void mqtt_disconnect(mqtt_client_t *c) { (void)c;client_connected=false;if(pending_ack){mqtt_request_cb_t cb=pending_ack;pending_ack=NULL;cb(pending_context,ERR_ARG);} }
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
 (void)c;(void)retain;assert(qos<=1);assert(n<=SCAN_PACKET_BYTES);
 if (publication_error) return publication_error;
 assert(transmitted<256);snprintf(sent[transmitted].topic,128,"%s",t);
 memcpy(sent[transmitted].bytes,p,n);sent[transmitted].length=n;++transmitted;
 if(qos){assert(cb && !pending_ack);pending_ack=cb;pending_context=ctx;}
 return ERR_OK;
}
int ipaddr_aton(const char *s,ip_addr_t *a) { (void)s;a->addr=1;return 1; }
uint32_t get_rand_32(void) { return 1; }
uint32_t time_us_32(void) { return 0; }
uint64_t get_absolute_time(void) { return 0; }
uint32_t to_ms_since_boot(uint64_t t) { return (uint32_t)(t/1000); }
void mtk_bridge_delay_ms(uint32_t ms) { (void)ms; }
void board_stop(uint8_t why) { assert(why==STOP_BUTTON);++stops; }
void board_read(uint8_t b[MOTION_RECORD_BYTES])
{ motion_record_t r={.valid=true};assert(motion_record_encode(&r,b,MOTION_RECORD_BYTES)); }
static void online(uint32_t ms)
{
 flags=INITIALIZED;policy=200;state(NETWORK_JOINING,ms);link=CYW43_LINK_UP;
 step(ms);assert(network_state()==NETWORK_CONNECTING && network_wifi_link()==CYW43_LINK_UP);
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
 uint16_t raw[3]={0,4095,141};uint8_t line_bytes[LINE_RECORD_BYTES];
 assert(line_record_encode(UINT32_MAX,raw,7,true,line_bytes) && line_bytes[8]&128u);
 raw[0]=4096;assert(!line_record_encode(0,raw,0,true,line_bytes));
 assert(line_record_encode(0,raw,0,false,line_bytes));
 for(unsigned i=4;i<9;i++)assert(line_bytes[i]==0);
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
 assert(network_wifi_pm_result()==INT8_MIN);
 init_result=-1;assert(!initialize_radio() && sta_calls==0 && pm_calls==0);
 init_result=0;assert(initialize_radio() && network_wifi_pm_result()==0 && pm_calls==1);
 pm_result=-1;assert(initialize_radio() && network_wifi_pm_result()==-1 && pm_calls==2);
 pm_result=0;
 protocols();uint16_t raw[3]={191,2908,141};assert(network_line(0,raw,2,true));
 online(UINT32_MAX-500);step(0);
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
 for (int diagnostic=-3;diagnostic<=3;++diagnostic) {
  flags=INITIALIZED;state(NETWORK_JOINING,20000);link=diagnostic;
  step(20001);assert(network_wifi_link()==diagnostic);
 }
 subscription_error=ERR_OK;online(30000);mock_scan_stopped=true;step(31000);assert(scan_pending && scan_index==0 && pending_ack);
 unsigned queued=transmitted;step(31005);assert(transmitted==queued && scan_index==0);
 pending_ack=NULL;scan_ack(NULL,ERR_ARG);assert(!scan_pending && scan_index==0);
 step(31010);assert(pending_ack && scan_index==0);
 pending_ack=NULL;scan_ack(NULL,ERR_OK);assert(!scan_pending && scan_index==1);
 step(31015);assert(scan_index==1);
 /* A new run while an old ACK is outstanding cannot skip its first record. */
 mock_scan_run=2;step(31020);assert(scan_pending && scan_index==0);
 mock_scan_run=3;step(31025);assert(scan_run==3 && scan_index==0);
 pending_ack=NULL;scan_ack(NULL,ERR_OK);assert(scan_index==0);
 publication_error=ERR_ARG;step(31030);assert(!scan_pending && scan_index==0);
 publication_error=ERR_OK;step(31035);assert(scan_pending);
 client_connected=false;step(31040);assert(!scan_pending && scan_index==0);
 online(32000);step(33000);assert(scan_pending && scan_index==0);
 pending_ack=NULL;scan_ack(NULL,ERR_OK);assert(scan_index==1);
 puts("PASS scan diagnostic network: QoS1 ACK ordering, backpressure retry, old-run ACK isolation and reconnect replay");
 puts("PASS actual MQTT service: explicit byte lengths, Z preservation, STOP-only commands, no start topic, period changes, backpressure, reconnect, subscription failure, clock wrap, signed Wi-Fi diagnostics");
 for(unsigned i=0;i<transmitted;++i) {
  printf("PACKET %s ",sent[i].topic);
  for(size_t j=0;j<sent[i].length;++j)printf("%02x",sent[i].bytes[j]);
  puts("");
 }
 return 0;
}
