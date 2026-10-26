#ifndef INF2004_TERRAIN_H
#define INF2004_TERRAIN_H

#include <stdbool.h>
#include <stdint.h>

enum {
    TERRAIN_SESSION_MAX_MS = 1800000u,
    TERRAIN_SPEED_MAX_MM_S = 500,
    TERRAIN_HUMP_MAX_MM = 5000u,
    TERRAIN_HUMP_MAX_COUNT = 3000u,
    TERRAIN_PAYLOAD_BITS = 110u,
    TERRAIN_PAYLOAD_BYTES = 14u,
    ROBOT_FAULT_MOTOR_STALL = 1u << 0,
    ROBOT_FAULT_ENCODER = 1u << 1,
    ROBOT_FAULT_IMU = 1u << 2,
    ROBOT_FAULT_NO_ECHO = 1u << 3,
    ROBOT_FAULT_WIFI = 1u << 4,
    ROBOT_FAULT_MASK = 0x1fu,
};

typedef enum {
    TERRAIN_MOTION_UNKNOWN,
    TERRAIN_MOTION_STATIONARY,
    TERRAIN_MOTION_ACCELERATING,
    TERRAIN_MOTION_TURNING,
    TERRAIN_MOTION_CLIMBING,
    TERRAIN_MOTION_DESCENDING,
    TERRAIN_MOTION_IMPACT,
    TERRAIN_MOTION_MOVING,
} terrain_motion_t;

typedef struct __attribute__((packed)) {
    unsigned int available : 1;
    unsigned int calibrated : 1;
    unsigned int sample_valid : 1;
    unsigned int height_valid : 1;
    unsigned int highest_height_valid : 1;
    unsigned int motion : 3;
    unsigned int hump_count : 12;
    unsigned int highest_hump_number : 12;
    unsigned int highest_height_hump_number : 12;
    unsigned int highest_peak_angle_cdeg : 14;
    unsigned int current_hump_mm : 13;
    unsigned int highest_hump_mm : 13;
    unsigned int captured_ms : 21;
    unsigned int faults : 5;
} terrain_status_t;

_Static_assert(sizeof(terrain_status_t) == TERRAIN_PAYLOAD_BYTES, "Check target packed terrain layout");

static inline void terrain_pack_bits(uint8_t *bytes, unsigned int offset, unsigned int count, uint32_t value)
{
    for (unsigned int bit = 0u; bit < count; ++bit) {
        unsigned int index = offset + bit;
        uint8_t mask = (uint8_t)(1u << (index % 8u));
        bytes[index / 8u] = (uint8_t)((bytes[index / 8u] & (uint8_t)~mask) | (((value >> bit) & 1u) ? mask : 0u));
    }
}

static inline uint32_t terrain_unpack_bits(const uint8_t *bytes, unsigned int offset, unsigned int count)
{
    uint32_t value = 0u;
    for (unsigned int bit = 0u; bit < count; ++bit) {
        unsigned int index = offset + bit;
        value |= (uint32_t)((bytes[index / 8u] >> (index % 8u)) & 1u) << bit;
    }
    return value;
}

static inline size_t terrain_encode_status(const terrain_status_t *s, uint8_t *output, size_t capacity)
{
    if (s == NULL || output == NULL || capacity < TERRAIN_PAYLOAD_BYTES ||
        s->hump_count > TERRAIN_HUMP_MAX_COUNT ||
        s->highest_hump_number > s->hump_count ||
        s->highest_height_hump_number > s->hump_count ||
        s->highest_peak_angle_cdeg > 9000u ||
        s->current_hump_mm > TERRAIN_HUMP_MAX_MM ||
        s->highest_hump_mm > TERRAIN_HUMP_MAX_MM ||
        s->captured_ms > TERRAIN_SESSION_MAX_MS) {
        return 0u;
    }
    for (unsigned int i = 0u; i < TERRAIN_PAYLOAD_BYTES; ++i) {
        output[i] = 0u;
    }
    terrain_pack_bits(output, 0u, 1u, s->available);
    terrain_pack_bits(output, 1u, 1u, s->calibrated);
    terrain_pack_bits(output, 2u, 1u, s->sample_valid);
    terrain_pack_bits(output, 3u, 1u, s->height_valid);
    terrain_pack_bits(output, 4u, 1u, s->highest_height_valid);
    terrain_pack_bits(output, 5u, 3u, s->motion);
    terrain_pack_bits(output, 8u, 12u, s->hump_count);
    terrain_pack_bits(output, 20u, 12u, s->highest_hump_number);
    terrain_pack_bits(output, 32u, 12u, s->highest_height_hump_number);
    terrain_pack_bits(output, 44u, 14u, s->highest_peak_angle_cdeg);
    terrain_pack_bits(output, 58u, 13u, s->current_hump_mm);
    terrain_pack_bits(output, 71u, 13u, s->highest_hump_mm);
    terrain_pack_bits(output, 84u, 21u, s->captured_ms);
    terrain_pack_bits(output, 105u, 5u, s->faults);
    return TERRAIN_PAYLOAD_BYTES;
}

/* GY-511 / LSM303DLHC on Grove 3: SDA=GP4, SCL=GP5. */
bool terrain_init(void);
/* Call while the car is stationary and level; takes about two seconds. */
bool terrain_calibrate(void);
/* Call frequently while driving. Reports completed humps over USB serial. */
void terrain_poll(void);
terrain_status_t terrain_get_status(void);
/* Calls from task context on core 0. False invalidates input; no partial/truncated values. */
bool terrain_set_motion(uint32_t captured_ms, int32_t left_mm_s, int32_t right_mm_s, bool valid);
/* Cancel an interrupted traversal, preserving completed counts/maxima. */
void terrain_suspend(void);
/* Core 0 task/ISR safe five-bit SHARED application fault mask. Each buddy updates only its own bit. Core 1 is not supported by this IRQ-only lock. */
void robot_fault_update(uint8_t mask, bool active);
uint8_t robot_fault_get(void);

#endif
