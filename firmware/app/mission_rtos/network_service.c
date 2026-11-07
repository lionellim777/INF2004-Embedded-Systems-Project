/* Binary adaptation of Buddy1's single-owner async Wi-Fi/MQTT lifecycle.
 * Reference: wifi-buddy-1 0a7604d, src/comm.c. The bridge and ownership policy
 * are reused; text serialization and large generic sample/event stores are
 * replaced by validated binary snapshots. No networking in Motion or IRQs. */
#include "network_service.h"
#include "binary_protocol.h"
#include "board.h"
#include "port/mtk_bridge.h"
#include "network_settings.h"
#include "pico/cyw43_arch.h"
#include "pico/rand.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "lwip/apps/mqtt.h"
#include "lwip/ip_addr.h"
#include <string.h>

enum { INITIALIZED=1u, CONNECTED=2u, FAILED=4u, FORCE_STATUS=8u,
       SUB_SHIFT=4u, SUB_MASK=48u, RESULT_PENDING=64u, RETRY_SHIFT=7u, RETRY_MASK=896u,
       ERROR_SHIFT=10u,ERROR_MASK=15360u };
/* policy: period bits0..12 (200..5000), state13..15. flags: above, error10..13;
 * bits14..15 zero.
 * Network task is the sole writer; observer reads an aligned halfword. */
static volatile uint16_t policy=200u;
/* Keep the complete radio path compiled even in the default unconfigured
 * candidate. No radio init/association is attempted until local configuration. */
static const volatile uint8_t network_configured=RTOS_NETWORK_CONFIGURED;
static volatile uint16_t flags; /* one network writer; aligned diagnostic reads */
static uint32_t state_ms, status_ms, motion_ms, online_ms, boot_id;
static uint8_t barcode_latest[BARCODE_RECORD_BYTES], result;
static binary_command_frame_t command_frame;
static mqtt_client_t *client;
static struct mqtt_connect_client_info_t info;
static ip_addr_t address;
static const char motion_topic[]=RTOS_TOPIC_PREFIX "motion";
static const char barcode_topic[]=RTOS_TOPIC_PREFIX "barcode";
static const char status_topic[]=RTOS_TOPIC_PREFIX "status";
static const char link_topic[]=RTOS_TOPIC_PREFIX "link";
static const char status_command[]=RTOS_TOPIC_PREFIX "command/get_status";
static const char period_command[]=RTOS_TOPIC_PREFIX "command/set_period_ms";
static const char stop_command[]=RTOS_TOPIC_PREFIX "command/stop";
static const char result_topic[]=RTOS_TOPIC_PREFIX "command_result";

uint8_t network_state(void) { return (uint8_t)(policy>>13u); }
uint8_t network_error(void) { return (uint8_t)((flags&ERROR_MASK)>>ERROR_SHIFT); }
static void error_code(uint8_t code)
{ flags=(uint16_t)((flags&~ERROR_MASK) | ((uint16_t)code<<ERROR_SHIFT)); }
static void state(uint8_t value,uint32_t now)
{ policy=(uint16_t)((policy&8191u) | ((uint16_t)value<<13u)); state_ms=now; }
bool network_barcode(uint32_t ms,char letter,bool valid,bool simulated)
{
    uint8_t bytes[BARCODE_RECORD_BYTES];
    if (!barcode_record_encode(ms,letter,valid,simulated,bytes)) return false;
    uint32_t irq=save_and_disable_interrupts();
    memcpy(barcode_latest,bytes,sizeof bytes);
    restore_interrupts(irq); return true;
}
static void read_barcode(uint8_t bytes[BARCODE_RECORD_BYTES])
{
    uint32_t irq=save_and_disable_interrupts();
    memcpy(bytes,barcode_latest,BARCODE_RECORD_BYTES);
    restore_interrupts(irq);
}
static void fail(uint32_t now,uint8_t code)
{
    if (client!=NULL) mqtt_disconnect(client);
    flags&=(uint16_t)~(CONNECTED|FAILED|SUB_MASK|RESULT_PENDING);
    error_code(code); /* cancellation callbacks must not replace the root cause */
    command_frame.state=0;
    state(NETWORK_RETRYING,now);
}
static void connection(mqtt_client_t *c,void *context,mqtt_connection_status_t result_code)
{
    (void)c; (void)context;
    if (network_state()!=NETWORK_CONNECTING) return;
    if (result_code==MQTT_CONNECT_ACCEPTED) flags|=CONNECTED;
    else {
        error_code(result_code==MQTT_CONNECT_TIMEOUT?9:
            result_code==MQTT_CONNECT_DISCONNECTED?11:7);
        flags|=FAILED;
    }
}
static void subscribed(void *context,err_t error)
{
    (void)context;
    if (network_state()!=NETWORK_SUBSCRIBING) return;
    unsigned count=(flags&SUB_MASK)>>SUB_SHIFT;
    if (error!=ERR_OK || count>=3) { error_code(8);flags|=FAILED; }
    else flags=(uint16_t)((flags&~SUB_MASK) | ((count+1u)<<SUB_SHIFT));
}
static void incoming(void *context,const char *topic,u32_t length)
{
    (void)context;
    uint8_t kind=topic==NULL?0:!strcmp(topic,status_command)?BINARY_COMMAND_STATUS:
        !strcmp(topic,period_command)?BINARY_COMMAND_PERIOD:
        !strcmp(topic,stop_command)?BINARY_COMMAND_STOP:0;
    (void)binary_command_begin(&command_frame,kind,length);
}
static void data(void *context,const u8_t *bytes,u16_t length,u8_t data_flags)
{
    (void)context;
    bool last=(data_flags&MQTT_DATA_FLAG_LAST)!=0;
    uint8_t kind=command_frame.state&3u;
    binary_command_t command;
    bool accepted=binary_command_append(&command_frame,bytes,length,last,&command);
    if (last) {
        result=(uint8_t)((kind<<1u) | (accepted?1u:0u));
        flags|=RESULT_PENDING;
        if (accepted) {
            if (command.kind==BINARY_COMMAND_STOP) board_stop(STOP_BUTTON);
            else if (command.kind==BINARY_COMMAND_STATUS) flags|=FORCE_STATUS;
            else if (command.kind==BINARY_COMMAND_PERIOD)
                policy=(uint16_t)((policy&0xe000u) | command.period_ms);
        }
    }
}
static bool publish(const char *topic,const uint8_t *bytes,size_t length,uint8_t retain)
{
    /* Length is bytes, never strlen: valid packets routinely contain zeros.
     * lwIP copies the supplied bytes into its MQTT output ring before return. */
    return length<=UINT16_MAX && mqtt_publish(client,topic,bytes,(u16_t)length,0,retain,NULL,NULL)==ERR_OK;
}
static void join(uint32_t now)
{
    if (cyw43_arch_wifi_connect_async(RTOS_WIFI_SSID,RTOS_WIFI_PASSWORD,CYW43_AUTH_WPA2_AES_PSK)==0)
        state(NETWORK_JOINING,now);
    else fail(now,2);
}
static void connect_broker(uint32_t now)
{
    if (client==NULL) client=mqtt_client_new();
    if (client==NULL || !ipaddr_aton(RTOS_BROKER_IPV4,&address)) { fail(now,5); return; }
    /* SDK2.1.1 lwIP only accepts a NUL-terminated will. A one-byte binary
     * link code avoids embedded-NUL truncation without modifying the SDK:
     * 0 unknown, 1 offline, 2 online. Numeric code, not an ASCII digit. */
    info=(struct mqtt_connect_client_info_t){.client_id="aai10-rtos-motion",
        .client_user=RTOS_BROKER_USER,.client_pass=RTOS_BROKER_PASSWORD,
        .keep_alive=15,.will_topic=link_topic,.will_msg="\x01",
        .will_qos=1,.will_retain=1};
    mqtt_set_inpub_callback(client,incoming,data,NULL);
    flags&=(uint16_t)~(CONNECTED|FAILED|SUB_MASK);
    state(NETWORK_CONNECTING,now);
    if (mqtt_client_connect(client,&address,RTOS_BROKER_PORT,connection,NULL,&info)!=ERR_OK) fail(now,6);
}
static void step(uint32_t now)
{
    uint8_t current=network_state();
    int link=cyw43_tcpip_link_status(&cyw43_state,CYW43_ITF_STA);
    if ((flags&FAILED) || (current!=NETWORK_JOINING && current!=NETWORK_RETRYING && link!=CYW43_LINK_UP)) {
        fail(now,(flags&FAILED)?network_error():4); return;
    }
    if (current==NETWORK_JOINING) {
        if (link==CYW43_LINK_UP) connect_broker(now);
        else if ((uint32_t)(now-state_ms)>=15000u) fail(now,3);
    } else if (current==NETWORK_CONNECTING) {
        if (flags&CONNECTED) {
            flags&=(uint16_t)~SUB_MASK; state(NETWORK_SUBSCRIBING,now);
            if (mqtt_subscribe(client,status_command,1,subscribed,NULL)!=ERR_OK ||
                mqtt_subscribe(client,period_command,1,subscribed,NULL)!=ERR_OK ||
                mqtt_subscribe(client,stop_command,1,subscribed,NULL)!=ERR_OK) fail(now,8);
        } else if ((uint32_t)(now-state_ms)>=10000u) fail(now,9);
    } else if (current==NETWORK_SUBSCRIBING) {
        if (((flags&SUB_MASK)>>SUB_SHIFT)==3u) {
            state(NETWORK_ONLINE,now); online_ms=now; status_ms=now; motion_ms=now;
            flags|=FORCE_STATUS;
            error_code(0);
        } else if ((uint32_t)(now-state_ms)>=10000u) fail(now,10);
    } else if (current==NETWORK_ONLINE) {
        if (!mqtt_client_is_connected(client)) { fail(now,11); return; }
        if ((uint32_t)(now-online_ms)>=30000u) flags&=(uint16_t)~RETRY_MASK;
        if ((flags&FORCE_STATUS) || (uint32_t)(now-status_ms)>=1000u) {
            uint8_t barcode[5],status_bytes[10],motion[MOTION_RECORD_BYTES]; read_barcode(barcode);
            board_read(motion);
            uint8_t available=(uint8_t)((motion[12]&1u) | ((barcode[4]&32u)?2u:0u));
            uint8_t simulated=(barcode[4]&64u)?2u:0u;
            (void)status_record_encode(now,boot_id,available,simulated,NETWORK_ONLINE,status_bytes);
            uint8_t online_code=2;
            bool status_ok=publish(status_topic,status_bytes,sizeof status_bytes,1);
            bool link_ok=publish(link_topic,&online_code,1,1);
            if (status_ok && link_ok) {
                flags&=(uint16_t)~FORCE_STATUS; status_ms=now;
            }
            (void)publish(barcode_topic,barcode,sizeof barcode,0);
        }
        if (flags&RESULT_PENDING)
            if (publish(result_topic,&result,1,0)) flags&=(uint16_t)~RESULT_PENDING;
        if ((uint32_t)(now-motion_ms)>=(policy&8191u)) {
            uint8_t motion[MOTION_RECORD_BYTES]; board_read(motion);
            if (publish(motion_topic,motion,sizeof motion,0)) motion_ms=now;
        }
    } else if (current==NETWORK_RETRYING) {
        unsigned shift=(flags&RETRY_MASK)>>RETRY_SHIFT;
        uint32_t delay=1000u<<shift; if (delay>30000u) delay=30000u;
        if ((uint32_t)(now-state_ms)>=delay) {
            if (shift<5) flags=(uint16_t)((flags&~RETRY_MASK) | ((shift+1u)<<RETRY_SHIFT));
            if (link==CYW43_LINK_UP) connect_broker(now); else join(now);
        }
    }
}
void network_task(int start_code,void *context)
{
    (void)start_code; (void)context;
    boot_id=get_rand_32();
    for (;;) {
        uint32_t now=to_ms_since_boot(get_absolute_time());
        if (network_configured) {
            if (!(flags&INITIALIZED)) {
                if (network_state()!=NETWORK_RETRYING || (uint32_t)(now-state_ms)>=1000u) {
                    if (cyw43_arch_init()==0) {
                        flags|=INITIALIZED; cyw43_arch_enable_sta_mode(); join(now);
                    } else fail(now,1);
                }
            } else { cyw43_arch_poll(); step(now); }
        }
        mtk_bridge_delay_ms(network_configured?5:1000);
    }
}
