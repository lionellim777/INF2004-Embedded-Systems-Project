/*
 * Barcode straight-drive test (no line following).
 *
 * Drives straight for a set distance (default 400 mm) at the course scanning
 * speed (600 counts/s per wheel, about 44 mm/s) while the offset GP26 sensor
 * reads a Code 39 barcode placed beside the path. Uses the same sampler,
 * 12-sample filter, reader and decoder as the course firmware. Black/white
 * can come from fixed levels (the course firmware's 226/220, or a mid level),
 * levels set from the measured white, or "swing": a rise or drop from the
 * last gap or bar. 'l' switches between them and the choice is kept in
 * flash. Line sensors, sonar, servo, IMU and Wi-Fi are not used.
 *
 * Wiring:
 *   Motor 1 (left):  M1A GP8,  M1B GP9
 *   Motor 2 (right): M2A GP10, M2B GP11
 *   Encoders: left GP2/GP3, right GP16/GP17
 *   Barcode sensor AO: GP26 via Grove 5
 *   GP20 button: start (after a 3 s countdown) / abort
 *
 * USB serial: 3/4/5 select 300/400/500 mm, x aborts, r prints every stored
 * run, e prints the last run's edges, o prints the last run's raw sensor
 * trace, l switches the black/white levels, c clears the history, ? prints
 * help.
 *
 * Runs can be done without USB (car on battery). The last 20 runs are saved
 * in flash, so they survive switching off or a reset, and are printed when
 * USB is connected. Each run records the power-on number it was made in.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
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
    DEFAULT_DISTANCE_MM = 400,
    HISTORY_RUNS = 20,
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
    uint16_t widths_ms[BARCODE_CODE39_RUN_COUNT];
    uint16_t boot;  /* Power-on number when the run was made. */
    uint16_t black_on;
    uint16_t black_off;
    uint16_t white;  /* Measured white before the run; 0 for fixed levels. */
    uint8_t width_count;
    char end_reason[24];
    bool integrity_fault;
    bool valid;
} run_summary_t;

/* Kept in the last flash sector so results survive power-off and resets. */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t boot_count;
    uint32_t run_total;
    uint32_t level_index;
    run_summary_t history[HISTORY_RUNS];
    uint32_t checksum;
} saved_state_t;

enum {
    STATE_MAGIC = 0x42435354,
    STATE_VERSION = 3,
    STATE_FLASH_BYTES = ((sizeof(saved_state_t) + FLASH_PAGE_SIZE - 1u) /
                         FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE,
};

#define STATE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

_Static_assert(STATE_FLASH_BYTES <= FLASH_SECTOR_SIZE,
               "saved run history must fit one flash sector");

/* Every black/white change of the most recent run, kept in the sector before
 * the history so it survives the restart caused by reconnecting USB. Each
 * entry is (ms since the run started << 1) | 1 if the change was to black. */
enum { EDGE_LOG_MAX = 960, EDGE_MAGIC = 0x45444745 };

typedef struct {
    uint32_t magic;
    uint32_t run_number;
    uint32_t count;
    uint32_t dropped;
    uint16_t entry[EDGE_LOG_MAX];
    uint32_t checksum;
} edge_log_t;

enum {
    EDGE_FLASH_BYTES = ((sizeof(edge_log_t) + FLASH_PAGE_SIZE - 1u) /
                        FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE,
};

#define EDGE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (2u * FLASH_SECTOR_SIZE))

_Static_assert(EDGE_FLASH_BYTES <= FLASH_SECTOR_SIZE,
               "edge log must fit one flash sector");

/* Raw GP26 minimum/maximum per 10 ms of the most recent run, kept in the
 * third-last sector so 'o' still works after a restart. */
enum { OPTICAL_MAGIC = 0x4f505431 };

typedef struct {
    uint32_t magic;
    uint32_t run_number;
    uint32_t samples;
    uint32_t max_gap_us;
    uint16_t low[OPTICAL_BINS];
    uint16_t high[OPTICAL_BINS];
    uint32_t checksum;
} optical_log_t;

enum {
    OPTICAL_FLASH_BYTES = ((sizeof(optical_log_t) + FLASH_PAGE_SIZE - 1u) /
                           FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE,
};

#define OPTICAL_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (3u * FLASH_SECTOR_SIZE))

_Static_assert(OPTICAL_FLASH_BYTES <= FLASH_SECTOR_SIZE,
               "raw trace must fit one flash sector");

/* Black/white levels for the filtered GP26 value (0-4095, higher = darker).
 * "course" is the course firmware's calibration. "mid" sits roughly halfway
 * between white paper (about 190) and printed bars (about 3300-3600).
 *
 * "auto" measures the white paper under GP26 while the car waits to start
 * and sets the levels relative to it. Margins from raw traces on this car:
 * white stayed within about +550 of its resting value (paper edge drift),
 * and the faintest narrow bar of the small barcode peaked about +1260
 * above it. +850 / +650 sit between the two.
 *
 * "swing" follows the barcode itself: a bar starts when the reading rises
 * 700 above the lowest point of the current gap, and a gap starts when it
 * drops 700 below the highest point of the current bar. On the track, narrow
 * gaps read 2000-2500 (never white) but still dropped 1000+ from the bars
 * beside them; the floor and paper drift stayed within about +550. A bar
 * must also reach the "auto" level (measured white +850), so the slow grey
 * rise seen just before the first bar does not start it early; anything at
 * or below white +650 is always a gap. A bar must also rise at least half
 * way from the gap's lowest point to the previous bar's highest: after a
 * wide white gap the sensor rises slowly, and without this the narrow bar
 * that follows read about 25 ms long (86 ms against 62 ms) and the letter
 * was rejected. */
typedef enum {
    LEVEL_FIXED,
    LEVEL_RELATIVE,  /* on/off are margins above the measured white. */
    LEVEL_SWING,     /* on is the rise/drop that starts a bar/gap. */
} level_mode_t;

typedef struct {
    uint16_t on;
    uint16_t off;
    level_mode_t mode;
    const char *p_name;
} levels_t;

static const levels_t k_levels[] = {
    { BARCODE_DRIVE_BLACK_ON, BARCODE_DRIVE_BLACK_OFF, LEVEL_FIXED, "course" },
    { 1600u, 1400u, LEVEL_FIXED, "mid" },
    { 850u, 650u, LEVEL_RELATIVE, "auto" },
    { 850u, 650u, LEVEL_SWING, "swing" },
};

enum {
    LEVEL_COUNT = sizeof(k_levels) / sizeof(k_levels[0]),
    LEVEL_DEFAULT = 3,
    SWING_AMOUNT = 700,
    /* In swing mode the sampler sees only these two values. */
    SWING_SAMPLER_BLACK = 4095,
    SWING_SAMPLER_WHITE = 0,
    SWING_SAMPLER_ON = 2048,
    SWING_SAMPLER_OFF = 2047,
    ADC_FULL_SCALE = 4095,
    WHITE_SAMPLES = 31,
    WHITE_SPACING_US = 50000,  /* 31 readings span the last 1.5 s of waiting. */
    WHITE_SPREAD_MAX = 100,    /* Still paper reads within ~20 counts. */
};

/* White readings taken while the car waits to start, oldest overwritten. */
typedef struct {
    uint16_t samples[WHITE_SAMPLES];
    uint32_t count;
    uint32_t next;
    uint64_t last_us;
} white_baseline_t;

static white_baseline_t g_white;
static uint16_t g_level_on;
static uint16_t g_level_off;
static uint16_t g_level_white;
static uint16_t g_swing;  /* 0 unless the swing mode is in use. */
static uint16_t g_swing_black;  /* A bar must reach at least this. */
static uint16_t g_swing_white;  /* At or below this is always a gap. */

/* Swing detector, run by the sampling alarm. */
typedef struct {
    bool black;
    bool started;
    uint16_t extreme;  /* Lowest point of this gap, or highest of this bar. */
    uint16_t last_bar_max;  /* Highest point of the previous bar, 0 if none. */
} swing_t;

static swing_t g_swing_state;

static uint16_t swing_step(swing_t *p_swing, uint16_t level)
{
    if (!p_swing->started) {
        p_swing->started = true;
        p_swing->black = false;
        p_swing->extreme = level;
        p_swing->last_bar_max = 0u;
    } else if (p_swing->black) {
        if (level > p_swing->extreme) {
            p_swing->extreme = level;
        } else if (((uint32_t)level + g_swing <= p_swing->extreme) ||
                   (level <= g_swing_white)) {
            p_swing->black = false;
            p_swing->last_bar_max = p_swing->extreme;
            p_swing->extreme = level;
        }
    } else {
        uint32_t half = (p_swing->last_bar_max > p_swing->extreme)
            ? ((uint32_t)p_swing->last_bar_max - p_swing->extreme) / 2u : 0u;

        if (level < p_swing->extreme) {
            p_swing->extreme = level;
        } else if (((uint32_t)level >= (uint32_t)p_swing->extreme + g_swing) &&
                   ((uint32_t)level >= (uint32_t)p_swing->extreme + half) &&
                   (level >= g_swing_black)) {
            p_swing->black = true;
            p_swing->extreme = level;
        }
    }
    return p_swing->black ? SWING_SAMPLER_BLACK : SWING_SAMPLER_WHITE;
}

static void white_add(uint16_t level, uint64_t now_us)
{
    if ((g_white.count > 0u) && ((now_us - g_white.last_us) < WHITE_SPACING_US)) {
        return;
    }
    g_white.samples[g_white.next] = level;
    g_white.next = (g_white.next + 1u) % WHITE_SAMPLES;
    if (g_white.count < WHITE_SAMPLES) {
        ++g_white.count;
    }
    g_white.last_us = now_us;
}

/* Median resists a passing hand or spike; the spread (middle half of the
 * readings) shows whether the car or paper was still moving. */
static bool white_value(uint16_t *p_median, uint16_t *p_spread)
{
    uint16_t sorted[WHITE_SAMPLES];

    if (g_white.count < WHITE_SAMPLES) {
        return false;
    }
    for (uint32_t i = 0u; i < WHITE_SAMPLES; ++i) {
        uint32_t j = i;
        uint16_t v = g_white.samples[i];
        while ((j > 0u) && (sorted[j - 1u] > v)) {
            sorted[j] = sorted[j - 1u];
            --j;
        }
        sorted[j] = v;
    }
    *p_median = sorted[WHITE_SAMPLES / 2];
    *p_spread = (uint16_t)(sorted[(3 * WHITE_SAMPLES) / 4] - sorted[WHITE_SAMPLES / 4]);
    return true;
}

/* Sets g_level_on/off for the next run. False if "auto" could not measure a
 * steady white under GP26. */
static bool levels_select(const levels_t *p_levels)
{
    g_level_white = 0u;
    g_swing = 0u;
    if (p_levels->mode == LEVEL_FIXED) {
        g_level_on = p_levels->on;
        g_level_off = p_levels->off;
        return true;
    }

    uint16_t white = 0u;
    uint16_t spread = 0u;
    if (!white_value(&white, &spread) || (spread > WHITE_SPREAD_MAX) ||
        ((uint32_t)white + p_levels->on > ADC_FULL_SCALE)) {
        printf("[STOP] No steady white under GP26 (white=%u spread=%u). "
               "Put GP26 on white paper and keep the car still.\n",
               (unsigned)white, (unsigned)spread);
        return false;
    }
    g_level_white = white;
    g_level_on = (uint16_t)(white + p_levels->on);
    g_level_off = (uint16_t)(white + p_levels->off);
    if (p_levels->mode == LEVEL_SWING) {
        g_swing = SWING_AMOUNT;
        g_swing_black = g_level_on;
        g_swing_white = g_level_off;
        g_level_on = SWING_SAMPLER_ON;
        g_level_off = SWING_SAMPLER_OFF;
    }
    return true;
}

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
static volatile uint16_t g_filtered_min;  /* Over the current run. */
static volatile uint16_t g_filtered_max;
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
static saved_state_t g_state;
static edge_log_t g_edges;

static uint32_t fnv_hash(const void *p_data, size_t bytes)
{
    const uint8_t *p_byte = (const uint8_t *)p_data;
    uint32_t hash = 2166136261u;

    for (size_t i = 0u; i < bytes; ++i) {
        hash = (hash ^ p_byte[i]) * 16777619u;
    }
    return hash;
}

static uint32_t state_checksum(const saved_state_t *p_state)
{
    return fnv_hash(p_state, offsetof(saved_state_t, checksum));
}

/* Erases one sector and programs `bytes` (a whole number of pages) into it.
 * Call only with the motors stopped: interrupts (and so sampling) pause for
 * the few tens of ms this takes. */
static void flash_write_sector(uint32_t offset, const void *p_data,
                               size_t size, size_t bytes)
{
    static uint8_t s_page_buffer[FLASH_SECTOR_SIZE];

    memset(s_page_buffer, 0xff, bytes);
    memcpy(s_page_buffer, p_data, size);

    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    flash_range_program(offset, s_page_buffer, bytes);
    restore_interrupts(irq);
}

static void state_save(void)
{
    g_state.checksum = state_checksum(&g_state);
    flash_write_sector(STATE_FLASH_OFFSET, &g_state, sizeof(g_state),
                       STATE_FLASH_BYTES);
}

static void edges_save(void)
{
    g_edges.magic = EDGE_MAGIC;
    g_edges.checksum = fnv_hash(&g_edges, offsetof(edge_log_t, checksum));
    flash_write_sector(EDGE_FLASH_OFFSET, &g_edges, sizeof(g_edges),
                       EDGE_FLASH_BYTES);
}

static void optical_save(uint32_t run_number)
{
    static optical_log_t s_log;

    s_log.magic = OPTICAL_MAGIC;
    s_log.run_number = run_number;
    s_log.samples = g_optical.samples;
    s_log.max_gap_us = g_optical.max_gap_us;
    memcpy(s_log.low, g_optical.low, sizeof(s_log.low));
    memcpy(s_log.high, g_optical.high, sizeof(s_log.high));
    s_log.checksum = fnv_hash(&s_log, offsetof(optical_log_t, checksum));
    flash_write_sector(OPTICAL_FLASH_OFFSET, &s_log, sizeof(s_log),
                       OPTICAL_FLASH_BYTES);
}

/* Prints the saved edge list of the last run and the widths between edges. */
static void print_edges(void)
{
    const edge_log_t *p_log = (const edge_log_t *)(XIP_BASE + EDGE_FLASH_OFFSET);

    if ((p_log->magic != EDGE_MAGIC) || (p_log->count > EDGE_LOG_MAX) ||
        (p_log->checksum != fnv_hash(p_log, offsetof(edge_log_t, checksum)))) {
        printf("[EDGES] No saved edge log.\n");
        return;
    }
    printf("[EDGES] run=%lu count=%lu dropped=%lu (ms since start:B=to black/W=to white)\n",
           (unsigned long)p_log->run_number, (unsigned long)p_log->count,
           (unsigned long)p_log->dropped);
    for (uint32_t i = 0u; i < p_log->count; ++i) {
        uint16_t e = p_log->entry[i];
        printf("%s%u:%c", ((i % 20u) == 0u) ? ((i > 0u) ? "\n[EDGES] " : "[EDGES] ") : ",",
               (unsigned)(e >> 1), ((e & 1u) != 0u) ? 'B' : 'W');
    }
    printf("\n[EDGE WIDTHS] ms between changes:");
    for (uint32_t i = 1u; i < p_log->count; ++i) {
        printf("%s%u", (i > 1u) ? "," : " ",
               (unsigned)((p_log->entry[i] >> 1) - (p_log->entry[i - 1u] >> 1)));
    }
    printf("\n");
}

/* Loads the saved history, or starts a fresh one if none is valid. */
static void state_load(void)
{
    const saved_state_t *p_saved =
        (const saved_state_t *)(XIP_BASE + STATE_FLASH_OFFSET);

    if ((p_saved->magic == STATE_MAGIC) &&
        (p_saved->version == STATE_VERSION) &&
        (p_saved->checksum == state_checksum(p_saved))) {
        g_state = *p_saved;
    } else {
        memset(&g_state, 0, sizeof(g_state));
        g_state.magic = STATE_MAGIC;
        g_state.version = STATE_VERSION;
        g_state.level_index = LEVEL_DEFAULT;
    }
    if (g_state.level_index >= LEVEL_COUNT) {
        g_state.level_index = LEVEL_DEFAULT;
    }
}

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
    if (g_sampler.enabled) {
        if (filtered < g_filtered_min) {
            g_filtered_min = filtered;
        }
        if (filtered > g_filtered_max) {
            g_filtered_max = filtered;
        }
        if (g_swing > 0u) {
            filtered = swing_step(&g_swing_state, filtered);
        }
    }
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

    for (uint32_t i = 0u; i < g_batch.count; ++i) {
        uint32_t ms = (uint32_t)(g_batch.edges[i].us - g_origin_us) / 1000u;
        if ((g_edges.count < EDGE_LOG_MAX) && (ms <= 0x7fffu)) {
            g_edges.entry[g_edges.count] =
                (uint16_t)((ms << 1) | (g_batch.edges[i].black ? 1u : 0u));
            ++g_edges.count;
        } else {
            ++g_edges.dropped;
        }
    }
    barcode_capture_consume(&g_capture, &g_batch);
    report_new_events();
}

static bool barcode_begin(void)
{
    if (!levels_select(&k_levels[g_state.level_index])) {
        return false;
    }

    uint32_t irq = save_and_disable_interrupts();
    bool fresh = (uint32_t)(time_us_32() - g_sample_us) <= BARCODE_SAMPLE_GAP_US;
    uint16_t raw = g_raw_latest;

    if (fresh) {
        g_origin_us = time_us_32();
        g_signal = (barcode_signal_t){0};
        g_swing_state.started = false;
        g_filtered_min = UINT16_MAX;
        g_filtered_max = 0u;
        barcode_sampler_begin_levels(&g_sampler, g_level_on, g_level_off);
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
    g_edges.count = 0u;
    g_edges.dropped = 0u;
    g_rejected_logged = 0u;
    g_timeouts_logged = 0u;

    if ((g_swing == 0u) && (raw > g_level_off)) {
        printf("[WARN] GP26 is not on white (raw=%u). Reading starts after 250 ms of white.\n",
               (unsigned)raw);
    }
    return true;
}

static void print_summary(uint32_t number, const run_summary_t *p_run)
{
    printf("[RUN %lu] power_on=%u levels=%s%u/%u white=%u reads=%lu sequence=%s rejected=%lu timeouts=%lu edges=%lu "
           "raw=%u..%u max_gap_us=%lu integrity_fault=%u end=%s target_mm=%u "
           "counts_L/R=%ld/%ld nominal_mm=%.0f widths_ms=",
           (unsigned long)number, (unsigned)p_run->boot,
           (p_run->black_off == 0u) ? "swing" : "",
           (unsigned)p_run->black_on, (unsigned)p_run->black_off,
           (unsigned)p_run->white,
           (unsigned long)p_run->reads,
           (p_run->reads > 0u) ? p_run->sequence : "-",
           (unsigned long)p_run->rejected, (unsigned long)p_run->timeouts,
           (unsigned long)p_run->edges, (unsigned)p_run->raw_min,
           (unsigned)p_run->raw_max, (unsigned long)p_run->max_gap_us,
           p_run->integrity_fault ? 1u : 0u, p_run->end_reason,
           (unsigned)p_run->distance_mm, (long)p_run->left_counts,
           (long)p_run->right_counts,
           (double)counts_to_mm((p_run->left_counts + p_run->right_counts) / 2));
    for (uint32_t i = 0u; i < p_run->width_count; ++i) {
        printf("%s%u", (i > 0u) ? "," : "", (unsigned)p_run->widths_ms[i]);
    }
    printf("%s\n", (p_run->width_count > 0u) ? "" : "-");
}

/* Prints every stored run, oldest first, then a pass count. */
static void print_history(void)
{
    uint32_t stored = (g_state.run_total < HISTORY_RUNS) ? g_state.run_total : HISTORY_RUNS;
    uint32_t first = g_state.run_total - stored;
    uint32_t read_ok = 0u;

    printf("\n[HISTORY] %lu stored run(s) (saved in flash), showing the last %lu; this is power-on %lu\n",
           (unsigned long)g_state.run_total, (unsigned long)stored,
           (unsigned long)g_state.boot_count);
    for (uint32_t n = first; n < g_state.run_total; ++n) {
        const run_summary_t *p_run = &g_state.history[n % HISTORY_RUNS];
        print_summary(n + 1u, p_run);
        if (p_run->reads > 0u) {
            ++read_ok;
        }
    }
    printf("[HISTORY] runs with a letter read: %lu of %lu\n\n",
           (unsigned long)read_ok, (unsigned long)stored);
}

/* Stops the raw GP26 recording so it holds the run that just ended. */
static void optical_stop(void)
{
    uint32_t irq = save_and_disable_interrupts();
    if (g_optical.active) {
        barcode_optical_end(&g_optical, time_us_32());
    }
    g_optical.armed = false;
    restore_interrupts(irq);
}

/* Raw GP26 min/max per 10 ms window from the first edge of the last run. */
static void print_optical_bins(void)
{
    const optical_log_t *p_log =
        (const optical_log_t *)(XIP_BASE + OPTICAL_FLASH_OFFSET);

    if ((p_log->magic != OPTICAL_MAGIC) ||
        (p_log->checksum != fnv_hash(p_log, offsetof(optical_log_t, checksum)))) {
        printf("[OPTICAL] No saved raw trace.\n");
        return;
    }
    printf("[OPTICAL RESULT] run=%lu samples=%lu max_gap_us=%lu window_ms=10\n",
           (unsigned long)p_log->run_number,
           (unsigned long)p_log->samples,
           (unsigned long)p_log->max_gap_us);
    for (uint32_t i = 0u; i < OPTICAL_BINS; i += OPTICAL_BINS_PER_LINE) {
        printf("[OPTICAL DATA] bin=%lu", (unsigned long)i);
        for (uint32_t j = i; j < (i + OPTICAL_BINS_PER_LINE); ++j) {
            if (p_log->low[j] == UINT16_MAX) {
                printf(" -");
            } else {
                printf(" %u:%u", (unsigned)p_log->low[j],
                       (unsigned)p_log->high[j]);
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

    optical_stop();

    run.reads = g_capture.event_count;
    for (uint32_t i = 0u; (i < g_capture.event_count) &&
                          (i < BARCODE_STREAM_EVENTS); ++i) {
        run.sequence[i] = g_capture.events[i].letter;
    }
    run.rejected = g_capture.rejected;
    run.timeouts = g_capture.timeouts;
    run.edges = p_stats->edges;
    run.raw_min = (g_filtered_max > 0u) ? g_filtered_min : 0u;
    run.raw_max = g_filtered_max;
    run.max_gap_us = p_stats->max_gap_us;
    run.integrity_fault = g_capture.integrity_fault;
    run.distance_mm = g_distance_mm;
    run.left_counts = motion_left_position(p_motion, sample);
    run.right_counts = motion_right_position(p_motion, sample);
    (void)snprintf(run.end_reason, sizeof(run.end_reason), "%s", p_reason);
    run.boot = (uint16_t)g_state.boot_count;
    run.black_on = (g_swing > 0u) ? g_swing : g_level_on;
    run.black_off = (g_swing > 0u) ? 0u : g_level_off;
    run.white = g_level_white;
    run.width_count = g_capture.saved_runs;
    for (uint32_t i = 0u; i < run.width_count; ++i) {
        run.widths_ms[i] = (uint16_t)(barcode_runs_get(&g_capture.last_runs, i) /
                                      BARCODE_RUN_QUANTUM);
    }
    run.valid = true;
    g_state.history[g_state.run_total % HISTORY_RUNS] = run;
    ++g_state.run_total;
    state_save(); /* Motors are already stopped here. */
    g_edges.run_number = g_state.run_total;
    edges_save();
    optical_save(g_state.run_total);

    /* Printed now if USB is attached; otherwise kept for print_history(). */
    print_summary(g_state.run_total, &run);
    printf("adc_max_us=%lu. Ready: reposition the car and press GP20 for another run.\n",
           (unsigned long)g_adc_max_us);
}

static void print_levels(const char *p_label)
{
    const levels_t *p_levels = &k_levels[g_state.level_index];
    bool relative = (p_levels->mode != LEVEL_FIXED);

    if (p_levels->mode == LEVEL_SWING) {
        printf("%s (%s): bar = rise of %u (and half way to the last bar's highest) from the gap's lowest "
               "and >= measured white+%u; gap = drop of %u from the bar's highest or <= measured white+%u\n",
               p_label, p_levels->p_name, (unsigned)SWING_AMOUNT, (unsigned)p_levels->on,
               (unsigned)SWING_AMOUNT, (unsigned)p_levels->off);
        return;
    }
    printf("%s (%s): black>=%s%u white<=%s%u\n", p_label, p_levels->p_name,
           relative ? "measured white+" : "", (unsigned)p_levels->on,
           relative ? "measured white+" : "", (unsigned)p_levels->off);
}

static void print_help(void)
{
    printf("\n=== BARCODE STRAIGHT TEST (no line following) ===\n");
    printf("Drives straight %u mm at ~44 mm/s; GP26 reads the barcode beside the path.\n",
           (unsigned)g_distance_mm);
    printf("Place GP26 on white at least 30 mm before the first bar, with white after the last bar.\n");
    printf("GP20: start after 3 s countdown / abort. USB can be unplugged; results are saved and survive power-off.\n");
    printf("Serial: r = all stored runs, e = last run's edges, o = last run's raw trace, l = switch levels, c = clear, 3/4/5 = 300/400/500 mm, x = abort, ? = help.\n");
    print_levels("Levels");
    printf("%u-sample mean at %u us.\n\n",
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
    /* Count this power-on in flash before any timers or USB start. If the
     * count rises between runs, the car restarted (power switch or battery). */
    state_load();
    ++g_state.boot_count;
    state_save();
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
            print_history();
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
                print_history();
            } else if ((ch == 'o') || (ch == 'O')) {
                print_optical_bins();
            } else if ((ch == 'e') || (ch == 'E')) {
                print_edges();
            } else if ((ch == 'l') || (ch == 'L')) {
                if (state == RUN_IDLE) {
                    g_state.level_index = (g_state.level_index + 1u) % LEVEL_COUNT;
                    state_save();
                    print_levels("[LEVELS] Saved");
                } else {
                    printf("[LEVELS] Stop the run before changing levels.\n");
                }
            } else if ((ch == 'c') || (ch == 'C')) {
                if (state == RUN_IDLE) {
                    g_state.run_total = 0u;
                    memset(g_state.history, 0, sizeof(g_state.history));
                    state_save();
                    printf("[HISTORY] Cleared.\n");
                }
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
                    memset(&g_white, 0, sizeof(g_white));
                    state = RUN_COUNTDOWN;
                }
            }
            break;

        case RUN_COUNTDOWN:
            motors_stop();
            white_add(g_raw_latest, now_us);
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
