#include "terrain.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "hardware/i2c.h"
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
static acceleration_t samples[FILTER_SAMPLES];
static uint sample_next;
static uint sample_count;
static float level_pitch;
static uint64_t next_sample_ms;
static hump_phase_t phase;
static uint64_t event_start_ms;
static uint64_t phase_start_ms;
static uint64_t level_since_ms;
static float peak_climb;
static float peak_descent;

static bool write_register(uint8_t reg, uint8_t value)
{
    uint8_t message[2] = {reg, value};
    return i2c_write_blocking(i2c0, ACCEL_ADDRESS, message, 2, false) == 2;
}

static bool read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    if (i2c_write_blocking(i2c0, ACCEL_ADDRESS, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(i2c0, ACCEL_ADDRESS, data, length, false) ==
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
                           write_register(0x23u, 0x08u);
    }
    printf("Terrain sensor: %s\n", status.available ? "READY" : "NOT FOUND");
    return status.available;
}

bool terrain_calibrate(void)
{
    if (!status.available) {
        return false;
    }
    status.calibrated = false;
    printf("Terrain calibration: keep the car level and still for 2 seconds.\n");
    acceleration_t first;
    acceleration_t second;
    if (!average_stationary(CALIBRATION_SAMPLES / 2u, &first) ||
        !average_stationary(CALIBRATION_SAMPLES / 2u, &second)) {
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
    level_pitch = pitch(&level);
    status.calibrated = true;
    status.hump_count = 0;
    status.highest_hump_number = 0;
    status.highest_peak_angle_deg = 0.0f;
    sample_count = 0;
    sample_next = 0;
    next_sample_ms = 0;
    phase = HUMP_WAITING;
    level_since_ms = 0;
    printf("Terrain calibrated. Level pitch=%.1f degrees.\n", level_pitch);
    return true;
}

static void finish_hump(uint64_t now_ms)
{
    ++status.hump_count;
    if (peak_climb > status.highest_peak_angle_deg) {
        status.highest_peak_angle_deg = peak_climb;
        status.highest_hump_number = status.hump_count;
    }
    printf("HUMP %lu COMPLETE: climb=+%.1f deg descent=%.1f deg "
           "duration=%llu ms; highest=hump %lu (+%.1f deg)\n",
           (unsigned long)status.hump_count, peak_climb, peak_descent,
           (unsigned long long)(now_ms - event_start_ms),
           (unsigned long)status.highest_hump_number,
           status.highest_peak_angle_deg);
}

void terrain_poll(void)
{
    if (!status.calibrated) {
        return;
    }
    uint64_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms < next_sample_ms) {
        return;
    }
    next_sample_ms = now_ms + SAMPLE_INTERVAL_MS;

    acceleration_t reading;
    if (!read_acceleration(&reading)) {
        status.calibrated = false;
        printf("Terrain sensor read failed; hump monitoring paused.\n");
        return;
    }
    samples[sample_next] = reading;
    sample_next = (sample_next + 1u) % FILTER_SAMPLES;
    if (sample_count < FILTER_SAMPLES) {
        ++sample_count;
    }
    if (sample_count < FILTER_SAMPLES) {
        return;
    }

    acceleration_t average = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < FILTER_SAMPLES; ++i) {
        average.x += samples[i].x;
        average.y += samples[i].y;
        average.z += samples[i].z;
    }
    average.x /= (float)FILTER_SAMPLES;
    average.y /= (float)FILTER_SAMPLES;
    average.z /= (float)FILTER_SAMPLES;

    /* The mounted GY-511 gives negative pitch when the front is raised. */
    float tilt = -(pitch(&average) - level_pitch);
    const float threshold = 5.0f;
    const bool level = fabsf(tilt) < 2.0f;

    if (phase == HUMP_WAITING) {
        if (tilt > threshold) {
            phase = HUMP_CLIMBING;
            event_start_ms = now_ms;
            phase_start_ms = now_ms;
            peak_climb = tilt;
            peak_descent = 0.0f;
            level_since_ms = 0;
            printf("HUMP: climbing started.\n");
        }
    } else if (phase == HUMP_CLIMBING) {
        if (tilt > peak_climb) {
            peak_climb = tilt;
        }
        if (tilt < -threshold && now_ms - phase_start_ms >= MIN_PHASE_MS) {
            phase = HUMP_DESCENDING;
            phase_start_ms = now_ms;
            peak_descent = tilt;
            level_since_ms = 0;
            printf("HUMP: descending started.\n");
        } else if (level) {
            if (level_since_ms == 0) {
                level_since_ms = now_ms;
            } else if (now_ms - level_since_ms >= 800u) {
                phase = HUMP_WAITING;
                printf("HUMP cancelled: no descent detected.\n");
            }
        } else {
            level_since_ms = 0;
        }
        if (phase == HUMP_CLIMBING &&
            now_ms - event_start_ms > EVENT_TIMEOUT_MS) {
            phase = HUMP_WAITING;
            printf("HUMP cancelled: event timed out.\n");
        }
    } else {
        if (tilt < peak_descent) {
            peak_descent = tilt;
        }
        if (tilt > threshold && now_ms - phase_start_ms >= MIN_PHASE_MS) {
            finish_hump(now_ms);
            phase = HUMP_CLIMBING;
            event_start_ms = now_ms;
            phase_start_ms = now_ms;
            peak_climb = tilt;
            peak_descent = 0.0f;
            level_since_ms = 0;
            printf("HUMP: next climb started.\n");
        } else if (level) {
            if (level_since_ms == 0) {
                level_since_ms = now_ms;
            } else if (now_ms - level_since_ms >= 480u) {
                finish_hump(now_ms);
                phase = HUMP_WAITING;
                level_since_ms = 0;
            }
        } else {
            level_since_ms = 0;
        }
        if (phase == HUMP_DESCENDING &&
            now_ms - event_start_ms > EVENT_TIMEOUT_MS) {
            phase = HUMP_WAITING;
            printf("HUMP cancelled: event timed out.\n");
        }
    }
}

terrain_status_t terrain_get_status(void)
{
    return status;
}
