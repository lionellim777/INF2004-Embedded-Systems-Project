/*
 * INF2004 encoder counts-per-millimetre calibration (hand-pushed, motors OFF).
 *
 * Purpose: measure how many quadrature counts each wheel produces per
 * millimetre of floor travel, so distance-based estimators can convert counts
 * to millimetres with a measured number rather than the nominal 2543 CPR /
 * 60 mm wheel model.
 *
 * Counting uses the application's own encoders.c, so the counts measured here
 * are produced by exactly the code the robot runs.
 *
 *   Left encoder:  Grove 2, GP2/GP3.   Right encoder: Grove 4, GP16/GP17.
 *   Button:        GP20 (arm / latch).
 *
 * All motor-driver inputs and the servo pin are held LOW. Nothing is driven.
 *
 * Procedure:
 *   1. Lay a straight edge or tape along a straight line, mark START and END
 *      exactly MARKED_DISTANCE_MM apart. Mark at the wheel-contact centres
 *      (the drive-axle line), not at the chassis edge.
 *   2. Put the car on START, press GP20 (counting starts from zero).
 *   3. Push the car by hand, straight and slowly, until the same wheel-centre
 *      point reaches END. Do not let it twist or skid.
 *   4. Press GP20 again: counts are latched and the result is printed.
 *   5. Carry the car back to START and repeat (at least 5 runs).
 *
 * If you use a different distance than 1000 mm, change MARKED_DISTANCE_MM
 * below and rebuild. The raw counts are always printed, so you can also divide
 * by hand.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "encoders.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/* EDIT THIS to the distance you marked on the floor, in millimetres. */
static const double MARKED_DISTANCE_MM = 1000.0;

enum {
    MOTOR_1_A_PIN = 8,
    MOTOR_1_B_PIN = 9,
    MOTOR_2_A_PIN = 10,
    MOTOR_2_B_PIN = 11,
    SERVO_SIGNAL_PIN = 12,
    BUTTON_PIN = 20,
    TICK_MS = 10,
    LIVE_REPORT_MS = 250,
    IDLE_REMINDER_MS = 5000,
    MAX_RUNS = 12,
};

/* Documented raw polarity: forward LEFT is negative, forward RIGHT positive
 * (docs/development-status.md, encoder_wiring_test). Applied so that pushing
 * the car forward gives positive numbers for both wheels. */
static const int LEFT_FORWARD_SIGN = -1;
static const int RIGHT_FORWARD_SIGN = 1;

/* Nominal reference only: 2543 counts / (pi * 60 mm). Not a measured result. */
static const double NOMINAL_COUNTS_PER_MM = 2543.0 / (3.14159265358979 * 60.0);

typedef struct {
    int64_t left_forward;
    int64_t right_forward;
    int32_t left_raw;
    int32_t right_raw;
    uint32_t left_invalid;
    uint32_t right_invalid;
} run_delta_t;

static double left_cpm[MAX_RUNS];
static double right_cpm[MAX_RUNS];
static uint stored_runs;
static uint attempted_runs;

static int32_t signed_delta(uint32_t now, uint32_t start)
{
    uint32_t delta = now - start; /* modulo 2^32, valid for |delta| < 2^31 */
    return delta <= (uint32_t)INT32_MAX
               ? (int32_t)delta
               : (int32_t)((int64_t)delta - INT64_C(4294967296));
}

static run_delta_t delta_since(const encoder_sample_t *start)
{
    encoder_sample_t now = encoders_snapshot();
    run_delta_t delta;
    delta.left_raw = signed_delta(now.left_raw, start->left_raw);
    delta.right_raw = signed_delta(now.right_raw, start->right_raw);
    delta.left_forward = (int64_t)LEFT_FORWARD_SIGN * delta.left_raw;
    delta.right_forward = (int64_t)RIGHT_FORWARD_SIGN * delta.right_raw;
    delta.left_invalid = now.left_invalid - start->left_invalid;
    delta.right_invalid = now.right_invalid - start->right_invalid;
    return delta;
}

static void hold_outputs_low(void)
{
    const uint pins[] = {MOTOR_1_A_PIN, MOTOR_1_B_PIN, MOTOR_2_A_PIN,
                         MOTOR_2_B_PIN, SERVO_SIGNAL_PIN};
    for (uint i = 0u; i < sizeof pins / sizeof pins[0]; ++i) {
        gpio_init(pins[i]);
        gpio_put(pins[i], 0);
        gpio_set_dir(pins[i], GPIO_OUT);
    }
}

/* GP20 is active low (pull-up). Returns true once per press, after the level
 * has differed from the stable state for three consecutive 10 ms ticks. */
static bool button_pressed_event(void)
{
    static bool stable_level = true;
    static uint8_t differing_ticks = 0u;
    bool level = gpio_get(BUTTON_PIN);
    if (level == stable_level) {
        differing_ticks = 0u;
        return false;
    }
    if (++differing_ticks < 3u) {
        return false;
    }
    stable_level = level;
    differing_ticks = 0u;
    return !stable_level;
}

static void print_help(void)
{
    printf("\n=== INF2004 ENCODER DISTANCE CALIBRATION | %s %s ===\n",
           __DATE__, __TIME__);
    printf("MOTORS OFF: GP8-GP12 are held LOW. Push the car by hand.\n");
    printf("Marked distance in this build: %.1f mm (edit MARKED_DISTANCE_MM to change).\n",
           MARKED_DISTANCE_MM);
    printf("1. Put the car on START with the wheel-centre (axle) line on the mark.\n");
    printf("2. Press GP20: counting starts from zero.\n");
    printf("3. Push straight and slowly until the axle line reaches END.\n");
    printf("4. Press GP20 again: counts are latched and the result is printed.\n");
    printf("5. Carry the car back to START and repeat. Aim for at least 5 clean runs.\n");
    printf("Counts are shown forward-positive (left raw is negated, right is not).\n");
    printf("The hump goes under the RIGHT wheel only, so the RIGHT counts/mm is the value\n");
    printf("the hump estimator needs; the left value is a cross-check on the same push.\n");
    printf("Negative totals mean you pushed backwards or the polarity differs.\n");
    printf("Reference only, NOT a measurement: nominal %.3f counts/mm (2543 CPR, 60 mm wheel).\n",
           NOMINAL_COUNTS_PER_MM);
    printf("Earlier motor-driven runs implied 12.10 and 12.60 counts/mm (one run each,\n");
    printf("docs/testing/motion-integration-distance-v2/v3.md); they disagree with each\n");
    printf("other and with nominal, which is why a repeated measurement is needed.\n\n");
}

static void print_summary(void)
{
    if (stored_runs == 0u) {
        return;
    }
    double sum_l = 0.0;
    double sum_r = 0.0;
    double min_l = left_cpm[0];
    double max_l = left_cpm[0];
    double min_r = right_cpm[0];
    double max_r = right_cpm[0];
    printf("--- Stored clean runs (%u) ---\n", stored_runs);
    for (uint i = 0u; i < stored_runs; ++i) {
        printf("  run %2u: left %.4f  right %.4f counts/mm\n", i + 1u,
               left_cpm[i], right_cpm[i]);
        sum_l += left_cpm[i];
        sum_r += right_cpm[i];
        if (left_cpm[i] < min_l) min_l = left_cpm[i];
        if (left_cpm[i] > max_l) max_l = left_cpm[i];
        if (right_cpm[i] < min_r) min_r = right_cpm[i];
        if (right_cpm[i] > max_r) max_r = right_cpm[i];
    }
    double mean_l = sum_l / (double)stored_runs;
    double mean_r = sum_r / (double)stored_runs;
    printf("  MEAN   left %.4f (%.5f mm/count)  right %.4f (%.5f mm/count)\n",
           mean_l, 1.0 / mean_l, mean_r, 1.0 / mean_r);
    printf("  SPREAD left %.4f..%.4f (%.2f%% of mean)  right %.4f..%.4f (%.2f%% of mean)\n",
           min_l, max_l, 100.0 * (max_l - min_l) / mean_l,
           min_r, max_r, 100.0 * (max_r - min_r) / mean_r);
    printf("  Both-wheel mean: %.4f counts/mm. Left/right mean differ by %.2f%%.\n",
           (mean_l + mean_r) * 0.5,
           100.0 * (mean_l - mean_r) / ((mean_l + mean_r) * 0.5));
    printf("  USE FOR THE HUMP (right wheel): %.4f counts/mm = %.5f mm/count.\n",
           mean_r, 1.0 / mean_r);
    if (stored_runs < 5u) {
        printf("  NOTE: fewer than 5 clean runs; repeatability not yet established.\n");
    }
    printf("-------------------------------\n");
}

static void latch_and_report(const encoder_sample_t *start)
{
    run_delta_t d = delta_since(start);
    ++attempted_runs;
    printf("\n=== RUN %u LATCHED (marked distance %.1f mm) ===\n",
           attempted_runs, MARKED_DISTANCE_MM);
    printf("Forward-positive counts: LEFT %lld  RIGHT %lld  (raw signed: %ld / %ld)\n",
           (long long)d.left_forward, (long long)d.right_forward,
           (long)d.left_raw, (long)d.right_raw);
    printf("Invalid transitions this run: LEFT %lu  RIGHT %lu\n",
           (unsigned long)d.left_invalid, (unsigned long)d.right_invalid);

    bool clean = true;
    if (d.left_forward <= 0 || d.right_forward <= 0) {
        printf("REJECTED: a wheel total is zero or negative. Pushed backwards, "
               "wheel did not turn, or polarity differs from the documented one.\n");
        clean = false;
    }
    if (d.left_invalid != 0u || d.right_invalid != 0u) {
        printf("REJECTED: skipped A/B transitions mean counts were lost. Push "
               "more slowly and check encoder wiring.\n");
        clean = false;
    }
    if (clean) {
        double cpm_l = (double)d.left_forward / MARKED_DISTANCE_MM;
        double cpm_r = (double)d.right_forward / MARKED_DISTANCE_MM;
        double mean = ((double)d.left_forward + (double)d.right_forward) * 0.5;
        printf("This run: LEFT %.4f counts/mm (%.5f mm/count)   RIGHT %.4f counts/mm (%.5f mm/count)\n",
               cpm_l, 1.0 / cpm_l, cpm_r, 1.0 / cpm_r);
        printf("Left/right count difference: %lld counts (%.2f%% of mean).\n",
               (long long)(d.left_forward - d.right_forward),
               100.0 * (double)(d.left_forward - d.right_forward) / mean);
        if (cpm_l < NOMINAL_COUNTS_PER_MM * 0.85 ||
            cpm_l > NOMINAL_COUNTS_PER_MM * 1.15 ||
            cpm_r < NOMINAL_COUNTS_PER_MM * 0.85 ||
            cpm_r > NOMINAL_COUNTS_PER_MM * 1.15) {
            printf("CHECK: more than 15%% from nominal. Confirm MARKED_DISTANCE_MM "
                   "(units!), the marks, and that both wheels were on the floor.\n");
        }
        if (stored_runs < MAX_RUNS) {
            left_cpm[stored_runs] = cpm_l;
            right_cpm[stored_runs] = cpm_r;
            ++stored_runs;
            printf("Run stored as clean run %u.\n", stored_runs);
        } else {
            printf("Run table full (%d). Reboot to clear it.\n", MAX_RUNS);
        }
    }
    print_summary();
    printf("Carry the car back to START and press GP20 for the next run.\n\n");
}

int main(void)
{
    hold_outputs_low();
    encoders_init();
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    stdio_init_all();

    bool usb_was_connected = false;
    bool counting = false;
    encoder_sample_t start = encoders_snapshot();
    uint64_t next_report_ms = 0u;
    uint64_t next_reminder_ms = 0u;

    while (true) {
        sleep_ms(TICK_MS);
        uint64_t now_ms = to_ms_since_boot(get_absolute_time());

        bool connected = stdio_usb_connected();
        if (connected && !usb_was_connected) {
            print_help();
            next_reminder_ms = now_ms + IDLE_REMINDER_MS;
        }
        usb_was_connected = connected;

        if (button_pressed_event()) {
            if (!counting) {
                start = encoders_snapshot();
                counting = true;
                next_report_ms = now_ms;
                printf("\n[ARMED] Counts zeroed. Push the car from START to END, "
                       "then press GP20.\n");
            } else {
                counting = false;
                latch_and_report(&start);
                next_reminder_ms = now_ms + IDLE_REMINDER_MS;
            }
            continue;
        }

        if (counting) {
            if (now_ms >= next_report_ms) {
                next_report_ms = now_ms + LIVE_REPORT_MS;
                run_delta_t d = delta_since(&start);
                printf("  LEFT %7lld  RIGHT %7lld  (about %.0f mm at nominal)\n",
                       (long long)d.left_forward, (long long)d.right_forward,
                       ((double)d.left_forward + (double)d.right_forward) * 0.5 /
                           NOMINAL_COUNTS_PER_MM);
            }
        } else if (now_ms >= next_reminder_ms) {
            next_reminder_ms = now_ms + IDLE_REMINDER_MS;
            printf("[IDLE] Put the car on START and press GP20 to zero and begin.\n");
        }
    }
}
