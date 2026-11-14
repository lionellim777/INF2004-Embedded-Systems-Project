#ifndef INF2004_HUMP_ROLL_H
#define INF2004_HUMP_ROLL_H
/* Roll-based hump detector and height estimate (pure logic, no hardware).
 *
 * Why roll: the hump is 50 mm wide and sits under ONE drive wheel, so it tips the car
 * sideways about 0.5 degrees per millimetre of lift (measured, GY-511, see
 * docs/testing/measurement-dashboard.md) and barely tilts it forward (pitch saturates
 * and the old 5 degree pitch detector never triggers on a 10 mm hump).
 *
 * Input: roll change from the level calibration, in degrees, one filtered sample every
 * 20 ms. Sign: right wheel raised reads NEGATIVE, left wheel raised POSITIVE.
 *
 * Height: rigid three-point car, one drive wheel raised by h, track T between tyre
 * contacts: tan(roll) ~= h / T. The real car reads low at small heights (the ideal formula
 * gave 8.9/14.5/19.8 mm for stacks of 10/15/20 mm), so hump_roll_height_mm() interpolates
 * the measured response. At 50-100 mm/s the 160 ms filter lowers the apex by well under 0.5 mm.
 * The estimate assumes the car is moving straight. The level baseline moved about 0.7
 * degrees between floor spots, which alone would shift the height by about 1.4 mm, so the
 * detector keeps its own baseline. For the first 2 seconds of a run (the stationary
 * countdown) it converges quickly (time constant 0.33 s); after that it follows only very
 * slowly (10 s) and only while within EXIT of itself, so it cannot chase a slow ramp or a
 * hump. The level calibration is just its starting value. */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define HUMP_ROLL_TRACK_MM 110.0f
#define HUMP_ROLL_ENTER_DEG 2.5f   /* about 5 mm of lift */
#define HUMP_ROLL_EXIT_DEG 1.5f    /* hysteresis */
#define HUMP_ROLL_ENTER_MS 120u    /* must stay above ENTER this long */
#define HUMP_ROLL_EXIT_MS 200u     /* must stay below EXIT this long */
#define HUMP_ROLL_MIN_MS 350u      /* shorter events are bumps, not humps (real humps: 500+ ms; one at 381 ms was missed at 450) */
#define HUMP_ROLL_TIMEOUT_MS 10000u
#define HUMP_ROLL_BASELINE_FAST 0.06f  /* per 20 ms sample: 0.33 s time constant */
#define HUMP_ROLL_BASELINE_SLOW 0.002f /* per 20 ms sample: 10 s time constant */
#define HUMP_ROLL_FAST_SAMPLES 100u    /* 2 s at the start of every run */
/* A real moving hump read about 26% high against the paper-stack table (14 runs, 2026-11-14:
 * mean 12.6 mm, with the hump fixed down 12.7 mm, true 10 mm), so the logged height is scaled. */
#define HUMP_ROLL_HEIGHT_SCALE 0.80f
#define HUMP_ROLL_LAUNCH_HOLDOFF_MS 600u /* ignore roll after GO: launch acceleration reads as roll */

typedef enum {
    HUMP_ROLL_WAITING = 0,
    HUMP_ROLL_CANDIDATE,
    HUMP_ROLL_ACTIVE,
} hump_roll_phase_t;

typedef struct {
    hump_roll_phase_t phase;
    int8_t sign;           /* -1 right wheel raised, +1 left wheel raised */
    float baseline_deg;    /* adapting level reference, relative to the calibration */
    uint16_t fast_left;    /* samples of fast baseline convergence still to go */
    float peak_deg;        /* largest |roll change| this event, from the baseline */
    float sum_deg;         /* sum of |roll change| over this event's samples above ENTER */
    uint16_t count;        /* number of those samples */
    uint32_t start_ms;
    uint32_t below_ms;     /* when roll first fell under EXIT, 0 if not below */
    bool below;
    bool need_clear;       /* after a timeout: ignore until roll returns below EXIT */
    bool holdoff_active;   /* launch hold-off armed by hump_roll_motion_go() */
    uint32_t holdoff_until_ms;
} hump_roll_t;

typedef enum {
    HUMP_ROLL_NO_EVENT = 0,
    HUMP_ROLL_COMPLETED,
    HUMP_ROLL_TOO_SHORT,   /* bump: counted nowhere, reported for diagnostics */
    HUMP_ROLL_WRONG_SIDE, /* long enough, but LEFT wheel raised: the hump is only under the right wheel */
    HUMP_ROLL_TIMED_OUT,
} hump_roll_result_t;

typedef struct {
    hump_roll_result_t result;
    int8_t sign;
    float peak_deg;
    float height_mm;
    float baseline_deg;
    uint32_t duration_ms;
    float mean_deg;        /* mean |roll| over the samples above ENTER (diagnostic) */
} hump_roll_event_t;

/* Measured response (right wheel on flat stacks of paper, one spot, 2026-11-14): roll change
 * from level for a lift of 0, 10, 15 and 20 mm. The ideal model gives 0, 5.18, 7.73 and
 * 10.23 degrees; the real car reads lower at small heights (tyre sag), so the estimate
 * interpolates the measurements instead of T*tan(roll). Beyond 20 mm it continues with the
 * last measured slope. Re-measure and edit these numbers if the tyres or chassis change. */
#define HUMP_ROLL_POINTS 4
static inline float hump_roll_height_mm(float roll_change_deg)
{
    static const float roll_deg[HUMP_ROLL_POINTS] = {0.0f, 4.62f, 7.49f, 10.16f};
    static const float lift_mm[HUMP_ROLL_POINTS] = {0.0f, 10.0f, 15.0f, 20.0f};
    const float r = fabsf(roll_change_deg);
    int i = 1;
    while (i < HUMP_ROLL_POINTS - 1 && r > roll_deg[i]) {
        ++i;
    }
    return lift_mm[i - 1] + (lift_mm[i] - lift_mm[i - 1]) * (r - roll_deg[i - 1]) / (roll_deg[i] - roll_deg[i - 1]);
}

/* After level calibration (car stationary): forget everything, converge quickly. */
static inline void hump_roll_reset(hump_roll_t *h)
{
    *h = (hump_roll_t){0};
    h->fast_left = HUMP_ROLL_FAST_SAMPLES;
}

/* At the start of each run: restart from the level calibration (baseline 0) and re-converge
 * quickly. The old baseline is deliberately NOT kept: a car left tilted before a run would
 * otherwise carry that tilt into later runs and raise false events. */
static inline void hump_roll_begin_run(hump_roll_t *h)
{
    *h = (hump_roll_t){0};
    h->fast_left = HUMP_ROLL_FAST_SAMPLES;
}

/* Call when the motors start moving. For HUMP_ROLL_LAUNCH_HOLDOFF_MS the detector ignores
 * roll (launch acceleration shows up as a false roll swing) and drops any event in progress. */
static inline void hump_roll_motion_go(hump_roll_t *h, uint32_t now_ms)
{
    h->holdoff_active = true;
    h->holdoff_until_ms = now_ms + HUMP_ROLL_LAUNCH_HOLDOFF_MS;
    h->phase = HUMP_ROLL_WAITING;
    h->below = false;
    h->need_clear = false;
}

/* Flat-floor runs raised long LEFT events from the car's own sway; the hump is only ever under
 * the RIGHT wheel (negative roll), so only right-side events can complete. */
static inline hump_roll_result_t hump_roll_classify(const hump_roll_t *h, uint32_t duration_ms)
{
    if (duration_ms < HUMP_ROLL_MIN_MS) {
        return HUMP_ROLL_TOO_SHORT;
    }
    return h->sign < 0 ? HUMP_ROLL_COMPLETED : HUMP_ROLL_WRONG_SIDE;
}

static inline void hump_roll_fill(hump_roll_event_t *e, hump_roll_result_t result,
                                  const hump_roll_t *h, uint32_t end_ms)
{
    e->result = result;
    e->sign = h->sign;
    e->peak_deg = h->peak_deg;
    /* Height still comes from the peak. The mean over the samples above ENTER is only logged: a
     * plateau estimate (2*mean - ENTER) was tried in simulation and was only about 28% less
     * sensitive to a dynamic spike, too marginal to ship. Compare peak and mean in real logs
     * before changing the estimator. */
    e->mean_deg = h->count > 0u ? h->sum_deg / (float)h->count : h->peak_deg;
    e->height_mm = hump_roll_height_mm(h->peak_deg) * HUMP_ROLL_HEIGHT_SCALE;
    e->baseline_deg = h->baseline_deg;
    e->duration_ms = end_ms - h->start_ms;
}

/* Feed one sample. Returns the event produced by this sample, if any. */
static inline hump_roll_event_t hump_roll_update(hump_roll_t *h, float calibrated_roll_change_deg, uint32_t now_ms)
{
    hump_roll_event_t event = {HUMP_ROLL_NO_EVENT, 0, 0.0f, 0.0f, 0.0f, 0u, 0.0f};
    if (h->holdoff_active) {
        if ((int32_t)(now_ms - h->holdoff_until_ms) < 0) {
            return event;
        }
        h->holdoff_active = false;
    }
    float roll_change_deg = calibrated_roll_change_deg - h->baseline_deg;
    if (h->phase == HUMP_ROLL_WAITING && !h->need_clear) {
        const bool fast = h->fast_left > 0u;
        if (fast) {
            --h->fast_left;
        }
        /* Both phases adapt only within EXIT of the baseline, so a tilt cannot be absorbed. */
        if (fabsf(roll_change_deg) < HUMP_ROLL_EXIT_DEG) {
            h->baseline_deg += roll_change_deg * (fast ? HUMP_ROLL_BASELINE_FAST : HUMP_ROLL_BASELINE_SLOW);
            roll_change_deg = calibrated_roll_change_deg - h->baseline_deg;
        }
    }
    const float magnitude = fabsf(roll_change_deg);
    const int8_t sign = roll_change_deg < 0.0f ? -1 : 1;

    if (h->need_clear) {
        if (magnitude < HUMP_ROLL_EXIT_DEG) {
            h->need_clear = false;
        }
        return event;
    }

    switch (h->phase) {
    case HUMP_ROLL_WAITING:
        if (magnitude > HUMP_ROLL_ENTER_DEG) {
            h->phase = HUMP_ROLL_CANDIDATE;
            h->sign = sign;
            h->peak_deg = magnitude;
            h->sum_deg = magnitude;
            h->count = 1u;
            h->start_ms = now_ms;
            h->below = false;
        }
        break;
    case HUMP_ROLL_CANDIDATE:
        if (magnitude > HUMP_ROLL_ENTER_DEG && sign == h->sign) {
            if (magnitude > h->peak_deg) {
                h->peak_deg = magnitude;
            }
            h->sum_deg += magnitude;
            if (h->count < UINT16_MAX) {
                ++h->count;
            }
            if (now_ms - h->start_ms >= HUMP_ROLL_ENTER_MS) {
                h->phase = HUMP_ROLL_ACTIVE;
                h->below = false;
            }
        } else {
            h->phase = HUMP_ROLL_WAITING;
        }
        break;
    case HUMP_ROLL_ACTIVE:
        if (magnitude > HUMP_ROLL_ENTER_DEG && sign != h->sign) {
            /* Rolled the other way without settling: close this event, start the next. */
            const uint32_t duration = now_ms - h->start_ms;
            hump_roll_fill(&event, hump_roll_classify(h, duration),
                           h, now_ms);
            h->phase = HUMP_ROLL_CANDIDATE;
            h->sign = sign;
            h->peak_deg = magnitude;
            h->sum_deg = magnitude;
            h->count = 1u;
            h->start_ms = now_ms;
            h->below = false;
            break;
        }
        if (sign == h->sign && magnitude > h->peak_deg) {
            h->peak_deg = magnitude;
        }
        if (sign == h->sign && magnitude > HUMP_ROLL_ENTER_DEG) {
            h->sum_deg += magnitude;
            if (h->count < UINT16_MAX) {
                ++h->count;
            }
        }
        if (magnitude < HUMP_ROLL_EXIT_DEG) {
            if (!h->below) {
                h->below = true;
                h->below_ms = now_ms;
            }
            if (now_ms - h->below_ms >= HUMP_ROLL_EXIT_MS) {
                const uint32_t duration = h->below_ms - h->start_ms;
                hump_roll_fill(&event, hump_roll_classify(h, duration),
                               h, h->below_ms);
                h->phase = HUMP_ROLL_WAITING;
            }
        } else {
            h->below = false;
        }
        if (h->phase == HUMP_ROLL_ACTIVE && now_ms - h->start_ms > HUMP_ROLL_TIMEOUT_MS) {
            hump_roll_fill(&event, HUMP_ROLL_TIMED_OUT, h, now_ms);
            h->phase = HUMP_ROLL_WAITING;
            h->need_clear = true;
        }
        break;
    }
    return event;
}
#endif
