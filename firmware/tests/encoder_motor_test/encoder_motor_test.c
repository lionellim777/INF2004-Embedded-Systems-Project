/*
 * INF2004 raised-wheel motor and encoder test for Cytron Robo Pico + Pico W.
 *
 * IMPORTANT: Raise both driven wheels before pressing GP20. The test drives
 * the left wheel, right wheel, then both wheels at 35% duty. Press GP20 again
 * at any time to stop. Motors remain stopped at startup and after completion.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

enum {
    LEFT_ENCODER_A_PIN = 2,
    LEFT_ENCODER_B_PIN = 3,
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    RIGHT_ENCODER_A_PIN = 16,
    RIGHT_ENCODER_B_PIN = 17,
    START_STOP_BUTTON_PIN = 20,
    CONTROL_INTERVAL_MS = 100,
    REPORT_INTERVAL_MS = 500,
    COUNTS_PER_REVOLUTION = 2543,
    TARGET_COUNTS_PER_SECOND = 3000,
};

static const float TEST_THROTTLE = 0.35f;
static const float SPEED_KP = 0.000035f;
static const float SPEED_KI = 0.000015f;

typedef struct {
    uint slice;
    uint channel_a;
    uint channel_b;
    float forward_sign;
} motor_t;

typedef struct {
    volatile uint32_t a_edges;
    volatile uint32_t b_edges;
    volatile uint32_t invalid_transitions;
    volatile int32_t position;
    volatile uint8_t previous_ab;
} encoder_t;

typedef struct {
    uint32_t a_edges;
    uint32_t b_edges;
    uint32_t invalid_transitions;
    int32_t position;
} encoder_snapshot_t;

typedef struct {
    const char *name;
    uint32_t duration_ms;
    float left_throttle;
    float right_throttle;
    bool measure;
    bool closed_loop;
} test_stage_t;

static motor_t left_motor;
static motor_t right_motor;
static uint16_t motor_wrap;
static encoder_t left_encoder;
static encoder_t right_encoder;

static const test_stage_t stages[] = {
    {"SAFETY COUNTDOWN", 3000u, 0.0f, 0.0f, false, false},
    {"LEFT ONLY",        4000u, TEST_THROTTLE, 0.0f, true, false},
    {"PAUSE",            1000u, 0.0f, 0.0f, false, false},
    {"RIGHT ONLY",       4000u, 0.0f, TEST_THROTTLE, true, false},
    {"PAUSE",            1000u, 0.0f, 0.0f, false, false},
    {"BOTH OPEN LOOP",   5000u, TEST_THROTTLE, TEST_THROTTLE, true, false},
    {"PAUSE",            1000u, 0.0f, 0.0f, false, false},
    {"BOTH CLOSED LOOP", 8000u, 0.325f, 0.310f, true, true},
};

static uint8_t read_ab(uint a_pin, uint b_pin)
{
    return (uint8_t)((gpio_get(a_pin) ? 2u : 0u) |
                     (gpio_get(b_pin) ? 1u : 0u));
}

static void encoder_edge_callback(uint gpio, uint32_t events)
{
    (void)events;
    encoder_t *encoder;
    uint a_pin;
    uint b_pin;

    if (gpio == LEFT_ENCODER_A_PIN || gpio == LEFT_ENCODER_B_PIN) {
        encoder = &left_encoder;
        a_pin = LEFT_ENCODER_A_PIN;
        b_pin = LEFT_ENCODER_B_PIN;
    } else if (gpio == RIGHT_ENCODER_A_PIN ||
               gpio == RIGHT_ENCODER_B_PIN) {
        encoder = &right_encoder;
        a_pin = RIGHT_ENCODER_A_PIN;
        b_pin = RIGHT_ENCODER_B_PIN;
    } else {
        return;
    }

    if (gpio == a_pin) {
        ++encoder->a_edges;
    } else {
        ++encoder->b_edges;
    }

    static const int8_t quadrature_step[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0,
    };
    uint8_t current_ab = read_ab(a_pin, b_pin);
    uint8_t old_ab = encoder->previous_ab;
    if ((old_ab ^ current_ab) == 3u) {
        ++encoder->invalid_transitions;
    }
    encoder->position += quadrature_step[(old_ab << 2u) | current_ab];
    encoder->previous_ab = current_ab;
}

static encoder_snapshot_t encoder_snapshot(encoder_t *encoder)
{
    uint32_t saved_interrupts = save_and_disable_interrupts();
    encoder_snapshot_t snapshot = {
        .a_edges = encoder->a_edges,
        .b_edges = encoder->b_edges,
        .invalid_transitions = encoder->invalid_transitions,
        .position = encoder->position,
    };
    restore_interrupts(saved_interrupts);
    return snapshot;
}

static void encoders_init(void)
{
    const uint pins[] = {
        LEFT_ENCODER_A_PIN, LEFT_ENCODER_B_PIN,
        RIGHT_ENCODER_A_PIN, RIGHT_ENCODER_B_PIN,
    };
    for (uint i = 0u; i < 4u; ++i) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    left_encoder.previous_ab = read_ab(LEFT_ENCODER_A_PIN,
                                       LEFT_ENCODER_B_PIN);
    right_encoder.previous_ab = read_ab(RIGHT_ENCODER_A_PIN,
                                        RIGHT_ENCODER_B_PIN);

    const uint32_t both_edges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
    gpio_set_irq_enabled_with_callback(LEFT_ENCODER_A_PIN, both_edges, true,
                                       encoder_edge_callback);
    gpio_set_irq_enabled(LEFT_ENCODER_B_PIN, both_edges, true);
    gpio_set_irq_enabled(RIGHT_ENCODER_A_PIN, both_edges, true);
    gpio_set_irq_enabled(RIGHT_ENCODER_B_PIN, both_edges, true);
}

static void motor_stop(motor_t *motor)
{
    pwm_set_chan_level(motor->slice, motor->channel_a, 0u);
    pwm_set_chan_level(motor->slice, motor->channel_b, 0u);
}

static void motor_set(motor_t *motor, float forward_throttle)
{
    float throttle = forward_throttle * motor->forward_sign;
    if (throttle > 1.0f) {
        throttle = 1.0f;
    } else if (throttle < -1.0f) {
        throttle = -1.0f;
    }

    float magnitude = throttle < 0.0f ? -throttle : throttle;
    uint16_t level = (uint16_t)((float)(motor_wrap + 1u) * magnitude);
    if (throttle > 0.0f) {
        pwm_set_chan_level(motor->slice, motor->channel_a, level);
        pwm_set_chan_level(motor->slice, motor->channel_b, 0u);
    } else if (throttle < 0.0f) {
        pwm_set_chan_level(motor->slice, motor->channel_a, 0u);
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
    motor_init_one(&left_motor, LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN, 1.0f);
    motor_init_one(&right_motor, RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN, 1.0f);
}

static void stop_both_motors(void)
{
    motor_stop(&left_motor);
    motor_stop(&right_motor);
}

static bool button_pressed(void)
{
    if (gpio_get(START_STOP_BUTTON_PIN)) {
        return false;
    }
    sleep_ms(20u);
    return !gpio_get(START_STOP_BUTTON_PIN);
}

static int32_t normalized_left_position(encoder_snapshot_t snapshot)
{
    return -snapshot.position;
}

static int32_t normalized_right_position(encoder_snapshot_t snapshot)
{
    return snapshot.position;
}

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

static void print_measurement(const test_stage_t *stage,
                              encoder_snapshot_t phase_left,
                              encoder_snapshot_t phase_right,
                              encoder_snapshot_t previous_left,
                              encoder_snapshot_t previous_right,
                              encoder_snapshot_t current_left,
                              encoder_snapshot_t current_right,
                              uint32_t interval_ms,
                              float left_command,
                              float right_command)
{
    int32_t left_total = normalized_left_position(current_left) -
                         normalized_left_position(phase_left);
    int32_t right_total = normalized_right_position(current_right) -
                          normalized_right_position(phase_right);
    int32_t left_interval = normalized_left_position(current_left) -
                            normalized_left_position(previous_left);
    int32_t right_interval = normalized_right_position(current_right) -
                             normalized_right_position(previous_right);
    float interval_seconds = (float)interval_ms / 1000.0f;
    float left_cps = (float)left_interval / interval_seconds;
    float right_cps = (float)right_interval / interval_seconds;
    float left_rpm = left_cps * 60.0f / (float)COUNTS_PER_REVOLUTION;
    float right_rpm = right_cps * 60.0f / (float)COUNTS_PER_REVOLUTION;

    printf("Stage=%s L=%ld R=%ld | L=%+.0f cps %+.1f rpm @ %.1f%% | "
           "R=%+.0f cps %+.1f rpm @ %.1f%% | invalid=%lu/%lu",
           stage->name, (long)left_total, (long)right_total,
           (double)left_cps, (double)left_rpm, (double)(left_command * 100.0f),
           (double)right_cps, (double)right_rpm,
           (double)(right_command * 100.0f),
           (unsigned long)(current_left.invalid_transitions -
                           phase_left.invalid_transitions),
           (unsigned long)(current_right.invalid_transitions -
                           phase_right.invalid_transitions));
    if (stage->closed_loop) {
        printf(" target=%u", TARGET_COUNTS_PER_SECOND);
    }
    printf("\n");
}

static void print_stage_summary(const test_stage_t *stage,
                                encoder_snapshot_t start_left,
                                encoder_snapshot_t start_right,
                                encoder_snapshot_t end_left,
                                encoder_snapshot_t end_right)
{
    int32_t left = normalized_left_position(end_left) -
                   normalized_left_position(start_left);
    int32_t right = normalized_right_position(end_right) -
                    normalized_right_position(start_right);
    float seconds = (float)stage->duration_ms / 1000.0f;
    float left_cps = (float)left / seconds;
    float right_cps = (float)right / seconds;

    printf("SUMMARY %s: L=%ld (%+.0f cps) R=%ld (%+.0f cps)",
           stage->name, (long)left, (double)left_cps,
           (long)right, (double)right_cps);
    if (stage->left_throttle > 0.0f && stage->right_throttle > 0.0f) {
        float left_magnitude = left_cps < 0.0f ? -left_cps : left_cps;
        float right_magnitude = right_cps < 0.0f ? -right_cps : right_cps;
        float average = (left_magnitude + right_magnitude) * 0.5f;
        float difference = left_magnitude - right_magnitude;
        difference = difference < 0.0f ? -difference : difference;
        float mismatch = average > 0.0f ? difference * 100.0f / average : 0.0f;
        printf(" mismatch=%.1f%%", (double)mismatch);
    }
    if ((stage->left_throttle > 0.0f && left < 0) ||
        (stage->right_throttle > 0.0f && right < 0)) {
        printf(" DIRECTION=REVERSED");
    }
    printf(" invalid=%lu/%lu\n",
           (unsigned long)(end_left.invalid_transitions -
                           start_left.invalid_transitions),
           (unsigned long)(end_right.invalid_transitions -
                           start_right.invalid_transitions));
}

static void run_test(void)
{
    stop_both_motors();
    while (!gpio_get(START_STOP_BUTTON_PIN)) {
        sleep_ms(10u);
    }
    printf("\nTest armed. Keep clear: motors start in 3 seconds.\n");

    for (uint stage_index = 0u;
         stage_index < sizeof stages / sizeof stages[0];
         ++stage_index) {
        const test_stage_t *stage = &stages[stage_index];
        encoder_snapshot_t phase_left = encoder_snapshot(&left_encoder);
        encoder_snapshot_t phase_right = encoder_snapshot(&right_encoder);
        encoder_snapshot_t previous_left = phase_left;
        encoder_snapshot_t previous_right = phase_right;
        encoder_snapshot_t control_left = phase_left;
        encoder_snapshot_t control_right = phase_right;
        uint64_t stage_start_us = time_us_64();
        uint64_t last_report_us = stage_start_us;
        uint64_t last_control_us = stage_start_us;
        float left_command = stage->left_throttle;
        float right_command = stage->right_throttle;
        float left_integral = 0.0f;
        float right_integral = 0.0f;

        motor_set(&left_motor, left_command);
        motor_set(&right_motor, right_command);
        printf("\n--- %s (%lu ms) ---\n", stage->name,
               (unsigned long)stage->duration_ms);

        while ((time_us_64() - stage_start_us) / 1000u < stage->duration_ms) {
            if (button_pressed()) {
                stop_both_motors();
                printf("\nABORTED BY GP20: both motors stopped.\n");
                while (!gpio_get(START_STOP_BUTTON_PIN)) {
                    sleep_ms(10u);
                }
                return;
            }

            uint64_t now_us = time_us_64();
            if (stage->closed_loop &&
                (now_us - last_control_us) / 1000u >= CONTROL_INTERVAL_MS) {
                encoder_snapshot_t current_left =
                    encoder_snapshot(&left_encoder);
                encoder_snapshot_t current_right =
                    encoder_snapshot(&right_encoder);
                float dt = (float)(now_us - last_control_us) / 1000000.0f;
                float left_cps =
                    (float)(normalized_left_position(current_left) -
                            normalized_left_position(control_left)) / dt;
                float right_cps =
                    (float)(normalized_right_position(current_right) -
                            normalized_right_position(control_right)) / dt;
                float left_error = (float)TARGET_COUNTS_PER_SECOND - left_cps;
                float right_error = (float)TARGET_COUNTS_PER_SECOND - right_cps;

                left_integral = clamp_float(left_integral + left_error * dt,
                                            -2000.0f, 2000.0f);
                right_integral = clamp_float(right_integral + right_error * dt,
                                             -2000.0f, 2000.0f);
                left_command = clamp_float(stage->left_throttle +
                                           SPEED_KP * left_error +
                                           SPEED_KI * left_integral,
                                           0.15f, 0.60f);
                right_command = clamp_float(stage->right_throttle +
                                            SPEED_KP * right_error +
                                            SPEED_KI * right_integral,
                                            0.15f, 0.60f);
                motor_set(&left_motor, left_command);
                motor_set(&right_motor, right_command);
                control_left = current_left;
                control_right = current_right;
                last_control_us = now_us;
            }
            if (stage->measure &&
                (now_us - last_report_us) / 1000u >= REPORT_INTERVAL_MS) {
                encoder_snapshot_t current_left =
                    encoder_snapshot(&left_encoder);
                encoder_snapshot_t current_right =
                    encoder_snapshot(&right_encoder);
                uint32_t interval_ms = (uint32_t)((now_us - last_report_us) /
                                                  1000u);
                print_measurement(stage, phase_left, phase_right,
                                  previous_left, previous_right,
                                  current_left, current_right, interval_ms,
                                  left_command, right_command);
                previous_left = current_left;
                previous_right = current_right;
                last_report_us = now_us;
            }
            sleep_ms(10u);
        }

        stop_both_motors();
        if (stage->measure) {
            encoder_snapshot_t end_left = encoder_snapshot(&left_encoder);
            encoder_snapshot_t end_right = encoder_snapshot(&right_encoder);
            print_stage_summary(stage, phase_left, phase_right,
                                end_left, end_right);
        }
    }

    stop_both_motors();
    printf("\nTEST COMPLETE: both motors stopped.\n");
    printf("Press GP20 to run again.\n");
}

int main(void)
{
    motors_init();
    stop_both_motors();
    encoders_init();

    gpio_init(START_STOP_BUTTON_PIN);
    gpio_set_dir(START_STOP_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_STOP_BUTTON_PIN);

    stdio_init_all();
    sleep_ms(1500u);
    printf("\nINF2004 RAISED-WHEEL MOTOR + ENCODER TEST\n");
    printf("Both motors are OFF. Raise both driven wheels.\n");
    printf("Connect motor power, keep hands clear, then press GP20.\n");
    printf("Sequence: left 35%%, right 35%%, both 35%%, then PI speed control.\n");
    printf("Press GP20 during the sequence for immediate stop.\n");
    printf("Calibration: %u quadrature counts per wheel revolution.\n\n",
           COUNTS_PER_REVOLUTION);

    bool previous_button = gpio_get(START_STOP_BUTTON_PIN);
    while (true) {
        bool current_button = gpio_get(START_STOP_BUTTON_PIN);
        if (previous_button && !current_button && button_pressed()) {
            run_test();
        }
        previous_button = gpio_get(START_STOP_BUTTON_PIN);
        sleep_ms(10u);
    }
}
