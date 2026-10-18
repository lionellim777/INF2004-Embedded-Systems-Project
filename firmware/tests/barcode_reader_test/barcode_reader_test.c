/*
 * Standalone Buddy 3 Code 39 reader test.
 * Motors are held off. Move a complete barcode perpendicular to its bars.
 * GP20 starts/stops scanning; GP21 resets capture and duplicate history.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"

#include "barcode_reader.h"
#include "navigation_commands.h"

enum {
    LEFT_IR_PIN = 26,
    CENTRE_IR_PIN = 28,
    RIGHT_IR_PIN = 27,
    START_STOP_BUTTON_PIN = 20,
    RESET_BUTTON_PIN = 21,
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    SENSOR_SAMPLE_COUNT = 8,
    BLACKNESS_THRESHOLD = 250,
    REPORT_INTERVAL_MS = 200,
};

enum {
    LEFT_IR_WHITE = 191,
    LEFT_IR_BLACK = 3164,
    CENTRE_IR_WHITE = 178,
    CENTRE_IR_BLACK = 2908,
    RIGHT_IR_WHITE = 141,
    RIGHT_IR_BLACK = 842,
};

typedef struct {
    uint16_t left_raw;
    uint16_t centre_raw;
    uint16_t right_raw;
    int32_t left_blackness;
    int32_t centre_blackness;
    int32_t right_blackness;
    unsigned int black_count;
} sensor_sample_t;

static void hold_motors_stopped(void)
{
    const uint pins[] = {
        LEFT_MOTOR_A_PIN,
        LEFT_MOTOR_B_PIN,
        RIGHT_MOTOR_A_PIN,
        RIGHT_MOTOR_B_PIN,
    };

    for (uint index = 0u; index < sizeof pins / sizeof pins[0]; ++index) {
        gpio_init(pins[index]);
        gpio_put(pins[index], 0);
        gpio_set_dir(pins[index], GPIO_OUT);
    }
}

static void sensors_init(void)
{
    adc_init();
    adc_gpio_init(LEFT_IR_PIN);
    adc_gpio_init(CENTRE_IR_PIN);
    adc_gpio_init(RIGHT_IR_PIN);
}

static uint16_t read_average(uint pin)
{
    uint32_t total = 0u;
    adc_select_input(pin - 26u);
    sleep_us(5u);

    for (uint index = 0u; index < SENSOR_SAMPLE_COUNT; ++index) {
        total += adc_read();
    }
    return (uint16_t)(total / SENSOR_SAMPLE_COUNT);
}

static int32_t blackness(uint16_t raw, int32_t white, int32_t black)
{
    int32_t value = ((int32_t)raw - white) * 1000 / (black - white);
    if (value < 0) {
        return 0;
    }
    if (value > 1000) {
        return 1000;
    }
    return value;
}

static sensor_sample_t read_sensors(void)
{
    sensor_sample_t sample = {0};
    sample.left_raw = read_average(LEFT_IR_PIN);
    sample.centre_raw = read_average(CENTRE_IR_PIN);
    sample.right_raw = read_average(RIGHT_IR_PIN);
    sample.left_blackness = blackness(
        sample.left_raw, LEFT_IR_WHITE, LEFT_IR_BLACK);
    sample.centre_blackness = blackness(
        sample.centre_raw, CENTRE_IR_WHITE, CENTRE_IR_BLACK);
    sample.right_blackness = blackness(
        sample.right_raw, RIGHT_IR_WHITE, RIGHT_IR_BLACK);
    sample.black_count =
        (sample.left_blackness >= BLACKNESS_THRESHOLD ? 1u : 0u) +
        (sample.centre_blackness >= BLACKNESS_THRESHOLD ? 1u : 0u) +
        (sample.right_blackness >= BLACKNESS_THRESHOLD ? 1u : 0u);
    return sample;
}

static bool button_pressed(uint pin)
{
    if (gpio_get(pin)) {
        return false;
    }
    sleep_ms(25u);
    if (gpio_get(pin)) {
        return false;
    }
    while (!gpio_get(pin)) {
        sleep_ms(5u);
    }
    return true;
}

int main(void)
{
    stdio_init_all();
    hold_motors_stopped();
    sensors_init();

    gpio_init(START_STOP_BUTTON_PIN);
    gpio_set_dir(START_STOP_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_STOP_BUTTON_PIN);
    gpio_init(RESET_BUTTON_PIN);
    gpio_set_dir(RESET_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(RESET_BUTTON_PIN);

    sleep_ms(1500u);
    printf("\n=== BUDDY 3 BARCODE READER TEST ===\n");
    printf("Motors disabled. GP20=start/stop, GP21=reset.\n");
    printf("Begin on white, then move perpendicular across all bars.\n");
    printf("Keep every bar/space below 750 ms.\n\n");

    barcode_reader_t reader;
    barcode_reader_init(&reader);
    bool scanning = false;
    bool previous_start = true;
    bool previous_reset = true;
    uint64_t next_report_ms = 0u;
    uint32_t decoded_count = 0u;

    while (true) {
        bool start = gpio_get(START_STOP_BUTTON_PIN);
        bool reset = gpio_get(RESET_BUTTON_PIN);

        if (previous_start && !start &&
            button_pressed(START_STOP_BUTTON_PIN)) {
            scanning = !scanning;
            barcode_reader_init(&reader);
            printf("Scanning: %s\n", scanning ? "STARTED" : "STOPPED");
        }
        if (previous_reset && !reset && button_pressed(RESET_BUTTON_PIN)) {
            barcode_reader_init(&reader);
            decoded_count = 0u;
            printf("Reader and counters reset.\n");
        }
        previous_start = gpio_get(START_STOP_BUTTON_PIN);
        previous_reset = gpio_get(RESET_BUTTON_PIN);

        sensor_sample_t sample = read_sensors();
        uint64_t now_us = time_us_64();
        bool broad_black = sample.black_count >= 2u;
        char letter = '\0';

        if (scanning && barcode_reader_update(
                &reader, broad_black, (uint32_t)now_us, &letter)) {
            navigation_command_t command = navigation_decode_symbol(letter);
            navigation_manoeuvre_t manoeuvre =
                navigation_create_manoeuvre(command);
            ++decoded_count;
            printf("DECODED=%c Command=%s Turn=%+.0fdeg Count=%lu\n",
                   letter, navigation_command_name(command),
                   (double)manoeuvre.requested_turn_degrees,
                   (unsigned long)decoded_count);
        }

        uint64_t now_ms = now_us / 1000u;
        if (now_ms >= next_report_ms) {
            printf("Scan=%u Capture=%u Pattern=%u%u%u "
                   "Raw=%u/%u/%u Blackness=%ld/%ld/%ld\n",
                   scanning ? 1u : 0u,
                   barcode_reader_active(&reader) ? 1u : 0u,
                   sample.left_blackness >= BLACKNESS_THRESHOLD ? 1u : 0u,
                   sample.centre_blackness >= BLACKNESS_THRESHOLD ? 1u : 0u,
                   sample.right_blackness >= BLACKNESS_THRESHOLD ? 1u : 0u,
                   sample.left_raw, sample.centre_raw, sample.right_raw,
                   (long)sample.left_blackness,
                   (long)sample.centre_blackness,
                   (long)sample.right_blackness);
            next_report_ms = now_ms + REPORT_INTERVAL_MS;
        }

        sleep_ms(5u);
    }
}
