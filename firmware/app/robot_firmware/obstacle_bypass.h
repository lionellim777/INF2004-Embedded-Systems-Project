#ifndef OBSTACLE_BYPASS_H
#define OBSTACLE_BYPASS_H
#include "course_control.h"
#include "obstacle_plan.h"
/* Supervised floor test, left-side bypass. No reverse and no cliff sensing.
 * Servo remains within the measured 50..125 degree mechanical envelope.
 * Side observations use chassis turns, not an unverified 180-degree servo arc. */
typedef enum {
    AVOID_FOLLOW, AVOID_WAIT, AVOID_TURN_OUT, AVOID_SCAN_OUT, AVOID_MOVE_OUT,
    AVOID_TURN_FORWARD, AVOID_SCAN_FORWARD, AVOID_MOVE_ALONG, AVOID_TURN_LOOK,
    AVOID_SCAN_SIDE, AVOID_TURN_ALONG, AVOID_RETURN_LINE, AVOID_VERIFY, AVOID_CHECK_TAPE,
    AVOID_TAPE_PROBE, AVOID_AXLE_ADVANCE, AVOID_ALIGN_TURN, AVOID_ALIGN_CONFIRM, AVOID_SCAN_INITIAL, AVOID_CHECK_LEFT, AVOID_CLEAR_TAIL,
    AVOID_TURN_RETURN, AVOID_SCAN_RETURN, AVOID_TURN_CLEARANCE, AVOID_TURN_TO_LINE, AVOID_FAILED
} avoid_phase_t;
typedef struct {
    uint32_t left_raw;
    uint32_t right_raw;
} obstacle_encoder_origin_t;

/* Subtract in modulo-32 space before converting to a signed delta. */
static inline int32_t obstacle_encoder_delta(uint32_t current, uint32_t start)
{
    uint32_t difference = current - start;
    return difference <= INT32_MAX ? (int32_t)difference :
        (int32_t)((int64_t)difference - INT64_C(4294967296));
}

static inline obstacle_encoder_origin_t obstacle_encoder_origin(encoder_sample_t enc)
{
    return (obstacle_encoder_origin_t){enc.left_raw, enc.right_raw};
}

typedef struct {
    course_control_t line;
    motion_control_t leg, saved_leg;
    encoder_sample_t origin;
    obstacle_encoder_origin_t tape_origin, episode_origin, align_origin;
    bool probe_black, probe_gap;
    unsigned probe_first_black_mm;
    int route_heading_deg;
    obstacle_plan_t plan;
    float plan_cm[OBSTACLE_PLAN_RAYS];
    obstacle_ray_counts_t plan_counts[OBSTACLE_PLAN_RAYS];
    unsigned departure_deg : 7;
    unsigned return_deg : 7;
    /* Accepted lateral plans cap at 600 mm and A is at least 45 degrees. */
    unsigned departure_travel_mm : 10;
    unsigned departure_forward_mm : 10;
    unsigned departure_done_mm : 10;
    unsigned departure_step_mm : 7;
    unsigned edge_shift_count, clearance_adjustments;
    bool edge_shift_active, angled_probe;
    unsigned left_samples : 2;
    unsigned left_far : 2;
    unsigned tail_margin_done : 1;
    unsigned left_near : 1;
    unsigned axle_mm : 8; /* Validated 40..140 mm before storing. */
    signed int align_direction : 2; /* -1 left, 0 unset, 1 right */
    unsigned align_reversed : 1;
    unsigned align_armed : 1;
    uint64_t align_hit_us;
    unsigned align_rejected;
    unsigned fast_clear : 2;
    bool approach_pending, approach_warning;
    obstacle_encoder_origin_t approach_origin;
    float approach_estimate_cm, approach_anchor_cm, approach_yaw_rad;
    uint64_t approach_us;
    float approach_min_cm;
    unsigned approach_hits : 2;
    unsigned phase : 5;
    unsigned next : 5;
    unsigned resume_phase : 5;
    uint64_t white_us, candidate_us;
    unsigned resume_servo : 7;
    unsigned rejoin_good : 2;
    unsigned rejoin_attempts, rejoin_rejected;
    bool rejoin_armed;
    bool no_return; /* Fresh completed no-return pulse, not NO_RISE or a sensor fault. */
    const char *tape_decision;
    uint64_t phase_us, last_us, episode_us, range_us, hit_us;
    uint64_t wall_us, active_us, sonar_wait_us;
    bool sonar_wait;
    unsigned sonar_good : 2;
    unsigned sonar_retries, sonar_resumes;
    uint32_t encoder_seen[2];
    uint64_t encoder_error_us[2];
    unsigned encoder_skips[2];
    /* Motion owns this state. Each angle contributes at most three samples.
     * The scan index also holds the terminal value equal to the ray count. */
    unsigned servo_angle : 7;
    unsigned scan_index : 4;
    unsigned samples : 2;
    unsigned scan_center_clear : 2;
    unsigned scan_center_no_return : 2;
    unsigned scan_ray_far : 2;
    unsigned scan_far_rays : 4;
    unsigned scan_far_samples : 5;
    unsigned scan_no_echo : 5;
    unsigned scan_nearest_angle : 7;
    unsigned scan_blocked_angle : 7;
    unsigned scan_blocked : 1;
    unsigned scan_core_blocked : 1;
    unsigned side_seen : 1;
    unsigned echo_valid : 1;
    unsigned close_count, moving_invalid, clear_checks;
    unsigned last_side_seen_mm : 11;
    unsigned along_mm : 11;
    unsigned offset_mm : 10;
    unsigned outward_mm : 10;
    unsigned detected, completed, scans, no_echo, scan_serial;
    float scan_min_cm;
    int32_t reference;
    const char *fault;
} obstacle_bypass_t;
void obstacle_bypass_begin(obstacle_bypass_t *a, encoder_sample_t enc, uint64_t now, int32_t reference);
/* Fresh range belongs to the servo command returned by the preceding call.
 * Every servo move discards samples during settling. No hardware I/O here. */
line_check_output_t obstacle_bypass_step(obstacle_bypass_t *a, ir_roles_t ir,
    int32_t center_level, encoder_sample_t enc, uint64_t now,
    bool range_fresh, bool range_valid, float range_cm);
/* Hardware fault gate: stop outputs during transient ECHO faults; two valid
 * samples resume the same manoeuvre. One second without recovery ends the run.
 * Controller time excludes the stopped wait; encoder positions are preserved. */
line_check_output_t obstacle_bypass_guarded_step(obstacle_bypass_t *a, ir_roles_t ir,
    int32_t center_level, encoder_sample_t enc, uint64_t wall_now,
    bool range_fresh, bool range_valid, float range_cm, const char *sonar_error, bool completed_no_return);
/* completed_no_return is true only for a completed over-range/no-return
 * measurement. A missing ECHO rising edge must pass false. It permits only
 * monitored tape rejoin; it does not certify side-scan clearance. */
const char *obstacle_bypass_phase(avoid_phase_t phase);
int32_t obstacle_bypass_alignment_heading(const obstacle_bypass_t *a, encoder_sample_t enc);
#endif
