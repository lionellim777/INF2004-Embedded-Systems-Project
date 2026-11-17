#ifndef RTOS_TERRAIN_TELEMETRY_H
#define RTOS_TERRAIN_TELEMETRY_H
#include "binary_protocol.h"
#include <stdbool.h>
#include <stdint.h>

/* Fields copied from Ezra's terrain_get_status() (origin/ezra/ramp):
 * available, calibrated, pitch_valid -> sample_valid, current_roll_change_deg,
 * roll_hump_count, last_roll_hump_mm, highest_roll_hump_mm. */
typedef struct {
    bool available, calibrated, sample_valid;
    float roll_change_deg;
    uint32_t humps;
    float last_height_mm, highest_height_mm;
    /* From terrain_status_t of the current firmware: motion_state (a motion_class_t value),
     * tilt_alert_active, tilt_alert_count (counted this run). */
    uint8_t motion_state;
    bool tilt_active;
    uint32_t tilt_alerts;
} terrain_source_t;

/* Rounds to the terrain/v1 units. Roll beyond +/-25.5 degrees saturates. Returns false,
 * leaving out untouched, for NaN or other values outside the record ranges; the caller then
 * keeps the last published snapshot, whose capture time stops advancing. */
bool terrain_reading_from(uint32_t ms, const terrain_source_t *source, terrain_reading_t *out);

/* Completed humps waiting for MQTT acknowledgement. When full, the oldest
 * event is overwritten and counted in every later record's dropped field. */
#define HUMP_QUEUE_CAPACITY 8u
typedef struct {
    uint32_t ms[HUMP_QUEUE_CAPACITY];
    uint16_t height_dmm[HUMP_QUEUE_CAPACITY];
    uint8_t index[HUMP_QUEUE_CAPACITY];
    uint8_t head, count, dropped;
} hump_queue_t;
bool hump_queue_push(hump_queue_t *q, uint32_t ms, uint8_t index, uint16_t height_dmm);
bool hump_queue_peek(const hump_queue_t *q, uint8_t out[HUMP_EVENT_BYTES]);
void hump_queue_pop(hump_queue_t *q);
#endif
