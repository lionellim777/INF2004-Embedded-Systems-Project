#include "terrain.h"
#include "hump_roll.h"
#include "tilt_alert.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

enum {
    SDA_PIN = 4,
    SCL_PIN = 5,
    ACCEL_ADDRESS = 0x19,
    CALIBRATION_SAMPLES = 200,
    FILTER_SAMPLES = 8,
    SAMPLE_INTERVAL_MS = 20,
    MIN_PHASE_MS = 300,
    EVENT_TIMEOUT_MS = 10000,
};

typedef struct {
    float x;
    float y;
    float z;
} acceleration_t;

typedef enum {
    HUMP_WAITING,
    HUMP_CLIMBING,
    HUMP_DESCENDING,
} hump_phase_t;

static terrain_status_t status;
static acceleration_t samples[FILTER_SAMPLES];
static uint sample_next;
static uint sample_count;
static float level_pitch;
static float level_roll;
static hump_roll_t roll_detector;
static tilt_alert_t tilt_detector;
static uint64_t last_roll_poll_ms;
/* Roll trace for tuning: every detector input sample of a run, kept in RAM and printed after the
 * stop. filtered = the 8-sample average the detector sees, raw = the single newest sample, both as
 * roll change from the level calibration in 0.1 degree. */
enum { ROLL_TRACE_SAMPLES = 600 }; /* about 16 s: covers the slow 50 cm move */
static struct { uint16_t ms; int16_t filtered; int16_t raw; } roll_trace[ROLL_TRACE_SAMPLES];
static unsigned roll_trace_count;
static uint64_t roll_trace_start_ms;
static bool roll_trace_started;
static uint64_t next_sample_ms;
static hump_phase_t phase;
static uint64_t event_start_ms;
static uint64_t phase_start_ms;
static uint64_t level_since_ms;
static float peak_climb;
static float peak_descent;

static bool write_register(uint8_t reg, uint8_t value)
{
    uint8_t message[2] = {reg, value};
    return i2c_write_timeout_us(i2c0, ACCEL_ADDRESS, message, 2, false, 2000u) == 2;
}

static bool read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    if (i2c_write_timeout_us(i2c0, ACCEL_ADDRESS, &reg, 1, true, 2000u) != 1) {
        return false;
    }
    return i2c_read_timeout_us(i2c0, ACCEL_ADDRESS, data, length, false, 2000u) ==
           (int)length;
}

static bool read_acceleration(acceleration_t *reading)
{
    uint8_t data[6];
    if (!read_registers(0x28u | 0x80u, data, sizeof data)) {
        return false;
    }
    reading->x = (float)((int16_t)(((uint16_t)data[1] << 8) | data[0]) >> 4);
    reading->y = (float)((int16_t)(((uint16_t)data[3] << 8) | data[2]) >> 4);
    reading->z = (float)((int16_t)(((uint16_t)data[5] << 8) | data[4]) >> 4);
    return true;
}

static float pitch(const acceleration_t *reading)
{
    return atan2f(reading->y,
                  sqrtf(reading->x * reading->x +
                        reading->z * reading->z)) * 57.2957795f;
}

static float roll_of(const acceleration_t *reading)
{
    return atan2f(reading->x,
                  sqrtf(reading->y * reading->y +
                        reading->z * reading->z)) * 57.2957795f;
}

static float magnitude(const acceleration_t *reading)
{
    return sqrtf(reading->x * reading->x +
                 reading->y * reading->y +
                 reading->z * reading->z);
}

static bool average_stationary(uint count, acceleration_t *average)
{
    acceleration_t total = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < count; ++i) {
        acceleration_t reading;
        if (!read_acceleration(&reading)) {
            return false;
        }
        total.x += reading.x;
        total.y += reading.y;
        total.z += reading.z;
        sleep_ms(10);
    }
    average->x = total.x / (float)count;
    average->y = total.y / (float)count;
    average->z = total.z / (float)count;
    return true;
}

bool terrain_init(void)
{
    i2c_init(i2c0, 100000u);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    uint8_t identity = 0;
    status.available = read_registers(0x0fu, &identity, 1u) &&
                       identity == 0x33u;
    if (status.available) {
        status.available = write_register(0x20u, 0x57u) &&
                           write_register(0x23u, 0x08u);
    }
    printf("Terrain sensor: %s\n", status.available ? "READY" : "NOT FOUND");
    return status.available;
}

bool terrain_calibrate(void)
{
    /* The sensor may have missed its one probe at boot (loose Grove lead, slow power-up): probe
     * again here instead of staying silently unavailable until the next power cycle. */
    if (!status.available && !terrain_init()) {
        printf("[TERRAIN] Accelerometer NOT FOUND on GP4/GP5: hump detection is OFF. Check the Grove lead, then press GP21 again.\n");
        return false;
    }
    status.calibrated = false;
    status.pitch_valid = false;
    printf("Terrain calibration: keep the car level and still for 2 seconds.\n");
    acceleration_t first;
    acceleration_t second;
    if (!average_stationary(CALIBRATION_SAMPLES / 2u, &first) ||
        !average_stationary(CALIBRATION_SAMPLES / 2u, &second)) {
        printf("Terrain calibration failed: sensor read error.\n");
        return false;
    }
    if (fabsf(pitch(&first) - pitch(&second)) > 1.5f ||
        fabsf(magnitude(&first) - magnitude(&second)) > 50.0f) {
        printf("Terrain calibration rejected: car moved.\n");
        return false;
    }
    acceleration_t level = {
        (first.x + second.x) * 0.5f,
        (first.y + second.y) * 0.5f,
        (first.z + second.z) * 0.5f,
    };
    if (magnitude(&level) < 700.0f || magnitude(&level) > 1500.0f) {
        printf("Terrain calibration rejected: unexpected gravity reading.\n");
        return false;
    }
    level_pitch = pitch(&level);
    level_roll = roll_of(&level);
    hump_roll_reset(&roll_detector);
    tilt_alert_reset(&tilt_detector);
    status.tilt_alert_active = false;
    status.tilt_alert_count = 0;
    status.max_tilt_deg = 0.0f;
    last_roll_poll_ms = 0;
    status.roll_hump_count = 0;
    status.last_roll_hump_mm = 0.0f;
    status.highest_roll_hump_mm = 0.0f;
    status.current_roll_change_deg = 0.0f;
    status.calibrated = true;
    status.hump_count = 0;
    status.highest_hump_number = 0;
    status.highest_peak_angle_deg = 0.0f;
    sample_count = 0;
    sample_next = 0;
    next_sample_ms = 0;
    phase = HUMP_WAITING;
    level_since_ms = 0;
    printf("Terrain calibrated. Level pitch=%.1f degrees.\n", level_pitch);
    printf("Terrain level roll=%.2f degrees (roll hump detector armed).\n", level_roll);
    return true;
}

static void finish_hump(uint64_t now_ms)
{
    ++status.hump_count;
    if (peak_climb > status.highest_peak_angle_deg) {
        status.highest_peak_angle_deg = peak_climb;
        status.highest_hump_number = status.hump_count;
    }
    printf("HUMP %lu COMPLETE: climb=+%.1f deg descent=%.1f deg "
           "duration=%llu ms; highest=hump %lu (+%.1f deg)\n",
           (unsigned long)status.hump_count, peak_climb, peak_descent,
           (unsigned long long)(now_ms - event_start_ms),
           (unsigned long)status.highest_hump_number,
           status.highest_peak_angle_deg);
}

static void report_tilt_event(tilt_alert_event_t tilt_event)
{
    if (tilt_event.result == TILT_ALERT_RAISED) {
        status.tilt_alert_active = true;
        ++status.tilt_alert_count;
        printf("[TILT ALERT] count=%lu tilt=%.1f pitch=%.1f roll=%.1f threshold=%.0f hold_ms=%u.\n",
               (unsigned long)status.tilt_alert_count, tilt_event.tilt_deg, tilt_event.pitch_deg, tilt_event.roll_deg,
               (double)TILT_ALERT_ENTER_DEG, (unsigned)TILT_ALERT_ENTER_MS);
    } else if (tilt_event.result == TILT_ALERT_CLEARED) {
        status.tilt_alert_active = false;
        printf("[TILT CLEARED] peak=%.1f pitch=%.1f roll=%.1f lasted_ms=%lu.\n",
               tilt_event.tilt_deg, tilt_event.pitch_deg, tilt_event.roll_deg, (unsigned long)tilt_event.duration_ms);
    }
}

static void report_roll_event(hump_roll_event_t roll_event)
{
    if (roll_event.result == HUMP_ROLL_COMPLETED) {
        ++status.roll_hump_count;
        status.last_roll_hump_mm = roll_event.height_mm;
        if (roll_event.height_mm > status.highest_roll_hump_mm) {
            status.highest_roll_hump_mm = roll_event.height_mm;
        }
        printf("[HUMP] n=%lu side=%s height_mm=%.1f peak_roll=%.1f mean_roll=%.1f duration_ms=%lu baseline=%.2f highest_mm=%.1f.\n",
               (unsigned long)status.roll_hump_count, roll_event.sign < 0 ? "RIGHT" : "LEFT",
               roll_event.height_mm, roll_event.peak_deg, roll_event.mean_deg, (unsigned long)roll_event.duration_ms,
               roll_event.baseline_deg,
               status.highest_roll_hump_mm);
    } else if (roll_event.result == HUMP_ROLL_WRONG_SIDE) {
        printf("[HUMP IGNORED] reason=LEFT_SIDE peak_roll=%.1f duration_ms=%lu.\n",
               roll_event.peak_deg, (unsigned long)roll_event.duration_ms);
    } else if (roll_event.result == HUMP_ROLL_TOO_SHORT) {
        printf("[HUMP IGNORED] reason=TOO_SHORT peak_roll=%.1f duration_ms=%lu min_ms=%u.\n",
               roll_event.peak_deg, (unsigned long)roll_event.duration_ms, (unsigned)HUMP_ROLL_MIN_MS);
    } else if (roll_event.result == HUMP_ROLL_TIMED_OUT) {
        printf("[HUMP IGNORED] reason=STILL_TILTED timeout_ms=%u.\n", (unsigned)HUMP_ROLL_TIMEOUT_MS);
    }
}

void terrain_run_start(void)
{
    if (!status.calibrated) {
        printf("[TERRAIN] WARNING: %s. Hump detection is OFF for this run; press GP21 with the car level and still.\n",
               status.available ? "level calibration missing or lost" : "accelerometer not found");
    }
    hump_roll_begin_run(&roll_detector);
    last_roll_poll_ms = 0;
    roll_trace_count = 0;
    roll_trace_started = false;
    tilt_alert_reset(&tilt_detector);
    /* The brief asks for the highest hump "during the run", and the dashboard labels these "this run":
     * start every run from zero instead of carrying the totals over from earlier runs. */
    status.tilt_alert_active = false;
    status.tilt_alert_count = 0;
    status.max_tilt_deg = 0.0f;
    status.roll_hump_count = 0;
    status.last_roll_hump_mm = 0.0f;
    status.highest_roll_hump_mm = 0.0f;
}

static int16_t tenths(float degrees)
{
    const float scaled = degrees * 10.0f;
    return (int16_t)(scaled > 3000.0f ? 3000.0f : (scaled < -3000.0f ? -3000.0f : scaled));
}

void terrain_run_end(void)
{
    if (status.calibrated && tilt_detector.active) {
        printf("[TILT END] still_active=1 peak=%.1f pitch=%.1f roll=%.1f.\n",
               tilt_detector.peak_deg, tilt_detector.peak_pitch_deg, tilt_detector.peak_roll_deg);
    }
    if (status.calibrated) {
        report_roll_event(hump_roll_finish(&roll_detector, (uint32_t)to_ms_since_boot(get_absolute_time())));
    }
}

void terrain_print_roll_trace(void)
{
    if (roll_trace_count == 0u) {
        return;
    }
    printf("[ROLL TRACE] samples=%u unit=0.1deg format=ms_since_go:filtered/raw baseline=%.2f deg; "
           "right wheel raised is negative.\n", roll_trace_count, roll_detector.baseline_deg);
    for (unsigned i = 0; i < roll_trace_count; ++i) {
        if (i % 25u == 0u) {
            printf("%s[ROLL TRACE %u]", i ? "\n" : "", i / 25u);
        }
        printf(" %u:%d/%d", roll_trace[i].ms, roll_trace[i].filtered, roll_trace[i].raw);
    }
    printf("\n");
    roll_trace_count = 0;
    roll_trace_started = false;
}

void terrain_motion_go(void)
{
    const uint64_t now_ms = to_ms_since_boot(get_absolute_time());
    hump_roll_motion_go(&roll_detector, (uint32_t)now_ms);
    /* The trace covers the first GO of a run only; later calls (course turns) do not restart it. */
    if (!roll_trace_started) {
        roll_trace_started = true;
        roll_trace_start_ms = now_ms;
        roll_trace_count = 0;
    }
}

void terrain_poll(void)
{
    if (!status.calibrated) {
        return;
    }
    uint64_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms < next_sample_ms) {
        return;
    }
    next_sample_ms = now_ms + SAMPLE_INTERVAL_MS;

    acceleration_t reading;
    if (!read_acceleration(&reading)) {
        status.calibrated = false;
        status.pitch_valid = false;
        ++status.read_errors;
        printf("Terrain sensor read failed; hump monitoring paused.\n");
        return;
    }
    samples[sample_next] = reading;
    sample_next = (sample_next + 1u) % FILTER_SAMPLES;
    if (sample_count < FILTER_SAMPLES) {
        ++sample_count;
    }
    if (sample_count < FILTER_SAMPLES) {
        return;
    }

    acceleration_t average = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < FILTER_SAMPLES; ++i) {
        average.x += samples[i].x;
        average.y += samples[i].y;
        average.z += samples[i].z;
    }
    average.x /= (float)FILTER_SAMPLES;
    average.y /= (float)FILTER_SAMPLES;
    average.z /= (float)FILTER_SAMPLES;

    /* The mounted GY-511 gives negative pitch when the front is raised. */
    float tilt = -(pitch(&average) - level_pitch);
    status.current_pitch_deg = tilt;
    status.pitch_valid = true;
    ++status.pitch_sequence;
    /* Roll detector: right wheel raised reads negative. Independent of the pitch detector below. */
    /* terrain_run_start() is the normal run-start hook. This gap check is only a fallback for a
     * caller that polls without it: terrain_poll() only runs during a run, so a long gap means a new run. */
    if (last_roll_poll_ms != 0u && now_ms - last_roll_poll_ms > 1000u) {
        hump_roll_begin_run(&roll_detector);
    }
    last_roll_poll_ms = now_ms;
    const float roll_change = roll_of(&average) - level_roll;
    status.current_roll_change_deg = roll_change;
    if (roll_trace_started && roll_trace_count < ROLL_TRACE_SAMPLES) {
        roll_trace[roll_trace_count].ms = (uint16_t)(now_ms - roll_trace_start_ms);
        roll_trace[roll_trace_count].filtered = tenths(roll_change);
        roll_trace[roll_trace_count].raw = tenths(roll_of(&reading) - level_roll);
        ++roll_trace_count;
    }
    report_roll_event(hump_roll_update(&roll_detector, roll_change, (uint32_t)now_ms));
    /* Tilt alert: skip the launch/turn hold-off, when acceleration reads as tilt. The roll detector
     * clears its hold-off flag lazily inside the update call above, so it is current here. */
    if (!roll_detector.holdoff_active) {
        const float tilt_now = tilt_alert_magnitude(tilt, roll_change);
        if (tilt_now > status.max_tilt_deg) {
            status.max_tilt_deg = tilt_now;
        }
        report_tilt_event(tilt_alert_update(&tilt_detector, tilt, roll_change, (uint32_t)now_ms));
    }
    const float threshold = 5.0f;
    const bool level = fabsf(tilt) < 2.0f;


    if (phase == HUMP_WAITING) {
        if (tilt > threshold) {
            phase = HUMP_CLIMBING;
            event_start_ms = now_ms;
            phase_start_ms = now_ms;
            peak_climb = tilt;
            peak_descent = 0.0f;
            level_since_ms = 0;
            printf("HUMP: climbing started.\n");
        }
    } else if (phase == HUMP_CLIMBING) {
        if (tilt > peak_climb) {
            peak_climb = tilt;
        }
        if (tilt < -threshold && now_ms - phase_start_ms >= MIN_PHASE_MS) {
            phase = HUMP_DESCENDING;
            phase_start_ms = now_ms;
            peak_descent = tilt;
            level_since_ms = 0;
            printf("HUMP: descending started.\n");
        } else if (level) {
            if (level_since_ms == 0) {
                level_since_ms = now_ms;
            } else if (now_ms - level_since_ms >= 800u) {
                phase = HUMP_WAITING;
                printf("HUMP cancelled: no descent detected.\n");
            }
        } else {
            level_since_ms = 0;
        }
        if (phase == HUMP_CLIMBING &&
            now_ms - event_start_ms > EVENT_TIMEOUT_MS) {
            phase = HUMP_WAITING;
            printf("HUMP cancelled: event timed out.\n");
        }
    } else {
        if (tilt < peak_descent) {
            peak_descent = tilt;
        }
        if (tilt > threshold && now_ms - phase_start_ms >= MIN_PHASE_MS) {
            finish_hump(now_ms);
            phase = HUMP_CLIMBING;
            event_start_ms = now_ms;
            phase_start_ms = now_ms;
            peak_climb = tilt;
            peak_descent = 0.0f;
            level_since_ms = 0;
            printf("HUMP: next climb started.\n");
        } else if (level) {
            if (level_since_ms == 0) {
                level_since_ms = now_ms;
            } else if (now_ms - level_since_ms >= 480u) {
                finish_hump(now_ms);
                phase = HUMP_WAITING;
                level_since_ms = 0;
            }
        } else {
            level_since_ms = 0;
        }
        if (phase == HUMP_DESCENDING &&
            now_ms - event_start_ms > EVENT_TIMEOUT_MS) {
            phase = HUMP_WAITING;
            printf("HUMP cancelled: event timed out.\n");
        }
    }
}

terrain_status_t terrain_get_status(void)
{
    return status;
}
