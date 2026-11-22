#include "track_runtime.h"
#include "track_sensor.h"
#include "network_service.h"
#ifdef RTOS_BARCODE_ACTIONS
#include "barcode_actions.h"
#endif
#include <string.h>
#ifdef RTOS_OBSTACLE
#include "obstacle_bypass.h"
#include "obstacle_io.h"
#include "binary_protocol.h"
/* The Motion task alone reads and writes the controller. */
static obstacle_bypass_t obstacle;
static obstacle_range_t last_echo; /* last fresh range, for telemetry only */
static uint8_t count8(unsigned n)
{
    return n > 255u ? 255u : (uint8_t)n;
}
/* Scanned obstacle width in whole cm, 0 until the detour plan has found the
 * obstacle's edge. */
static uint8_t obstacle_width_cm(void)
{
    if (!obstacle.plan.left_edge_known && !obstacle.plan.edge_from_no_return)
    {
        return 0u;
    }
    unsigned cm = (obstacle.plan.observed_width_mm + 5u) / 10u;
    return cm > 63u ? 63u : (uint8_t)cm;
}
static void report_obstacle(bool active, uint64_t now)
{
    bool ranged = active && last_echo.valid && last_echo.cm >= 0.0f &&
                  last_echo.cm < 409.5f;
    obstacle_reading_t r = {
        .ms = (uint32_t)(now / 1000u),
        .active = active,
        .phase = (uint8_t)obstacle.phase,
        .servo_deg = (uint8_t)obstacle.servo_angle,
        .range_valid = ranged,
        .range_mm = ranged ? (uint16_t)(last_echo.cm * 10.0f + 0.5f) : 0u,
        .detected = count8(obstacle.detected),
        .completed = count8(obstacle.completed),
        .width_cm = active ? obstacle_width_cm() : 0u};
    (void)network_obstacle(&r);
}
#endif

static int32_t level(uint16_t raw, int white, int black)
{
    int32_t n = ((int32_t)raw - white) * 1000 / (black - white);
    return n < 0 ? 0 : n > 1000 ? 1000 : n;
}
static bool sensors(track_ir_t *ir, uint64_t now)
{
    track_sensor_read(ir);
    bool valid = ir->ready && ir->age_us <= TRACK_FRESH_US;
    int32_t levels[3] = {level(ir->raw[0], 191, 3164),
                         level(ir->raw[1], 178, 2908),
                         level(ir->raw[2], 141, 842)};
    uint8_t pattern = 0;
    for (unsigned i = 0; i < 3; i++)
    {
        if (levels[i] >= IR_BLACK_LEVEL)
        {
            pattern |= (uint8_t)(1u << i);
        }
    }
    (void)network_line((uint32_t)(now / 1000u) - ir->age_us / 1000u, ir->raw,
                       pattern, valid);
    return valid;
}
/* The course options that were tuned on the car. Also re-applied by the
 * obstacle bypass whenever its own line follower restarts after a detour, so
 * obstacle mode follows the line the same way. */
static void line_options(course_control_t *c)
{
#ifdef RTOS_BARCODE_ACTIONS
    course_set_letter_actions(c, barcode_actions_table());
#endif
    course_enable_flow_follow(c, true);
#ifdef RTOS_CURVE_FOLLOW
    course_set_curve_follow(c, true);
#endif
#ifdef RTOS_LAUNCH_CAP
    course_set_launch_cap(c, 0.34f);
#endif
#ifdef RTOS_TRIM_GAIN
    course_set_trim_gain(c, 2.0f);
#endif
}
void track_runtime_idle(track_runtime_t *t, uint64_t now)
{
    track_ir_t ir;
    if (sensors(&ir, now))
    {
        center_baseline_add(&t->baseline, level(ir.raw[1], 178, 2908), now);
    }
}
void track_runtime_observe(uint64_t now)
{
    track_ir_t ir;
    (void)sensors(&ir, now);
}
bool track_runtime_begin(track_runtime_t *t, uint16_t token,
                         encoder_sample_t enc, uint64_t now)
{
    track_ir_t ir;
    int32_t baseline;
    int32_t spread;
#ifdef RTOS_OBSTACLE
    t->obstacle_enabled = false;
#endif
    t->status = "TRACK_START_NOT_ALIGNED";
    t->reads = 0;
    t->letter = 0;
    if (!sensors(&ir, now) ||
        !center_baseline_value(&t->baseline, now, &baseline, &spread) ||
        level(ir.raw[1], 178, 2908) < IR_BLACK_LEVEL)
    {
        return false;
    }
    /* Reuse exact validated follow/recovery algorithm. Without the
     * barcode-action table Y/Z stay straight as in the AAZ test; with it (the
     * real RTOS build) every letter follows barcode_actions.h. */
    course_begin(&t->course, (course_config_t){'s', 's', 120}, enc, now);
    line_options(&t->course);
    course_set_center_reference(&t->course, baseline);
    t->token = token;
    t->requested_us = now;
    t->state = 0;
    t->reads = 0;
    t->letter = 0;
    t->status = "TRACK_DECODER_ARMING";
    track_request(token);
    return true;
}
#ifdef RTOS_OBSTACLE
bool track_runtime_begin_obstacle(track_runtime_t *t, uint16_t token,
                                  encoder_sample_t enc, uint64_t now)
{
    if (!track_runtime_begin(t, token, enc, now))
    {
        return false;
    }
    obstacle_bypass_begin(&obstacle, enc, now, t->course.center_reference);
    obstacle_bypass_set_line_setup(&obstacle, line_options);
    last_echo = (obstacle_range_t){0};
    obstacle_io_begin();
    t->obstacle_enabled = true;
    return true;
}
#endif
line_check_output_t track_runtime_step(track_runtime_t *t, encoder_sample_t enc,
                                       uint64_t now)
{
    track_ir_t ir;
    if (!sensors(&ir, now))
    {
        t->status = "TRACK_ADC_STALE";
        return (line_check_output_t){0, 0, true, t->status};
    }
    if (track_faulted())
    {
        t->status = "TRACK_CAPTURE_FAULT";
        return (line_check_output_t){0, 0, true, t->status};
    }
    if (!t->state)
    {
        if (!track_ready(t->token))
        {
            return (line_check_output_t){0, 0, now - t->requested_us >= 100000u,
                                         "TRACK_DECODER_ARMING"};
        }
        t->state = 1;
    }
#ifdef RTOS_OBSTACLE
    /* Obstacle mode keeps its own follower inside the bypass. Barcodes go to
     * that one, and only while it is following the line: during a detour a read
     * has no crossing to act on. */
    course_control_t *line = t->obstacle_enabled ? &obstacle.line : &t->course;
    const bool line_active =
        !t->obstacle_enabled || obstacle.phase == AVOID_FOLLOW;
#else
    course_control_t *line = &t->course;
    const bool line_active = true;
#endif
    if (track_recording() && line_active)
    {
        course_note_barcode_activity(line, now);
    }
    track_event_t event;
    while (track_event_take(t->token, &event))
    {
        t->reads++;
        t->letter = event.letter;
        if (line_active)
        {
            (void)course_on_barcode(line, event.letter, enc, now,
                                    event.crossing);
        }
    }
    int32_t levels[3] = {level(ir.raw[0], 191, 3164),
                         level(ir.raw[1], 178, 2908),
                         level(ir.raw[2], 141, 842)};
#ifdef RTOS_OBSTACLE
    if (t->obstacle_enabled)
    {
        obstacle_range_t range = obstacle_io_poll();
        course_right_level(&obstacle.line, levels[2]);
        line_check_output_t out = obstacle_bypass_guarded_step(
            &obstacle, ir_roles(levels), levels[1], enc, now, range.fresh,
            range.valid, range.cm, range.error, range.no_return);
        obstacle_io_angle(obstacle.servo_angle);
        if (range.fresh)
        {
            last_echo = range;
        }
        report_obstacle(true, now);
        track_recoveries(obstacle.line.recoveries);
        t->status = out.state;
        return out;
    }
#endif
    course_center_level(&t->course, levels[1]);
    course_right_level(&t->course, levels[2]);
    line_check_output_t out =
        course_step(&t->course, ir_roles(levels), enc, now);
    track_recoveries(t->course.recoveries);
    t->status = out.state;
    return out;
}
void track_runtime_end(void)
{
#ifdef RTOS_OBSTACLE
    obstacle_io_end();
    report_obstacle(false, 0);
#endif
    track_stop();
}
