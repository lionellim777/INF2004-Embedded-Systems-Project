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
} terrain_status_t;

/* GY-511 / LSM303DLHC on Grove 3: SDA=GP4, SCL=GP5. */
bool terrain_init(void);
/* Call while the car is stationary and level; takes about two seconds. */
bool terrain_calibrate(void);
/* Call frequently while driving. Reports completed humps over USB serial. */
void terrain_poll(void);
/* Call at the instant the motors start (MOTION GO): roll is ignored for a short launch hold-off. */
void terrain_motion_go(void);
terrain_status_t terrain_get_status(void);

#endif
