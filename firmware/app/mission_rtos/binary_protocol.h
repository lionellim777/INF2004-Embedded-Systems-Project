#ifndef RTOS_BINARY_PROTOCOL_H
#define RTOS_BINARY_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Transport topics carry version v1. Byte padding is zero and validated.
 * Barcode: capture_ms32, code5 (0 none,1..26 A..Z,27 unreadable), valid1,
 * simulated1, padding1. Status: capture_ms32, boot_id32, available5,
 * simulated5, connection_state3, padding3. Simulation is a subset of available. */
#define BARCODE_RECORD_BYTES 5u
#define STATUS_RECORD_BYTES 10u
/* Line: time32, ADC12x3 in GP26/GP28/GP27 order, pattern3, valid1. */
#define LINE_RECORD_BYTES 9u
bool line_record_encode(uint32_t ms,const uint16_t raw[3],uint8_t pattern,
                        bool valid,uint8_t out[LINE_RECORD_BYTES]);
/* Terrain: time32, then LSB-first valid1, calibrated1, roll change signed9
 * (0.1 deg, -255..255), humps8, last and highest hump height9 each
 * (0.1 mm, 0..511), padding3, then one more byte: motion state3 (0 stationary,
 * 1 accelerating, 2 cruising, 3 decelerating, 4 turning, 5 climbing hump,
 * 6 descending hump), tilt alert active1, tilt alerts this run4 (saturating at 15).
 * Out-of-range values are rejected, not clipped (tilt alerts saturate).
 * Hump count and heights are results of the run: they are kept while the sensor
 * reading is invalid. Invalid readings (valid=0) zero the live values only:
 * roll change and the tilt fields. The motion state comes from the wheel
 * encoders and is always sent. */
#define TERRAIN_RECORD_BYTES 10u
typedef struct terrain_reading {
    uint32_t ms;
    int16_t roll_decideg;
    uint16_t humps, last_height_dmm, highest_height_dmm;
    uint8_t motion_state, tilt_alerts;
    bool valid, calibrated, tilt_active;
} terrain_reading_t;
bool terrain_record_encode(const terrain_reading_t *reading, uint8_t out[TERRAIN_RECORD_BYTES]);
/* Hump event: completion time32, then LSB-first hump number8, height9
 * (0.1 mm, 0..511), events dropped so far8 (saturating), padding7. */
#define HUMP_EVENT_BYTES 8u
bool hump_event_record_encode(uint32_t ms, uint8_t index, uint16_t height_dmm,
                              uint8_t dropped, uint8_t out[HUMP_EVENT_BYTES]);
/* Obstacle: time32, then LSB-first active1, bypass phase5, servo angle7 (deg),
 * range valid1, range12 (mm), episodes detected8 and completed8, padding6.
 * Inactive records zero every field after time; invalid range zeroes range. */
#define OBSTACLE_RECORD_BYTES 10u
typedef struct obstacle_reading {
    uint32_t ms;
    uint16_t range_mm;
    uint8_t phase, servo_deg, detected, completed;
    bool active, range_valid;
} obstacle_reading_t;
bool obstacle_record_encode(const obstacle_reading_t *reading, uint8_t out[OBSTACLE_RECORD_BYTES]);
bool barcode_record_encode(uint32_t ms, char letter, bool valid, bool simulated,
                           uint8_t out[BARCODE_RECORD_BYTES]);
bool barcode_record_valid(const uint8_t *bytes, size_t size);
bool status_record_encode(uint32_t ms, uint32_t boot_id, uint8_t available,
    uint8_t simulated, uint8_t state, uint8_t out[STATUS_RECORD_BYTES]);

enum { BINARY_COMMAND_INVALID, BINARY_COMMAND_STATUS, BINARY_COMMAND_PERIOD, BINARY_COMMAND_STOP };
/* Shared command state is network-task-owned. Maximum two payload bytes.
 * state bits: kind0..1, active2, expected3..4, received5..6, padding7. */
typedef struct { uint8_t payload[2], state; } binary_command_frame_t;
typedef struct { uint16_t period_ms; uint8_t kind; } binary_command_t;
bool binary_command_begin(binary_command_frame_t *f, uint8_t kind, size_t length);
bool binary_command_append(binary_command_frame_t *f, const uint8_t *bytes,
                           size_t length, bool last, binary_command_t *out);
#endif
