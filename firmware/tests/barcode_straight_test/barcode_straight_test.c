/*
 * Barcode straight-drive test (no line following).
 *
 * Drives straight for a set distance (default 300 mm) at the course scanning
 * speed (600 counts/s per wheel, about 44 mm/s) while the offset GP26 sensor
 * reads a Code 39 barcode placed beside the path. Uses the same sampler,
 * 12-sample filter, 226/220 thresholds, reader and decoder as the course
 * firmware. Line sensors, sonar, servo, IMU and Wi-Fi are not used.
 *
 * Wiring:
 *   Motor 1 (left):  M1A GP8,  M1B GP9
 *   Motor 2 (right): M2A GP10, M2B GP11
 *   Encoders: left GP2/GP3, right GP16/GP17
 *   Barcode sensor AO: GP26 via Grove 5
 *   GP20 button: start (after a 3 s countdown) / abort
 *
 * USB serial: 3/4/5 select 300/400/500 mm, x aborts, r reprints the last
 * result, ? prints help. Results are printed when each run ends.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include "barcode_acquisition.h"
#include "barcode_optical.h"
#include "barcode_signal.h"
#include "encoders.h"
#include "ir_fault.h"
#include "motion_control.h"
#include "motor_guard.h"
#include "navigation_commands.h"

enum {
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    START_BUTTON_PIN = 20,
    BARCODE_IR_PIN = 26,
};

enum {
    ADC_AVERAGE_COUNT = 8,
    DEFAULT_DISTANCE_MM = 300,
    COUNTDOWN_US = 3000000,
    SETTLE_US = 300000,
    BUTTON_DEBOUNCE_US = 25000,
    STATUS_INTERVAL_US = 500000,
    WHEEL_CPR = 2543,
    OPTICAL_BINS_PER_LINE = 25,
};

static const float WHEEL_DIAMETER_MM = 60.0f;
static const float PI_VALUE = 3.14159265f;

typedef enum {
    RUN_IDLE,
    RUN_COUNTDOWN,
    RUN_DRIVING,
    RUN_SETTLING
} run_state_t;

typedef struct {
    uint slice;
    uint channel_a;
    uint channel_b;
} motor_t;

typedef struct {
    char sequence[BARCODE_STREAM_EVENTS + 1u];
    uint32_t reads;
    uint32_t rejected;
    uint32_t timeouts;
    uint32_t edges;
    uint32_t max_gap_us;
    int32_t left_counts;
    int32_t right_counts;
    uint16_t raw_min;
    uint16_t raw_max;
    uint16_t distance_mm;
    const char *p_end_reason;
    bool integrity_fault;
    bool valid;
} run_summary_t;

/* Motor outputs: written by the main loop and the guard alarm only. */
static motor_t g_left_motor;
static motor_t g_right_motor;
static uint16_t g_motor_wrap;
static volatile motor_guard_t g_guard;
static repeating_timer_t g_guard_timer;
static bool g_guard_ready;

/* The sampling alarm owns the ADC, filter and sampler. */
static repeating_timer_t g_sample_timer;
static bool g_sample_ready;
static volatile uint16_t g_raw_latest;
static volatile uint32_t g_sample_us;
static volatile uint32_t g_adc_max_us;
static barcode_signal_t g_signal;
static barcode_sampler_t g_sampler;
static barcode_optical_t g_optical;
static barcode_batch_t g_batch;
static barcode_capture_t g_capture;
static uint32_t g_origin_us;
static uint32_t g_events_logged;
static uint32_t g_rejected_logged;
static uint32_t g_timeouts_logged;

static uint16_t g_distance_mm = DEFAULT_DISTANCE_MM;
static run_summary_t g_last;

static void motor_stop(const motor_t *p_motor)
{
    pwm_set_chan_level(p_motor->slice, p_motor->channel_a, 0u);
    pwm_set_chan_level(p_motor->slice, p_motor->channel_b, 0u);
}

static void motors_stop(void)
{
    g_guard.armed = false;
    motor_stop(&g_left_motor);
    motor_stop(&g_right_motor);
}

static void motor_set(const motor_t *p_motor, float throttle)
{
    if (throttle > 1.0f) {
        throttle = 1.0f;
    } else if (throttle < -1.0f) {
        throttle = -1.0f;
    }

    float magnitude = (throttle < 0.0f) ? -throttle : throttle;
    uint16_t level = (uint16_t)((float)(g_motor_wrap + 1u) * magnitude);

    if (throttle > 0.0f) {
        pwm_set_chan_level(p_motor->slice, p_motor->channel_a, level);
        pwm_set_chan_level(p_motor->slice, p_motor->channel_b, 0u);
    } else if (throttle < 0.0f) {
        pwm_set_chan_level(p_motor->slice, p_motor->channel_a, 0u);
        pwm_set_chan_level(p_motor->slice, p_motor->channel_b, level);
    } else {
        motor_stop(p_motor);
    }
}

/* Removes drive if the main loop stops refreshing it. Never prints. */
static bool guard_alarm(repeating_timer_t *p_timer)
{
    (void)p_timer;
    if (motor_guard_tick(&g_guard, time_us_32())) {
        motors_stop();
    }
    return true;
}

static void motors_drive(float left, float right)
{
    uint32_t irq = save_and_disable_interrupts();
    bool enabled = (left != 0.0f) || (right != 0.0f);

    if (!g_guard_ready ||
        !motor_guard_refresh(&g_guard, time_us_32(), enabled)) {
        motors_stop();
    } else {
        motor_set(&g_left_motor, left);
        motor_set(&g_right_motor, right);
    }
    restore_interrupts(irq);
}

static void motor_init_one(motor_t *p_motor, uint pin_a, uint pin_b)
{
    gpio_set_function(pin_a, GPIO_FUNC_PWM);
    gpio_set_function(pin_b, GPIO_FUNC_PWM);
    p_motor->slice = pwm_gpio_to_slice_num(pin_a);
    p_motor->channel_a = pwm_gpio_to_channel(pin_a);
    p_motor->channel_b = pwm_gpio_to_channel(pin_b);
    pwm_set_clkdiv(p_motor->slice, 1.0f);
    pwm_set_wrap(p_motor->slice, g_motor_wrap);
    motor_stop(p_motor);
    pwm_set_enabled(p_motor->slice, true);
}

static void motors_init(void)
{
    g_motor_wrap = (uint16_t)(clock_get_hz(clk_sys) / 20000u - 1u);
    motor_init_one(&g_left_motor, LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN);
    motor_init_one(&g_right_motor, RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN);
}

/* Same averaging as the course firmware, so the thresholds mean the same. */
static uint16_t barcode_ir_read(void)
{
    uint32_t total = 0u;

    adc_select_input(BARCODE_IR_PIN - 26u);
    busy_wait_us_32(5u);
    for (uint32_t i = 0u; i < ADC_AVERAGE_COUNT; ++i) {
        total += adc_read();
    }
    return (uint16_t)(total / ADC_AVERAGE_COUNT);
}

/* 2 kHz sampling alarm. Never prints or decodes. */
static bool sample_alarm(repeating_timer_t *p_timer)
{
    (void)p_timer;
    uint32_t start_us = time_us_32();
    uint16_t raw = barcode_ir_read();
    uint16_t filtered = barcode_signal_update(&g_signal, raw);

    g_raw_latest = raw;
    g_sample_us = start_us;
    barcode_sampler_sample(&g_sampler, filtered, start_us);
    if (g_optical.armed && g_sampler.enabled && (g_sampler.stats.edges > 0u)) {
        barcode_optical_trigger(&g_optical, start_us);
    }
    barcode_optical_sample(&g_optical, raw, start_us);

    uint32_t duration_us = time_us_32() - start_us;
    if (g_sampler.enabled && (duration_us > g_adc_max_us)) {
        g_adc_max_us = duration_us;
    }
    return true;
}

static float counts_to_mm(int32_t counts)
{
    return (float)counts * PI_VALUE * WHEEL_DIAMETER_MM / (float)WHEEL_CPR;
}

static void print_widths(void)
{
    for (uint32_t i = 0u; i < g_capture.saved_runs; ++i) {
        printf("%s%lu", (i > 0u) ? "," : "",
               (unsigned long)barcode_runs_get(&g_capture.last_runs, i));
    }
}

static void report_new_events(void)
{
    while (g_events_logged < g_capture.event_count) {
        char letter = g_capture.events[g_events_logged].letter;
        navigation_command_t command = navigation_decode_symbol(letter);

        printf("[BARCODE EVENT] seq=%lu letter=%c value=%u command=%s at_ms=%lu\n",
               (unsigned long)(g_events_logged + 1u), letter,
               (unsigned)navigation_barcode_value(letter, false),
               navigation_command_name(command),
               (unsigned long)(g_capture.events[g_events_logged].us / 1000u));
        ++g_events_logged;
    }

    if ((g_rejected_logged != g_capture.rejected) ||
        (g_timeouts_logged != g_capture.timeouts)) {
        bool new_reject = (g_rejected_logged != g_capture.rejected);

        printf("[BARCODE RETRY] rejected=%lu timeouts=%lu value=%u ir_fault=0x%02x completed_runs=%u widths_us=",
               (unsigned long)g_capture.rejected,
               (unsigned long)g_capture.timeouts,
               (unsigned)navigation_barcode_value('\0', new_reject),
               (unsigned)ir_fault_bits(&g_capture),
               (unsigned)g_capture.saved_runs);
        print_widths();
        printf("\n");
        g_rejected_logged = g_capture.rejected;
        g_timeouts_logged = g_capture.timeouts;
    }
}

static void barcode_drain(bool end)
{
    uint32_t irq = save_and_disable_interrupts();
    if (end) {
        g_sampler.enabled = false;
    }
    barcode_sampler_take(&g_sampler, &g_batch);
    restore_interrupts(irq);

    barcode_capture_consume(&g_capture, &g_batch);
    report_new_events();
}

static bool barcode_begin(void)
{
    uint32_t irq = save_and_disable_interrupts();
    bool fresh = (uint32_t)(time_us_32() - g_sample_us) <= BARCODE_SAMPLE_GAP_US;
    uint16_t raw = g_raw_latest;

    if (fresh) {
        g_origin_us = time_us_32();
        g_signal = (barcode_signal_t){0};
        barcode_sampler_begin_levels(&g_sampler, BARCODE_DRIVE_BLACK_ON,
                                     BARCODE_DRIVE_BLACK_OFF);
        barcode_optical_arm(&g_optical);
        g_adc_max_us = 0u;
    }
    restore_interrupts(irq);

    if (!fresh) {
        printf("[STOP] GP26 sampling is not running; run not started.\n");
        return false;
    }

    /* Same capture mode as the course run: time widths, strict framed
     * letters, sliding search, and 250 ms of white before reading starts. */
    barcode_capture_begin_stream(&g_capture, g_origin_us);
    g_capture.distance_mode = false;
    g_capture.independent_optical = true;
    g_capture.waiting_gap = true;
    g_events_logged = 0u;
    g_rejected_logged = 0u;
    g_timeouts_logged = 0u;

    if (raw > BARCODE_DRIVE_BLACK_OFF) {
        printf("[WARN] GP26 is not on white (raw=%u). Reading starts after 250 ms of white.\n",
               (unsigned)raw);
    }
    return true;
}

static void print_summary(const run_summary_t *p_run)
{
    if (!p_run->valid) {
        printf("[RESULT] No completed run yet.\n");
        return;
    }
    printf("[RESULT] reads=%lu sequence=%s rejected=%lu timeouts=%lu edges=%lu "
           "raw=%u..%u max_gap_us=%lu integrity_fault=%u end=%s target_mm=%u "
           "counts_L/R=%ld/%ld nominal_mm=%.0f\n",
           (unsigned long)p_run->reads,
           (p_run->reads > 0u) ? p_run->sequence : "-",
           (unsigned long)p_run->rejected, (unsigned long)p_run->timeouts,
           (unsigned long)p_run->edges, (unsigned)p_run->raw_min,
           (unsigned)p_run->raw_max, (unsigned long)p_run->max_gap_us,
           p_run->integrity_fault ? 1u : 0u, p_run->p_end_reason,
           (unsigned)p_run->distance_mm, (long)p_run->left_counts,
           (long)p_run->right_counts,
           (double)counts_to_mm((p_run->left_counts + p_run->right_counts) / 2));
}

/* Raw GP26 min/max per 10 ms window from the first edge, for later analysis. */
static void print_optical_bins(void)
{
    uint32_t irq = save_and_disable_interrupts();
    if (g_optical.active) {
        barcode_optical_end(&g_optical, time_us_32());
    }
    g_optical.armed = false;
    restore_interrupts(irq);

    printf("[OPTICAL RESULT] samples=%lu max_gap_us=%lu window_ms=10\n",
           (unsigned long)g_optical.samples,
           (unsigned long)g_optical.max_gap_us);
    for (uint32_t i = 0u; i < OPTICAL_BINS; i += OPTICAL_BINS_PER_LINE) {
        printf("[OPTICAL DATA] bin=%lu", (unsigned long)i);
        for (uint32_t j = i; j < (i + OPTICAL_BINS_PER_LINE); ++j) {
            if (g_optical.low[j] == UINT16_MAX) {
                printf(" -");
            } else {
                printf(" %u:%u", (unsigned)g_optical.low[j],
                       (unsigned)g_optical.high[j]);
            }
        }
        printf("\n");
    }
    printf("[OPTICAL END]\n");
}

static void finish_run(const motion_control_t *p_motion, const char *p_reason)
{
    encoder_sample_t sample = encoders_snapshot();
    const barcode_sample_stats_t *p_stats = &g_capture.stats;
    run_summary_t run = {0};

    run.reads = g_capture.event_count;
    for (uint32_t i = 0u; (i < g_capture.event_count) &&
                          (i < BARCODE_STREAM_EVENTS); ++i) {
        run.sequence[i] = g_capture.events[i].letter;
    }
    run.rejected = g_capture.rejected;
    run.timeouts = g_capture.timeouts;
    run.edges = p_stats->edges;
    run.raw_min = (p_stats->samples > 0u) ? p_stats->raw_min : 0u;
    run.raw_max = p_stats->raw_max;
    run.max_gap_us = p_stats->max_gap_us;
    run.integrity_fault = g_capture.integrity_fault;
    run.distance_mm = g_distance_mm;
    run.left_counts = motion_left_position(p_motion, sample);
    run.right_counts = motion_right_position(p_motion, sample);
    run.p_end_reason = p_reason;
    run.valid = true;
    g_last = run;

    print_summary(&g_last);
    printf("[LAST WIDTHS] runs=%u widths_us=", (unsigned)g_capture.saved_runs);
    print_widths();
    printf(" adc_max_us=%lu\n", (unsigned long)g_adc_max_us);
    print_optical_bins();
    printf("Ready. Reposition the car and press GP20 for another run.\n");
}

static void print_help(void)
{
    printf("\n=== BARCODE STRAIGHT TEST (no line following) ===\n");
    printf("Drives straight %u mm at ~44 mm/s; GP26 reads the barcode beside the path.\n",
           (unsigned)g_distance_mm);
    printf("Place GP26 on white at least 30 mm before the first bar, with white after the last bar.\n");
    printf("GP20: start after 3 s countdown / abort. Serial: 3/4/5 = 300/400/500 mm, x = abort, r = last result, ? = help.\n");
    printf("Thresholds: black>=%u white<=%u, %u-sample mean at %u us.\n\n",
           (unsigned)BARCODE_DRIVE_BLACK_ON, (unsigned)BARCODE_DRIVE_BLACK_OFF,
           (unsigned)BARCODE_FILTER_SAMPLES, (unsigned)BARCODE_SAMPLE_US);
}

/* Debounced falling edge on an active-low button. */
static bool button_pressed(uint64_t now_us)
{
    static bool s_stable = true;
    static bool s_candidate = true;
    static uint64_t s_changed_us = 0u;
    bool level = gpio_get(START_BUTTON_PIN);
    bool pressed = false;

    if (level != s_candidate) {
        s_candidate = level;
        s_changed_us = now_us;
    } else if ((level != s_stable) &&
               ((now_us - s_changed_us) >= BUTTON_DEBOUNCE_US)) {
        s_stable = level;
        pressed = !level;
    }
    return pressed;
}

int main(void)
{
    motors_init();
    g_guard_ready = add_repeating_timer_ms(-10, guard_alarm, NULL,
                                           &g_guard_timer);
    stdio_init_all();
    encoders_init();
    adc_init();
    adc_gpio_init(BARCODE_IR_PIN);
    gpio_init(START_BUTTON_PIN);
    gpio_set_dir(START_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_BUTTON_PIN);

    g_sample_ready = add_repeating_timer_us(-(int64_t)BARCODE_SAMPLE_US,
                                            sample_alarm, NULL,
                                            &g_sample_timer);
    sleep_ms(2000u);
    print_help();

    motion_control_t motion;
    motion_init(&motion);
    run_state_t state = RUN_IDLE;
    uint64_t deadline_us = 0u;
    uint64_t next_status_us = 0u;
    uint32_t last_countdown = 0u;
    const char *p_end_reason = "DISTANCE_REACHED";
    bool usb_was_connected = false;

    while (true) {
        uint64_t now_us = time_us_64();
        bool pressed = button_pressed(now_us);
        bool abort_requested = false;

        bool usb_connected = stdio_usb_connected();
        if (usb_connected && !usb_was_connected) {
            print_help();
            print_summary(&g_last);
        }
        usb_was_connected = usb_connected;

        for (uint32_t i = 0u; i < 8u; ++i) {
            int ch = getchar_timeout_us(0u);
            if (ch == PICO_ERROR_TIMEOUT) {
                break;
            }
            if ((ch == 'x') || (ch == 'X')) {
                abort_requested = true;
            } else if ((ch == 'r') || (ch == 'R')) {
                print_summary(&g_last);
            } else if (ch == '?') {
                print_help();
            } else if ((ch >= '3') && (ch <= '5')) {
                if (state == RUN_IDLE) {
                    g_distance_mm = (uint16_t)((ch - '0') * 100);
                    printf("[SELECT] distance=%u mm\n", (unsigned)g_distance_mm);
                } else {
                    printf("[SELECT] Stop the run before changing distance.\n");
                }
            } else {
                /* Other characters are ignored. */
            }
        }

        if (g_guard.tripped && (state == RUN_DRIVING)) {
            motion_abort(&motion);
            p_end_reason = "MOTOR_SERVICE_TIMEOUT";
            state = RUN_SETTLING;
            deadline_us = now_us + SETTLE_US;
        }

        switch (state) {
        case RUN_IDLE:
            motors_stop();
            if (pressed) {
                if (!g_sample_ready || !g_guard_ready) {
                    printf("[STOP] Timers unavailable; cannot start.\n");
                } else {
                    deadline_us = now_us + COUNTDOWN_US;
                    last_countdown = 0u;
                    state = RUN_COUNTDOWN;
                }
            }
            break;

        case RUN_COUNTDOWN:
            motors_stop();
            if (pressed || abort_requested) {
                printf("[ABORT] Countdown cancelled.\n");
                state = RUN_IDLE;
            } else if (now_us >= deadline_us) {
                uint32_t irq = save_and_disable_interrupts();
                motor_guard_reset(&g_guard);
                restore_interrupts(irq);
                if (barcode_begin() &&
                    motion_begin_distance(&motion, g_distance_mm,
                                          encoders_snapshot(), now_us)) {
                    p_end_reason = "DISTANCE_REACHED";
                    next_status_us = now_us;
                    printf("[GO] distance=%u mm target_counts=%ld\n",
                           (unsigned)g_distance_mm, (long)motion.left.target);
                    state = RUN_DRIVING;
                } else {
                    printf("[STOP] Run could not start.\n");
                    state = RUN_IDLE;
                }
            } else {
                uint32_t seconds = (uint32_t)((deadline_us - now_us + 999999u) / 1000000u);
                if (seconds != last_countdown) {
                    printf("[ARM] Starting in %lu s. GP20 cancels.\n",
                           (unsigned long)seconds);
                    last_countdown = seconds;
                }
            }
            break;

        case RUN_DRIVING:
            barcode_drain(false);
            motion_poll(&motion, encoders_snapshot(), now_us);
            if (pressed || abort_requested) {
                motion_abort(&motion);
                p_end_reason = "ABORTED";
            }
            if (motion.phase == MOTION_ACTIVE) {
                motors_drive(motion.left.throttle, motion.right.throttle);
                if (now_us >= next_status_us) {
                    encoder_sample_t sample = encoders_snapshot();
                    printf("[DRIVE] raw=%u edges=%lu letters=%lu travelled_mm=%.0f\n",
                           (unsigned)g_raw_latest,
                           (unsigned long)g_capture.stats.edges,
                           (unsigned long)g_capture.event_count,
                           (double)counts_to_mm(motion_left_position(&motion, sample)));
                    next_status_us = now_us + STATUS_INTERVAL_US;
                }
            } else {
                motors_stop();
                if (motion.phase == MOTION_FAULT) {
                    p_end_reason = motion_fault_name(motion.fault);
                }
                deadline_us = now_us + SETTLE_US;
                state = RUN_SETTLING;
            }
            break;

        case RUN_SETTLING:
            /* Keep reading while the car coasts to a stop. */
            motors_stop();
            barcode_drain(false);
            if (now_us >= deadline_us) {
                barcode_drain(true);
                finish_run(&motion, p_end_reason);
                state = RUN_IDLE;
            }
            break;

        default:
            motors_stop();
            state = RUN_IDLE;
            break;
        }
    }
}
