#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TELEMETRY_JSON_SIZE 1024U

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
