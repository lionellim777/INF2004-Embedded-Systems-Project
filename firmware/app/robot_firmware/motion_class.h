#ifndef INF2004_MOTION_CLASS_H
#define INF2004_MOTION_CLASS_H
/* Motion classification and turn rate (pure logic, no hardware).
 *
 * States, from the Buddy 4 brief: stationary, accelerating, turning, climbing hump, descending
 * hump; plus cruising (steady speed) and decelerating so that every moment has a state.
 *
 * Inputs: forward-positive cumulative wheel counts (both wheels count up when the car drives
 * forward), the right-wheel-raised roll in degrees and whether the hump detector (hump_roll.h)
 * currently has a right-side hump event open. The GY-511 has no gyroscope, so speed and turn rate
 * come from the wheel encoders: v = mean wheel speed, turn rate = (vR - vL) / track.
 *
 * Stability: speeds are smoothed, acceleration is measured over about half a second, and a state
 * change only takes effect once the new state has held for MOTION_CLASS_DWELL_MS. Limits come from
 * the slow (600 counts/s) hump runs of 2026-11-17: cruise speed wanders by +/-100 counts/s from
 * the PI controller, the launch ramp reaches 500 counts/s in about 1.5 s, and line steering
 * produces under about 10 deg/s while a pivot turn produces about 30 deg/s. */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define MOTION_CLASS_COUNTS_PER_MM 12.13f /* measured: 3640 counts for a 300 mm move */
#define MOTION_CLASS_TRACK_MM 115.0f
#define MOTION_CLASS_WINDOW_MS 100u       /* shortest span between speed updates */
#define MOTION_CLASS_DWELL_MS 300u        /* a new state must hold this long */
#define MOTION_CLASS_MOVE_MM_S 15.0f      /* above this the car is moving (about 180 counts/s) */
#define MOTION_CLASS_STOP_MM_S 8.0f       /* below this it has stopped (about 100 counts/s) */
#define MOTION_CLASS_ACCEL_MM_S2 12.0f    /* speeding up or slowing down faster than this (about 145 counts/s per s) */
#define MOTION_CLASS_TURN_DPS 20.0f       /* turning above this ... */
#define MOTION_CLASS_TURN_END_DPS 12.0f   /* ... until it falls under this */
#define MOTION_CLASS_HUMP_DROP_DEG 1.5f   /* roll falling this far below the peak means descending */
#define MOTION_CLASS_HIST 8u
#define MOTION_CLASS_ACCEL_MIN_AGE_MS 300u
#define MOTION_CLASS_ACCEL_MAX_AGE_MS 800u
#define MOTION_CLASS_MAX_GAP_MS 1000u     /* a longer gap in the calls restarts the measurement */

typedef enum {
    MOTION_CLASS_STATIONARY = 0,
    MOTION_CLASS_ACCELERATING,
    MOTION_CLASS_CRUISING,
    MOTION_CLASS_DECELERATING,
    MOTION_CLASS_TURNING,
    MOTION_CLASS_CLIMBING_HUMP,
    MOTION_CLASS_DESCENDING_HUMP,
} motion_class_t;

typedef struct {
    bool have_last;
    uint32_t last_ms;
    int32_t last_left;
    int32_t last_right;
    float speed_mm_s;      /* smoothed mean wheel speed, forward positive */
    float turn_dps;        /* smoothed turn rate, positive = turning left (right wheel faster) */
    bool moving;
    bool turning;
    float hist_v[MOTION_CLASS_HIST];
    uint32_t hist_t[MOTION_CLASS_HIST];
    uint8_t hist_count;
    uint8_t hist_next;
    float accel_mm_s2;
    float hump_peak_deg;
    float hump_low_deg;
    bool hump_descending;
    bool hump_was_active;
    motion_class_t state;
    motion_class_t candidate;
    uint32_t candidate_ms;
} motion_class_state_t;

typedef struct {
    bool changed;
    motion_class_t from;
    motion_class_t to;
} motion_class_event_t;

static inline const char *motion_class_name(motion_class_t s)
{
    switch (s) {
    case MOTION_CLASS_STATIONARY: return "STATIONARY";
    case MOTION_CLASS_ACCELERATING: return "ACCELERATING";
    case MOTION_CLASS_CRUISING: return "CRUISING";
    case MOTION_CLASS_DECELERATING: return "DECELERATING";
    case MOTION_CLASS_TURNING: return "TURNING";
    case MOTION_CLASS_CLIMBING_HUMP: return "CLIMBING_HUMP";
    case MOTION_CLASS_DESCENDING_HUMP: return "DESCENDING_HUMP";
    }
    return "UNKNOWN";
}

static inline void motion_class_reset(motion_class_state_t *m)
{
    *m = (motion_class_state_t){0};
}

/* Smoothing factor for a span of dt_ms with time constant tau_ms. */
static inline float motion_class_alpha(uint32_t dt_ms, float tau_ms)
{
    return 1.0f - expf(-(float)dt_ms / tau_ms);
}

/* Call as often as convenient (the car's main loop). hump_roll_deg is positive when the right
 * wheel is raised, relative to the hump detector's baseline; hump_active is true while the hump
 * detector has a right-side event open. */
static inline motion_class_event_t motion_class_update(motion_class_state_t *m, int32_t left_counts,
                                                       int32_t right_counts, float hump_roll_deg,
                                                       bool hump_active, uint32_t now_ms)
{
    motion_class_event_t event = {false, m->state, m->state};
    if (!m->have_last || now_ms - m->last_ms > MOTION_CLASS_MAX_GAP_MS) {
        const motion_class_state_t keep = *m;
        motion_class_reset(m);
        m->state = keep.state;
        m->candidate = keep.state;
        m->have_last = true;
        m->last_ms = now_ms;
        m->last_left = left_counts;
        m->last_right = right_counts;
        return event;
    }

    /* Hump phase follows the roll sample by sample, not the 100 ms speed window. */
    if (hump_active) {
        if (!m->hump_was_active) {
            m->hump_peak_deg = hump_roll_deg;
            m->hump_low_deg = hump_roll_deg;
            m->hump_descending = false;
        } else if (!m->hump_descending) {
            if (hump_roll_deg > m->hump_peak_deg) {
                m->hump_peak_deg = hump_roll_deg;
            } else if (hump_roll_deg < m->hump_peak_deg - MOTION_CLASS_HUMP_DROP_DEG) {
                m->hump_descending = true;
                m->hump_low_deg = hump_roll_deg;
            }
        } else {
            if (hump_roll_deg < m->hump_low_deg) {
                m->hump_low_deg = hump_roll_deg;
            } else if (hump_roll_deg > m->hump_low_deg + MOTION_CLASS_HUMP_DROP_DEG) {
                m->hump_descending = false;   /* a second rise (lip end, sway): climbing again */
                m->hump_peak_deg = hump_roll_deg;
            }
        }
    }
    m->hump_was_active = hump_active;

    const uint32_t dt_ms = now_ms - m->last_ms;
    if (dt_ms >= MOTION_CLASS_WINDOW_MS) {
        const float dt_s = (float)dt_ms / 1000.0f;
        const float vl = (float)(left_counts - m->last_left) / dt_s / MOTION_CLASS_COUNTS_PER_MM;
        const float vr = (float)(right_counts - m->last_right) / dt_s / MOTION_CLASS_COUNTS_PER_MM;
        const float v = 0.5f * (vl + vr);
        const float yaw = (vr - vl) / MOTION_CLASS_TRACK_MM * 57.2957795f;
        const float a_speed = motion_class_alpha(dt_ms, 300.0f);
        const float a_yaw = motion_class_alpha(dt_ms, 200.0f);
        m->speed_mm_s += (v - m->speed_mm_s) * a_speed;
        m->turn_dps += (yaw - m->turn_dps) * a_yaw;
        m->last_ms = now_ms;
        m->last_left = left_counts;
        m->last_right = right_counts;

        /* Acceleration: change of smoothed speed over the oldest sample that is 0.3-0.8 s old. */
        float accel = 0.0f;
        uint32_t best_age = 0u;
        for (uint8_t i = 0; i < m->hist_count; ++i) {
            const uint32_t age = now_ms - m->hist_t[i];
            if (age >= MOTION_CLASS_ACCEL_MIN_AGE_MS && age <= MOTION_CLASS_ACCEL_MAX_AGE_MS && age > best_age) {
                best_age = age;
                accel = (m->speed_mm_s - m->hist_v[i]) / ((float)age / 1000.0f);
            }
        }
        m->accel_mm_s2 = accel;
        m->hist_v[m->hist_next] = m->speed_mm_s;
        m->hist_t[m->hist_next] = now_ms;
        m->hist_next = (uint8_t)((m->hist_next + 1u) % MOTION_CLASS_HIST);
        if (m->hist_count < MOTION_CLASS_HIST) {
            ++m->hist_count;
        }

        if (!m->moving && m->speed_mm_s > MOTION_CLASS_MOVE_MM_S) {
            m->moving = true;
        } else if (m->moving && m->speed_mm_s < MOTION_CLASS_STOP_MM_S) {
            m->moving = false;
        }
        const float turn = fabsf(m->turn_dps);
        if (!m->turning && turn > MOTION_CLASS_TURN_DPS) {
            m->turning = true;
        } else if (m->turning && turn < MOTION_CLASS_TURN_END_DPS) {
            m->turning = false;
        }
    }

    motion_class_t now_class;
    if (hump_active) {
        now_class = m->hump_descending ? MOTION_CLASS_DESCENDING_HUMP : MOTION_CLASS_CLIMBING_HUMP;
    } else if (m->turning) {
        now_class = MOTION_CLASS_TURNING;
    } else if (!m->moving) {
        now_class = MOTION_CLASS_STATIONARY;
    } else if (m->accel_mm_s2 > MOTION_CLASS_ACCEL_MM_S2) {
        now_class = MOTION_CLASS_ACCELERATING;
    } else if (m->accel_mm_s2 < -MOTION_CLASS_ACCEL_MM_S2) {
        now_class = MOTION_CLASS_DECELERATING;
    } else {
        now_class = MOTION_CLASS_CRUISING;
    }

    if (now_class == m->state) {
        m->candidate = m->state;
    } else if (now_class != m->candidate) {
        m->candidate = now_class;
        m->candidate_ms = now_ms;
    } else if (now_ms - m->candidate_ms >= MOTION_CLASS_DWELL_MS) {
        event.changed = true;
        event.from = m->state;
        event.to = now_class;
        m->state = now_class;
    }
    return event;
}
#endif
