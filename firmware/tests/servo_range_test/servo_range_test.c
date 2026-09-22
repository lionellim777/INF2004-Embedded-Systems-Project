/*
 * Manual servo clearance test for the INF2004 robot.
 *
 * GP20: decrease the commanded angle by 5 degrees.
 * GP21: increase the commanded angle by 5 degrees.
 * Motors are not configured and cannot run.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

enum {
    SERVO_PIN = 12,
    DECREASE_BUTTON_PIN = 20,
    INCREASE_BUTTON_PIN = 21,
    START_ANGLE = 90,
    STEP_DEGREES = 5,
};

static uint servo_slice;
static uint servo_channel;

static void servo_set_angle(uint angle_degrees)
{
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
    servo_init();

    gpio_init(DECREASE_BUTTON_PIN);
    gpio_set_dir(DECREASE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(DECREASE_BUTTON_PIN);

    gpio_init(INCREASE_BUTTON_PIN);
    gpio_set_dir(INCREASE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(INCREASE_BUTTON_PIN);

    int angle = START_ANGLE;
    servo_set_angle((uint)angle);

    sleep_ms(2500);
    printf("\nManual servo clearance test\n");
    printf("Motors disabled. Starting angle: %d degrees.\n", angle);
    printf("GP20 = -5 degrees, GP21 = +5 degrees.\n");
    printf("Stop before the sensor or its wires touch anything.\n");

    bool previous_decrease = true;
    bool previous_increase = true;

    while (true) {
        bool decrease = gpio_get(DECREASE_BUTTON_PIN);
        bool increase = gpio_get(INCREASE_BUTTON_PIN);

        if (previous_decrease && !decrease &&
            button_pressed(DECREASE_BUTTON_PIN)) {
            if (angle >= STEP_DEGREES) {
                angle -= STEP_DEGREES;
                servo_set_angle((uint)angle);
                printf("Commanded angle: %d degrees\n", angle);
            }
        }

        if (previous_increase && !increase &&
            button_pressed(INCREASE_BUTTON_PIN)) {
            if (angle <= 180 - STEP_DEGREES) {
                angle += STEP_DEGREES;
                servo_set_angle((uint)angle);
                printf("Commanded angle: %d degrees\n", angle);
            }
        }

        previous_decrease = gpio_get(DECREASE_BUTTON_PIN);
        previous_increase = gpio_get(INCREASE_BUTTON_PIN);
        sleep_ms(10);
    }
}
