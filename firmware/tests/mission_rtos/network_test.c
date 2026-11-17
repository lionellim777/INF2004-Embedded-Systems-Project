#include "network_test_sdk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Include the real translation unit so its async state steps can be driven
 * deterministically without replacing the implementation with a test model. */
#include "network_service.c"
#include "terrain_telemetry.h"
cyw43_t cyw43_state;
static mqtt_client_t fake_client;
static int link=CYW43_LINK_UP;
static bool client_connected=true;
static err_t publication_error, subscription_error;
static unsigned stops, transmitted;
static bool limited_requests;
static unsigned request_budget;
static uint16_t mock_scan_run=1;
static mqtt_request_cb_t pending_ack;static void *pending_context;
#ifdef RTOS_TERRAIN
static mqtt_request_cb_t hump_puback;static void *hump_puback_context; /* second QoS1 flight */
#endif
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
void mqtt_disconnect(mqtt_client_t *c) { (void)c;client_connected=false;if(pending_ack){mqtt_request_cb_t cb=pending_ack;pending_ack=NULL;cb(pending_context,ERR_ARG);}
#ifdef RTOS_TERRAIN
 hump_puback=NULL; /* lwIP mqtt_close frees requests without callbacks */
#endif
}
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
 if(limited_requests){if(!request_budget)return ERR_ARG;request_budget--;}
 assert(transmitted<256);snprintf(sent[transmitted].topic,128,"%s",t);
 memcpy(sent[transmitted].bytes,p,n);sent[transmitted].length=n;++transmitted;
#ifdef RTOS_TERRAIN
 if(qos && strstr(t,"/hump_event")){assert(cb && !hump_puback);hump_puback=cb;hump_puback_context=ctx;}
 else
#endif
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
static void terrain_protocol(void)
{
 /* Hand-packed: valid1,calibrated1, roll -25 (0.1 deg) as signed9, humps 2,
  * last 100 and highest 126 (0.1 mm) as 9 bits each -> 0x7E320179F. */
 terrain_reading_t r={.ms=0x01020304u,.valid=true,.calibrated=true,.roll_decideg=-25,
                      .humps=2,.last_height_dmm=100,.highest_height_dmm=126};
 const uint8_t expected[TERRAIN_RECORD_BYTES]={4,3,2,1,0x9f,0x17,0x20,0xe3,0x07};
 uint8_t b[TERRAIN_RECORD_BYTES];
 assert(terrain_record_encode(&r,b) && !memcmp(b,expected,sizeof b));
 terrain_reading_t edge={.valid=true,.roll_decideg=-255,.humps=255,.last_height_dmm=511,.highest_height_dmm=511};
 assert(terrain_record_encode(&edge,b));
 edge.roll_decideg=255;assert(terrain_record_encode(&edge,b));
 terrain_reading_t bad=edge;bad.roll_decideg=256;assert(!terrain_record_encode(&bad,b));
 bad=edge;bad.roll_decideg=-256;assert(!terrain_record_encode(&bad,b));
 bad=edge;bad.humps=256;assert(!terrain_record_encode(&bad,b));
 bad=edge;bad.last_height_dmm=512;assert(!terrain_record_encode(&bad,b));
 bad=edge;bad.highest_height_dmm=512;assert(!terrain_record_encode(&bad,b));
 r.valid=false;assert(terrain_record_encode(&r,b));
 const uint8_t invalid[TERRAIN_RECORD_BYTES]={4,3,2,1,2,0,0,0,0};
 assert(!memcmp(b,invalid,sizeof b));
 puts("PASS terrain record: hand-packed signed roll, hump count and heights");
}
static void terrain_converter(void)
{
 terrain_source_t s={.available=true,.calibrated=true,.sample_valid=true,.roll_change_deg=-2.46f,
                     .humps=3,.last_height_mm=10.04f,.highest_height_mm=12.66f};
 terrain_reading_t r;
 assert(terrain_reading_from(500,&s,&r));
 assert(r.ms==500 && r.valid && r.calibrated && r.roll_decideg==-25 && r.humps==3 &&
        r.last_height_dmm==100 && r.highest_height_dmm==127);
 terrain_source_t off=s;off.available=false;assert(terrain_reading_from(1,&off,&r) && !r.valid && r.calibrated);
 off=s;off.calibrated=false;assert(terrain_reading_from(1,&off,&r) && !r.valid && !r.calibrated);
 off=s;off.sample_valid=false;assert(terrain_reading_from(1,&off,&r) && !r.valid && r.calibrated);
 const terrain_reading_t kept=r;
 terrain_source_t bad[]={s,s,s,s,s,s,s};
 bad[0].roll_change_deg=0.0f/0.0f;bad[1].roll_change_deg=25.56f;bad[2].roll_change_deg=-25.56f;
 bad[3].humps=256;bad[4].last_height_mm=-0.06f;bad[5].highest_height_mm=51.16f;bad[6].last_height_mm=1e30f;
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
  assert(!terrain_reading_from(2,&bad[i],&r));assert(!memcmp(&r,&kept,sizeof r));
 }
 terrain_source_t edge=s;edge.roll_change_deg=-25.54f;edge.humps=255;edge.highest_height_mm=51.14f;
 assert(terrain_reading_from(3,&edge,&r) && r.roll_decideg==-255 && r.highest_height_dmm==511);
 puts("PASS terrain converter: rounds Ezra's float status to 0.1 deg / 0.1 mm");
}
static void obstacle_protocol(void)
{
 /* Hand-packed: active, phase 9 (AVOID_SCAN_SIDE), servo 125, range valid
  * 1234 mm, detected 2, completed 1. */
 const uint8_t expected[OBSTACLE_RECORD_BYTES]={0x44,0x33,0x22,0x11,0x53,0xbf,0x34,0x09,0x04,0x00};
 obstacle_reading_t r={.ms=0x11223344u,.active=true,.phase=9,.servo_deg=125,.range_valid=true,
                       .range_mm=1234,.detected=2,.completed=1};
 uint8_t b[OBSTACLE_RECORD_BYTES];
 assert(obstacle_record_encode(&r,b) && !memcmp(b,expected,sizeof b));
 obstacle_reading_t bad=r;bad.phase=32;assert(!obstacle_record_encode(&bad,b));
 bad=r;bad.servo_deg=128;assert(!obstacle_record_encode(&bad,b));
 bad=r;bad.range_mm=4096;assert(!obstacle_record_encode(&bad,b));
 obstacle_reading_t edge=r;edge.phase=31;edge.servo_deg=127;edge.range_mm=4095;assert(obstacle_record_encode(&edge,b));
 r.range_valid=false;assert(obstacle_record_encode(&r,b) && b[5]==0x1f && b[6]==0x00 && b[7]==0x08);
 r.active=false;assert(obstacle_record_encode(&r,b));
 for(unsigned i=4;i<OBSTACLE_RECORD_BYTES;i++)assert(b[i]==0);
 puts("PASS obstacle record: hand-packed phase, servo, range and episode counts");
}
static void hump_events(void)
{
 const uint8_t expected[HUMP_EVENT_BYTES]={0x0d,0x0c,0x0b,0x0a,0x03,0x7e,0x04,0x00};
 uint8_t b[HUMP_EVENT_BYTES];
 assert(hump_event_record_encode(0x0A0B0C0Du,3,126,2,b) && !memcmp(b,expected,sizeof b));
 puts("PASS hump event record: hand-packed index, height and dropped count");
 hump_queue_t q={0};
 assert(!hump_queue_peek(&q,b));
 assert(hump_queue_push(&q,100,1,50) && hump_queue_push(&q,200,2,60));
 assert(hump_queue_peek(&q,b) && b[0]==100 && b[4]==1);
 assert(hump_queue_peek(&q,b) && b[4]==1);
 hump_queue_pop(&q);
 assert(hump_queue_peek(&q,b) && b[0]==200 && b[4]==2);
 hump_queue_pop(&q);
 assert(!hump_queue_peek(&q,b));
 hump_queue_pop(&q);
 assert(!hump_queue_peek(&q,b));
 puts("PASS hump queue: first in, first out; peek repeats until pop");
 assert(!hump_queue_push(&q,1,1,512) && !hump_queue_peek(&q,b));
 for(uint8_t i=1;i<=10;i++)assert(hump_queue_push(&q,i,i,i));
 for(uint8_t i=3;i<=10;i++){
  assert(hump_queue_peek(&q,b) && b[4]==i && (b[6]>>1)==2);
  hump_queue_pop(&q);
 }
 assert(!hump_queue_peek(&q,b));
 for(unsigned i=0;i<300;i++)assert(hump_queue_push(&q,i,(uint8_t)i,0));
 assert(hump_queue_peek(&q,b) && b[4]==(uint8_t)292 && ((b[6]>>1)|((b[7]&1)<<7))==255);
 puts("PASS hump queue overflow: oldest dropped, loss count kept and saturates at 255");
}
#ifdef RTOS_TERRAIN
static unsigned hump_sends(unsigned from,uint8_t *last)
{
 unsigned n=0;
 for(unsigned i=from;i<transmitted;++i) if(strstr(sent[i].topic,"/hump_event")) {
  assert(sent[i].length==HUMP_EVENT_BYTES);memcpy(last,sent[i].bytes,HUMP_EVENT_BYTES);++n;
 }
 return n;
}
static void ack_hump(err_t e){mqtt_request_cb_t cb=hump_puback;assert(cb);hump_puback=NULL;cb(hump_puback_context,e);}
static void hump_network(void)
{
 uint8_t b[HUMP_EVENT_BYTES];online(60000);
 assert(network_hump_event(10,1,50) && network_hump_event(20,2,60));
 unsigned from=transmitted;step(60001);
 assert(hump_sends(from,b)==1 && b[0]==10 && b[4]==1 && hump_puback);
 from=transmitted;step(60006);assert(hump_sends(from,b)==0); /* one in flight */
 ack_hump(ERR_OK);from=transmitted;step(60011);
 assert(hump_sends(from,b)==1 && b[4]==2);
 client_connected=false;step(60016);assert(network_state()==NETWORK_RETRYING && !hump_puback);
 online(61000);from=transmitted;step(61001);
 assert(hump_sends(from,b)==1 && b[4]==2); /* unacknowledged event replayed */
 for(uint8_t i=3;i<=10;i++)assert(network_hump_event(i*10u,i,0)); /* overwrites 2 */
 ack_hump(ERR_OK);from=transmitted;step(61006);
 assert(hump_sends(from,b)==1 && b[4]==3 && (b[6]>>1)==1); /* stale ACK popped nothing */
 ack_hump(ERR_ARG);from=transmitted;step(61011);
 assert(hump_sends(from,b)==1 && b[4]==3); /* failed PUBACK retried */
 ack_hump(ERR_OK);
 puts("PASS hump events: QoS1 one at a time, advance on PUBACK, replay after reconnect, stale ACK ignored");
}
#endif
#ifdef RTOS_OBSTACLE
static void obstacle_network(void)
{
 online(70000);unsigned from=transmitted;step(70001);
 for(unsigned i=from;i<transmitted;++i){
  assert(!strstr(sent[i].topic,"/obstacle")); /* nothing until Motion reports */
  if(strstr(sent[i].topic,"/status"))assert(!(sent[i].bytes[8]&16u));
 }
 obstacle_reading_t r={.ms=70002,.active=true,.phase=9,.servo_deg=125,.range_valid=true,.range_mm=1234,.detected=1};
 uint8_t expected[OBSTACLE_RECORD_BYTES];assert(obstacle_record_encode(&r,expected));
 assert(network_obstacle(&r));
 obstacle_reading_t bad=r;bad.servo_deg=200;assert(!network_obstacle(&bad));
 flags|=FORCE_STATUS;from=transmitted;step(70300);
 bool seen=false,status=false;
 for(unsigned i=from;i<transmitted;++i){
  if(strstr(sent[i].topic,"/obstacle")){seen=true;assert(sent[i].length==OBSTACLE_RECORD_BYTES && !memcmp(sent[i].bytes,expected,sizeof expected));}
  if(strstr(sent[i].topic,"/status")){status=true;assert(sent[i].bytes[8]&16u);}
 }
 assert(seen && status);
 puts("PASS obstacle network: snapshot published with motion, rejected reading kept previous, status bit4");
}
#endif
int main(void)
{
 assert(network_wifi_pm_result()==INT8_MIN);
 init_result=-1;assert(!initialize_radio() && sta_calls==0 && pm_calls==0);
 init_result=0;assert(initialize_radio() && network_wifi_pm_result()==0 && pm_calls==1);
 pm_result=-1;assert(initialize_radio() && network_wifi_pm_result()==-1 && pm_calls==2);
 pm_result=0;
 protocols();terrain_protocol();terrain_converter();hump_events();obstacle_protocol();uint16_t raw[3]={191,2908,141};assert(network_line(0,raw,2,true));
 online(UINT32_MAX-500);step(0);
 bool motion_seen=false;
 for(unsigned i=0;i<transmitted;++i) if(strstr(sent[i].topic,"/motion")) {
  motion_seen=true;assert(sent[i].length==13 && sent[i].bytes[0]==0 && sent[i].bytes[12]==1);
 }
 assert(motion_seen); /* strlen would have sent zero bytes */
#ifdef RTOS_TERRAIN
 terrain_reading_t hump={.ms=7,.valid=true,.calibrated=true,.roll_decideg=-25,.humps=1,
                         .last_height_dmm=100,.highest_height_dmm=100};
 uint8_t hump_bytes[TERRAIN_RECORD_BYTES];assert(terrain_record_encode(&hump,hump_bytes));
 assert(network_terrain(&hump));unsigned terrain_from=transmitted;step(300);
 bool terrain_seen=false;
 for(unsigned i=terrain_from;i<transmitted;++i) if(strstr(sent[i].topic,"/terrain")) {
  terrain_seen=true;assert(sent[i].length==TERRAIN_RECORD_BYTES && !memcmp(sent[i].bytes,hump_bytes,TERRAIN_RECORD_BYTES));
 }
 assert(terrain_seen);
 terrain_reading_t rejected=hump;rejected.roll_decideg=300;assert(!network_terrain(&rejected));
 unsigned kept_from=transmitted;step(600);terrain_seen=false;
 for(unsigned i=kept_from;i<transmitted;++i) if(strstr(sent[i].topic,"/terrain")) {
  terrain_seen=true;assert(!memcmp(sent[i].bytes,hump_bytes,TERRAIN_RECORD_BYTES));
 }
 assert(terrain_seen); /* a rejected reading keeps the previous snapshot */
#endif
 assert(network_barcode(0,'Z',true,false));step(1000);
 bool z_seen=false;
 for(unsigned i=0;i<transmitted;++i) if(strstr(sent[i].topic,"/barcode") && sent[i].bytes[4]==58) z_seen=true;
 assert(z_seen);
#ifndef RTOS_TERRAIN
 for(unsigned i=0;i<transmitted;++i) assert(!strstr(sent[i].topic,"/terrain"));
 puts("PASS default build: no terrain topic");
#else
 unsigned status_before=0,status_after=0;
 for(unsigned i=0;i<transmitted;++i) if(strstr(sent[i].topic,"/status")) {
  if(i<terrain_from){assert(!(sent[i].bytes[8]&8u));++status_before;}
  else {assert(sent[i].bytes[8]&8u);++status_after;}
 }
 assert(status_before && status_after); /* bit3: terrain available */
 puts("PASS terrain network: snapshot published with motion, rejected reading kept previous, status bit3");
#endif
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
 mock_scan_stopped=false;online(40000);limited_requests=true;request_budget=4;step(40005);
 assert(runtime_slot==0 && request_budget==0); /* status consumes all four */
 unsigned before_slot=runtime_slot;step(40010);assert(runtime_slot==before_slot);
 unsigned seen=0;
 for(unsigned tick=1;tick<=12;tick++){
  request_budget=4;unsigned first=transmitted;step(40005+tick*250u);
  for(unsigned i=first;i<transmitted;i++)if(strstr(sent[i].topic,"/runtime_diag")){
   unsigned row=sent[i].bytes[0]?1u+(sent[i].bytes[7]&3u):0u;seen|=1u<<row;
  }
 }
 assert(seen==31u);limited_requests=false;
 puts("PASS four-handle MQTT budget: blocked runtime row retained; paced summary and all four tasks eventually delivered after motion/status traffic");
#ifdef RTOS_TERRAIN
 hump_network();
#endif
#ifdef RTOS_OBSTACLE
 obstacle_network();
#else
 for(unsigned i=0;i<transmitted;++i) assert(!strstr(sent[i].topic,"/obstacle"));
#endif
 puts("PASS actual MQTT service: explicit byte lengths, Z preservation, STOP-only commands, no start topic, period changes, backpressure, reconnect, subscription failure, clock wrap, signed Wi-Fi diagnostics");
 for(unsigned i=0;i<transmitted;++i) {
  printf("PACKET %s ",sent[i].topic);
  for(size_t j=0;j<sent[i].length;++j)printf("%02x",sent[i].bytes[j]);
  puts("");
 }
 return 0;
}
