#ifndef INF2004_TERRAIN_H
#define INF2004_TERRAIN_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool available;
    bool calibrated;
    uint32_t hump_count;
    uint32_t highest_hump_number;
    float highest_peak_angle_deg;
} terrain_status_t;

/* GY-511 / LSM303DLHC on Grove 3: SDA=GP4, SCL=GP5. */
bool terrain_init(void);
/* Call while the car is stationary and level; takes about two seconds. */
bool terrain_calibrate(void);
/* Call frequently while driving. Reports completed humps over USB serial. */
void terrain_poll(void);
terrain_status_t terrain_get_status(void);

#endif
