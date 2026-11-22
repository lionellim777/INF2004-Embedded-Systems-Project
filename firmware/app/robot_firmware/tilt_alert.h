#ifndef INF2004_TILT_ALERT_H
#define INF2004_TILT_ALERT_H
/* Tilt alert (pure logic, no hardware): reports when the car stays tilted well beyond anything a
 * hump or normal driving produces, for example driving up a slope or being tipped.
 *
 * Input: pitch and roll change from the level calibration in degrees (front raised and right wheel
 * raised as terrain.c defines them), one filtered sample every 20-60 ms. The combined tilt is
 * sqrt(pitch^2 + roll^2), the angle of the car's up axis away from the calibrated level.
 *
 * Thresholds come from the logs of 2026-11-17 at course speed: the 10 mm hump tips the car 4.7-5.5
 * degrees sideways and under 4 degrees in pitch, flat-floor sway stays within about 1 degree, and
 * even the fast 'k' moves peaked at about 10 degrees for well under 400 ms. A 20 mm hump (the top of
 * the measured paper-stack range) tips the car about 10 degrees, about 11 combined with pitch, so the
 * alert sits at 15 degrees: it never fires on a hump up to about 25 mm or on driving, but fires on
 * a real slope or tip. */
#include "control_representation.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define TILT_ALERT_ENTER_DEG 15.0f  /* above this ... */
#define TILT_ALERT_ENTER_MS 400u    /* ... for this long raises the alert */
#define TILT_ALERT_EXIT_DEG 10.0f   /* below this ... (hysteresis) */
#define TILT_ALERT_EXIT_MS 300u     /* ... for this long clears it */

typedef enum {
    TILT_ALERT_NO_EVENT = 0,
    TILT_ALERT_RAISED,
    TILT_ALERT_CLEARED,
} tilt_alert_result_t;

typedef struct {
#if ROBOCAR_COMPACT_CONTROL_STATE
    bool active:1;
    bool above:1;          /* tilt is above ENTER and the alert is not yet raised */
    bool below:1;          /* tilt is below EXIT and the alert is still raised */
#else
    bool active;
    bool above;            /* tilt is above ENTER and the alert is not yet raised */
    bool below;            /* tilt is below EXIT and the alert is still raised */
#endif
    uint32_t since_ms;     /* when tilt first rose above ENTER, or the alert was raised */
    uint32_t below_ms;     /* when tilt first fell below EXIT */
    float peak_deg;        /* largest combined tilt since it first rose above ENTER */
    float peak_pitch_deg;  /* pitch and roll at that peak */
    float peak_roll_deg;
} tilt_alert_t;

typedef struct {
    tilt_alert_result_t result;
    float tilt_deg;        /* combined tilt now (RAISED) or the peak of the episode (CLEARED) */
    float pitch_deg;
    float roll_deg;
    uint32_t duration_ms;  /* CLEARED only: raised until it fell below EXIT */
} tilt_alert_event_t;

static inline void tilt_alert_reset(tilt_alert_t *t)
{
    *t = (tilt_alert_t){0};
}

static inline float tilt_alert_magnitude(float pitch_deg, float roll_deg)
{
    return sqrtf(pitch_deg * pitch_deg + roll_deg * roll_deg);
}

/* Feed one sample. Returns RAISED once when the alert starts and CLEARED once when it ends. */
static inline tilt_alert_event_t tilt_alert_update(tilt_alert_t *t, float pitch_deg, float roll_deg, uint32_t now_ms)
{
    tilt_alert_event_t event = {TILT_ALERT_NO_EVENT, 0.0f, 0.0f, 0.0f, 0u};
    const float tilt = tilt_alert_magnitude(pitch_deg, roll_deg);

    if (!t->active) {
        if (tilt <= TILT_ALERT_ENTER_DEG) {
            t->above = false;
            return event;
        }
        if (!t->above) {
            t->above = true;
            t->since_ms = now_ms;
            t->peak_deg = 0.0f;
        }
        if (tilt > t->peak_deg) {
            t->peak_deg = tilt;
            t->peak_pitch_deg = pitch_deg;
            t->peak_roll_deg = roll_deg;
        }
        if (now_ms - t->since_ms >= TILT_ALERT_ENTER_MS) {
            t->active = true;
            t->above = false;
            t->below = false;
            event.result = TILT_ALERT_RAISED;
            event.tilt_deg = tilt;
            event.pitch_deg = pitch_deg;
            event.roll_deg = roll_deg;
        }
        return event;
    }

    if (tilt > t->peak_deg) {
        t->peak_deg = tilt;
        t->peak_pitch_deg = pitch_deg;
        t->peak_roll_deg = roll_deg;
    }
    if (tilt < TILT_ALERT_EXIT_DEG) {
        if (!t->below) {
            t->below = true;
            t->below_ms = now_ms;
        }
        if (now_ms - t->below_ms >= TILT_ALERT_EXIT_MS) {
            event.result = TILT_ALERT_CLEARED;
            event.tilt_deg = t->peak_deg;
            event.pitch_deg = t->peak_pitch_deg;
            event.roll_deg = t->peak_roll_deg;
            event.duration_ms = t->below_ms - t->since_ms;
            t->active = false;
            t->below = false;
            t->above = false;
        }
    } else {
        t->below = false;
    }
    return event;
}
#endif
