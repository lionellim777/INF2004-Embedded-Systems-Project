#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TELEMETRY_JSON_SIZE 1024U
#define TELEMETRY_WHEEL_HISTORY_CAPACITY 26U
#define TELEMETRY_WHEEL_CAPTURE_MAX_MS 1800000U

/* Speed: bits 0-9, two's complement. Capture ms: 10-30. Bit 31: zero. */
typedef struct
{
    uint32_t records[TELEMETRY_WHEEL_HISTORY_CAPACITY];
    /* Bits 0-4: oldest; 5-9: count; 10: recovery pending; others zero. */
    uint16_t state;
} telemetry_wheel_history_t;

bool telemetry_wheel_record_pack(int32_t speed_mm_s, uint32_t captured_ms,
                                 uint32_t *p_record);
bool telemetry_wheel_record_unpack(uint32_t record, int32_t *p_speed_mm_s,
                                   uint32_t *p_captured_ms);
/* Zero-initialize each wheel's history; synchronize shared access. */
/* Use capture-time order; equal speeds keep their original timestamp. */
bool telemetry_wheel_history_append(telemetry_wheel_history_t *p_history,
                                    int32_t speed_mm_s, uint32_t captured_ms);
uint8_t telemetry_wheel_history_count(
    telemetry_wheel_history_t const *p_history);
/* Index zero is the oldest retained record; a read does not remove it. */
bool telemetry_wheel_history_get(telemetry_wheel_history_t const *p_history,
                                 size_t index, int32_t *p_speed_mm_s,
                                 uint32_t *p_captured_ms);

typedef enum
{
    TELEMETRY_MISSION_UNKNOWN,
    TELEMETRY_MISSION_IDLE,
    TELEMETRY_MISSION_RUNNING,
    TELEMETRY_MISSION_STOPPED
} telemetry_mission_state_t;

typedef enum
{
    TELEMETRY_MOTION_UNKNOWN,
    TELEMETRY_MOTION_STATIONARY,
    TELEMETRY_MOTION_ACCELERATING,
    TELEMETRY_MOTION_TURNING,
    TELEMETRY_MOTION_CLIMBING,
    TELEMETRY_MOTION_DESCENDING,
    TELEMETRY_MOTION_IMPACT
} telemetry_motion_state_t;

typedef enum
{
    TELEMETRY_SOURCE_MISSION,
    TELEMETRY_SOURCE_MOTION,
    TELEMETRY_SOURCE_LINE,
    TELEMETRY_SOURCE_TERRAIN,
    TELEMETRY_SOURCE_OBSTACLE
} telemetry_source_t;

typedef struct
{
    bool b_is_valid;
    bool b_is_simulated;
    uint32_t captured_ms;
    telemetry_mission_state_t state;
} telemetry_mission_t;

typedef struct
{
    bool b_is_valid;
    bool b_is_simulated;
    uint32_t captured_ms;
    int32_t left_mm_s;
    int32_t right_mm_s;
    int32_t left_ticks;
    int32_t right_ticks;
    uint32_t distance_mm;
} telemetry_motion_t;

/* Caller synchronizes access. Invalid samples refresh current, not history. */
/* Malformed valid samples return false without changing current or history. */
bool telemetry_apply_motion_sample(
    telemetry_motion_t *p_current, telemetry_wheel_history_t *p_left,
    telemetry_wheel_history_t *p_right, telemetry_motion_t const *p_sample);

typedef struct
{
    bool b_is_valid;
    bool b_is_simulated;
    uint32_t captured_ms;
    uint16_t ir_left;
    uint16_t ir_middle;
    uint16_t ir_right;
    bool b_is_on_line;
    char barcode;
} telemetry_line_t;

typedef struct
{
    bool b_is_valid;
    bool b_is_simulated;
    uint32_t captured_ms;
    telemetry_motion_state_t motion;
    int32_t hump_mm;
    int32_t highest_hump_mm;
} telemetry_terrain_t;

typedef struct
{
    bool b_is_valid;
    bool b_is_simulated;
    uint32_t captured_ms;
    uint32_t scan_id;
    uint32_t closest_mm;
    uint32_t width_mm;
    uint32_t left_clearance_mm;
    uint32_t right_clearance_mm;
} telemetry_obstacle_t;

typedef struct
{
    telemetry_mission_t mission;
    telemetry_motion_t motion;
    telemetry_line_t line;
    telemetry_terrain_t terrain;
    telemetry_obstacle_t obstacle;
} telemetry_snapshot_t;

typedef struct
{
    telemetry_source_t source;
    union
    {
        telemetry_mission_t mission;
        telemetry_motion_t motion;
        telemetry_line_t line;
        telemetry_terrain_t terrain;
        telemetry_obstacle_t obstacle;
    } data;
} telemetry_sample_t;

typedef enum
{
    TELEMETRY_EVENT_BARCODE,
    TELEMETRY_EVENT_HUMP,
    TELEMETRY_EVENT_SCAN_POINT,
    TELEMETRY_EVENT_OBSTACLE
} telemetry_event_kind_t;

typedef struct
{
    telemetry_event_kind_t kind;
    uint32_t captured_ms;
    uint32_t scan_id;
    int32_t value;
    int32_t angle_mdeg;
} telemetry_event_t;

bool telemetry_apply_sample(telemetry_snapshot_t *p_snapshot,
                            telemetry_sample_t const *p_sample);
bool telemetry_format_snapshot(telemetry_snapshot_t const *p_snapshot,
                               uint32_t boot_id, uint32_t sequence,
                               uint32_t captured_ms, char *p_output,
                               size_t capacity);
bool telemetry_format_event(telemetry_event_t const *p_event,
                            uint32_t boot_id, uint32_t sequence,
                            char *p_output, size_t capacity);

#endif /* TELEMETRY_H */
