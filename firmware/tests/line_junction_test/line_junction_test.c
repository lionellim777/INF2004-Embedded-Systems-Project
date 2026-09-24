/*
 * INF2004 line and junction diagnostic.
 *
 * Motors are held off. Move the robot manually across the real course.
 * GP20 starts/stops event recording. GP21 clears recorded counters.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "pico/stdlib.h"

enum {
    LEFT_IR_PIN = 26,
    CENTRE_IR_PIN = 28,
    RIGHT_IR_PIN = 27,
    RECORD_BUTTON_PIN = 20,
    CLEAR_BUTTON_PIN = 21,
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    SENSOR_SAMPLE_COUNT = 8,
    BLACKNESS_THRESHOLD = 250,
    PATTERN_STABLE_MS = 40,
    JUNCTION_CONFIRM_MS = 100,
    REPORT_INTERVAL_MS = 200,
};

enum {
    MASK_RIGHT = 1u,
    MASK_CENTRE = 2u,
    MASK_LEFT = 4u,
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
    uint8_t mask;
} line_sample_t;

static void hold_motors_stopped(void)
{
    const uint motor_pins[] = {
        LEFT_MOTOR_A_PIN,
        LEFT_MOTOR_B_PIN,
        RIGHT_MOTOR_A_PIN,
        RIGHT_MOTOR_B_PIN,
    };

    for (uint index = 0u;
         index < (sizeof motor_pins / sizeof motor_pins[0]);
         ++index) {
        gpio_init(motor_pins[index]);
        gpio_put(motor_pins[index], 0);
        gpio_set_dir(motor_pins[index], GPIO_OUT);
    }
}

static void line_sensors_init(void)
{
    adc_init();
    adc_gpio_init(LEFT_IR_PIN);
    adc_gpio_init(CENTRE_IR_PIN);
    adc_gpio_init(RIGHT_IR_PIN);
}

static uint16_t read_sensor_average(uint gpio_pin)
{
    uint32_t total = 0u;

    adc_select_input(gpio_pin - 26u);
    sleep_us(5);

    for (uint index = 0u; index < SENSOR_SAMPLE_COUNT; ++index) {
        total += adc_read();
    }

    return (uint16_t)(total / SENSOR_SAMPLE_COUNT);
}

static int32_t calculate_blackness(uint16_t raw,
                                   int32_t white_value,
                                   int32_t black_value)
{
    int32_t denominator = black_value - white_value;
    int32_t result;

    if (denominator == 0) {
        return 0;
    }

    result = (((int32_t)raw - white_value) * 1000) / denominator;

    if (result < 0) {
        result = 0;
    } else if (result > 1000) {
        result = 1000;
    }

    return result;
}

static line_sample_t read_line(void)
{
    line_sample_t sample = {0};

    sample.left_raw = read_sensor_average(LEFT_IR_PIN);
    sample.centre_raw = read_sensor_average(CENTRE_IR_PIN);
    sample.right_raw = read_sensor_average(RIGHT_IR_PIN);

    sample.left_blackness = calculate_blackness(
        sample.left_raw, LEFT_IR_WHITE, LEFT_IR_BLACK);
    sample.centre_blackness = calculate_blackness(
        sample.centre_raw, CENTRE_IR_WHITE, CENTRE_IR_BLACK);
    sample.right_blackness = calculate_blackness(
        sample.right_raw, RIGHT_IR_WHITE, RIGHT_IR_BLACK);

    if (sample.left_blackness >= BLACKNESS_THRESHOLD) {
        sample.mask |= MASK_LEFT;
    }
    if (sample.centre_blackness >= BLACKNESS_THRESHOLD) {
        sample.mask |= MASK_CENTRE;
    }
    if (sample.right_blackness >= BLACKNESS_THRESHOLD) {
        sample.mask |= MASK_RIGHT;
    }

    return sample;
}

static const char *pattern_name(uint8_t mask)
{
    const char *name;

    switch (mask) {
        case 0u:
            name = "LINE_LOST";
            break;
        case MASK_LEFT:
            name = "LEFT_ONLY";
            break;
        case MASK_CENTRE:
            name = "CENTRED";
            break;
        case MASK_RIGHT:
            name = "RIGHT_ONLY";
            break;
        case MASK_LEFT | MASK_CENTRE:
            name = "LEFT_BRANCH_OR_CURVE";
            break;
        case MASK_CENTRE | MASK_RIGHT:
            name = "RIGHT_BRANCH_OR_CURVE";
            break;
        case MASK_LEFT | MASK_RIGHT:
            name = "SPLIT_OR_AMBIGUOUS";
            break;
        case MASK_LEFT | MASK_CENTRE | MASK_RIGHT:
            name = "BROAD_MARK_OR_JUNCTION";
            break;
        default:
            name = "UNKNOWN";
            break;
    }

    return name;
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

int main(void)
{
    stdio_init_all();
    hold_motors_stopped();
    line_sensors_init();

    gpio_init(RECORD_BUTTON_PIN);
    gpio_set_dir(RECORD_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(RECORD_BUTTON_PIN);
    gpio_init(CLEAR_BUTTON_PIN);
    gpio_set_dir(CLEAR_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(CLEAR_BUTTON_PIN);

    sleep_ms(1500);
    printf("\n=== LINE AND JUNCTION DIAGNOSTIC ===\n");
    printf("Motors are disabled. Move the car manually.\n");
    printf("GP20=start/stop recording. GP21=clear counters.\n\n");

    bool recording = false;
    bool previous_record_button = true;
    bool previous_clear_button = true;
    uint8_t candidate_mask = 0u;
    uint8_t stable_mask = 0u;
    uint64_t now_ms = to_ms_since_boot(get_absolute_time());
    uint64_t candidate_start_ms = now_ms;
    uint64_t stable_start_ms = now_ms;
    uint64_t next_report_ms = now_ms;
    bool junction_reported = false;
    uint32_t event_count = 0u;
    uint32_t junction_count = 0u;

    while (true) {
        bool record_button = gpio_get(RECORD_BUTTON_PIN);
        bool clear_button = gpio_get(CLEAR_BUTTON_PIN);

        if (previous_record_button && !record_button &&
            button_pressed(RECORD_BUTTON_PIN)) {
            recording = !recording;
            printf("Course recording: %s\n",
                   recording ? "STARTED" : "STOPPED");
        }

        if (previous_clear_button && !clear_button &&
            button_pressed(CLEAR_BUTTON_PIN)) {
            event_count = 0u;
            junction_count = 0u;
            printf("Recorded counters cleared.\n");
        }

        previous_record_button = gpio_get(RECORD_BUTTON_PIN);
        previous_clear_button = gpio_get(CLEAR_BUTTON_PIN);

        line_sample_t sample = read_line();
        now_ms = to_ms_since_boot(get_absolute_time());

        if (sample.mask != candidate_mask) {
            candidate_mask = sample.mask;
            candidate_start_ms = now_ms;
        }

        if ((candidate_mask != stable_mask) &&
            ((now_ms - candidate_start_ms) >= PATTERN_STABLE_MS)) {
            uint64_t previous_duration_ms = now_ms - stable_start_ms;

            stable_mask = candidate_mask;
            stable_start_ms = now_ms;
            junction_reported = false;

            if (recording) {
                ++event_count;
                printf("EVENT=%lu Pattern=%u%u%u Name=%s "
                       "PreviousDuration=%llu ms\n",
                       (unsigned long)event_count,
                       (stable_mask & MASK_LEFT) ? 1u : 0u,
                       (stable_mask & MASK_CENTRE) ? 1u : 0u,
                       (stable_mask & MASK_RIGHT) ? 1u : 0u,
                       pattern_name(stable_mask),
                       (unsigned long long)previous_duration_ms);
            }
        }

        bool broad_pattern = stable_mask ==
            (MASK_LEFT | MASK_CENTRE | MASK_RIGHT);

        if (recording && broad_pattern && !junction_reported &&
            ((now_ms - stable_start_ms) >= JUNCTION_CONFIRM_MS)) {
            ++junction_count;
            junction_reported = true;
            printf("JUNCTION_CANDIDATE=%lu Pattern=111 StableFor=%llu ms\n",
                   (unsigned long)junction_count,
                   (unsigned long long)(now_ms - stable_start_ms));
        }

        if (now_ms >= next_report_ms) {
            printf("Record=%u Pattern=%u%u%u Name=%s "
                   "Raw=%u/%u/%u Blackness=%ld/%ld/%ld "
                   "Stable=%llu ms Junctions=%lu\n",
                   recording ? 1u : 0u,
                   (sample.mask & MASK_LEFT) ? 1u : 0u,
                   (sample.mask & MASK_CENTRE) ? 1u : 0u,
                   (sample.mask & MASK_RIGHT) ? 1u : 0u,
                   pattern_name(sample.mask),
                   sample.left_raw,
                   sample.centre_raw,
                   sample.right_raw,
                   (long)sample.left_blackness,
                   (long)sample.centre_blackness,
                   (long)sample.right_blackness,
                   (unsigned long long)(now_ms - stable_start_ms),
                   (unsigned long)junction_count);

            next_report_ms = now_ms + REPORT_INTERVAL_MS;
        }

        sleep_ms(5);
    }
}
