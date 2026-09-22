/*
 * INF2004 GY-511 calibration and basic terrain test.
 *
 * Wiring:
 *   GY-511 / LSM303DLHC: Grove 3, SDA GP4, SCL GP5
 *   Start / automatic level calibration button: GP20
 *
 * This test deliberately keeps both motors and the servo signal off.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

enum {
    I2C_SDA_PIN = 4,
    I2C_SCL_PIN = 5,
    MOTOR_1_A_PIN = 8,
    MOTOR_1_B_PIN = 9,
    MOTOR_2_A_PIN = 10,
    MOTOR_2_B_PIN = 11,
    SERVO_SIGNAL_PIN = 12,
    START_BUTTON_PIN = 20,
    LSM303_ACCEL_ADDRESS = 0x19,
    CALIBRATION_SAMPLES = 200,
    LIVE_AVERAGE_SAMPLES = 8,
    MIN_PHASE_TIME_MS = 300,
    EVENT_TIMEOUT_MS = 10000,
};

typedef struct {
    float x;
    float y;
    float z;
} acceleration_t;

typedef enum {
    HUMP_UNARMED,
    HUMP_WAITING,
    HUMP_CLIMBING,
    HUMP_DESCENDING,
} hump_state_t;

static bool i2c_write_register(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t message[2] = {reg, value};
    return i2c_write_blocking(i2c0, address, message, 2, false) == 2;
}

static bool i2c_read_registers(uint8_t address, uint8_t reg,
                               uint8_t *data, size_t length)
{
    if (i2c_write_blocking(i2c0, address, &reg, 1, true) != 1) {
        return false;
    }

    return i2c_read_blocking(i2c0, address, data, length, false) ==
           (int)length;
}

static bool accelerometer_init(void)
{
    i2c_init(i2c0, 100000u);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    uint8_t identity = 0;
    if (!i2c_read_registers(LSM303_ACCEL_ADDRESS, 0x0fu, &identity, 1u)) {
        printf("ERROR: accelerometer did not answer on I2C.\n");
        return false;
    }

    printf("Accelerometer WHO_AM_I=0x%02x (expected 0x33)\n", identity);
    if (identity != 0x33u) {
        return false;
    }

    /* 100 Hz, X/Y/Z enabled, high-resolution mode, +/-2 g. */
    bool ok = i2c_write_register(LSM303_ACCEL_ADDRESS, 0x20u, 0x57u);
    ok = i2c_write_register(LSM303_ACCEL_ADDRESS, 0x23u, 0x08u) && ok;
    return ok;
}

static bool acceleration_read_raw(acceleration_t *reading)
{
    uint8_t data[6];
    if (!i2c_read_registers(LSM303_ACCEL_ADDRESS, 0x28u | 0x80u,
                            data, sizeof data)) {
        return false;
    }

    int16_t x = (int16_t)(((uint16_t)data[1] << 8) | data[0]) >> 4;
    int16_t y = (int16_t)(((uint16_t)data[3] << 8) | data[2]) >> 4;
    int16_t z = (int16_t)(((uint16_t)data[5] << 8) | data[4]) >> 4;

    reading->x = (float)x;
    reading->y = (float)y;
    reading->z = (float)z;
    return true;
}

static bool acceleration_average(acceleration_t *average, uint sample_count)
{
    acceleration_t total = {0.0f, 0.0f, 0.0f};

    for (uint sample = 0; sample < sample_count; ++sample) {
        acceleration_t reading;
        if (!acceleration_read_raw(&reading)) {
            return false;
        }

        total.x += reading.x;
        total.y += reading.y;
        total.z += reading.z;
        sleep_ms(10);
    }

    average->x = total.x / (float)sample_count;
    average->y = total.y / (float)sample_count;
    average->z = total.z / (float)sample_count;
    return true;
}

static float pitch_degrees(const acceleration_t *reading);
static float vector_magnitude(const acceleration_t *reading);

static bool automatic_level_calibration(acceleration_t *level)
{
    acceleration_t first_half;
    acceleration_t second_half;

    if (!acceleration_average(&first_half, CALIBRATION_SAMPLES / 2u) ||
        !acceleration_average(&second_half, CALIBRATION_SAMPLES / 2u)) {
        return false;
    }

    float pitch_difference =
        fabsf(pitch_degrees(&first_half) - pitch_degrees(&second_half));
    float magnitude_difference =
        fabsf(vector_magnitude(&first_half) -
              vector_magnitude(&second_half));

    if (pitch_difference > 1.5f || magnitude_difference > 50.0f) {
        printf("CALIBRATION REJECTED: car moved during the two-second sample.\n");
        printf("Pitch changed %.1f deg; acceleration magnitude changed %.1f.\n",
               pitch_difference, magnitude_difference);
        return false;
    }

    level->x = (first_half.x + second_half.x) * 0.5f;
    level->y = (first_half.y + second_half.y) * 0.5f;
    level->z = (first_half.z + second_half.z) * 0.5f;

    float magnitude = vector_magnitude(level);
    if (magnitude < 700.0f || magnitude > 1500.0f) {
        printf("CALIBRATION REJECTED: unexpected gravity magnitude %.1f.\n",
               magnitude);
        return false;
    }

    return true;
}

static float pitch_degrees(const acceleration_t *reading)
{
    const float radians_to_degrees = 57.2957795f;
    return atan2f(reading->y,
                  sqrtf((reading->x * reading->x) +
                        (reading->z * reading->z))) * radians_to_degrees;
}

static float vector_magnitude(const acceleration_t *reading)
{
    return sqrtf((reading->x * reading->x) +
                 (reading->y * reading->y) +
                 (reading->z * reading->z));
}

static bool button_pressed(uint pin, bool *previous_state)
{
    bool current_state = gpio_get(pin);
    bool pressed = *previous_state && !current_state;
    *previous_state = current_state;

    if (pressed) {
        sleep_ms(30);
    }
    return pressed;
}

static void outputs_safe_init(void)
{
    const uint output_pins[] = {
        MOTOR_1_A_PIN,
        MOTOR_1_B_PIN,
        MOTOR_2_A_PIN,
        MOTOR_2_B_PIN,
        SERVO_SIGNAL_PIN,
    };

    for (uint index = 0; index < sizeof output_pins / sizeof output_pins[0];
         ++index) {
        gpio_init(output_pins[index]);
        gpio_set_dir(output_pins[index], GPIO_OUT);
        gpio_put(output_pins[index], 0);
    }
}

static void report_completed_hump(uint32_t *hump_count,
                                  float peak_climb_degrees,
                                  float peak_descent_degrees,
                                  uint64_t duration_ms,
                                  float *highest_climb_degrees,
                                  uint32_t *highest_hump_number)
{
    ++(*hump_count);
    if (peak_climb_degrees > *highest_climb_degrees) {
        *highest_climb_degrees = peak_climb_degrees;
        *highest_hump_number = *hump_count;
    }

    printf("\n=== HUMP %lu COMPLETE ===\n", (unsigned long)*hump_count);
    printf("Peak climb angle:   +%.1f degrees\n", peak_climb_degrees);
    printf("Peak descent angle:  %.1f degrees\n", peak_descent_degrees);
    printf("Event duration:      %llu ms\n",
           (unsigned long long)duration_ms);
    printf("Highest climb so far: hump %lu at +%.1f degrees\n",
           (unsigned long)*highest_hump_number, *highest_climb_degrees);
    printf("========================\n\n");
}

int main(void)
{
    stdio_init_all();
    outputs_safe_init();

    gpio_init(START_BUTTON_PIN);
    gpio_set_dir(START_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_BUTTON_PIN);

    sleep_ms(2500);
    printf("\n=== INF2004 IMU / HUMP BENCH TEST ===\n");
    printf("Motors and servo are disabled.\n");
    printf("1. Put the car flat and completely still.\n");
    printf("2. Press GP20 once to calibrate and start.\n");
    printf("3. Do not touch the car during the two-second calibration.\n\n");

    if (!accelerometer_init()) {
        printf("STOP: GY-511 accelerometer was not initialised.\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    acceleration_t level = {0.0f, 0.0f, 0.0f};
    float level_pitch = 0.0f;
    float level_magnitude = 0.0f;
    /* Established from this car's fixed GY-511 mounting orientation. */
    const float raised_sign = -1.0f;
    float tilt_threshold = 5.0f;
    bool level_ready = false;
    bool previous_start_button = true;
    hump_state_t hump_state = HUMP_UNARMED;
    uint32_t hump_count = 0u;
    uint32_t level_windows = 0u;
    uint64_t hump_start_ms = 0u;
    uint64_t phase_start_ms = 0u;
    float peak_climb_degrees = 0.0f;
    float peak_descent_degrees = 0.0f;
    float highest_climb_degrees = 0.0f;
    uint32_t highest_hump_number = 0u;

    while (true) {
        if (button_pressed(START_BUTTON_PIN, &previous_start_button)) {
            printf("AUTO CALIBRATION: keep the car level and still for 2 seconds.\n");
            if (automatic_level_calibration(&level)) {
                level_pitch = pitch_degrees(&level);
                level_magnitude = vector_magnitude(&level);
                level_ready = true;
                hump_state = HUMP_WAITING;
                hump_count = 0u;
                level_windows = 0u;
                highest_climb_degrees = 0.0f;
                highest_hump_number = 0u;
                printf("LEVEL saved: X=%.1f Y=%.1f Z=%.1f, pitch=%.1f deg\n",
                       level.x, level.y, level.z, level_pitch);
                printf("Hump threshold: %.1f degrees\n", tilt_threshold);
                printf("READY: autonomous hump detector armed.\n");
            } else {
                level_ready = false;
                printf("Place the car flat and press GP20 to try again.\n");
            }
        }

        acceleration_t live;
        if (!acceleration_average(&live, LIVE_AVERAGE_SAMPLES)) {
            printf("ERROR: accelerometer read failed.\n");
            sleep_ms(200);
            continue;
        }

        float pitch = pitch_degrees(&live);
        float magnitude = vector_magnitude(&live);

        if (!level_ready) {
            printf("Raw X=%7.1f Y=%7.1f Z=%7.1f | PLACE LEVEL, PRESS GP20\n",
                   live.x, live.y, live.z);
        } else {
            float signed_tilt = (pitch - level_pitch) * raised_sign;
            float magnitude_change = fabsf(magnitude - level_magnitude);
            const char *state = "LEVEL";

            if (magnitude_change > 300.0f) {
                state = "SUDDEN MOTION / IMPACT";
            } else if (signed_tilt > tilt_threshold) {
                state = "FRONT RAISED / CLIMBING";
            } else if (signed_tilt < -tilt_threshold) {
                state = "FRONT LOWERED / DESCENDING";
            }

            printf("Pitch delta=%+6.1f deg | Accel change=%6.1f | %s\n",
                   signed_tilt, magnitude_change, state);

            uint64_t now_ms = to_ms_since_boot(get_absolute_time());

            switch (hump_state) {
                case HUMP_UNARMED:
                    if (fabsf(signed_tilt) < 2.0f) {
                        ++level_windows;
                    } else {
                        level_windows = 0u;
                    }

                    if (level_windows >= 4u) {
                        hump_state = HUMP_WAITING;
                        level_windows = 0u;
                        printf("HUMP DETECTOR ARMED: car is level.\n");
                    }
                    break;

                case HUMP_WAITING:
                    if (signed_tilt > tilt_threshold) {
                        hump_state = HUMP_CLIMBING;
                        hump_start_ms = now_ms;
                        phase_start_ms = now_ms;
                        peak_climb_degrees = signed_tilt;
                        peak_descent_degrees = 0.0f;
                        level_windows = 0u;
                        printf("HUMP EVENT: climbing started.\n");
                    }
                    break;

                case HUMP_CLIMBING:
                    if (signed_tilt > peak_climb_degrees) {
                        peak_climb_degrees = signed_tilt;
                    }

                    if (signed_tilt < -tilt_threshold &&
                        now_ms - phase_start_ms >= MIN_PHASE_TIME_MS) {
                        hump_state = HUMP_DESCENDING;
                        phase_start_ms = now_ms;
                        peak_descent_degrees = signed_tilt;
                        level_windows = 0u;
                        printf("HUMP EVENT: crest passed; descending started.\n");
                    } else if (fabsf(signed_tilt) < 2.0f) {
                        ++level_windows;
                        if (level_windows >= 8u) {
                            printf("HUMP EVENT CANCELLED: climb had no descent.\n");
                            hump_state = HUMP_WAITING;
                            level_windows = 0u;
                        }
                    } else {
                        level_windows = 0u;
                    }

                    if (hump_state == HUMP_CLIMBING &&
                        now_ms - hump_start_ms > EVENT_TIMEOUT_MS) {
                        printf("HUMP EVENT CANCELLED: sequence timed out.\n");
                        hump_state = HUMP_UNARMED;
                        level_windows = 0u;
                    }
                    break;

                case HUMP_DESCENDING:
                    if (signed_tilt < peak_descent_degrees) {
                        peak_descent_degrees = signed_tilt;
                    }

                    if (signed_tilt > tilt_threshold &&
                        now_ms - phase_start_ms >= MIN_PHASE_TIME_MS) {
                        report_completed_hump(&hump_count,
                                              peak_climb_degrees,
                                              peak_descent_degrees,
                                              now_ms - hump_start_ms,
                                              &highest_climb_degrees,
                                              &highest_hump_number);

                        /* The new climb starts the next back-to-back hump. */
                        hump_state = HUMP_CLIMBING;
                        hump_start_ms = now_ms;
                        phase_start_ms = now_ms;
                        peak_climb_degrees = signed_tilt;
                        peak_descent_degrees = 0.0f;
                        level_windows = 0u;
                        printf("HUMP EVENT: next back-to-back climb started.\n");
                    } else if (fabsf(signed_tilt) < 2.0f) {
                        ++level_windows;
                    } else {
                        level_windows = 0u;
                    }

                    if (hump_state == HUMP_DESCENDING &&
                        level_windows >= 4u) {
                        report_completed_hump(&hump_count,
                                              peak_climb_degrees,
                                              peak_descent_degrees,
                                              now_ms - hump_start_ms,
                                              &highest_climb_degrees,
                                              &highest_hump_number);
                        hump_state = HUMP_WAITING;
                        level_windows = 0u;
                    } else if (hump_state == HUMP_DESCENDING &&
                               now_ms - hump_start_ms > EVENT_TIMEOUT_MS) {
                        printf("HUMP EVENT CANCELLED: sequence timed out.\n");
                        hump_state = HUMP_UNARMED;
                        level_windows = 0u;
                    }
                    break;

                default:
                    hump_state = HUMP_WAITING;
                    break;
            }
        }

        sleep_ms(120);
    }
}
