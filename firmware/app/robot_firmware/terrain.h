#ifndef INF2004_TERRAIN_H
#define INF2004_TERRAIN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool available;
    bool calibrated;
    bool pitch_valid;
    float current_pitch_deg;
    uint32_t pitch_sequence;
    uint32_t read_errors;
    uint32_t hump_count;
    uint32_t highest_hump_number;
    float highest_peak_angle_deg;
    /* Roll-based hump detector (hump_roll.h): sideways tip from a one-wheel hump. */
    float current_roll_change_deg;
    uint32_t roll_hump_count;
    float last_roll_hump_mm;
    float highest_roll_hump_mm;
    /* Tilt alert (tilt_alert.h): sustained tilt well beyond a hump, from the level calibration. */
    bool tilt_alert_active;
    uint32_t tilt_alert_count;
    float max_tilt_deg;
} terrain_status_t;

/* GY-511 / LSM303DLHC on Grove 3: SDA=GP4, SCL=GP5. */
bool terrain_init(void);
/* Call while the car is stationary and level; takes about two seconds. */
bool terrain_calibrate(void);
/* Call frequently while driving. Reports completed humps over USB serial. */
void terrain_poll(void);
/* Call when a run is started (START press, before the countdown): the roll detector restarts
 * from the level calibration and re-converges its baseline while the car stands still. */
void terrain_run_start(void);
/* Call at the instant the motors start (MOTION GO), and while the car is deliberately stopped or
 * pivoting inside a run: roll is ignored until a short hold-off after the last call. */
void terrain_motion_go(void);
/* Call when a run has ended and settled (polling is about to stop): reports a roll event that is
 * still open instead of dropping it. */
void terrain_run_end(void);
/* Print the roll samples recorded since the last terrain_motion_go()/terrain_run_start() (up to
 * about 16 s) as [ROLL TRACE] lines. Call after the motors have stopped, never while driving. */
void terrain_print_roll_trace(void);
terrain_status_t terrain_get_status(void);

#endif
