#ifndef RTOS_NETWORK_SERVICE_H
#define RTOS_NETWORK_SERVICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { NETWORK_UNCONFIGURED, NETWORK_JOINING, NETWORK_CONNECTING,
       NETWORK_SUBSCRIBING, NETWORK_ONLINE, NETWORK_RETRYING };
void network_task(int start_code,void *context);
uint8_t network_state(void);
uint8_t network_error(void);
int8_t network_wifi_link(void);
/* Latest barcode snapshot, not an unbounded/offline event queue. Thread-safe
 * on core0; rejected writes leave the previous snapshot unchanged. */
bool network_barcode(uint32_t ms,char letter,bool valid,bool simulated);
#endif
