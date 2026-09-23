/*
 * INF2004 autonomous robot - line following with terrain and obstacle safety
 *
 * Wiring:
 *   Motor 1 (left):  M1A GP8,  M1B GP9
 *   Motor 2 (right): M2A GP10, M2B GP11
 *   Servo signal: GP12 (held at centre)
 *   Physical left line sensor AO:   GP26 via Grove 5
 *   Physical centre line sensor AO: GP28 via Grove 7
 *   Physical right line sensor AO:  GP27 via Grove 6
 *   GP20 button: start/stop line following
 *   HC-SR04+ TRIG: GP0, ECHO: GP1 (Grove 1)
 *
 * The robot starts stopped. After losing a previously detected line, it
 * crosses a short gap and then searches briefly toward the last seen side.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "encoders.h"
#include "terrain.h"

enum {
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    SERVO_PIN = 12,
    START_STOP_BUTTON_PIN = 20,
    TERRAIN_CALIBRATE_BUTTON_PIN = 21,
    LEFT_IR_PIN = 26,
    CENTRE_IR_PIN = 28,
    RIGHT_IR_PIN = 27,
    ULTRASONIC_TRIG_PIN = 0,
    ULTRASONIC_ECHO_PIN = 1,
};

/* Values measured on the assembled robot on 23 September 2026. */
enum {
    LEFT_IR_WHITE = 191,
    LEFT_IR_BLACK = 3164,
    CENTRE_IR_WHITE = 178,
    CENTRE_IR_BLACK = 2908,
    RIGHT_IR_WHITE = 141,
    RIGHT_IR_BLACK = 842,
};

/* These signs reflect the motor wiring verified on the assembled robot. */
static const float LEFT_FORWARD_SIGN = 1.0f;
static const float RIGHT_FORWARD_SIGN = -1.0f;

/* 23% spun unloaded wheels but could not start the car on the track. */
static const float BASE_THROTTLE = 0.55f;
static const float MAX_THROTTLE = 0.80f;
static const float MIN_THROTTLE = 0.20f;
static const float STEERING_GAIN = 0.00035f;
static const int32_t LINE_PRESENT_LEVEL = 250;
static const int32_t SIDE_ERROR_LEVEL = 100;
static const uint64_t GAP_CROSSING_US = 120000u;
static const uint64_t RECOVERY_TIMEOUT_US = 900000u;
static const float GAP_THROTTLE = 0.40f;
static const float SEARCH_THROTTLE = 0.40f;
static const float OBSTACLE_STOP_CM = 30.0f;
static const uint64_t RANGE_INTERVAL_US = 100000u;

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

static void line_sensors_init(void)
{
    adc_init();
    adc_gpio_init(LEFT_IR_PIN);
    adc_gpio_init(CENTRE_IR_PIN);
    adc_gpio_init(RIGHT_IR_PIN);
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
    uint64_t start = time_us_64();
    deadline = start + 30000u;
    while (gpio_get(ULTRASONIC_ECHO_PIN)) {
        if (time_us_64() >= deadline) {
            return false;
        }
        tight_loop_contents();
    }
    *distance_cm = (float)(time_us_64() - start) * 0.01715f;
    return *distance_cm >= 2.0f && *distance_cm <= 400.0f;
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
    return !gpio_get(pin);
}

int main(void)
{
    stdio_init_all();
    motors_init();
    servo_init_centred();
    line_sensors_init();
    ultrasonic_init();
    encoders_init();

    gpio_init(START_STOP_BUTTON_PIN);
    gpio_set_dir(START_STOP_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_STOP_BUTTON_PIN);
    gpio_init(TERRAIN_CALIBRATE_BUTTON_PIN);
    gpio_set_dir(TERRAIN_CALIBRATE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(TERRAIN_CALIBRATE_BUTTON_PIN);

    sleep_ms(2500);
    printf("\nINF2004 three-sensor line follower\n");
    printf("Robot starts STOPPED. GP20=start/stop.\n");
    printf("GP21=recalibrate terrain while stopped.\n");
    printf("Physical order: left=G5 centre=G7 right=G6.\n\n");
    if (terrain_init()) {
        terrain_calibrate();
    }

    bool running = false;
    bool previous_start_button = true;
    bool previous_calibrate_button = true;
    uint64_t next_report_us = 0;
    uint64_t last_line_us = 0;
    uint64_t next_range_us = 0;
    float distance_cm = 0.0f;
    bool range_valid = false;
    uint close_readings = 0;
    int last_seen_side = 0;
    bool line_ever_seen = false;
    const char *motion_state = "STOPPED";

    while (true) {
        bool start_button = gpio_get(START_STOP_BUTTON_PIN);
        bool calibrate_button = gpio_get(TERRAIN_CALIBRATE_BUTTON_PIN);

        if (previous_start_button && !start_button &&
            button_pressed(START_STOP_BUTTON_PIN)) {
            running = !running;
            line_ever_seen = false;
            last_seen_side = 0;
            if (!running) {
                motors_stop();
            }
            printf("Line following: %s\n", running ? "STARTED" : "STOPPED");
        }

        previous_start_button = gpio_get(START_STOP_BUTTON_PIN);
        if (previous_calibrate_button && !calibrate_button &&
            button_pressed(TERRAIN_CALIBRATE_BUTTON_PIN)) {
            if (running) {
                printf("Stop the car with GP20 before terrain calibration.\n");
            } else {
                terrain_calibrate();
            }
        }
        previous_calibrate_button = gpio_get(TERRAIN_CALIBRATE_BUTTON_PIN);

        uint16_t left_raw = line_sensor_read(LEFT_IR_PIN);
        uint16_t centre_raw = line_sensor_read(CENTRE_IR_PIN);
        uint16_t right_raw = line_sensor_read(RIGHT_IR_PIN);
        int32_t left_blackness = blackness(left_raw, LEFT_IR_WHITE,
                                           LEFT_IR_BLACK);
        int32_t centre_blackness = blackness(centre_raw, CENTRE_IR_WHITE,
                                             CENTRE_IR_BLACK);
        int32_t right_blackness = blackness(right_raw, RIGHT_IR_WHITE,
                                            RIGHT_IR_BLACK);

        uint64_t now_us = time_us_64();
        if (now_us >= next_range_us) {
            range_valid = ultrasonic_read_cm(&distance_cm);
            next_range_us = time_us_64() + RANGE_INTERVAL_US;
            if (range_valid && distance_cm < OBSTACLE_STOP_CM) {
                if (close_readings < 2u) {
                    ++close_readings;
                }
            } else {
                close_readings = 0;
            }
        }

        int32_t strongest_blackness = left_blackness;
        if (centre_blackness > strongest_blackness) {
            strongest_blackness = centre_blackness;
        }
        if (right_blackness > strongest_blackness) {
            strongest_blackness = right_blackness;
        }
        bool line_visible = strongest_blackness >= LINE_PRESENT_LEVEL;

        float left_throttle = 0.0f;
        float right_throttle = 0.0f;
        now_us = time_us_64();

        if (running && close_readings >= 2u) {
            motors_stop();
            running = false;
            motion_state = "OBSTACLE_STOP";
            printf("OBSTACLE at %.1f cm: motors stopped. Clear it and press GP20 to restart.\n",
                   distance_cm);
        } else if (running && line_visible) {
            /*
             * Positive error means the line is under the left sensor:
             * slow the left wheel and speed up the right wheel to turn left.
             * The centre sensor confirms that a line is present but adds no
             * left/right error when the robot is centred.
             */
            int32_t error = left_blackness - right_blackness;
            if (error > SIDE_ERROR_LEVEL) {
                last_seen_side = -1;
            } else if (error < -SIDE_ERROR_LEVEL) {
                last_seen_side = 1;
            }
            line_ever_seen = true;
            last_line_us = now_us;
            float correction = (float)error * STEERING_GAIN;
            left_throttle = clamp_float(BASE_THROTTLE - correction,
                                        MIN_THROTTLE, MAX_THROTTLE);
            right_throttle = clamp_float(BASE_THROTTLE + correction,
                                         MIN_THROTTLE, MAX_THROTTLE);
            motor_set(&left_motor, left_throttle);
            motor_set(&right_motor, right_throttle);
            motion_state = "FOLLOW";
        } else if (running && line_ever_seen &&
                   now_us - last_line_us < GAP_CROSSING_US) {
            /* Carry straight across a brief imperfection in the black line. */
            left_throttle = GAP_THROTTLE;
            right_throttle = GAP_THROTTLE;
            motor_set(&left_motor, left_throttle);
            motor_set(&right_motor, right_throttle);
            motion_state = "SHORT_GAP";
        } else if (running && line_ever_seen && last_seen_side != 0 &&
                   now_us - last_line_us < RECOVERY_TIMEOUT_US) {
            /* Turn in place toward the last side that detected black. */
            left_throttle = last_seen_side < 0 ? -SEARCH_THROTTLE
                                                : SEARCH_THROTTLE;
            right_throttle = -left_throttle;
            motor_set(&left_motor, left_throttle);
            motor_set(&right_motor, right_throttle);
            motion_state = last_seen_side < 0 ? "SEARCH_LEFT" : "SEARCH_RIGHT";
        } else {
            motors_stop();
            motion_state = running ? "WAIT_LINE" : "STOPPED";
            if (running && line_ever_seen &&
                now_us - last_line_us >= RECOVERY_TIMEOUT_US) {
                running = false;
                motion_state = "LOST_TIMEOUT";
                printf("LINE LOST: recovery timed out; motors stopped.\n");
            }
        }

        if (running) {
            terrain_poll();
        }

        if (now_us >= next_report_us) {
            printf("Run=%u State=%s Line=%s L=%4u(%4ld) C=%4u(%4ld) R=%4u(%4ld) Drive=%+.2f/%+.2f",
                   running ? 1u : 0u,
                   motion_state,
                   line_visible ? "SEEN" : "LOST",
                   left_raw, (long)left_blackness,
                   centre_raw, (long)centre_blackness,
                   right_raw, (long)right_blackness,
                   left_throttle, right_throttle);
            terrain_status_t terrain = terrain_get_status();
            if (terrain.calibrated) {
                printf(" Humps=%lu HighestTilt=%.1fdeg",
                       (unsigned long)terrain.hump_count,
                       terrain.highest_peak_angle_deg);
            } else {
                printf(" Terrain=UNAVAILABLE");
            }
            if (range_valid) {
                printf(" Range=%.1fcm", distance_cm);
            } else {
                printf(" Range=NO_ECHO");
            }
            printf(" EncL=%ld EncR=%ld",
                   (long)encoder_left_count(),
                   (long)encoder_right_count());
            printf("\n");
            next_report_us = now_us + 250000u;
        }

        sleep_ms(5);
    }
}
