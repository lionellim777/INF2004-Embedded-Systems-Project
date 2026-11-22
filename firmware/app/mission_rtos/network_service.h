#ifndef RTOS_NETWORK_SERVICE_H
#define RTOS_NETWORK_SERVICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum
{
    NETWORK_UNCONFIGURED,
    NETWORK_JOINING,
    NETWORK_CONNECTING,
    NETWORK_SUBSCRIBING,
    NETWORK_ONLINE,
    NETWORK_RETRYING
};
void network_task(int start_code, void *context);
uint8_t network_state(void);
uint8_t network_error(void);
int8_t network_wifi_link(void);
int8_t network_wifi_pm_result(void);
bool network_line(uint32_t ms, const uint16_t raw[3], uint8_t pattern,
                  bool valid);
/* Latest barcode snapshot, not an unbounded/offline event queue. Thread-safe
 * on core0; rejected writes leave the previous snapshot unchanged. */
bool network_barcode(uint32_t ms, char letter, bool valid, bool simulated);
/* RTOS_TERRAIN builds only (default OFF). Latest terrain snapshot, sent with
 * motion/line at the motion period; status mask bit3 while valid. Same core0
 * rule: rejected readings leave the previous snapshot unchanged. */
typedef struct terrain_reading terrain_reading_t;
bool network_terrain(const terrain_reading_t *reading);
/* RTOS_TERRAIN builds only. Queue one completed hump for QoS1 delivery on
 * hump_event; up to 8 survive an outage, older ones are dropped and counted.
 * False only for heights above 511 (0.1 mm). */
bool network_hump_event(uint32_t ms, uint8_t index, uint16_t height_dmm);
/* RTOS_OBSTACLE builds only. Latest bypass state from Motion, sent with
 * motion at the motion period; status mask bit4 while active. */
typedef struct obstacle_reading obstacle_reading_t;
bool network_obstacle(const obstacle_reading_t *reading);
#endif
