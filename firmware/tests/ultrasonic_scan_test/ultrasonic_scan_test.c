/*
 * INF2004 Buddy 5 bench test: ultrasonic obstacle scanning.
 *
 * Wiring:
 *   HC-SR04+ TRIG -> GP0 (Grove 1)
 *   HC-SR04+ ECHO -> GP1 (Grove 1)
 *   SG90 signal   -> GP12 (Servo 1)
 *   GP20          -> begin scan
 *   GP21          -> centre servo
 *
 * The motor GPIOs are deliberately not configured, so the car cannot drive.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

enum {
    ULTRASONIC_TRIG_PIN = 0,
    ULTRASONIC_ECHO_PIN = 1,
    SERVO_PIN = 12,
    SCAN_BUTTON_PIN = 20,
    CENTRE_BUTTON_PIN = 21,
};

static uint servo_slice;
static uint servo_channel;

static void servo_set_angle(uint angle_degrees)
{
    if (angle_degrees > 180u) {
        angle_degrees = 180u;
    }

    /* Conservative SG90 range: 500 us at 0 degrees to 2500 us at 180. */
    uint16_t pulse_us = (uint16_t)(500u + (angle_degrees * 2000u) / 180u);
    pwm_set_chan_level(servo_slice, servo_channel, pulse_us);
}

static void servo_init(void)
{
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    servo_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    servo_channel = pwm_gpio_to_channel(SERVO_PIN);

    float divider = (float)clock_get_hz(clk_sys) / 1000000.0f;
    pwm_set_clkdiv(servo_slice, divider);
    pwm_set_wrap(servo_slice, 19999u);
    pwm_set_chan_level(servo_slice, servo_channel, 1500u);
    pwm_set_enabled(servo_slice, true);
}

static void ultrasonic_init(void)
{
    gpio_init(ULTRASONIC_TRIG_PIN);
    gpio_set_dir(ULTRASONIC_TRIG_PIN, GPIO_OUT);
    gpio_put(ULTRASONIC_TRIG_PIN, 0);

    gpio_init(ULTRASONIC_ECHO_PIN);
    gpio_set_dir(ULTRASONIC_ECHO_PIN, GPIO_IN);
}

static bool ultrasonic_read_once(float *distance_cm)
{
    gpio_put(ULTRASONIC_TRIG_PIN, 0);
    sleep_us(2);
    gpio_put(ULTRASONIC_TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(ULTRASONIC_TRIG_PIN, 0);

    uint64_t deadline = time_us_64() + 30000u;
    while (!gpio_get(ULTRASONIC_ECHO_PIN)) {
        if (time_us_64() >= deadline) {
            return false;
        }
        tight_loop_contents();
    }

    uint64_t pulse_start = time_us_64();
    deadline = pulse_start + 30000u;
    while (gpio_get(ULTRASONIC_ECHO_PIN)) {
        if (time_us_64() >= deadline) {
            return false;
        }
        tight_loop_contents();
    }

    *distance_cm = (float)(time_us_64() - pulse_start) * 0.0343f / 2.0f;
    return *distance_cm >= 2.0f && *distance_cm <= 400.0f;
}

static bool ultrasonic_read_filtered(float *distance_cm)
{
    float total = 0.0f;
    uint valid_count = 0;

    for (uint sample = 0; sample < 3u; ++sample) {
        float reading = 0.0f;
        if (ultrasonic_read_once(&reading)) {
            total += reading;
            ++valid_count;
        }
        sleep_ms(60);
    }

    if (valid_count == 0u) {
        return false;
    }

    *distance_cm = total / (float)valid_count;
    return true;
}

static bool button_pressed(uint pin)
{
    if (gpio_get(pin)) {
        return false;
    }

    sleep_ms(25);
    if (gpio_get(pin)) {
        return false;
    }

    while (!gpio_get(pin)) {
        sleep_ms(5);
    }
    return true;
}

static bool measure_at_angle(uint angle, float *distance_cm)
{
    servo_set_angle(angle);
    sleep_ms(500);

    bool valid = ultrasonic_read_filtered(distance_cm);
    if (valid) {
        printf("Angle=%3u degrees  Distance=%6.1f cm\n", angle, *distance_cm);
    } else {
        printf("Angle=%3u degrees  Distance=no echo\n", angle);
    }
    return valid;
}

static void perform_scan(void)
{
    /*
     * Measured mechanical limits are 45..130 degrees.  Stay 5 degrees
     * inside those limits so the sensor cannot strike the Robo Pico.
     */
    static const uint coarse_angles[] = {50u, 70u, 90u, 110u, 125u};
    float nearest_distance = 401.0f;
    uint nearest_angle = 90u;
    bool obstacle_found = false;

    printf("\n=== COARSE SCAN ===\n");
    for (uint i = 0; i < sizeof coarse_angles / sizeof coarse_angles[0]; ++i) {
        float distance = 0.0f;
        if (measure_at_angle(coarse_angles[i], &distance) &&
            distance < nearest_distance) {
            nearest_distance = distance;
            nearest_angle = coarse_angles[i];
            obstacle_found = true;
        }
    }

    if (!obstacle_found) {
        printf("Result: no obstacle returned a valid echo.\n");
        servo_set_angle(90u);
        return;
    }

    printf("Nearest coarse result: %u degrees, %.1f cm\n",
           nearest_angle, nearest_distance);

    /*
     * The available mechanical range is small enough to rescan all of it at
     * 5-degree resolution.  This prevents a wide obstacle edge from falling
     * outside a narrower fine-scan window.
     */
    uint fine_start = 50u;
    uint fine_end = 125u;

    printf("\n=== FINE SCAN ===\n");
    nearest_distance = 401.0f;
    bool fine_obstacle_found = false;
    uint fine_angles[16] = {0};
    float fine_distances[16] = {0.0f};
    bool fine_valid[16] = {false};
    uint fine_count = 0u;
    uint nearest_index = 0u;

    for (uint angle = fine_start; angle <= fine_end; angle += 5u) {
        float distance = 0.0f;
        bool valid = measure_at_angle(angle, &distance);

        fine_angles[fine_count] = angle;
        fine_distances[fine_count] = distance;
        fine_valid[fine_count] = valid;

        if (valid && distance < nearest_distance) {
            nearest_distance = distance;
            nearest_angle = angle;
            nearest_index = fine_count;
            fine_obstacle_found = true;
        }
        ++fine_count;
    }

    if (fine_obstacle_found) {
        printf("Nearest fine result: %u degrees, %.1f cm\n",
               nearest_angle, nearest_distance);

        /*
         * Treat neighbouring readings within 15 cm of the closest point as
         * belonging to the same obstacle.  A large distance jump or no echo
         * marks an edge.
         */
        float obstacle_limit = nearest_distance + 15.0f;
        uint first = nearest_index;
        uint last = nearest_index;

        while (first > 0u && fine_valid[first - 1u] &&
               fine_distances[first - 1u] <= obstacle_limit) {
            --first;
        }
        while (last + 1u < fine_count && fine_valid[last + 1u] &&
               fine_distances[last + 1u] <= obstacle_limit) {
            ++last;
        }

        uint angular_width = fine_angles[last] - fine_angles[first];
        const float degrees_to_radians = 0.01745329252f;
        float estimated_width = 2.0f * nearest_distance *
            tanf(0.5f * (float)angular_width * degrees_to_radians);

        printf("\n=== OBSTACLE PROFILE ===\n");
        printf("Detected angular region: %u to %u degrees\n",
               fine_angles[first], fine_angles[last]);
        printf("Angular width: %u degrees\n", angular_width);
        printf("Uncorrected scan footprint: %.1f cm (NOT object width)\n",
               estimated_width);
        printf("Object width requires calibration for the ultrasonic beam.\n");

        if (first > 0u) {
            if (fine_valid[first - 1u]) {
                printf("Low-angle-side clearance reading: %.1f cm at %u degrees\n",
                       fine_distances[first - 1u], fine_angles[first - 1u]);
            } else {
                printf("Low-angle-side clearance: no echo at %u degrees\n",
                       fine_angles[first - 1u]);
            }
        } else {
            printf("Low-angle obstacle edge is outside this fine scan.\n");
        }

        if (last + 1u < fine_count) {
            if (fine_valid[last + 1u]) {
                printf("High-angle-side clearance reading: %.1f cm at %u degrees\n",
                       fine_distances[last + 1u], fine_angles[last + 1u]);
            } else {
                printf("High-angle-side clearance: no echo at %u degrees\n",
                       fine_angles[last + 1u]);
            }
        } else {
            printf("High-angle obstacle edge is outside this fine scan.\n");
        }
    } else {
        printf("Fine scan result: no valid echo.\n");
    }
    printf("=== SCAN COMPLETE ===\n");
    servo_set_angle(90u);
}

int main(void)
{
    stdio_init_all();
    servo_init();
    ultrasonic_init();

    gpio_init(SCAN_BUTTON_PIN);
    gpio_set_dir(SCAN_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(SCAN_BUTTON_PIN);

    gpio_init(CENTRE_BUTTON_PIN);
    gpio_set_dir(CENTRE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(CENTRE_BUTTON_PIN);

    servo_set_angle(90u);
    sleep_ms(2500);
    printf("\nINF2004 ultrasonic scan test\n");
    printf("Motors are disabled. GP20=scan, GP21=centre.\n");

    bool previous_scan_button = true;
    bool previous_centre_button = true;

    while (true) {
        bool scan_button = gpio_get(SCAN_BUTTON_PIN);
        bool centre_button = gpio_get(CENTRE_BUTTON_PIN);

        if (previous_scan_button && !scan_button &&
            button_pressed(SCAN_BUTTON_PIN)) {
            perform_scan();
        }

        if (previous_centre_button && !centre_button &&
            button_pressed(CENTRE_BUTTON_PIN)) {
            servo_set_angle(90u);
            printf("Servo centred at 90 degrees.\n");
        }

        previous_scan_button = gpio_get(SCAN_BUTTON_PIN);
        previous_centre_button = gpio_get(CENTRE_BUTTON_PIN);
        sleep_ms(10);
    }
}
