#include "terrain.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

enum {
    SDA_PIN = 4,
    SCL_PIN = 5,
    ACCEL_ADDRESS = 0x19,
    CALIBRATION_SAMPLES = 200,
    FILTER_SAMPLES = 8,
    SAMPLE_INTERVAL_MS = 20,
    MIN_PHASE_MS = 300,
    EVENT_TIMEOUT_MS = 10000,
    I2C_TIMEOUT_US = 3000,
    MOTION_MAX_AGE_MS = 400,
};

typedef struct {
    float x;
    float y;
    float z;
} acceleration_t;

typedef enum {
    HUMP_WAITING,
    HUMP_CLIMBING,
    HUMP_DESCENDING,
} hump_phase_t;

static terrain_status_t status;
static terrain_status_t published_status;
static uint8_t samples[36];
static uint8_t fault_mask;

static struct __attribute__((packed)) {
    unsigned int sample_next : 3;
    unsigned int sample_count : 4;
    unsigned int phase : 2;
    unsigned int height_tracking : 1;
    unsigned int have_motion : 1;
    unsigned int have_height_time : 1;
    signed int previous_tilt_cdeg : 15;
    signed int previous_speed_twice : 11;
} state;

static struct __attribute__((packed)) {
    unsigned int next_sample_ms : 21;
    unsigned int event_start_ms : 21;
    unsigned int phase_start_ms : 21;
    unsigned int level_since_ms : 21;
    unsigned int previous_motion_ms : 21;
} times;

static struct __attribute__((packed)) {
    signed int level_pitch_cdeg : 15;
} calibration;

static struct __attribute__((packed)) {
    unsigned int climb_cdeg : 14;
    unsigned int descent_cdeg : 14;
} peaks;

static struct __attribute__((packed)) {
    signed int current_um : 24;
    unsigned int peak_um : 23;
} height;

static struct __attribute__((packed)) {
    unsigned int captured_ms : 21;
    signed int left_mm_s : 10;
    signed int right_mm_s : 10;
    unsigned int valid : 1;
} motion_sample;

_Static_assert(sizeof samples == 36u, "Packed filter size changed");
_Static_assert(sizeof state == 5u, "Packed terrain control size changed");
_Static_assert(sizeof times == 14u, "Packed terrain clock size changed");
_Static_assert(sizeof calibration == 2u, "Packed calibration size changed");
_Static_assert(sizeof peaks == 4u, "Packed peak size changed");
_Static_assert(sizeof height == 6u, "Packed height size changed");
_Static_assert(sizeof motion_sample == 6u, "Packed motion sample size changed");

void robot_fault_update(uint8_t mask, bool active)
{
    uint32_t saved = save_and_disable_interrupts();
    mask &= ROBOT_FAULT_MASK;
    fault_mask = active ? (uint8_t)(fault_mask | mask) :
                         (uint8_t)(fault_mask & (uint8_t)~mask);
    restore_interrupts(saved);
}

uint8_t robot_fault_get(void)
{
    uint32_t saved = save_and_disable_interrupts();
    uint8_t result = fault_mask;
    restore_interrupts(saved);
    return result;
}

static void publish_status(void)
{
    uint32_t saved = save_and_disable_interrupts();
    status.faults = fault_mask;
    published_status = status;
    restore_interrupts(saved);
}

bool terrain_set_motion(uint32_t captured_ms, int32_t left_mm_s,
                        int32_t right_mm_s, bool valid)
{
    bool accepted = captured_ms <= TERRAIN_SESSION_MAX_MS &&
        left_mm_s >= -TERRAIN_SPEED_MAX_MM_S &&
        left_mm_s <= TERRAIN_SPEED_MAX_MM_S &&
        right_mm_s >= -TERRAIN_SPEED_MAX_MM_S &&
        right_mm_s <= TERRAIN_SPEED_MAX_MM_S;
    uint32_t saved = save_and_disable_interrupts();
    if (valid && motion_sample.valid && captured_ms < motion_sample.captured_ms) {
        accepted = false;
    }
    motion_sample.valid = valid && accepted;
    motion_sample.captured_ms = accepted ? captured_ms : 0u;
    motion_sample.left_mm_s = accepted ? left_mm_s : 0;
    motion_sample.right_mm_s = accepted ? right_mm_s : 0;
    restore_interrupts(saved);
    return accepted;
}

static unsigned int angle_cdeg(float angle)
{
    float magnitude_deg = fabsf(angle);
    if (magnitude_deg > 90.0f) {
        magnitude_deg = 90.0f;
    }
    return (unsigned int)lroundf(magnitude_deg * 100.0f);
}

static acceleration_t filter_read(unsigned int index)
{
    acceleration_t result;
    float *axes[3] = {&result.x, &result.y, &result.z};
    for (unsigned int axis = 0u; axis < 3u; ++axis) {
        int32_t value = (int32_t)terrain_unpack_bits(samples,
            index * 36u + axis * 12u, 12u);
        if (value >= 2048) {
            value -= 4096;
        }
        *axes[axis] = (float)value;
    }
    return result;
}

static void filter_write(unsigned int index, const acceleration_t *reading)
{
    const float axes[3] = {reading->x, reading->y, reading->z};
    for (unsigned int axis = 0u; axis < 3u; ++axis) {
        terrain_pack_bits(samples, index * 36u + axis * 12u, 12u,
                          (uint32_t)(int32_t)axes[axis] & 0xfffu);
    }
}

static void reset_height(bool valid)
{
    height.current_um = 0;
    height.peak_um = 0u;
    state.height_tracking = valid;
    status.current_hump_mm = 0u;
    status.height_valid = valid;
}

void terrain_suspend(void)
{
    state.phase = HUMP_WAITING;
    state.sample_count = 0u;
    state.sample_next = 0u;
    state.have_motion = false;
    state.have_height_time = false;
    times.next_sample_ms = 0u;
    times.level_since_ms = 0u;
    status.sample_valid = false;
    reset_height(false);
    publish_status();
}

static bool write_register(uint8_t reg, uint8_t value)
{
    uint8_t message[2] = {reg, value};
    return i2c_write_timeout_us(i2c0, ACCEL_ADDRESS, message, 2, false, I2C_TIMEOUT_US) == 2;
}

static bool read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    if (i2c_write_timeout_us(i2c0, ACCEL_ADDRESS, &reg, 1, true, I2C_TIMEOUT_US) != 1) {
        return false;
    }
    return i2c_read_timeout_us(i2c0, ACCEL_ADDRESS, data, length, false, I2C_TIMEOUT_US) ==
           (int)length;
}

static bool read_acceleration(acceleration_t *reading)
{
    uint8_t data[6];
    if (!read_registers(0x28u | 0x80u, data, sizeof data)) {
        return false;
    }
    reading->x = (float)((int16_t)(((uint16_t)data[1] << 8) | data[0]) >> 4);
    reading->y = (float)((int16_t)(((uint16_t)data[3] << 8) | data[2]) >> 4);
    reading->z = (float)((int16_t)(((uint16_t)data[5] << 8) | data[4]) >> 4);
    return true;
}

static float pitch(const acceleration_t *reading)
{
    return atan2f(reading->y,
                  sqrtf(reading->x * reading->x +
                        reading->z * reading->z)) * 57.2957795f;
}

static float magnitude(const acceleration_t *reading)
{
    return sqrtf(reading->x * reading->x +
                 reading->y * reading->y +
                 reading->z * reading->z);
}

static bool average_stationary(uint count, acceleration_t *average)
{
    acceleration_t total = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < count; ++i) {
        acceleration_t reading;
        if (!read_acceleration(&reading)) {
            return false;
        }
        total.x += reading.x;
        total.y += reading.y;
        total.z += reading.z;
        sleep_ms(10);
    }
    average->x = total.x / (float)count;
    average->y = total.y / (float)count;
    average->z = total.z / (float)count;
    return true;
}

bool terrain_init(void)
{
    status.calibrated = false;
    terrain_suspend();
    i2c_init(i2c0, 100000u);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    uint8_t identity = 0;
    status.available = read_registers(0x0fu, &identity, 1u) &&
                       identity == 0x33u;
    if (status.available) {
        status.available = write_register(0x20u, 0x57u) &&
                           write_register(0x23u, 0x88u);
    }
    printf("Terrain sensor: %s\n", status.available ? "READY" : "NOT FOUND");
    robot_fault_update(ROBOT_FAULT_IMU, !status.available);
    publish_status();
    return status.available;
}

bool terrain_calibrate(void)
{
    if (!status.available) {
        return false;
    }
    terrain_suspend();
    status.calibrated = false;
    publish_status();
    printf("Terrain calibration: keep the car level and still for 2 seconds.\n");
    acceleration_t first;
    acceleration_t second;
    if (!average_stationary(CALIBRATION_SAMPLES / 2u, &first) ||
        !average_stationary(CALIBRATION_SAMPLES / 2u, &second)) {
        robot_fault_update(ROBOT_FAULT_IMU, true);
        publish_status();
        printf("Terrain calibration failed: sensor read error.\n");
        return false;
    }
    if (fabsf(pitch(&first) - pitch(&second)) > 1.5f ||
        fabsf(magnitude(&first) - magnitude(&second)) > 50.0f) {
        printf("Terrain calibration rejected: car moved.\n");
        return false;
    }
    acceleration_t level = {
        (first.x + second.x) * 0.5f,
        (first.y + second.y) * 0.5f,
        (first.z + second.z) * 0.5f,
    };
    if (magnitude(&level) < 700.0f || magnitude(&level) > 1500.0f) {
        printf("Terrain calibration rejected: unexpected gravity reading.\n");
        return false;
    }
    float level_pitch = pitch(&level);
    calibration.level_pitch_cdeg = (int)lroundf(level_pitch * 100.0f);
    memset(&status, 0, sizeof status);
    memset(&state, 0, sizeof state);
    memset(&times, 0, sizeof times);
    memset(&peaks, 0, sizeof peaks);
    status.available = true;
    status.calibrated = true;
    status.hump_count = 0;
    status.highest_hump_number = 0;
    status.highest_peak_angle_cdeg = 0u;
    state.sample_count = 0;
    state.sample_next = 0;
    times.next_sample_ms = 0;
    state.phase = HUMP_WAITING;
    times.level_since_ms = 0;
    reset_height(false);
    robot_fault_update(ROBOT_FAULT_IMU, false);
    publish_status();
    printf("Terrain calibrated. Level pitch=%.1f degrees.\n", level_pitch);
    return true;
}

static void finish_hump(uint32_t now_ms)
{
    if (status.hump_count >= TERRAIN_HUMP_MAX_COUNT) {
        status.calibrated = false;
        status.sample_valid = false;
        reset_height(false);
        return;
    }
    ++status.hump_count;
    if (peaks.climb_cdeg > status.highest_peak_angle_cdeg) {
        status.highest_peak_angle_cdeg = peaks.climb_cdeg;
        status.highest_hump_number = status.hump_count;
    }
    if (state.height_tracking && (!status.highest_height_valid ||
        (height.peak_um + 500u) / 1000u > status.highest_hump_mm)) {
        status.highest_hump_mm = (height.peak_um + 500u) / 1000u;
        status.highest_height_hump_number = status.hump_count;
        status.highest_height_valid = true;
    }
    status.current_hump_mm = 0u;
    printf("HUMP %lu COMPLETE: climb=+%.1f deg descent=%.1f deg "
           "duration=%llu ms; highest=hump %lu (+%.1f deg)\n",
           (unsigned long)status.hump_count, (double)peaks.climb_cdeg / 100.0,
           -(double)peaks.descent_cdeg / 100.0,
           (unsigned long long)(now_ms - times.event_start_ms),
           (unsigned long)status.highest_hump_number,
           (double)status.highest_peak_angle_cdeg / 100.0);
    if (state.height_tracking) {
        printf("HumpHeight=%umm HighestHump=%umm\n",
               (height.peak_um + 500u) / 1000u, status.highest_hump_mm);
    } else {
        printf("HumpHeight=UNAVAILABLE (valid wheel speeds required)\n");
    }
}

void terrain_poll(void)
{
    if (!status.calibrated) {
        return;
    }
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms > TERRAIN_SESSION_MAX_MS) {
        status.calibrated = false;
        status.sample_valid = false;
        reset_height(false);
        publish_status();
        printf("Terrain session limit reached; restart for a new 30-minute session.\n");
        return;
    }
    if (now_ms < times.next_sample_ms) {
        return;
    }
    times.next_sample_ms = now_ms + SAMPLE_INTERVAL_MS;

    acceleration_t reading;
    if (!read_acceleration(&reading)) {
        status.calibrated = false;
        robot_fault_update(ROBOT_FAULT_IMU, true);
        terrain_suspend();
        printf("Terrain sensor read failed; hump monitoring paused.\n");
        return;
    }
    robot_fault_update(ROBOT_FAULT_IMU, false);
    filter_write(state.sample_next, &reading);
    state.sample_next = (state.sample_next + 1u) % FILTER_SAMPLES;
    if (state.sample_count < FILTER_SAMPLES) {
        ++state.sample_count;
    }
    if (state.sample_count < FILTER_SAMPLES) {
        return;
    }

    acceleration_t average = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < FILTER_SAMPLES; ++i) {
        acceleration_t sample = filter_read(i);
        average.x += sample.x;
        average.y += sample.y;
        average.z += sample.z;
    }
    average.x /= (float)FILTER_SAMPLES;
    average.y /= (float)FILTER_SAMPLES;
    average.z /= (float)FILTER_SAMPLES;

    /* The mounted GY-511 gives negative pitch when the front is raised. */
    float tilt = -(pitch(&average) -
                   (float)calibration.level_pitch_cdeg / 100.0f);
    if (!isfinite(tilt) || tilt < -90.0f || tilt > 90.0f) {
        status.calibrated = false;
        robot_fault_update(ROBOT_FAULT_IMU, true);
        terrain_suspend();
        printf("Terrain orientation is outside the supported pitch range.\n");
        return;
    }
    const float threshold = 5.0f;
    const bool level = fabsf(tilt) < 2.0f;

    uint32_t saved = save_and_disable_interrupts();
    uint32_t motion_ms = motion_sample.captured_ms;
    int32_t left_mm_s = motion_sample.left_mm_s;
    int32_t right_mm_s = motion_sample.right_mm_s;
    bool motion_valid = motion_sample.valid;
    restore_interrupts(saved);
    motion_valid = motion_valid && motion_ms <= now_ms &&
                   now_ms - motion_ms <= MOTION_MAX_AGE_MS;
    int32_t speed_twice = left_mm_s + right_mm_s;
    bool turning = motion_valid && abs(left_mm_s - right_mm_s) > 30;
    bool impact = fabsf(magnitude(&reading) - 1000.0f) > 350.0f;
    bool height_input_valid = motion_valid && speed_twice >= 0 &&
                              !turning && !impact;
    status.motion = TERRAIN_MOTION_UNKNOWN;
    if (motion_valid) {
        status.motion = abs(speed_twice) < 10 ? TERRAIN_MOTION_STATIONARY :
                                               TERRAIN_MOTION_MOVING;
        if (state.have_motion && motion_ms > times.previous_motion_ms &&
            fabsf((float)(speed_twice - state.previous_speed_twice)) * 500.0f /
                (float)(motion_ms - times.previous_motion_ms) > 150.0f) {
            status.motion = TERRAIN_MOTION_ACCELERATING;
        }
        if (turning) {
            status.motion = TERRAIN_MOTION_TURNING;
        }
        state.previous_speed_twice = speed_twice;
        times.previous_motion_ms = motion_ms;
        state.have_motion = true;
    } else {
        state.have_motion = false;
    }
    if (tilt > threshold && !turning) {
        status.motion = TERRAIN_MOTION_CLIMBING;
    } else if (tilt < -threshold && !turning) {
        status.motion = TERRAIN_MOTION_DESCENDING;
    }
    if (impact) {
        status.motion = TERRAIN_MOTION_IMPACT;
        state.phase = HUMP_WAITING;
        state.have_height_time = false;
        status.sample_valid = true;
        status.captured_ms = now_ms;
        reset_height(false);
        publish_status();
        return;
    }
    if (state.phase != HUMP_WAITING) {
        uint32_t elapsed = now_ms - status.captured_ms;
        if (!height_input_valid || !state.have_height_time || elapsed > 200u ||
            now_ms - times.event_start_ms >= EVENT_TIMEOUT_MS) {
            state.height_tracking = false;
        }
        if (state.height_tracking) {
            const float radians_per_degree = 0.01745329252f;
            float previous_tilt = (float)state.previous_tilt_cdeg / 100.0f;
            float mean_sine = 0.5f * (sinf(previous_tilt * radians_per_degree) +
                                       sinf(tilt * radians_per_degree));
            int32_t delta_um = (int32_t)lroundf(
                (float)speed_twice * 0.5f * (float)elapsed * mean_sine);
            int32_t next_um = height.current_um + delta_um;
            if (next_um < -5000000 || next_um > 5000000) {
                state.height_tracking = false;
            } else {
                height.current_um = next_um;
                if (next_um > (int32_t)height.peak_um) {
                    height.peak_um = (unsigned int)next_um;
                }
            }
        }
        status.height_valid = state.height_tracking;
        status.current_hump_mm = state.height_tracking && height.current_um > 0 ?
            ((unsigned int)height.current_um + 500u) / 1000u : 0u;
    }
    status.sample_valid = true;

    if (state.phase == HUMP_WAITING) {
        if (tilt > threshold) {
            state.phase = HUMP_CLIMBING;
            reset_height(height_input_valid && state.have_height_time);
            times.event_start_ms = now_ms;
            times.phase_start_ms = now_ms;
            peaks.climb_cdeg = angle_cdeg(tilt);
            peaks.descent_cdeg = 0u;
            times.level_since_ms = 0;
            printf("HUMP: climbing started.\n");
        }
    } else if (state.phase == HUMP_CLIMBING) {
        if (tilt > (float)peaks.climb_cdeg / 100.0f) {
            peaks.climb_cdeg = angle_cdeg(tilt);
        }
        if (tilt < -threshold && now_ms - times.phase_start_ms >= MIN_PHASE_MS) {
            state.phase = HUMP_DESCENDING;
            times.phase_start_ms = now_ms;
            peaks.descent_cdeg = angle_cdeg(tilt);
            times.level_since_ms = 0;
            printf("HUMP: descending started.\n");
        } else if (level) {
            if (times.level_since_ms == 0) {
                times.level_since_ms = now_ms;
            } else if (now_ms - times.level_since_ms >= 800u) {
                state.phase = HUMP_WAITING;
                reset_height(false);
                printf("HUMP cancelled: no descent detected.\n");
            }
        } else {
            times.level_since_ms = 0;
        }
        if (state.phase == HUMP_CLIMBING &&
            now_ms - times.event_start_ms >= EVENT_TIMEOUT_MS) {
            state.phase = HUMP_WAITING;
            reset_height(false);
            printf("HUMP cancelled: event timed out.\n");
        }
    } else {
        if (tilt < -(float)peaks.descent_cdeg / 100.0f) {
            peaks.descent_cdeg = angle_cdeg(tilt);
        }
        if (tilt > threshold && now_ms - times.phase_start_ms >= MIN_PHASE_MS) {
            finish_hump(now_ms);
            state.phase = HUMP_CLIMBING;
            reset_height(height_input_valid && state.have_height_time);
            times.event_start_ms = now_ms;
            times.phase_start_ms = now_ms;
            peaks.climb_cdeg = angle_cdeg(tilt);
            peaks.descent_cdeg = 0u;
            times.level_since_ms = 0;
            printf("HUMP: next climb started.\n");
        } else if (level) {
            if (times.level_since_ms == 0) {
                times.level_since_ms = now_ms;
            } else if (now_ms - times.level_since_ms >= 480u) {
                finish_hump(now_ms);
                state.phase = HUMP_WAITING;
                times.level_since_ms = 0;
            }
        } else {
            times.level_since_ms = 0;
        }
        if (state.phase == HUMP_DESCENDING &&
            now_ms - times.event_start_ms >= EVENT_TIMEOUT_MS) {
            state.phase = HUMP_WAITING;
            reset_height(false);
            printf("HUMP cancelled: event timed out.\n");
        }
    }
    state.previous_tilt_cdeg = (int)lroundf(tilt * 100.0f);
    state.have_height_time = true;
    status.captured_ms = now_ms;
    publish_status();
}

terrain_status_t terrain_get_status(void)
{
    uint32_t saved = save_and_disable_interrupts();
    terrain_status_t result = published_status;
    result.faults = fault_mask;
    restore_interrupts(saved);
    return result;
}
