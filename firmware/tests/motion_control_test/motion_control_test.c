/*
 * INF2004 Buddy 2 ground motion calibration.
 *
 * Default: straight 30 cm, right 90, left 90, right 180 in one run.
 * GP21 selects the batch or an individual motion.
 * GP20 starts the selection and aborts it while running.
 * Keep a clear test area and be ready to press GP20.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/regs/addressmap.h"
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
    SELECT_BUTTON_PIN = 21,
    CONTROL_INTERVAL_MS = 100,
    REPORT_INTERVAL_MS = 500,
    MOTION_TIMEOUT_MS = 12000,
    COUNTS_PER_REVOLUTION = 2543,
    MOTION_COUNT = 4,
    RESULT_MAGIC = 0x42324d52u,
    RESULT_VERSION = 5u,
};

#define RESULT_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

static const float PI_VALUE = 3.14159265f;
static const float WHEEL_DIAMETER_MM = 60.0f;
static const float TRACK_WIDTH_MM = 115.0f;
/* Ground tests overshot 90 and 180 degree turns by about 20-25 degrees.
 * At the measured turn speed this is approximately 290 encoder counts of
 * extra wheel travel, so stop each turn that much earlier. */
static const int32_t TURN_STOP_EARLY_COUNTS = 290;
/* Keep straight-line wheel positions together throughout the run. */
static const float STRAIGHT_SYNC_CPS_PER_COUNT = 1.5f;
static const float STRAIGHT_SYNC_LIMIT_CPS = 120.0f;
/* Batch 4 left 90 overshot by about 5 degrees; reduce its extra travel. */
static const int32_t LEFT_90_EXTRA_COUNTS = 65;
/* Batch 4 right 180 overshot by about 5 degrees. */
static const int32_t RIGHT_180_SHORTEN_COUNTS = 60;
static const float SPEED_KP = 0.000035f;
static const float SPEED_KI = 0.000015f;

typedef struct {
    uint slice;
    uint channel_a;
    uint channel_b;
} motor_t;

typedef struct {
    volatile uint32_t invalid_transitions;
    volatile int32_t position;
    volatile uint8_t previous_ab;
} encoder_t;

typedef struct {
    uint32_t invalid_transitions;
    int32_t position;
} encoder_snapshot_t;

typedef struct {
    uint32_t id;
    const char *name;
    int32_t left_target;
    int32_t right_target;
} motion_t;

typedef struct {
    uint32_t motion_id;
    int32_t left_result;
    int32_t right_result;
    int32_t left_error;
    int32_t right_error;
    uint32_t left_invalid;
    uint32_t right_invalid;
    int32_t peak_straight_skew;
} motion_result_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t batch;
    uint32_t complete;
    motion_result_t results[MOTION_COUNT];
    uint32_t checksum;
} saved_result_t;

_Static_assert(sizeof(saved_result_t) <= FLASH_PAGE_SIZE,
               "Saved motion results exceed one flash page");

typedef struct {
    float integral;
    float command;
    int direction;
    int32_t target;
    bool finished;
} wheel_controller_t;

static motor_t left_motor;
static motor_t right_motor;
static uint16_t motor_wrap;
static encoder_t left_encoder;
static encoder_t right_encoder;
static saved_result_t saved_results;
static bool saved_results_available;

static uint32_t result_checksum(const saved_result_t *result)
{
    uint32_t checksum = result->magic ^ result->version ^ result->count ^
                        result->batch ^ result->complete ^ 0xa55a5aa5u;
    for (uint32_t i = 0u; i < MOTION_COUNT; ++i) {
        const motion_result_t *entry = &result->results[i];
        checksum ^= entry->motion_id ^ (uint32_t)entry->left_result ^
                    (uint32_t)entry->right_result ^
                    (uint32_t)entry->left_error ^
                    (uint32_t)entry->right_error ^
                    entry->left_invalid ^ entry->right_invalid ^
                    (uint32_t)entry->peak_straight_skew;
    }
    return checksum;
}

static const char *motion_name_from_id(uint32_t id)
{
    static const char *names[] = {
        "STRAIGHT 30 CM",
        "LEFT 90 DEGREES",
        "RIGHT 90 DEGREES",
        "RIGHT 180 DEGREES",
    };
    return id < sizeof names / sizeof names[0] ? names[id] : "UNKNOWN";
}

static void save_results(void)
{
    saved_results.checksum = result_checksum(&saved_results);

    uint8_t page[FLASH_PAGE_SIZE];
    memset(page, 0xff, sizeof page);
    memcpy(page, &saved_results, sizeof saved_results);
    uint32_t saved_interrupts = save_and_disable_interrupts();
    flash_range_erase(RESULT_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(RESULT_FLASH_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(saved_interrupts);
}

static void load_results(void)
{
    const saved_result_t *result =
        (const saved_result_t *)(XIP_BASE + RESULT_FLASH_OFFSET);
    if (result->magic != RESULT_MAGIC ||
        result->version != RESULT_VERSION ||
        result->count > MOTION_COUNT ||
        result->batch > 1u || result->complete > 1u ||
        result->checksum != result_checksum(result)) {
        return;
    }
    saved_results = *result;
    saved_results_available = true;
}

static void start_result_collection(bool batch)
{
    memset(&saved_results, 0, sizeof saved_results);
    saved_results.magic = RESULT_MAGIC;
    saved_results.version = RESULT_VERSION;
    saved_results.batch = batch ? 1u : 0u;
    saved_results_available = true;
    save_results();
}

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
    const uint32_t edges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
    gpio_set_irq_enabled_with_callback(LEFT_ENCODER_A_PIN, edges, true,
                                       encoder_edge_callback);
    gpio_set_irq_enabled(LEFT_ENCODER_B_PIN, edges, true);
    gpio_set_irq_enabled(RIGHT_ENCODER_A_PIN, edges, true);
    gpio_set_irq_enabled(RIGHT_ENCODER_B_PIN, edges, true);
}

static int32_t normalized_left(encoder_snapshot_t snapshot)
{
    return -snapshot.position;
}

static int32_t normalized_right(encoder_snapshot_t snapshot)
{
    return snapshot.position;
}

static void motor_stop(motor_t *motor)
{
    pwm_set_chan_level(motor->slice, motor->channel_a, 0u);
    pwm_set_chan_level(motor->slice, motor->channel_b, 0u);
}

static void motor_set(motor_t *motor, float throttle)
{
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

static void motor_init_one(motor_t *motor, uint pin_a, uint pin_b)
{
    gpio_set_function(pin_a, GPIO_FUNC_PWM);
    gpio_set_function(pin_b, GPIO_FUNC_PWM);
    motor->slice = pwm_gpio_to_slice_num(pin_a);
    motor->channel_a = pwm_gpio_to_channel(pin_a);
    motor->channel_b = pwm_gpio_to_channel(pin_b);
    pwm_set_clkdiv(motor->slice, 1.0f);
    pwm_set_wrap(motor->slice, motor_wrap);
    motor_stop(motor);
    pwm_set_enabled(motor->slice, true);
}

static void motors_init(void)
{
    motor_wrap = (uint16_t)(clock_get_hz(clk_sys) / 20000u - 1u);
    motor_init_one(&left_motor, LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN);
    motor_init_one(&right_motor, RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN);
}

static void stop_both_motors(void)
{
    motor_stop(&left_motor);
    motor_stop(&right_motor);
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

static int32_t absolute_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static int32_t millimetres_to_counts(float millimetres)
{
    float circumference_mm = PI_VALUE * WHEEL_DIAMETER_MM;
    return (int32_t)(millimetres * (float)COUNTS_PER_REVOLUTION /
                     circumference_mm + 0.5f);
}

static int32_t turn_to_counts(float degrees)
{
    float wheel_path_mm = PI_VALUE * TRACK_WIDTH_MM * degrees / 360.0f;
    int32_t geometric_counts = millimetres_to_counts(wheel_path_mm);
    return geometric_counts > TURN_STOP_EARLY_COUNTS
               ? geometric_counts - TURN_STOP_EARLY_COUNTS
               : 0;
}

static bool debounced_press(uint pin)
{
    if (gpio_get(pin)) {
        return false;
    }
    sleep_ms(20u);
    return !gpio_get(pin);
}

static float desired_speed_for_remaining(int32_t remaining)
{
    if (remaining > 2200) {
        return 1800.0f;
    }
    if (remaining > 700) {
        return 1200.0f;
    }
    return 600.0f;
}

static float feedforward_for_speed(float target_cps, bool right_wheel)
{
    float feedforward = 0.30f + 0.000055f * target_cps;
    if (right_wheel) {
        feedforward *= 0.95f;
    }
    return feedforward;
}

static void update_wheel(wheel_controller_t *controller, motor_t *motor,
                         int32_t progress, float measured_cps, float dt,
                         bool right_wheel, float speed_adjustment_cps)
{
    int32_t remaining = absolute_i32(controller->target) - progress;
    if (remaining <= 0) {
        controller->finished = true;
        controller->command = 0.0f;
        motor_stop(motor);
        return;
    }

    float target_cps = desired_speed_for_remaining(remaining) +
                       speed_adjustment_cps;
    float error = target_cps - measured_cps;
    controller->integral = clamp_float(controller->integral + error * dt,
                                       -2500.0f, 2500.0f);
    float command = feedforward_for_speed(target_cps, right_wheel) +
                    SPEED_KP * error + SPEED_KI * controller->integral;
    command = clamp_float(command, 0.25f, 0.65f);
    controller->command = command * (float)controller->direction;
    motor_set(motor, controller->command);
}

static bool run_motion(const motion_t *motion)
{
    stop_both_motors();
    while (!gpio_get(START_STOP_BUTTON_PIN)) {
        sleep_ms(10u);
    }

    printf("\n%s armed. Car moves in 3 seconds. GP20 aborts.\n",
           motion->name);
    for (uint countdown = 3u; countdown > 0u; --countdown) {
        printf("%u...\n", countdown);
        uint64_t deadline = time_us_64() + 1000000u;
        while (time_us_64() < deadline) {
            if (debounced_press(START_STOP_BUTTON_PIN)) {
                printf("Cancelled before movement.\n");
                while (!gpio_get(START_STOP_BUTTON_PIN)) {
                    sleep_ms(10u);
                }
                return false;
            }
            sleep_ms(10u);
        }
    }

    encoder_snapshot_t start_left = encoder_snapshot(&left_encoder);
    encoder_snapshot_t start_right = encoder_snapshot(&right_encoder);
    encoder_snapshot_t previous_left = start_left;
    encoder_snapshot_t previous_right = start_right;
    wheel_controller_t left = {
        .direction = motion->left_target < 0 ? -1 : 1,
        .target = motion->left_target,
    };
    wheel_controller_t right = {
        .direction = motion->right_target < 0 ? -1 : 1,
        .target = motion->right_target,
    };
    uint64_t start_us = time_us_64();
    uint64_t previous_control_us = start_us;
    uint64_t previous_report_us = start_us;
    float left_cps = 0.0f;
    float right_cps = 0.0f;
    int32_t peak_straight_skew = 0;

    printf("GO: target L=%ld R=%ld counts\n",
           (long)motion->left_target, (long)motion->right_target);

    while (!(left.finished && right.finished)) {
        if (debounced_press(START_STOP_BUTTON_PIN)) {
            stop_both_motors();
            printf("ABORTED BY GP20: both motors stopped.\n");
            while (!gpio_get(START_STOP_BUTTON_PIN)) {
                sleep_ms(10u);
            }
            return false;
        }

        uint64_t now_us = time_us_64();
        if ((now_us - start_us) / 1000u >= MOTION_TIMEOUT_MS) {
            stop_both_motors();
            printf("TIMEOUT: both motors stopped.\n");
            return false;
        }

        if ((now_us - previous_control_us) / 1000u >= CONTROL_INTERVAL_MS) {
            encoder_snapshot_t current_left = encoder_snapshot(&left_encoder);
            encoder_snapshot_t current_right = encoder_snapshot(&right_encoder);
            float dt = (float)(now_us - previous_control_us) / 1000000.0f;
            int32_t left_position = normalized_left(current_left) -
                                    normalized_left(start_left);
            int32_t right_position = normalized_right(current_right) -
                                     normalized_right(start_right);
            int32_t left_delta = normalized_left(current_left) -
                                 normalized_left(previous_left);
            int32_t right_delta = normalized_right(current_right) -
                                  normalized_right(previous_right);
            left_cps = (float)(left_delta * left.direction) / dt;
            right_cps = (float)(right_delta * right.direction) / dt;
            int32_t left_progress = left_position * left.direction;
            int32_t right_progress = right_position * right.direction;
            float straight_sync_cps = 0.0f;
            if (motion->id == 0u && !left.finished && !right.finished) {
                int32_t skew = left_position - right_position;
                if (absolute_i32(skew) > absolute_i32(peak_straight_skew)) {
                    peak_straight_skew = skew;
                }
                straight_sync_cps = clamp_float(
                    (float)skew * STRAIGHT_SYNC_CPS_PER_COUNT,
                    -STRAIGHT_SYNC_LIMIT_CPS, STRAIGHT_SYNC_LIMIT_CPS);
            }

            if (!left.finished) {
                update_wheel(&left, &left_motor, left_progress, left_cps, dt,
                             false, -straight_sync_cps);
            }
            if (!right.finished) {
                update_wheel(&right, &right_motor, right_progress, right_cps,
                             dt, true, straight_sync_cps);
            }

            previous_left = current_left;
            previous_right = current_right;
            previous_control_us = now_us;

            if ((now_us - previous_report_us) / 1000u >= REPORT_INTERVAL_MS) {
                printf("L=%ld/%ld cps=%+.0f pwm=%+.1f%% | "
                       "R=%ld/%ld cps=%+.0f pwm=%+.1f%%\n",
                       (long)left_position, (long)motion->left_target,
                       (double)left_cps, (double)(left.command * 100.0f),
                       (long)right_position, (long)motion->right_target,
                       (double)right_cps, (double)(right.command * 100.0f));
                previous_report_us = now_us;
            }
        }
        sleep_ms(5u);
    }

    stop_both_motors();
    sleep_ms(500u);
    encoder_snapshot_t end_left = encoder_snapshot(&left_encoder);
    encoder_snapshot_t end_right = encoder_snapshot(&right_encoder);
    int32_t left_result = normalized_left(end_left) - normalized_left(start_left);
    int32_t right_result = normalized_right(end_right) -
                           normalized_right(start_right);
    motion_result_t result = {
        .motion_id = motion->id,
        .left_result = left_result,
        .right_result = right_result,
        .left_error = left_result - motion->left_target,
        .right_error = right_result - motion->right_target,
        .left_invalid = end_left.invalid_transitions -
                        start_left.invalid_transitions,
        .right_invalid = end_right.invalid_transitions -
                         start_right.invalid_transitions,
        .peak_straight_skew = peak_straight_skew,
    };
    if (saved_results.count < MOTION_COUNT) {
        saved_results.results[saved_results.count++] = result;
        save_results();
    }
    printf("COMPLETE %s: L=%ld error=%ld R=%ld error=%ld invalid=%lu/%lu\n",
           motion->name, (long)result.left_result, (long)result.left_error,
           (long)result.right_result, (long)result.right_error,
           (unsigned long)result.left_invalid,
           (unsigned long)result.right_invalid);
    return true;
}

static void print_saved_results(void)
{
    if (!saved_results_available) {
        return;
    }
    printf("\nSAVED %s RESULTS: %lu/%lu %s\n",
           saved_results.batch ? "BATCH" : "SINGLE",
           (unsigned long)saved_results.count,
           (unsigned long)(saved_results.batch ? MOTION_COUNT : 1u),
           saved_results.complete ? "COMPLETE" : "INCOMPLETE");
    for (uint32_t i = 0u; i < saved_results.count; ++i) {
        const motion_result_t *result = &saved_results.results[i];
        printf("%lu. %s: L=%ld error=%ld R=%ld error=%ld "
               "invalid=%lu/%lu peak_skew=%ld\n",
               (unsigned long)(i + 1u),
               motion_name_from_id(result->motion_id),
               (long)result->left_result, (long)result->left_error,
               (long)result->right_result, (long)result->right_error,
               (unsigned long)result->left_invalid,
               (unsigned long)result->right_invalid,
               (long)result->peak_straight_skew);
    }
}

static void print_selection(const motion_t *motions, uint selection)
{
    if (selection == 0u) {
        printf("\nSelected: ALL FOUR TESTS (straight, right 90, left 90, right 180).\n");
    } else {
        const motion_t *motion = &motions[selection - 1u];
        printf("\nSelected: %s (targets L=%ld R=%ld).\n",
               motion->name,
               (long)motion->left_target, (long)motion->right_target);
    }
    printf("GP20=start/abort, GP21=select next while stopped.\n");
}

static bool pause_between_stages(void)
{
    printf("Motors stopped. Next stage in 1 second; GP20 aborts.\n");
    uint64_t deadline = time_us_64() + 1000000u;
    while (time_us_64() < deadline) {
        if (debounced_press(START_STOP_BUTTON_PIN)) {
            while (!gpio_get(START_STOP_BUTTON_PIN)) {
                sleep_ms(10u);
            }
            return false;
        }
        sleep_ms(10u);
    }
    return true;
}

static void run_selection(const motion_t *motions, uint selection)
{
    start_result_collection(selection == 0u);
    if (selection == 0u) {
        const uint order[MOTION_COUNT] = {0u, 2u, 1u, 3u};
        for (uint i = 0u; i < MOTION_COUNT; ++i) {
            printf("\nBATCH STAGE %u/%u: %s\n", i + 1u,
                   MOTION_COUNT, motions[order[i]].name);
            if (!run_motion(&motions[order[i]])) {
                printf("BATCH STOPPED after %lu completed stages.\n",
                       (unsigned long)saved_results.count);
                print_saved_results();
                return;
            }
            if (i + 1u < MOTION_COUNT && !pause_between_stages()) {
                printf("BATCH STOPPED after %lu completed stages.\n",
                       (unsigned long)saved_results.count);
                print_saved_results();
                return;
            }
        }
    } else if (!run_motion(&motions[selection - 1u])) {
        print_saved_results();
        return;
    }
    saved_results.complete = 1u;
    save_results();
    print_saved_results();
}

int main(void)
{
    motors_init();
    stop_both_motors();
    encoders_init();

    gpio_init(START_STOP_BUTTON_PIN);
    gpio_set_dir(START_STOP_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_STOP_BUTTON_PIN);
    gpio_init(SELECT_BUTTON_PIN);
    gpio_set_dir(SELECT_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(SELECT_BUTTON_PIN);

    stdio_init_all();
    sleep_ms(1500u);

    int32_t straight_counts = millimetres_to_counts(300.0f);
    int32_t turn_90_counts = turn_to_counts(90.0f);
    int32_t turn_180_counts = turn_to_counts(180.0f);
    const motion_t motions[] = {
        {0u, "STRAIGHT 30 CM", straight_counts, straight_counts},
        {1u, "LEFT 90 DEGREES", -(turn_90_counts + LEFT_90_EXTRA_COUNTS),
         turn_90_counts + LEFT_90_EXTRA_COUNTS},
        {2u, "RIGHT 90 DEGREES", turn_90_counts, -turn_90_counts},
        {3u, "RIGHT 180 DEGREES", turn_180_counts - RIGHT_180_SHORTEN_COUNTS,
         -(turn_180_counts - RIGHT_180_SHORTEN_COUNTS)},
    };
    _Static_assert(sizeof motions / sizeof motions[0] == MOTION_COUNT,
                   "Motion table and saved result count differ");
    uint selection = 0u;

    printf("\nINF2004 BUDDY 2 GROUND MOTION TEST (BATCH 5)\n");
    printf("Wheel=%.1f mm, track=%.1f mm, CPR=%u, counts/cm=%.2f\n",
           (double)WHEEL_DIAMETER_MM, (double)TRACK_WIDTH_MM,
           COUNTS_PER_REVOLUTION,
           (double)((float)COUNTS_PER_REVOLUTION /
                    (PI_VALUE * WHEEL_DIAMETER_MM / 10.0f)));
    printf("Clear the floor and keep a hand near GP20.\n");
    printf("Turn calibration stops %ld counts before geometric targets.\n",
           (long)TURN_STOP_EARLY_COUNTS);
    printf("Straight wheel sync active; left 90 extra=%ld counts; "
           "right 180 shorter by %ld counts.\n",
           (long)LEFT_90_EXTRA_COUNTS,
           (long)RIGHT_180_SHORTEN_COUNTS);
    load_results();
    if (saved_results_available) {
        printf("Recovered results from flash:\n");
        print_saved_results();
    }
    print_selection(motions, selection);

    bool previous_start = gpio_get(START_STOP_BUTTON_PIN);
    bool previous_select = gpio_get(SELECT_BUTTON_PIN);
    uint64_t last_repeat_us = time_us_64();
    while (true) {
        bool current_start = gpio_get(START_STOP_BUTTON_PIN);
        bool current_select = gpio_get(SELECT_BUTTON_PIN);
        if (previous_start && !current_start &&
            debounced_press(START_STOP_BUTTON_PIN)) {
            run_selection(motions, selection);
        }
        if (previous_select && !current_select &&
            debounced_press(SELECT_BUTTON_PIN)) {
            selection = (selection + 1u) % (MOTION_COUNT + 1u);
            print_selection(motions, selection);
        }
        previous_start = gpio_get(START_STOP_BUTTON_PIN);
        previous_select = gpio_get(SELECT_BUTTON_PIN);
        uint64_t now_us = time_us_64();
        if (saved_results_available &&
            (now_us - last_repeat_us) >= 3000000u) {
            print_saved_results();
            last_repeat_us = now_us;
        }
        sleep_ms(10u);
    }
}
