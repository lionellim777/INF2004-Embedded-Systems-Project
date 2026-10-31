#include "motion_control.h"
#include <stddef.h>
#include <string.h>

/* Batch 5 calibration, copied from the tested standalone controller. */
enum { CPR = 2543, CONTROL_US = 100000, TIMEOUT_US = 12000000,
       TURN_STOP_EARLY = 290, LEFT_90_EXTRA = 65, RIGHT_90_EXTRA = 58, RIGHT_180_SHORTEN = 60,
       LATE_POLL_US = 250000, STALL_US = 1000000 };
static const float PI_VALUE = 3.14159265f;
static const float WHEEL_MM = 60.0f;
static const float TRACK_MM = 115.0f;
static const float KP = 0.000035f;
static const float KI = 0.000015f;
/* Straight-only position balancing in PWM space, after the calibrated PI
 * computation. The old speed trim is ineffective when both PI outputs hit
 * the 0.25 minimum; allow the leading wheel below that floor while rolling.
 * Gain .0003/count, bounded to .04 PWM each side; zero skew changes nothing. */
static const float STRAIGHT_BALANCE_PWM_PER_COUNT = 0.0003f;
static const float STRAIGHT_BALANCE_PWM_LIMIT = 0.04f;
/* Provisional endpoint for this specific 30 cm manoeuvre, not a general
 * distance/count scale. v2 target3779 produced means3871.5/3869.5, with
 * reported320 mm travel. Move cutoff earlier by round(20*3870.5/320)=242.
 * 3779-242=3537. This includes the observed residual rolling; it does not
 * claim a new mechanical CPR, and must not be applied to other distances.
 * Validate the v3 ground result. Turn targets and PI tuning are unchanged. */
enum { STRAIGHT_30CM_STOP_COUNTS = 3537 };

static float clamp(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

static int32_t delta(uint32_t current, uint32_t start)
{
    uint32_t difference = current - start;
    return difference <= INT32_MAX ? (int32_t)difference :
        (int32_t)((int64_t)difference - INT64_C(4294967296));
}

int32_t motion_left_position(const motion_control_t *motion, encoder_sample_t sample)
{
    /* Left raw encoder decreases during forward travel. Subtract in unsigned
     * space before signed conversion, preserving wrap without resetting ISR data. */
    return delta(motion->start.left_raw, sample.left_raw);
}

int32_t motion_right_position(const motion_control_t *motion, encoder_sample_t sample)
{
    return delta(sample.right_raw, motion->start.right_raw);
}

static int32_t mm_counts(float mm)
{
    return (int32_t)(mm * (float)CPR / (PI_VALUE * WHEEL_MM) + 0.5f);
}

static int32_t turn_counts(float degrees)
{
    return mm_counts(PI_VALUE * TRACK_MM * degrees / 360.0f) - TURN_STOP_EARLY;
}

void motion_init(motion_control_t *motion)
{
    if (motion != NULL) {
        memset(motion, 0, sizeof(*motion));
    }
}

bool motion_begin(motion_control_t *motion, motion_kind_t kind,
                  encoder_sample_t sample, uint64_t now_us)
{
    if (motion == NULL || motion->phase == MOTION_ACTIVE ||
        kind < MOTION_STRAIGHT_30CM || kind > MOTION_RIGHT_180) {
        return false;
    }
    motion_init(motion);
    motion->kind = kind;
    motion->start = sample;
    motion->start_us = motion->control_us = now_us;
    motion->left.last_progress_us = motion->right.last_progress_us = now_us;
    int32_t count;
    switch (kind) {
    case MOTION_STRAIGHT_30CM:
        motion->left.target = motion->right.target = STRAIGHT_30CM_STOP_COUNTS;
        break;
    case MOTION_LEFT_90:
        count = turn_counts(90.0f) + LEFT_90_EXTRA;
        motion->left.target = -count;
        motion->right.target = count;
        break;
    case MOTION_RIGHT_90:
        /* Two completed v5.1 R90 runs settled at mean magnitudes 991/996.
         * Operator estimated ~85 degrees: round(993.5 * 5 / 85) = 58
         * additional counts. Provisional R90-only trim, not a global yaw scale. */
        count = turn_counts(90.0f) + RIGHT_90_EXTRA;
        motion->left.target = count;
        motion->right.target = -count;
        break;
    case MOTION_RIGHT_180:
        count = turn_counts(180.0f) - RIGHT_180_SHORTEN;
        motion->left.target = count;
        motion->right.target = -count;
        break;
    default:
        return false;
    }
    motion->phase = MOTION_ACTIVE;
    return true;
}

static void fail(motion_control_t *motion, motion_fault_t fault)
{
    motion->phase = MOTION_FAULT;
    motion->fault = fault;
    motion->left.throttle = motion->right.throttle = 0.0f;
}

void motion_abort(motion_control_t *motion)
{
    if (motion != NULL) {
        motion->phase = MOTION_ABORTED;
        motion->left.throttle = motion->right.throttle = 0.0f;
    }
}

static bool wheel_update(motion_wheel_t *wheel, int32_t position, float dt,
                          uint64_t now_us, bool right, float adjustment)
{
    int direction = wheel->target < 0 ? -1 : 1;
    int64_t progress = (int64_t)position * direction;
    int32_t target = wheel->target < 0 ? -wheel->target : wheel->target;
    int64_t remaining = target - progress;
    wheel->position = position;
    wheel->cps = (float)((int64_t)position - wheel->previous) * direction / dt;
    if (position != wheel->previous) {
        wheel->last_progress_us = now_us;
    }
    wheel->previous = position;
    if (wheel->finished) {
        return true;
    }
    /* All target completion is handled by check_endpoint before this update. */
    if (now_us - wheel->last_progress_us >= STALL_US) {
        return false;
    }
    float target_cps = (remaining > 2200 ? 1800.0f :
                        (remaining > 700 ? 1200.0f : 600.0f)) + adjustment;
    float error = target_cps - wheel->cps;
    wheel->integral = clamp(wheel->integral + error * dt, -2500.0f, 2500.0f);
    float feedforward = 0.30f + 0.000055f * target_cps;
    if (right) {
        feedforward *= 0.95f;
    }
    wheel->throttle = clamp(feedforward + KP * error + KI * wheel->integral,
                             0.25f, 0.65f) * direction;
    return true;
}

static void check_endpoint(motion_wheel_t *wheel, int32_t position,
                            uint32_t elapsed_us)
{
    wheel->position = position;
    bool reached = wheel->target > 0 ? position >= wheel->target : position <= wheel->target;
    if (!wheel->finished && reached) {
        wheel->cutoff_position = position;
        wheel->cutoff_elapsed_us = elapsed_us;
        wheel->finished = true;
        wheel->throttle = 0.0f;
    }
}

void motion_poll(motion_control_t *motion, encoder_sample_t sample, uint64_t now_us)
{
    if (motion == NULL || motion->phase != MOTION_ACTIVE) {
        return;
    }
    if (now_us < motion->control_us || now_us - motion->start_us >= TIMEOUT_US) {
        fail(motion, MOTION_TIMED_OUT);
        return;
    }
    if (sample.left_invalid != motion->start.left_invalid ||
        sample.right_invalid != motion->start.right_invalid) {
        fail(motion, MOTION_ENCODER_INVALID);
        return;
    }
    uint64_t elapsed = now_us - motion->control_us;
    if (elapsed > LATE_POLL_US) {
        fail(motion, MOTION_LATE_POLL);
        return;
    }
    int32_t left = motion_left_position(motion, sample);
    int32_t right = motion_right_position(motion, sample);
    int left_direction = motion->left.target < 0 ? -1 : 1;
    int right_direction = motion->right.target < 0 ? -1 : 1;
    if ((int64_t)left * left_direction < -64 ||
        (int64_t)right * right_direction < -64) {
        fail(motion, MOTION_WRONG_DIRECTION);
        return;
    }
    /* Endpoints are checked at every polling opportunity, independently of the
     * calibrated 100 ms PI interval. Never wait another whole PI tick to stop. */
    uint32_t move_elapsed_us = (uint32_t)(now_us - motion->start_us);
    check_endpoint(&motion->left, left, move_elapsed_us);
    check_endpoint(&motion->right, right, move_elapsed_us);
    if (motion->left.finished && motion->right.finished) {
        motion->phase = MOTION_DONE;
        return;
    }
    if (elapsed < CONTROL_US) {
        return;
    }
    float sync = 0.0f;
    if (motion->kind == MOTION_STRAIGHT_30CM &&
        !motion->left.finished && !motion->right.finished) {
        int64_t skew = (int64_t)left - right;
        if (skew > INT32_MAX || skew < INT32_MIN) {
            fail(motion, MOTION_WRONG_DIRECTION);
            return;
        }
        int64_t previous_peak = motion->peak_skew;
        if ((skew < 0 ? -skew : skew) >
            (previous_peak < 0 ? -previous_peak : previous_peak)) {
            motion->peak_skew = (int32_t)skew;
        }
        sync = clamp((float)skew * 1.5f, -120.0f, 120.0f);
    }
    float dt = (float)elapsed / 1000000.0f;
    if (!wheel_update(&motion->left, left, dt, now_us, false, -sync) ||
        !wheel_update(&motion->right, right, dt, now_us, true, sync)) {
        fail(motion, MOTION_STALLED);
        return;
    }
    if (motion->kind == MOTION_STRAIGHT_30CM &&
        !motion->left.finished && !motion->right.finished) {
        float balance = clamp((float)((int64_t)left - right) *
                              STRAIGHT_BALANCE_PWM_PER_COUNT,
                              -STRAIGHT_BALANCE_PWM_LIMIT, STRAIGHT_BALANCE_PWM_LIMIT);
        motion->left.throttle = clamp(motion->left.throttle - balance, 0.0f, 0.65f);
        motion->right.throttle = clamp(motion->right.throttle + balance, 0.0f, 0.65f);
    }
    motion->control_us = now_us;
    if (motion->left.finished && motion->right.finished) {
        motion->phase = MOTION_DONE;
    }
}

const char *motion_name(motion_kind_t kind)
{
    switch (kind) {
    case MOTION_STRAIGHT_30CM: return "STRAIGHT_30CM";
    case MOTION_LEFT_90: return "LEFT_90";
    case MOTION_RIGHT_90: return "RIGHT_90";
    case MOTION_RIGHT_180: return "RIGHT_180";
    case MOTION_BARCODE: return "BARCODE_PASS";
    case MOTION_COURSE: return "COURSE_TEST";
    case MOTION_CONTINUOUS: return "CONTINUOUS_SCAN";
    default: return "LINE_CHECK";
    }
}

const char *motion_fault_name(motion_fault_t fault)
{
    switch (fault) {
    case MOTION_ENCODER_INVALID: return "ENCODER_INVALID";
    case MOTION_WRONG_DIRECTION: return "WRONG_DIRECTION";
    case MOTION_STALLED: return "NO_ENCODER_PROGRESS_1S";
    case MOTION_LATE_POLL: return "CONTROL_DELAY_OVER_250MS";
    case MOTION_TIMED_OUT: return "MOTION_TIMEOUT_12S";
    default: return "NONE";
    }
}
