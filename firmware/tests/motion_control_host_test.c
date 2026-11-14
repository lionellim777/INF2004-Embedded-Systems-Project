#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "motion_control.h"

static encoder_sample_t at(const motion_control_t *motion, int32_t left, int32_t right)
{
    encoder_sample_t sample = motion->start;
    sample.left_raw -= (uint32_t)left;
    sample.right_raw += (uint32_t)right;
    return sample;
}

static motion_control_t start(motion_kind_t kind)
{
    motion_control_t motion;
    motion_init(&motion);
    encoder_sample_t baseline = {UINT32_MAX - 15u, UINT32_MAX - 15u, 7u, 11u};
    assert(motion_begin(&motion, kind, baseline, 0u));
    return motion;
}

static void stopped(const motion_control_t *motion)
{
    assert(motion->left.throttle == 0.0f && motion->right.throttle == 0.0f);
}

int main(void)
{
    const int32_t left_targets[] = {3537, -994, 987, 2087};
    const int32_t right_targets[] = {3537, 994, -987, -2087};
    for (int kind = MOTION_STRAIGHT_30CM; kind <= MOTION_RIGHT_180; ++kind) {
        motion_control_t motion = start((motion_kind_t)kind);
        assert(motion.left.target == left_targets[kind - 1]);
        assert(motion.right.target == right_targets[kind - 1]);
        assert(!motion_begin(&motion, MOTION_LEFT_90, motion.start, 0u));
        stopped(&motion);
        motion_poll(&motion, motion.start, 99999u);
        stopped(&motion);
        for (unsigned step = 1u; step < 100u && motion.phase == MOTION_ACTIVE; ++step) {
            int32_t left = (int32_t)step * 180;
            int32_t right = left;
            int32_t lt = motion.left.target, rt = motion.right.target;
            int32_t la = lt < 0 ? -lt : lt, ra = rt < 0 ? -rt : rt;
            if (left > la) { left = la; }
            if (right > ra) { right = ra; }
            if (lt < 0) { left = -left; }
            if (rt < 0) { right = -right; }
            encoder_sample_t sample = at(&motion, left, right);
            assert(motion_left_position(&motion, sample) == left);
            assert(motion_right_position(&motion, sample) == right);
            motion_poll(&motion, sample, (uint64_t)step * 100000u);
            if (motion.phase == MOTION_ACTIVE) {
                assert(fabsf(motion.left.throttle) <= 0.65f);
                assert(fabsf(motion.right.throttle) <= 0.65f);
            }
        }
        assert(motion.phase == MOTION_DONE);
        stopped(&motion);
        motion_poll(&motion, motion.start, 20000000u);
        assert(motion.phase == MOTION_DONE); /* No automatic restart. */
        stopped(&motion);
    }

    motion_control_t motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, motion.start, 100000u);
    /* The Batch 5 first PI update is 0.4647/0.44475 (1800cps, dt=.1s), but the straight launch
     * ramp limits the first tick to the breakaway floor (left 0.28, right 0.30), then +0.09 per 100 ms tick. */
    assert(fabsf(motion.left.throttle - 0.28f) < 0.000001f);
    assert(fabsf(motion.right.throttle - 0.30f) < 0.000001f);
    motion_poll(&motion, motion.start, 200000u);
    assert(fabsf(motion.left.throttle - 0.37f) < 0.00001f);
    assert(fabsf(motion.right.throttle - 0.39f) < 0.00001f);
    motion_poll(&motion, motion.start, 300000u);
    assert(motion.left.throttle > 0.44f && motion.left.throttle <= 0.65f); /* reaches the PI value */
    assert(motion.right.throttle > 0.42f && motion.right.throttle <= 0.65f);
    motion_abort(&motion);
    assert(motion.phase == MOTION_ABORTED);
    stopped(&motion);
    motion_poll(&motion, at(&motion, 500, 500), 200000u);
    assert(motion.phase == MOTION_ABORTED);
    stopped(&motion);

    motion = start(MOTION_LEFT_90);
    motion_poll(&motion, motion.start, 100000u);
    assert(fabsf(motion.left.throttle + 0.4098f) < 0.000001f);
    assert(fabsf(motion.right.throttle - 0.3915f) < 0.000001f);

    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, at(&motion, 3537, 100), 100000u);
    assert(motion.phase == MOTION_ACTIVE && motion.left.finished);
    assert(motion.left.throttle == 0.0f && motion.right.throttle > 0.0f);
    motion_poll(&motion, at(&motion, 3540, 3537), 200000u);
    assert(motion.phase == MOTION_DONE);
    stopped(&motion);

    /* Each wheel must stop between PI ticks, including reverse-turn endpoints.
     * Cutoff position must be retained when the wheel subsequently coasts. */
    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, motion.start, 100000u);
    motion_poll(&motion, at(&motion, 3537, 100), 105000u);
    assert(motion.phase == MOTION_ACTIVE && motion.left.finished);
    assert(motion.left.throttle == 0.0f && motion.left.cutoff_position == 3537);
    motion_poll(&motion, at(&motion, 3558, 3537), 110000u);
    assert(motion.phase == MOTION_DONE);
    assert(motion.left.cutoff_position == 3537 && motion.right.cutoff_position == 3537);
    assert(motion.left.cutoff_elapsed_us == 105000u && motion.right.cutoff_elapsed_us == 110000u);
    stopped(&motion);
    motion = start(MOTION_LEFT_90);
    motion_poll(&motion, motion.start, 100000u);
    motion_poll(&motion, at(&motion, -994, 994), 105000u);
    assert(motion.phase == MOTION_DONE && motion.left.cutoff_position == -994);
    assert(motion.left.cutoff_elapsed_us == 105000u && motion.right.cutoff_elapsed_us == 105000u);
    stopped(&motion);

    /* Straight balancing: leading left loses PWM, lagging right gains it.
     * Values below independently calculated old PI outputs differ by .03. */
    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, at(&motion, 100, 0), 100000u);
    /* Unramped PI+balance values were 0.38722/0.4854 here and 0.50568/0.3676 below; the launch
     * ramp caps the first tick of both at the 0.30 breakaway floor. */
    assert(fabsf(motion.left.throttle - 0.28f) < 0.000001f);
    assert(fabsf(motion.right.throttle - 0.30f) < 0.000001f);
    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, at(&motion, 0, 100), 100000u);
    assert(fabsf(motion.left.throttle - 0.28f) < 0.000001f);
    assert(fabsf(motion.right.throttle - 0.30f) < 0.000001f);
    /* Near endpoint both old PI outputs clamp to .25. Position feedback must
     * still be able to slow the leading wheel, and must not accumulate at
     * between-update polls. */
    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, at(&motion, 3400, 3300), 100000u);
    assert(fabsf(motion.left.throttle - 0.22f) < 0.000001f);
    assert(fabsf(motion.right.throttle - 0.28f) < 0.000001f);
    motion_poll(&motion, at(&motion, 3410, 3310), 105000u);
    assert(fabsf(motion.left.throttle - 0.22f) < 0.000001f);
    assert(fabsf(motion.right.throttle - 0.28f) < 0.000001f);

    motion = start(MOTION_STRAIGHT_30CM);
    for (unsigned i = 1u; i <= 10u; ++i) {
        motion_poll(&motion, motion.start, (uint64_t)i * 100000u);
    }
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_STALLED);
    stopped(&motion);

    motion = start(MOTION_STRAIGHT_30CM);
    encoder_sample_t invalid = motion.start;
    invalid.right_invalid += MOTION_ENCODER_INVALID_TOLERANCE;
    motion_poll(&motion, invalid, 1u);
    assert(motion.phase == MOTION_ACTIVE); /* isolated missed edges are tolerated */
    ++invalid.right_invalid;
    motion_poll(&motion, invalid, 2u);
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_ENCODER_INVALID);
    stopped(&motion);

    motion = start(MOTION_STRAIGHT_30CM);
    invalid = motion.start;
    invalid.left_invalid += MOTION_ENCODER_INVALID_TOLERANCE + 1u;
    motion_poll(&motion, invalid, 1u);
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_ENCODER_INVALID);
    stopped(&motion);

    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, at(&motion, -65, 0), 100000u);
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_WRONG_DIRECTION);
    stopped(&motion);

    motion = start(MOTION_RIGHT_90);
    motion_poll(&motion, at(&motion, 0, 65), 100000u);
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_WRONG_DIRECTION);
    stopped(&motion);

    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, motion.start, 250001u);
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_LATE_POLL);
    stopped(&motion);

    motion = start(MOTION_STRAIGHT_30CM);
    motion_poll(&motion, motion.start, 12000000u);
    assert(motion.phase == MOTION_FAULT && motion.fault == MOTION_TIMED_OUT);
    stopped(&motion);
    motion_init(&motion);
    assert(!motion_begin(&motion, MOTION_NONE, invalid, 0u));
    assert(!motion_begin(&motion, (motion_kind_t)99, invalid, 0u));
    puts("PASS motion host tests: straight correction, preserved turn targets/PI, "
         "straight balance/floor handling, between-tick endpoints, cutoff timestamps, "
         "cutoff retention, four manoeuvres, count wrap, "
         "independent wheel completion, abort, stall, wrong direction, "
         "invalid transition, timeout, delayed poll and request validation.");
    return 0;
}
