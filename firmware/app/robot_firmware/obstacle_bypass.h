#ifndef OBSTACLE_BYPASS_H
#define OBSTACLE_BYPASS_H
#include "course_control.h"
#include "obstacle_plan.h"
/* Supervised floor test, right-side bypass. No reverse and no cliff sensing.
 * Servo remains within the measured 50..125 degree mechanical envelope.
 * Side observations use chassis turns, not an unverified 180-degree servo arc. */
typedef enum {
    AVOID_FOLLOW, AVOID_WAIT, AVOID_TURN_OUT, AVOID_SCAN_OUT, AVOID_MOVE_OUT,
    AVOID_TURN_FORWARD, AVOID_SCAN_FORWARD, AVOID_MOVE_ALONG, AVOID_TURN_LOOK,
    AVOID_SCAN_SIDE, AVOID_TURN_ALONG, AVOID_RETURN_LINE, AVOID_VERIFY, AVOID_CHECK_TAPE,
    AVOID_TAPE_PROBE, AVOID_AXLE_ADVANCE, AVOID_ALIGN_TURN, AVOID_ALIGN_CONFIRM, AVOID_SCAN_INITIAL, AVOID_CHECK_LEFT, AVOID_CLEAR_TAIL, AVOID_FAILED
} avoid_phase_t;
typedef struct {
    course_control_t line;
    motion_control_t leg, origin, tape_origin, episode_origin, align_origin, saved_leg;
    bool probe_black, probe_gap;
    unsigned probe_first_black_mm;
    int route_heading_deg;
    obstacle_plan_t plan;
    float plan_cm[5];
    unsigned plan_good[5], departure_deg, departure_travel_mm, departure_forward_mm;
    unsigned left_samples, left_far;
    bool tail_margin_done, left_near;
    unsigned axle_mm;
    int align_direction;
    bool align_reversed;
    bool align_armed;
    uint64_t align_hit_us;
    unsigned align_rejected;
    unsigned fast_clear;
    bool approach_pending, approach_warning;
    motion_control_t approach_origin;
    float approach_estimate_cm, approach_anchor_cm, approach_yaw_rad;
    uint64_t approach_us;
    float approach_min_cm;
    unsigned approach_hits;
    avoid_phase_t phase, next, resume_phase;
    uint64_t white_us, candidate_us;
    unsigned resume_servo, rejoin_attempts, rejoin_rejected, rejoin_good;
    bool rejoin_armed;
    bool no_return; /* Fresh completed no-return pulse, not NO_RISE or a sensor fault. */
    const char *tape_decision;
    uint64_t phase_us, last_us, episode_us, range_us, hit_us;
    uint64_t wall_us, active_us, sonar_wait_us;
    bool sonar_wait;
    unsigned sonar_good, sonar_retries, sonar_resumes;
    uint32_t encoder_seen[2];
    uint64_t encoder_error_us[2];
    unsigned encoder_skips[2];
    unsigned servo_angle, scan_index, samples, close_count, clear_checks, scan_center_clear;
    unsigned scan_ray_far, scan_far_rays, scan_far_samples, scan_nearest_angle, scan_blocked_angle;
    unsigned last_side_seen_mm;
    unsigned offset_mm, along_mm, outward_mm;
    unsigned detected, completed, scans, no_echo, scan_no_echo, scan_serial;
    float scan_min_cm;
    bool scan_blocked, side_seen, echo_valid;
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
#endif
