/* Idle-only measurement helper for the dashboard "Measurements" tab.
 * See measurement.h. Logic mirrors measurement_diag.c (the motors-off build the
 * first measurements were taken with) so both produce identical result lines.
 *
 * Output:
 *   [MEAS RESULT] seq=N KIND ...   printf (USB + log); retained by the transport
 *                                  and returned by GET /measure after a reload.
 *   live readout                   NOT printed: handed to the transport with
 *                                  calibration_wifi_set_measure_live() so it
 *                                  never fills the shared log.
 */
#ifdef ROBOCAR_MEASUREMENTS
#include "measurement.h"

#include <math.h>
#include <stdio.h>

#include "calibration_wifi.h"
#include "encoders.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

enum {
    I2C_SDA_PIN = 4,
    I2C_SCL_PIN = 5,
    ACCEL_ADDRESS = 0x19,
    CAPTURE_SAMPLES = 100, /* even; 1 s at 10 ms spacing */
    LIVE_SAMPLES = 8,
    IMU_SAMPLE_US = 10000,
    LIVE_REPORT_US = 500000,
    KEEPALIVE_TIMEOUT_US = 30000000,
    POSE_COUNT = 7,
};

/* Documented raw polarity: forward LEFT is negative, forward RIGHT positive. */
static const int LEFT_FORWARD_SIGN = -1;
static const int RIGHT_FORWARD_SIGN = 1;

/* Same acceptance rules as terrain_calibrate() in terrain.c. */
static const double STATIONARY_PITCH_LIMIT_DEG = 1.5;
static const double STATIONARY_MAGNITUDE_LIMIT = 50.0;
static const double GRAVITY_MIN = 700.0;
static const double GRAVITY_MAX = 1500.0;
/* This tool's own pass marks (not firmware constants). */
static const double MIN_TILT_DEG = 3.0;
static const double REPEAT_LIMIT_DEG = 0.5;
static const double MODEL_TOLERANCE_DEG = 0.5;
static const double RAD_TO_DEG = 57.29577951308232;

/* Recorded measurements for the hump model (edit if re-measured). */
static const double TRACK_WIDTH_MM = 110.0; /* tyre-contact centre to centre */
static const double WHEELBASE_MM = 80.0;    /* drive-axle line to caster contact */
static const double HUMP_HEIGHT_MM = 10.0;  /* block height under the right wheel */

typedef struct {
    double v[3]; /* X, Y, Z in sensor LSB (about 1 mg per LSB at +/-2 g) */
} vec3_t;

typedef struct {
    vec3_t mean;
    vec3_t sigma;
    double magnitude;
    double pitch_deg; /* atan2(Y, sqrt(X^2+Z^2)), firmware formula */
    double roll_deg;  /* atan2(X, sqrt(Y^2+Z^2)) */
} pose_t;

typedef struct {
    pose_t pose;
    double firmware_tilt_deg; /* -(pitch - level_pitch); front raised = + */
    double roll_change_deg;
    int largest_axis;
    bool retry;
    const char *verdict;
} result_t;

typedef struct {
    bool active;
    uint count;
    double half_sum[2][3];
    double square_sum[3];
} capture_state_t;

typedef struct {
    int64_t left_forward;
    int64_t right_forward;
    int32_t left_raw;
    int32_t right_raw;
    uint32_t left_invalid;
    uint32_t right_invalid;
} run_delta_t;

static const char *const AXIS_NAME[3] = {"X", "Y", "Z"};

static const char *const POSE_PROMPT[POSE_COUNT] = {
    "1/7 LEVEL: car on the flat floor, all wheels and the caster down",
    "2/7 LEVEL AGAIN: lift the car, set it down again on the same floor",
    "3/7 FRONT RAISED by about 5 to 10 degrees (front = hump-first end)",
    "4/7 FRONT LOWERED by about 5 to 10 degrees",
    "5/7 LEFT SIDE RAISED by about 5 to 10 degrees (seen from behind)",
    "6/7 RIGHT SIDE RAISED by about 5 to 10 degrees (seen from behind)",
    "7/7 ONLY the RIGHT DRIVE WHEEL on a flat block of the hump height; left wheel + caster on the floor",
};

static result_t results[POSE_COUNT];
static uint next_pose;
static capture_state_t capture;
static uint capture_counter;
static uint result_seq;

static bool active;
static uint64_t last_keepalive_us;
static bool imu_ready;
static bool imu_ok;
static bool have_live;
static vec3_t live;
static vec3_t window_sum;
static uint window_count;
static uint64_t next_imu_us;
static uint64_t next_report_us;

static bool counting;
static encoder_sample_t run_start;
static uint run_number;

static uint next_seq(void)
{
    return ++result_seq;
}

/* ------------------------------------------------------------ accelerometer */
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

static bool accelerometer_init(void)
{
    /* Identical to terrain.c: same bus, same registers, so this is a no-op repeat. */
    i2c_init(i2c0, 100000u);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    uint8_t identity = 0u;
    if (!read_registers(0x0fu, &identity, 1u) || identity != 0x33u) {
        return false;
    }
    bool ok = write_register(0x20u, 0x57u);
    ok = write_register(0x23u, 0x08u) && ok;
    return ok;
}

static bool read_acceleration(vec3_t *reading)
{
    uint8_t data[6];
    if (!read_registers(0x28u | 0x80u, data, sizeof data)) {
        return false;
    }
    /* Identical decoding to terrain.c: 12-bit left-justified, shift right 4. */
    reading->v[0] = (double)((int16_t)(((uint16_t)data[1] << 8) | data[0]) >> 4);
    reading->v[1] = (double)((int16_t)(((uint16_t)data[3] << 8) | data[2]) >> 4);
    reading->v[2] = (double)((int16_t)(((uint16_t)data[5] << 8) | data[4]) >> 4);
    return true;
}

static double magnitude_of(const vec3_t *a)
{
    return sqrt(a->v[0] * a->v[0] + a->v[1] * a->v[1] + a->v[2] * a->v[2]);
}

static double pitch_of(const vec3_t *a)
{
    return atan2(a->v[1], sqrt(a->v[0] * a->v[0] + a->v[2] * a->v[2])) * RAD_TO_DEG;
}

static double roll_of(const vec3_t *a)
{
    return atan2(a->v[0], sqrt(a->v[1] * a->v[1] + a->v[2] * a->v[2])) * RAD_TO_DEG;
}

static int largest_axis(const vec3_t *a)
{
    int best = 0;
    for (int i = 1; i < 3; ++i) {
        if (fabs(a->v[i]) > fabs(a->v[best])) {
            best = i;
        }
    }
    return best;
}

/* Rigid three-point model: left wheel and caster on the floor, right wheel
 * raised by HUMP_HEIGHT_MM, caster on the centreline. */
static void expected_hump_tilts(double *pitch_deg, double *roll_deg)
{
    const double a = HUMP_HEIGHT_MM / (2.0 * WHEELBASE_MM);
    const double b = HUMP_HEIGHT_MM / TRACK_WIDTH_MM;
    const double norm = sqrt(1.0 + a * a + b * b);
    *pitch_deg = asin(a / norm) * RAD_TO_DEG;
    *roll_deg = asin(b / norm) * RAD_TO_DEG;
}

static bool pose_from_capture(const capture_state_t *cap, pose_t *pose, const char **reason)
{
    vec3_t first;
    vec3_t second;
    for (int a = 0; a < 3; ++a) {
        first.v[a] = cap->half_sum[0][a] / (CAPTURE_SAMPLES / 2.0);
        second.v[a] = cap->half_sum[1][a] / (CAPTURE_SAMPLES / 2.0);
        pose->mean.v[a] = (cap->half_sum[0][a] + cap->half_sum[1][a]) / CAPTURE_SAMPLES;
        double variance = cap->square_sum[a] / CAPTURE_SAMPLES -
                          pose->mean.v[a] * pose->mean.v[a];
        pose->sigma.v[a] = variance > 0.0 ? sqrt(variance) : 0.0;
    }
    double pitch_difference = fabs(pitch_of(&first) - pitch_of(&second));
    double magnitude_difference = fabs(magnitude_of(&first) - magnitude_of(&second));
    if (pitch_difference > STATIONARY_PITCH_LIMIT_DEG ||
        magnitude_difference > STATIONARY_MAGNITUDE_LIMIT) {
        *reason = "CAR_MOVED";
        return false;
    }
    pose->magnitude = magnitude_of(&pose->mean);
    if (pose->magnitude < GRAVITY_MIN || pose->magnitude > GRAVITY_MAX) {
        *reason = "GRAVITY_OUT_OF_RANGE";
        return false;
    }
    pose->pitch_deg = pitch_of(&pose->mean);
    pose->roll_deg = roll_of(&pose->mean);
    return true;
}

static void evaluate(uint index, result_t *result, const pose_t *level)
{
    result->firmware_tilt_deg = -(result->pose.pitch_deg - level->pitch_deg);
    result->roll_change_deg = result->pose.roll_deg - level->roll_deg;

    vec3_t change;
    for (int a = 0; a < 3; ++a) {
        change.v[a] = result->pose.mean.v[a] - level->mean.v[a];
    }
    result->largest_axis = largest_axis(&change);
    result->verdict = "INFO";
    result->retry = false;

    const double tilt = result->firmware_tilt_deg;
    const double roll = result->roll_change_deg;
    switch (index) {
    case 0:
        result->verdict = "BASELINE";
        break;
    case 1:
        result->verdict = (fabs(tilt) < REPEAT_LIMIT_DEG && fabs(roll) < REPEAT_LIMIT_DEG)
                              ? "PASS: level repeats"
                              : "WARN: level baseline moves between placements";
        break;
    case 2:
        if (fabs(tilt) < MIN_TILT_DEG) {
            result->verdict = "TOO SMALL: raise the front further, then capture again";
            result->retry = true;
        } else if (tilt < 0.0) {
            result->verdict = "FAIL: firmware sign is REVERSED (front raised read negative)";
        } else if (result->largest_axis != 1) {
            result->verdict = "WARN: sign OK but largest change is not on Y";
        } else {
            result->verdict = "PASS: front raised = positive tilt, forward axis = Y";
        }
        break;
    case 3:
        if (fabs(tilt) < MIN_TILT_DEG) {
            result->verdict = "TOO SMALL: lower the front further, then capture again";
            result->retry = true;
        } else if (tilt > 0.0) {
            result->verdict = "FAIL: firmware sign is REVERSED (front lowered read positive)";
        } else if (result->largest_axis != 1) {
            result->verdict = "WARN: sign OK but largest change is not on Y";
        } else {
            result->verdict = "PASS: front lowered = negative tilt, forward axis = Y";
        }
        break;
    case 4:
        if (fabs(roll) < MIN_TILT_DEG) {
            result->verdict = "TOO SMALL: raise the left side further, then capture again";
            result->retry = true;
        } else if (result->largest_axis != 0) {
            result->verdict = "WARN: largest change is not on X (is X the sideways axis?)";
        } else {
            result->verdict = roll > 0.0 ? "RECORD: LEFT raised gives POSITIVE roll"
                                         : "RECORD: LEFT raised gives NEGATIVE roll";
        }
        break;
    case 5: {
        double left_roll = results[4].roll_change_deg;
        if (fabs(roll) < MIN_TILT_DEG) {
            result->verdict = "TOO SMALL: raise the right side further, then capture again";
            result->retry = true;
        } else if (result->largest_axis != 0) {
            result->verdict = "WARN: largest change is not on X (is X the sideways axis?)";
        } else if ((roll > 0.0) == (left_roll > 0.0)) {
            result->verdict = "WARN: right raised has the SAME roll sign as left raised";
        } else {
            result->verdict = roll > 0.0 ? "RECORD: RIGHT raised gives POSITIVE roll"
                                         : "RECORD: RIGHT raised gives NEGATIVE roll";
        }
        break;
    }
    case 6: {
        double expected_pitch;
        double expected_roll;
        expected_hump_tilts(&expected_pitch, &expected_roll);
        double right_roll = results[5].roll_change_deg;
        if (fabs(roll) < 0.5 * expected_roll) {
            result->verdict = "TOO SMALL: right wheel not lifted enough (is it on the block?), then capture again";
            result->retry = true;
        } else if ((roll > 0.0) != (right_roll > 0.0)) {
            result->verdict = "FAIL: roll sign is OPPOSITE to RIGHT RAISED (block under the LEFT wheel, or poses 5/6 swapped)";
        } else if (fabs(fabs(roll) - expected_roll) > MODEL_TOLERANCE_DEG) {
            result->verdict = "WARN: roll does not match the model (check track width, block height, caster centred)";
        } else if (fabs(fabs(tilt) - expected_pitch) > MODEL_TOLERANCE_DEG) {
            result->verdict = "WARN: roll matches but pitch does not (caster off the centreline, or wheelbase wrong)";
        } else {
            result->verdict = tilt < 0.0 ? "PASS: matches model; caster is at the FRONT (front lowered)"
                                         : "PASS: matches model; caster is at the REAR (front raised)";
        }
        break;
    }
    default:
        break;
    }
}

static void print_next_prompt(void)
{
    printf("[MEAS RESULT] seq=%u IMU_NEXT pose=%u total=%u text=%s\n", next_seq(),
           next_pose + 1u, (unsigned)POSE_COUNT, POSE_PROMPT[next_pose]);
}

static void imu_restart(void)
{
    if (capture.active) {
        printf("[MEAS INFO] Capture in progress; wait for it to finish, then restart.\n");
        return;
    }
    next_pose = 0u;
    printf("[MEAS RESULT] seq=%u IMU_RESTART capture=%u\n", next_seq(), capture_counter);
    print_next_prompt();
}

static void imu_begin_capture(void)
{
    if (capture.active) {
        printf("[MEAS INFO] Already capturing; keep the car still.\n");
        return;
    }
    if (!imu_ok) {
        printf("[MEAS INFO] Accelerometer is not responding; check the GY-511 on Grove 3.\n");
        return;
    }
    capture = (capture_state_t){.active = true};
    printf("[MEAS INFO] Capturing pose %u... keep the car completely still for 1 second.\n",
           next_pose + 1u);
}

static void imu_finish_capture(void)
{
    capture.active = false;
    ++capture_counter;
    pose_t pose;
    const char *reason = "";
    if (!pose_from_capture(&capture, &pose, &reason)) {
        printf("[MEAS RESULT] seq=%u IMU_REJECT capture=%u pose=%u reason=%s\n", next_seq(),
               capture_counter, next_pose + 1u, reason);
        printf("[MEAS INFO] Capture rejected (%s). Hold the car still and capture again.\n", reason);
        return;
    }

    result_t *r = &results[next_pose];
    r->pose = pose;
    evaluate(next_pose, r, &results[0].pose);

    double expected_pitch;
    double expected_roll;
    expected_hump_tilts(&expected_pitch, &expected_roll);
    printf("[MEAS RESULT] seq=%u IMU_POSE capture=%u pose=%u x=%.2f y=%.2f z=%.2f "
           "sx=%.2f sy=%.2f sz=%.2f mag=%.2f pitch=%.3f roll=%.3f tilt=%.3f roll_chg=%.3f "
           "axis=%s exp_roll=%.3f exp_pitch=%.3f retry=%u verdict=%s\n",
           next_seq(), capture_counter, next_pose + 1u,
           pose.mean.v[0], pose.mean.v[1], pose.mean.v[2],
           pose.sigma.v[0], pose.sigma.v[1], pose.sigma.v[2],
           pose.magnitude, pose.pitch_deg, pose.roll_deg,
           r->firmware_tilt_deg, r->roll_change_deg, AXIS_NAME[r->largest_axis],
           expected_roll, expected_pitch, r->retry ? 1u : 0u, r->verdict);

    if (next_pose == 0u) {
        int up = largest_axis(&pose.mean);
        printf("[MEAS RESULT] seq=%u IMU_LEVEL vertical=%s value=%.1f level_pitch=%.3f "
               "level_roll=%.3f warn=%s\n",
               next_seq(), AXIS_NAME[up], pose.mean.v[up], pose.pitch_deg, pose.roll_deg,
               up == 1 ? "Y_CARRIES_GRAVITY" : "NONE");
    }

    if (!r->retry) {
        ++next_pose;
    }
    if (next_pose >= POSE_COUNT) {
        printf("[MEAS RESULT] seq=%u IMU_COMPLETE capture=%u\n", next_seq(), capture_counter);
        next_pose = 0u;
    }
    print_next_prompt();
}

static void imu_tick(void)
{
    vec3_t sample;
    if (!imu_ready || !read_acceleration(&sample)) {
        imu_ok = false;
        if (capture.active) {
            capture.active = false;
            ++capture_counter;
            printf("[MEAS RESULT] seq=%u IMU_REJECT capture=%u pose=%u reason=SENSOR_READ_ERROR\n",
                   next_seq(), capture_counter, next_pose + 1u);
        }
        return;
    }
    imu_ok = true;

    for (int a = 0; a < 3; ++a) {
        window_sum.v[a] += sample.v[a];
    }
    if (++window_count >= LIVE_SAMPLES) {
        for (int a = 0; a < 3; ++a) {
            live.v[a] = window_sum.v[a] / LIVE_SAMPLES;
            window_sum.v[a] = 0.0;
        }
        window_count = 0u;
        have_live = true;
    }

    if (capture.active) {
        uint half = capture.count < CAPTURE_SAMPLES / 2u ? 0u : 1u;
        for (int a = 0; a < 3; ++a) {
            capture.half_sum[half][a] += sample.v[a];
            capture.square_sum[a] += sample.v[a] * sample.v[a];
        }
        if (++capture.count >= CAPTURE_SAMPLES) {
            imu_finish_capture();
        }
    }
}

/* ------------------------------------------------------------ encoder runs */
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

static void dist_start(void)
{
    run_start = encoders_snapshot();
    counting = true;
    printf("[MEAS INFO] Distance run started: counts zeroed. Push the car from START to END, then latch the run.\n");
}

static void dist_latch(void)
{
    run_delta_t d = delta_since(&run_start);
    counting = false;
    ++run_number;
    printf("[MEAS RESULT] seq=%u DIST_RUN run=%u left=%lld right=%lld raw_left=%ld raw_right=%ld "
           "inv_left=%lu inv_right=%lu\n",
           next_seq(), run_number, (long long)d.left_forward, (long long)d.right_forward,
           (long)d.left_raw, (long)d.right_raw, (unsigned long)d.left_invalid,
           (unsigned long)d.right_invalid);
}

/* ------------------------------------------------------------ live readout */
static void format_number(char *out, size_t size, bool valid, double value, int decimals)
{
    if (valid) {
        snprintf(out, size, "%.*f", decimals, value);
    } else {
        snprintf(out, size, "-");
    }
}

static void publish_live(void)
{
    char left[24];
    char right[24];
    uint32_t inv_left = 0u;
    uint32_t inv_right = 0u;
    if (counting) {
        run_delta_t d = delta_since(&run_start);
        snprintf(left, sizeof left, "%lld", (long long)d.left_forward);
        snprintf(right, sizeof right, "%lld", (long long)d.right_forward);
        inv_left = d.left_invalid;
        inv_right = d.right_invalid;
    } else {
        snprintf(left, sizeof left, "-");
        snprintf(right, sizeof right, "-");
    }

    const bool imu_valid = imu_ok && have_live;
    const bool baseline = next_pose > 0u;
    char x[16], y[16], z[16], pitch[16], roll[16], tilt[16], roll_change[16];
    format_number(x, sizeof x, imu_valid, live.v[0], 1);
    format_number(y, sizeof y, imu_valid, live.v[1], 1);
    format_number(z, sizeof z, imu_valid, live.v[2], 1);
    format_number(pitch, sizeof pitch, imu_valid, imu_valid ? pitch_of(&live) : 0.0, 2);
    format_number(roll, sizeof roll, imu_valid, imu_valid ? roll_of(&live) : 0.0, 2);
    format_number(tilt, sizeof tilt, imu_valid && baseline,
                  imu_valid ? -(pitch_of(&live) - results[0].pose.pitch_deg) : 0.0, 2);
    format_number(roll_change, sizeof roll_change, imu_valid && baseline,
                  imu_valid ? roll_of(&live) - results[0].pose.roll_deg : 0.0, 2);

    char line[300];
    snprintf(line, sizeof line,
             "[MEAS LIVE] dist=%s left=%s right=%s inv_left=%lu inv_right=%lu imu=%s x=%s y=%s z=%s "
             "pitch=%s roll=%s tilt=%s roll_chg=%s pose=%u capturing=%u\n",
             counting ? "COUNTING" : "IDLE", left, right, (unsigned long)inv_left,
             (unsigned long)inv_right, imu_ok ? "OK" : "ERR", x, y, z, pitch, roll, tilt,
             roll_change, next_pose + 1u, capture.active ? 1u : 0u);
    calibration_wifi_set_measure_live(line);
}

/* ------------------------------------------------------------ public interface */
static void deactivate(void)
{
    active = false;
    counting = false;
    capture.active = false;
    window_count = 0u;
    have_live = false;
    calibration_wifi_set_measure_live("");
}

void measurement_init(void)
{
    imu_ready = accelerometer_init();
    if (!imu_ready) {
        printf("[MEAS INFO] Accelerometer not available for measurements (check the GY-511 on Grove 3).\n");
    }
}

bool measurement_command(int ch, bool idle, uint64_t now_us)
{
    if (ch != 'm' && ch != 'e' && ch != 'g' && ch != 'h' && ch != 'p' && ch != 'w') {
        return false;
    }
    if (!idle) {
        printf("[MEAS INFO] Measurement command '%c' refused: stop the car and wait for it to settle.\n", ch);
        return true;
    }
    if (ch == 'e') {
        if (active) {
            deactivate();
        }
        return true;
    }
    if (ch == 'm') {
        last_keepalive_us = now_us;
        if (!active) {
            active = true;
            next_imu_us = now_us;
            next_report_us = now_us;
            print_next_prompt();
        }
        return true;
    }
    if (!active) {
        printf("[MEAS INFO] Open the Measurements tab first; command '%c' ignored.\n", ch);
        return true;
    }
    last_keepalive_us = now_us;
    switch (ch) {
    case 'g':
        dist_start();
        break;
    case 'h':
        if (counting) {
            dist_latch();
        } else {
            printf("[MEAS INFO] No distance run in progress.\n");
        }
        break;
    case 'p':
        imu_begin_capture();
        break;
    case 'w':
        imu_restart();
        break;
    default:
        break;
    }
    return true;
}

void measurement_poll(bool idle, uint64_t now_us)
{
    if (!active) {
        return;
    }
    if (!idle || now_us - last_keepalive_us > (uint64_t)KEEPALIVE_TIMEOUT_US) {
        /* A run started, or the browser tab went away: go silent and drop any
         * half-finished capture. Results already printed are kept by the page. */
        deactivate();
        return;
    }
    if (now_us >= next_imu_us) {
        next_imu_us = now_us + IMU_SAMPLE_US;
        imu_tick();
    }
    if (now_us >= next_report_us) {
        next_report_us = now_us + LIVE_REPORT_US;
        publish_live();
    }
}
#endif /* ROBOCAR_MEASUREMENTS */
