#include "telemetry.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool telemetry_append(char *p_output, size_t capacity,
                             size_t *p_used, char const *p_format, ...);
static char const *telemetry_mission_name(telemetry_mission_state_t state);
static char const *telemetry_motion_name(telemetry_motion_state_t state);
static bool telemetry_write_mission(telemetry_mission_t const *p_sample,
                                    char *p_output, size_t capacity,
                                    size_t *p_used);
static bool telemetry_write_motion(telemetry_motion_t const *p_sample,
                                   char *p_output, size_t capacity,
                                   size_t *p_used);
static bool telemetry_write_line(telemetry_line_t const *p_sample,
                                 char *p_output, size_t capacity,
                                 size_t *p_used);
static bool telemetry_write_terrain(telemetry_terrain_t const *p_sample,
                                    char *p_output, size_t capacity,
                                    size_t *p_used);
static bool telemetry_write_obstacle(telemetry_obstacle_t const *p_sample,
                                     char *p_output, size_t capacity,
                                     size_t *p_used);

bool
telemetry_apply_sample(telemetry_snapshot_t *p_snapshot,
                       telemetry_sample_t const *p_sample)
{
    bool b_is_valid = false;

    if ((NULL != p_snapshot) && (NULL != p_sample))
    {
        b_is_valid = true;
        switch (p_sample->source)
        {
        case TELEMETRY_SOURCE_MISSION:
            p_snapshot->mission = p_sample->data.mission;
            break;
        case TELEMETRY_SOURCE_MOTION:
            p_snapshot->motion = p_sample->data.motion;
            break;
        case TELEMETRY_SOURCE_LINE:
            p_snapshot->line = p_sample->data.line;
            break;
        case TELEMETRY_SOURCE_TERRAIN:
            p_snapshot->terrain = p_sample->data.terrain;
            break;
        case TELEMETRY_SOURCE_OBSTACLE:
            p_snapshot->obstacle = p_sample->data.obstacle;
            break;
        default:
            b_is_valid = false;
            break;
        }
    }

    return (b_is_valid);
}

bool
telemetry_format_snapshot(telemetry_snapshot_t const *p_snapshot,
                          uint32_t boot_id, uint32_t sequence,
                          uint32_t captured_ms, char *p_output,
                          size_t capacity)
{
    bool b_is_valid = false;
    size_t used = 0U;

    if ((NULL != p_snapshot) && (NULL != p_output) && (0U < capacity) &&
        (TELEMETRY_JSON_SIZE >= capacity))
    {
        b_is_valid = telemetry_append(
            p_output, capacity, &used,
            "{\"schema\":1,\"boot_id\":%" PRIu32
            ",\"sequence\":%" PRIu32 ",\"captured_ms\":%" PRIu32,
            boot_id, sequence, captured_ms);
        if (true == b_is_valid)
        {
            b_is_valid = telemetry_write_mission(&p_snapshot->mission,
                                                  p_output, capacity, &used);
        }
        if (true == b_is_valid)
        {
            b_is_valid = telemetry_write_motion(&p_snapshot->motion,
                                                 p_output, capacity, &used);
        }
        if (true == b_is_valid)
        {
            b_is_valid = telemetry_write_line(&p_snapshot->line,
                                               p_output, capacity, &used);
        }
        if (true == b_is_valid)
        {
            b_is_valid = telemetry_write_terrain(&p_snapshot->terrain,
                                                  p_output, capacity, &used);
        }
        if (true == b_is_valid)
        {
            b_is_valid = telemetry_write_obstacle(&p_snapshot->obstacle,
                                                   p_output, capacity, &used);
        }
        if (true == b_is_valid)
        {
            b_is_valid = telemetry_append(p_output, capacity, &used, "}");
        }
    }

    if ((false == b_is_valid) && (NULL != p_output) && (0U < capacity))
    {
        p_output[0] = '\0';
    }

    return (b_is_valid);
}

bool
telemetry_format_event(telemetry_event_t const *p_event,
                       uint32_t boot_id, uint32_t sequence,
                       char *p_output, size_t capacity)
{
    bool b_is_valid = false;
    size_t used = 0U;
    char const *p_kind = NULL;

    if (NULL != p_event)
    {
        switch (p_event->kind)
        {
        case TELEMETRY_EVENT_BARCODE:
            p_kind = "barcode";
            break;
        case TELEMETRY_EVENT_HUMP:
            p_kind = "hump";
            break;
        case TELEMETRY_EVENT_SCAN_POINT:
            p_kind = "scan_point";
            break;
        case TELEMETRY_EVENT_OBSTACLE:
            p_kind = "obstacle";
            break;
        default:
            break;
        }
    }

    if ((NULL != p_kind) && (NULL != p_output) &&
        (0U < capacity) && (TELEMETRY_JSON_SIZE >= capacity))
    {
        b_is_valid = telemetry_append(
            p_output, capacity, &used,
            "{\"schema\":1,\"boot_id\":%" PRIu32
            ",\"sequence\":%" PRIu32 ",\"captured_ms\":%" PRIu32
            ",\"kind\":\"%s\",\"scan_id\":%" PRIu32
            ",\"value\":%" PRId32 ",\"angle_mdeg\":%" PRId32 "}",
            boot_id, sequence, p_event->captured_ms, p_kind,
            p_event->scan_id, p_event->value, p_event->angle_mdeg);
    }

    if ((false == b_is_valid) && (NULL != p_output) && (0U < capacity))
    {
        p_output[0] = '\0';
    }

    return (b_is_valid);
}

static bool
telemetry_append(char *p_output, size_t capacity, size_t *p_used,
                 char const *p_format, ...)
{
    bool b_is_valid = false;
    int written = 0;
    va_list arguments;

    if (*p_used < capacity)
    {
        va_start(arguments, p_format);
        written = vsnprintf(&p_output[*p_used], capacity - *p_used,
                            p_format, arguments);
        va_end(arguments);
        if ((0 <= written) && ((size_t)written < (capacity - *p_used)))
        {
            *p_used += (size_t)written;
            b_is_valid = true;
        }
    }

    return (b_is_valid);
}

static char const *
telemetry_mission_name(telemetry_mission_state_t state)
{
    char const *p_name = "unknown";

    switch (state)
    {
    case TELEMETRY_MISSION_IDLE:
        p_name = "idle";
        break;
    case TELEMETRY_MISSION_RUNNING:
        p_name = "running";
        break;
    case TELEMETRY_MISSION_STOPPED:
        p_name = "stopped";
        break;
    default:
        break;
    }

    return (p_name);
}

static char const *
telemetry_motion_name(telemetry_motion_state_t state)
{
    char const *p_name = "unknown";

    switch (state)
    {
    case TELEMETRY_MOTION_STATIONARY:
        p_name = "stationary";
        break;
    case TELEMETRY_MOTION_ACCELERATING:
        p_name = "accelerating";
        break;
    case TELEMETRY_MOTION_TURNING:
        p_name = "turning";
        break;
    case TELEMETRY_MOTION_CLIMBING:
        p_name = "climbing";
        break;
    case TELEMETRY_MOTION_DESCENDING:
        p_name = "descending";
        break;
    case TELEMETRY_MOTION_IMPACT:
        p_name = "impact";
        break;
    default:
        break;
    }

    return (p_name);
}

static bool
telemetry_write_mission(telemetry_mission_t const *p_sample,
                        char *p_output, size_t capacity, size_t *p_used)
{
    bool b_is_valid = false;

    if (false == p_sample->b_is_valid)
    {
        b_is_valid = telemetry_append(p_output, capacity, p_used,
                                      ",\"mission\":null");
    }
    else
    {
        b_is_valid = telemetry_append(
            p_output, capacity, p_used,
            ",\"mission\":{\"captured_ms\":%" PRIu32
            ",\"simulated\":%s,\"state\":\"%s\"}",
            p_sample->captured_ms,
            (true == p_sample->b_is_simulated) ? "true" : "false",
            telemetry_mission_name(p_sample->state));
    }

    return (b_is_valid);
}

static bool
telemetry_write_motion(telemetry_motion_t const *p_sample,
                       char *p_output, size_t capacity, size_t *p_used)
{
    bool b_is_valid = false;

    if (false == p_sample->b_is_valid)
    {
        b_is_valid = telemetry_append(p_output, capacity, p_used,
                                      ",\"motion\":null");
    }
    else
    {
        b_is_valid = telemetry_append(
            p_output, capacity, p_used,
            ",\"motion\":{\"captured_ms\":%" PRIu32
            ",\"simulated\":%s"
            ",\"left_mm_s\":%" PRId32 ",\"right_mm_s\":%" PRId32
            ",\"left_ticks\":%" PRId32 ",\"right_ticks\":%" PRId32
            ",\"distance_mm\":%" PRIu32 "}",
            p_sample->captured_ms,
            (true == p_sample->b_is_simulated) ? "true" : "false",
            p_sample->left_mm_s,
            p_sample->right_mm_s, p_sample->left_ticks,
            p_sample->right_ticks, p_sample->distance_mm);
    }

    return (b_is_valid);
}

static bool
telemetry_write_line(telemetry_line_t const *p_sample,
                     char *p_output, size_t capacity, size_t *p_used)
{
    bool b_is_valid = false;
    char barcode = '?';

    if (false == p_sample->b_is_valid)
    {
        b_is_valid = telemetry_append(p_output, capacity, p_used,
                                      ",\"line\":null");
    }
    else
    {
        if (('A' <= p_sample->barcode) && ('D' >= p_sample->barcode))
        {
            barcode = p_sample->barcode;
        }
        b_is_valid = telemetry_append(
            p_output, capacity, p_used,
            ",\"line\":{\"captured_ms\":%" PRIu32
            ",\"simulated\":%s"
            ",\"ir\":[%u,%u,%u],\"on_line\":%s,\"barcode\":\"%c\"}",
            p_sample->captured_ms,
            (true == p_sample->b_is_simulated) ? "true" : "false",
            (unsigned int)p_sample->ir_left,
            (unsigned int)p_sample->ir_middle,
            (unsigned int)p_sample->ir_right,
            (true == p_sample->b_is_on_line) ? "true" : "false",
            barcode);
    }

    return (b_is_valid);
}

static bool
telemetry_write_terrain(telemetry_terrain_t const *p_sample,
                        char *p_output, size_t capacity, size_t *p_used)
{
    bool b_is_valid = false;

    if (false == p_sample->b_is_valid)
    {
        b_is_valid = telemetry_append(p_output, capacity, p_used,
                                      ",\"terrain\":null");
    }
    else
    {
        b_is_valid = telemetry_append(
            p_output, capacity, p_used,
            ",\"terrain\":{\"captured_ms\":%" PRIu32
            ",\"simulated\":%s"
            ",\"motion\":\"%s\",\"hump_mm\":%" PRId32
            ",\"highest_hump_mm\":%" PRId32 "}",
            p_sample->captured_ms,
            (true == p_sample->b_is_simulated) ? "true" : "false",
            telemetry_motion_name(p_sample->motion),
            p_sample->hump_mm, p_sample->highest_hump_mm);
    }

    return (b_is_valid);
}

static bool
telemetry_write_obstacle(telemetry_obstacle_t const *p_sample,
                         char *p_output, size_t capacity, size_t *p_used)
{
    bool b_is_valid = false;

    if (false == p_sample->b_is_valid)
    {
        b_is_valid = telemetry_append(p_output, capacity, p_used,
                                      ",\"obstacle\":null");
    }
    else
    {
        b_is_valid = telemetry_append(
            p_output, capacity, p_used,
            ",\"obstacle\":{\"captured_ms\":%" PRIu32
            ",\"simulated\":%s"
            ",\"scan_id\":%" PRIu32 ",\"closest_mm\":%" PRIu32
            ",\"width_mm\":%" PRIu32
            ",\"left_clearance_mm\":%" PRIu32
            ",\"right_clearance_mm\":%" PRIu32 "}",
            p_sample->captured_ms,
            (true == p_sample->b_is_simulated) ? "true" : "false",
            p_sample->scan_id,
            p_sample->closest_mm, p_sample->width_mm,
            p_sample->left_clearance_mm,
            p_sample->right_clearance_mm);
    }

    return (b_is_valid);
}
