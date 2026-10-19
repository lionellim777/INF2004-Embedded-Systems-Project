#ifndef INF2004_OBSTACLE_DATA_H
#define INF2004_OBSTACLE_DATA_H

#include <stdint.h>

/* A scan point fits in one word: 4-bit fine-scan index, 2-bit status,
 * and 10-bit distance in millimetres. A distance is meaningful only when
 * status is VALID. The tested fine scan is 50 + 5 * index degrees.
 * If this word is shared across tasks/ISRs, synchronize whole-word updates.
 * For MQTT, serialize bytes explicitly rather than copying host memory. */
enum {
    OBSTACLE_MAX_MM = 1000u,
    OBSTACLE_STOP_MM = 300u,
    OBSTACLE_FINE_POINTS = 16u,
    OBSTACLE_DISTANCE_MASK = 0x03ffu,
    OBSTACLE_STATUS_SHIFT = 10u,
    OBSTACLE_STATUS_MASK = 0x0c00u,
    OBSTACLE_INDEX_SHIFT = 12u,
};

typedef enum {
    OBSTACLE_NOT_MEASURED = 0u,
    OBSTACLE_VALID = 1u,
    OBSTACLE_UNAVAILABLE = 2u, /* No echo or an unusable echo. */
    OBSTACLE_OVER_LIMIT = 3u,  /* Echo beyond the 1000 mm reporting limit. */
} obstacle_status_t;

typedef uint16_t obstacle_sample_t;

static inline obstacle_sample_t obstacle_sample_make(uint16_t distance_mm,
                                                     obstacle_status_t status)
{
    if (status == OBSTACLE_VALID && distance_mm > OBSTACLE_MAX_MM) {
        status = OBSTACLE_OVER_LIMIT;
    }
    if (status != OBSTACLE_VALID) {
        distance_mm = 0u;
    }
    return (obstacle_sample_t)(((uint16_t)status << OBSTACLE_STATUS_SHIFT) |
                               distance_mm);
}

static inline obstacle_status_t obstacle_sample_status(obstacle_sample_t sample)
{
    return (obstacle_status_t)((sample & OBSTACLE_STATUS_MASK) >>
                               OBSTACLE_STATUS_SHIFT);
}

static inline uint16_t obstacle_sample_mm(obstacle_sample_t sample)
{
    return (uint16_t)(sample & OBSTACLE_DISTANCE_MASK);
}

/* 343 m/s round-trip conversion, rounded to the nearest millimetre.
 * The caller bounds pulse_us to 30000; the existing 20 mm lower limit stays. */
static inline obstacle_sample_t obstacle_sample_from_echo_us(uint32_t pulse_us)
{
    uint32_t distance_mm = (pulse_us * 343u + 1000u) / 2000u;
    if (pulse_us == 0u || distance_mm < 20u) {
        return obstacle_sample_make(0u, OBSTACLE_UNAVAILABLE);
    }
    if (distance_mm > OBSTACLE_MAX_MM) {
        return obstacle_sample_make(0u, OBSTACLE_OVER_LIMIT);
    }
    return obstacle_sample_make((uint16_t)distance_mm, OBSTACLE_VALID);
}

static inline uint16_t obstacle_fine_point_make(uint8_t index,
                                                obstacle_sample_t sample)
{
    if (index >= OBSTACLE_FINE_POINTS) {
        return obstacle_sample_make(0u, OBSTACLE_NOT_MEASURED);
    }
    return (uint16_t)(((uint16_t)index << OBSTACLE_INDEX_SHIFT) |
                      (sample & 0x0fffu));
}

static inline uint8_t obstacle_fine_point_index(uint16_t point)
{
    return (uint8_t)(point >> OBSTACLE_INDEX_SHIFT);
}

static inline uint16_t obstacle_fine_point_angle_deg(uint16_t point)
{
    return (uint16_t)(50u + 5u * obstacle_fine_point_index(point));
}

#endif
