/*
 * INF2004 autonomous robot - Milestone 1: slow line following
 *
 * Wiring:
 *   Ultrasonic: TRIG GP0, ECHO GP1
 *   Motor 1 (left):  M1A GP8,  M1B GP9
 *   Motor 2 (right): M2A GP10, M2B GP11
 *   Servo signal: GP12 (held at centre)
 *   Left line sensor AO:  GP27 via Grove 6
 *   Right line sensor AO: GP28 via Grove 7
 *   GP20 button: start/stop line following
 *   GP21 button: one-second raised-wheel direction check
 *
 * SAFETY: The robot always starts stopped. Raise both wheels before using GP21.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

enum {
    ULTRASONIC_TRIG_PIN = 0,
    ULTRASONIC_ECHO_PIN = 1,
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    SERVO_PIN = 12,
    START_STOP_BUTTON_PIN = 20,
    DIRECTION_TEST_BUTTON_PIN = 21,
    LEFT_IR_PIN = 27,
    RIGHT_IR_PIN = 28,
};

/* Values measured on the assembled robot. */
enum {
    LEFT_IR_WHITE = 160,
    LEFT_IR_BLACK = 3500,
    RIGHT_IR_WHITE = 220,
    RIGHT_IR_BLACK = 3700,
};

/* Change one sign if its wheel runs backward during the GP21 direction test. */
static const float LEFT_FORWARD_SIGN = 1.0f;
static const float RIGHT_FORWARD_SIGN = -1.0f;

static const float BASE_THROTTLE = 0.26f;
static const float MAX_THROTTLE = 0.45f;
static const float STEERING_GAIN = 0.00020f;
static const float EMERGENCY_STOP_CM = 12.0f;

typedef struct {
    uint slice;
    uint channel_a;
    uint channel_b;
    float forward_sign;
} motor_t;

static motor_t left_motor;
static motor_t right_motor;
static uint16_t motor_wrap;
static uint servo_slice;
static uint servo_channel;

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void motor_stop(motor_t *motor)
{
    pwm_set_chan_level(motor->slice, motor->channel_a, 0);
    pwm_set_chan_level(motor->slice, motor->channel_b, 0);
}

static void motors_stop(void)
{
    motor_stop(&left_motor);
    motor_stop(&right_motor);
}

static void motor_set(motor_t *motor, float forward_throttle)
{
    float throttle = clamp_float(forward_throttle * motor->forward_sign,
                                 -1.0f, 1.0f);
    float magnitude = throttle < 0.0f ? -throttle : throttle;
    uint16_t level = (uint16_t)((float)(motor_wrap + 1u) * magnitude);

    if (throttle > 0.0f) {
        pwm_set_chan_level(motor->slice, motor->channel_a, level);
        pwm_set_chan_level(motor->slice, motor->channel_b, 0);
    } else if (throttle < 0.0f) {
        pwm_set_chan_level(motor->slice, motor->channel_a, 0);
        pwm_set_chan_level(motor->slice, motor->channel_b, level);
    } else {
        motor_stop(motor);
    }
}

static void motor_init_one(motor_t *motor, uint pin_a, uint pin_b,
                           float forward_sign)
{
    gpio_set_function(pin_a, GPIO_FUNC_PWM);
    gpio_set_function(pin_b, GPIO_FUNC_PWM);

    motor->slice = pwm_gpio_to_slice_num(pin_a);
    motor->channel_a = pwm_gpio_to_channel(pin_a);
    motor->channel_b = pwm_gpio_to_channel(pin_b);
    motor->forward_sign = forward_sign;

    pwm_set_clkdiv(motor->slice, 1.0f);
    pwm_set_wrap(motor->slice, motor_wrap);
    motor_stop(motor);
    pwm_set_enabled(motor->slice, true);
}

static void motors_init(void)
{
    motor_wrap = (uint16_t)(clock_get_hz(clk_sys) / 20000u - 1u);
    motor_init_one(&left_motor, LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN,
                   LEFT_FORWARD_SIGN);
    motor_init_one(&right_motor, RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN,
                   RIGHT_FORWARD_SIGN);
}

static void servo_init_centred(void)
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

static bool ultrasonic_read_cm(float *distance_cm)
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

    uint64_t pulse_width_us = time_us_64() - pulse_start;
    *distance_cm = (float)pulse_width_us * 0.0343f / 2.0f;
    return true;
}

static void line_sensors_init(void)
{
    adc_init();
    adc_gpio_init(LEFT_IR_PIN);
    adc_gpio_init(RIGHT_IR_PIN);
}

static uint16_t line_sensor_read(uint gpio_pin)
{
    adc_select_input(gpio_pin - 26u);
    sleep_us(5);

    uint32_t total = 0;
    for (uint sample = 0; sample < 8u; ++sample) {
        total += adc_read();
    }
    return (uint16_t)(total / 8u);
}

static int32_t blackness(uint16_t raw, int32_t white, int32_t black)
{
    int32_t result = ((int32_t)raw - white) * 1000 / (black - white);
    if (result < 0) {
        return 0;
    }
    if (result > 1000) {
        return 1000;
    }
    return result;
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

static void run_direction_test(void)
{
    printf("Direction test: both wheels commanded FORWARD for one second.\n");
    motor_set(&left_motor, 0.25f);
    motor_set(&right_motor, 0.25f);
    sleep_ms(1000);
    motors_stop();
    printf("Direction test stopped.\n");
}

int main(void)
{
    stdio_init_all();
    motors_init();
    servo_init_centred();
    ultrasonic_init();
    line_sensors_init();

    gpio_init(START_STOP_BUTTON_PIN);
    gpio_set_dir(START_STOP_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_STOP_BUTTON_PIN);

    gpio_init(DIRECTION_TEST_BUTTON_PIN);
    gpio_set_dir(DIRECTION_TEST_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(DIRECTION_TEST_BUTTON_PIN);

    sleep_ms(2500);
    printf("\nINF2004 robot firmware - line-following milestone\n");
    printf("Robot starts STOPPED. GP20=start/stop.\n");
    printf("Raise wheels before GP21 direction test.\n\n");

    bool running = false;
    bool previous_start_button = true;
    bool previous_test_button = true;
    uint32_t close_obstacle_count = 0;
    uint64_t next_ultrasonic_us = 0;
    uint64_t next_report_us = 0;
    bool obstacle_stop = false;
    float last_distance_cm = 0.0f;
    bool last_distance_valid = false;

    while (true) {
        bool start_button = gpio_get(START_STOP_BUTTON_PIN);
        bool test_button = gpio_get(DIRECTION_TEST_BUTTON_PIN);

        if (previous_start_button && !start_button &&
            button_pressed(START_STOP_BUTTON_PIN)) {
            running = !running;
            obstacle_stop = false;
            close_obstacle_count = 0;
            if (!running) {
                motors_stop();
            }
            printf("Line following: %s\n", running ? "STARTED" : "STOPPED");
        }

        if (previous_test_button && !test_button &&
            button_pressed(DIRECTION_TEST_BUTTON_PIN)) {
            running = false;
            motors_stop();
            run_direction_test();
        }

        previous_start_button = gpio_get(START_STOP_BUTTON_PIN);
        previous_test_button = gpio_get(DIRECTION_TEST_BUTTON_PIN);

        uint16_t left_raw = line_sensor_read(LEFT_IR_PIN);
        uint16_t right_raw = line_sensor_read(RIGHT_IR_PIN);
        int32_t left_blackness = blackness(left_raw, LEFT_IR_WHITE,
                                           LEFT_IR_BLACK);
        int32_t right_blackness = blackness(right_raw, RIGHT_IR_WHITE,
                                            RIGHT_IR_BLACK);

        uint64_t now_us = time_us_64();
        if (now_us >= next_ultrasonic_us) {
            last_distance_valid = ultrasonic_read_cm(&last_distance_cm);
            next_ultrasonic_us = time_us_64() + 60000u;

            if (last_distance_valid && last_distance_cm < EMERGENCY_STOP_CM) {
                ++close_obstacle_count;
                if (close_obstacle_count >= 3u) {
                    obstacle_stop = true;
                    running = false;
                    motors_stop();
                    printf("SAFETY STOP: obstacle %.1f cm ahead.\n",
                           last_distance_cm);
                }
            } else {
                close_obstacle_count = 0;
            }
        }

        if (running && !obstacle_stop) {
            int32_t error = left_blackness - right_blackness;
            float correction = (float)error * STEERING_GAIN;
            float left_throttle = clamp_float(BASE_THROTTLE - correction,
                                              0.08f, MAX_THROTTLE);
            float right_throttle = clamp_float(BASE_THROTTLE + correction,
                                               0.08f, MAX_THROTTLE);
            motor_set(&left_motor, left_throttle);
            motor_set(&right_motor, right_throttle);
        } else {
            motors_stop();
        }

        now_us = time_us_64();
        if (now_us >= next_report_us) {
            printf("Run=%u IR-L=%4u(%4ld) IR-R=%4u(%4ld)",
                   running ? 1u : 0u,
                   left_raw, (long)left_blackness,
                   right_raw, (long)right_blackness);
            if (last_distance_valid) {
                printf(" Distance=%.1fcm", last_distance_cm);
            } else {
                printf(" Distance=no-echo");
            }
            printf("\n");
            next_report_us = now_us + 250000u;
        }

        sleep_ms(5);
    }
}
