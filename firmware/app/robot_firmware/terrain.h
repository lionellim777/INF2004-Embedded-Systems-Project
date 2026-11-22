#ifndef INF2004_TERRAIN_H
#define INF2004_TERRAIN_H

#include <stdbool.h>
#include <stdint.h>
#include "control_representation.h"

typedef struct
{
#if ROBOCAR_COMPACT_CONTROL_STATE
    /* Observer task writes; consumers copy the whole snapshot. Four flags and
     * the 0..6 motion state: 7 bits. */
    bool available : 1;
    bool calibrated : 1;
    bool pitch_valid : 1;
    bool tilt_alert_active : 1;
    unsigned motion_state : 3;
#else
    bool available;
    bool calibrated;
    bool pitch_valid;
#endif
    float current_pitch_deg;
    uint32_t pitch_sequence;
    uint32_t read_errors;
    uint32_t hump_count;
    uint32_t highest_hump_number;
    float highest_peak_angle_deg;
    /* Roll-based hump detector (hump_roll.h): sideways tip from a one-wheel
     * hump. */
    float current_roll_change_deg;
    uint32_t roll_hump_count;
    float last_roll_hump_mm;
    float highest_roll_hump_mm;
    /* Tilt alert (tilt_alert.h): sustained tilt well beyond a hump, from the
     * level calibration. */
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool tilt_alert_active;
#endif
    uint32_t tilt_alert_count;
    float max_tilt_deg;
    /* Motion classification and turn rate (motion_class.h): a motion_class_t
     * value, wheel speed from the encoders in mm/s, turn rate in deg/s
     * (positive = turning left). */
#if !ROBOCAR_COMPACT_CONTROL_STATE
    uint8_t motion_state;
#endif
    float speed_mm_s;
    float turn_rate_dps;
} terrain_status_t;

/* GY-511 / LSM303DLHC on Grove 3: SDA=GP4, SCL=GP5. */
bool terrain_init(void);
/* Call while the car is stationary and level; takes about two seconds. */
bool terrain_calibrate(void);
/* Call frequently while driving. Reports completed humps over USB serial. */
void terrain_poll(void);
/* Call when a run is started (START press, before the countdown): the roll
 * detector restarts from the level calibration and re-converges its baseline
 * while the car stands still. */
void terrain_run_start(void);
/* Call at the instant the motors start (MOTION GO), and while the car is
 * deliberately stopped or pivoting inside a run: roll is ignored until a short
 * hold-off after the last call. */
void terrain_motion_go(void);
/* Call when a run has ended and settled (polling is about to stop): reports a
 * roll event that is still open instead of dropping it. */
void terrain_run_end(void);
/* Print the roll samples recorded since the last
 * terrain_motion_go()/terrain_run_start() (up to about 16 s) as [ROLL TRACE]
 * lines. Call after the motors have stopped, never while driving. */
void terrain_print_roll_trace(void);
/* Call every main-loop pass with the cumulative wheel counts, forward positive
 * for both wheels, and the time in ms. Classifies the motion (stationary,
 * accelerating, cruising, decelerating, turning, climbing hump, descending
 * hump) and prints a [MOTION STATE] line when it changes. Works with the
 * accelerometer missing (hump phases are then never reported). */
void terrain_motion_update(int32_t left_forward_counts,
                           int32_t right_forward_counts, uint32_t now_ms);
terrain_status_t terrain_get_status(void);

#endif
