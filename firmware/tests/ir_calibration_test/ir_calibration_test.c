/*
 * INF2004 three-sensor IR calibration test.
 *
 * Wiring:
 *   Grove 5 yellow signal / sensor AO -> GP26
 *   Grove 6 yellow signal / sensor AO -> GP27
 *   Grove 7 yellow signal / sensor AO -> GP28
 *   Sensor VCC -> red, GND -> black; white and DO remain disconnected.
 *
 * GP20 captures the current LIGHT-surface values.
 * GP21 captures the current DARK-surface values.
 * Motors and servo are not configured.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"

enum {
    SENSOR_COUNT = 3,
    SENSOR_GROVE_5_PIN = 26,
    SENSOR_GROVE_6_PIN = 27,
    SENSOR_GROVE_7_PIN = 28,
    LIGHT_CAPTURE_BUTTON_PIN = 20,
    DARK_CAPTURE_BUTTON_PIN = 21,
};

static const uint sensor_pins[SENSOR_COUNT] = {
    SENSOR_GROVE_5_PIN,
    SENSOR_GROVE_6_PIN,
    SENSOR_GROVE_7_PIN,
};

static uint16_t light_values[SENSOR_COUNT];
static uint16_t dark_values[SENSOR_COUNT];
static bool light_captured;
static bool dark_captured;

static uint16_t read_sensor_once(uint gpio_pin)
{
    adc_select_input(gpio_pin - 26u);
    sleep_us(5);
    return adc_read();
}

static uint16_t read_sensor_average(uint gpio_pin, uint samples)
{
    uint32_t total = 0u;
    for (uint i = 0u; i < samples; ++i) {
        total += read_sensor_once(gpio_pin);
        sleep_us(500);
    }
    return (uint16_t)(total / samples);
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

static void capture_surface(uint16_t destination[SENSOR_COUNT],
                            const char *surface_name)
{
    printf("Capturing %s surface... keep the robot still.\n", surface_name);
    for (uint i = 0u; i < SENSOR_COUNT; ++i) {
        destination[i] = read_sensor_average(sensor_pins[i], 64u);
    }
    printf("%s captured: G5=%u G6=%u G7=%u\n",
           surface_name,
           destination[0], destination[1], destination[2]);
}

static bool reading_is_dark(uint index, uint16_t reading)
{
    uint16_t threshold = (uint16_t)(((uint32_t)light_values[index] +
                                     dark_values[index]) / 2u);

    if (dark_values[index] > light_values[index]) {
        return reading > threshold;
    }
    return reading < threshold;
}

static void print_calibration(void)
{
    printf("\n=== CALIBRATION RESULTS ===\n");
    for (uint i = 0u; i < SENSOR_COUNT; ++i) {
        uint16_t threshold = (uint16_t)(((uint32_t)light_values[i] +
                                         dark_values[i]) / 2u);
        uint16_t difference = light_values[i] > dark_values[i]
            ? light_values[i] - dark_values[i]
            : dark_values[i] - light_values[i];

        printf("Grove %u: light=%u dark=%u threshold=%u difference=%u",
               i + 5u, light_values[i], dark_values[i], threshold, difference);
        if (difference < 300u) {
            printf("  WARNING: weak contrast");
        }
        printf("\n");
    }
    printf("===========================\n\n");
}

int main(void)
{
    stdio_init_all();
    adc_init();
    for (uint i = 0u; i < SENSOR_COUNT; ++i) {
        adc_gpio_init(sensor_pins[i]);
    }

    gpio_init(LIGHT_CAPTURE_BUTTON_PIN);
    gpio_set_dir(LIGHT_CAPTURE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(LIGHT_CAPTURE_BUTTON_PIN);

    gpio_init(DARK_CAPTURE_BUTTON_PIN);
    gpio_set_dir(DARK_CAPTURE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(DARK_CAPTURE_BUTTON_PIN);

    sleep_ms(2500);
    printf("\nThree-sensor IR calibration test\n");
    printf("Motors and servo are disabled.\n");
    printf("GP20=capture LIGHT surface, GP21=capture DARK surface.\n");

    bool previous_light_button = true;
    bool previous_dark_button = true;
    uint64_t next_report_us = 0u;

    while (true) {
        bool light_button = gpio_get(LIGHT_CAPTURE_BUTTON_PIN);
        bool dark_button = gpio_get(DARK_CAPTURE_BUTTON_PIN);

        if (previous_light_button && !light_button &&
            button_pressed(LIGHT_CAPTURE_BUTTON_PIN)) {
            capture_surface(light_values, "LIGHT");
            light_captured = true;
            if (dark_captured) {
                print_calibration();
            }
        }

        if (previous_dark_button && !dark_button &&
            button_pressed(DARK_CAPTURE_BUTTON_PIN)) {
            capture_surface(dark_values, "DARK");
            dark_captured = true;
            if (light_captured) {
                print_calibration();
            }
        }

        previous_light_button = gpio_get(LIGHT_CAPTURE_BUTTON_PIN);
        previous_dark_button = gpio_get(DARK_CAPTURE_BUTTON_PIN);

        uint64_t now_us = time_us_64();
        if (now_us >= next_report_us) {
            uint16_t readings[SENSOR_COUNT];
            for (uint i = 0u; i < SENSOR_COUNT; ++i) {
                readings[i] = read_sensor_average(sensor_pins[i], 8u);
            }

            printf("Raw: G5=%4u G6=%4u G7=%4u",
                   readings[0], readings[1], readings[2]);
            if (light_captured && dark_captured) {
                printf(" | Surface: G5=%s G6=%s G7=%s",
                       reading_is_dark(0u, readings[0]) ? "DARK" : "LIGHT",
                       reading_is_dark(1u, readings[1]) ? "DARK" : "LIGHT",
                       reading_is_dark(2u, readings[2]) ? "DARK" : "LIGHT");
            }
            printf("\n");
            next_report_us = now_us + 250000u;
        }

        sleep_ms(10);
    }
}
