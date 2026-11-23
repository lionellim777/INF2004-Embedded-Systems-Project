#ifndef NETWORK_TEST_SDK_H
#define NETWORK_TEST_SDK_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef int err_t;
typedef int mqtt_connection_status_t;
typedef struct { int dummy; } mqtt_client_t;
typedef struct { uint32_t addr; } ip_addr_t;
typedef struct { int dummy; } cyw43_t;
extern cyw43_t cyw43_state;
enum { ERR_OK, ERR_ARG, MQTT_CONNECT_ACCEPTED=0, MQTT_DATA_FLAG_LAST=1,
       CYW43_LINK_UP=3, CYW43_ITF_STA=0, CYW43_AUTH_WPA2_AES_PSK=1 };
enum { MQTT_CONNECT_DISCONNECTED=256, MQTT_CONNECT_TIMEOUT=257 };
#define CYW43_NONE_PM 16u
typedef void (*mqtt_request_cb_t)(void *,err_t);
struct mqtt_connect_client_info_t {
 const char *client_id,*client_user,*client_pass;
 uint16_t keep_alive;
 const char *will_topic,*will_msg;
 uint8_t will_qos,will_retain;
};
uint32_t save_and_disable_interrupts(void);
void restore_interrupts(uint32_t state);
int cyw43_arch_init(void);
void cyw43_arch_enable_sta_mode(void);
int cyw43_wifi_pm(cyw43_t *,uint32_t);
void cyw43_arch_poll(void);
int cyw43_arch_wifi_connect_async(const char *,const char *,uint32_t);
int cyw43_tcpip_link_status(cyw43_t *,int);
mqtt_client_t *mqtt_client_new(void);
void mqtt_disconnect(mqtt_client_t *);
bool mqtt_client_is_connected(mqtt_client_t *);
void mqtt_set_inpub_callback(mqtt_client_t *,void (*)(void *,const char *,u32_t),void (*)(void *,const u8_t *,u16_t,u8_t),void *);
err_t mqtt_client_connect(mqtt_client_t *,const ip_addr_t *,uint16_t,void (*)(mqtt_client_t *,void *,mqtt_connection_status_t),void *,const struct mqtt_connect_client_info_t *);
err_t mqtt_subscribe(mqtt_client_t *,const char *,uint8_t,mqtt_request_cb_t,void *);
err_t mqtt_publish(mqtt_client_t *,const char *,const void *,u16_t,u8_t,u8_t,mqtt_request_cb_t,void *);
int ipaddr_aton(const char *,ip_addr_t *);
uint32_t get_rand_32(void);
uint32_t time_us_32(void);
uint64_t get_absolute_time(void);
uint32_t to_ms_since_boot(uint64_t);
#endif
