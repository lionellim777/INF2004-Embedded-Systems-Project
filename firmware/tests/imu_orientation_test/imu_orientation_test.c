/*
 * INF2004 GY-511 (LSM303DLHC accelerometer) orientation and level-baseline
 * check. Motors and servo are OFF.
 *
 * Purpose: establish, on THIS car, which sensor axes are forward / sideways /
 * up, record the stationary level reading, and confirm the sign convention the
 * terrain firmware assumes:
 *     pitch      = atan2(Y, sqrt(X^2 + Z^2))
 *     tilt       = -(pitch - level_pitch)   -> positive when the FRONT is raised
 * (firmware/app/robot_firmware/terrain.c, imu_hump_test.c).
 *
 * Wiring: GY-511 on Grove 3, SDA = GP4, SCL = GP5. Button: GP20.
 *
 * GP20 walks through seven fixed poses. Each press averages one second of
 * samples and rejects the capture if the car moved. "Front" is the end that
 * goes over a hump first when the car drives forward. "Left"/"right" are seen
 * from behind the car, looking in the direction of travel.
 *
 *   1 LEVEL            all wheels + caster on the flat floor (the baseline)
 *   2 LEVEL AGAIN      lift the car and put it back down: repeatability
 *   3 FRONT RAISED     front up by roughly 5 to 10 degrees
 *   4 FRONT LOWERED    front down by roughly 5 to 10 degrees
 *   5 LEFT RAISED      left side up by roughly 5 to 10 degrees
 *   6 RIGHT RAISED     right side up by roughly 5 to 10 degrees
 *   7 RIGHT WHEEL ON A HUMP-HEIGHT STEP: only the right drive wheel on a
 *     flat block HUMP_HEIGHT_MM high; left wheel and caster on the floor.
 *
 * Poses 3 to 6 tilt the whole car rigidly (wedge or block under one end/side;
 * do not let the sensor board flex). Pose 7 is the real hump geometry: the car
 * rests on three points (left wheel, right wheel, caster). With the right
 * wheel lifted by h, the caster on the centreline at distance W from the axle
 * and track T, the chassis plane has
 *     roll  slope = h / T            (right side raised)
 *     pitch slope = h / (2 W)        (front lowered if the caster is at the front)
 * Pose 7 compares the measured angles with that model.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

enum {
    I2C_SDA_PIN = 4,
    I2C_SCL_PIN = 5,
    MOTOR_1_A_PIN = 8,
    MOTOR_1_B_PIN = 9,
    MOTOR_2_A_PIN = 10,
    MOTOR_2_B_PIN = 11,
    SERVO_SIGNAL_PIN = 12,
    BUTTON_PIN = 20,
    ACCEL_ADDRESS = 0x19,
    CAPTURE_SAMPLES = 100, /* even; 1 s at 10 ms spacing */
    LIVE_SAMPLES = 8,
    TICK_MS = 10,
    LIVE_REPORT_MS = 250,
    POSE_COUNT = 7,
};

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
static const double TRACK_WIDTH_MM = 110.0;  /* tyre-contact centre to centre */
static const double WHEELBASE_MM = 80.0;     /* drive-axle line to caster contact */
static const double HUMP_HEIGHT_MM = 10.0;   /* peak height under the right wheel */

typedef struct {
    double v[3]; /* X, Y, Z in sensor LSB (about 1 mg per LSB at +/-2 g) */
} vec3_t;

typedef struct {
    vec3_t mean;
    vec3_t sigma;
    double magnitude;
    double pitch_deg; /* atan2(Y, sqrt(X^2+Z^2)), firmware formula */
    double roll_deg;  /* atan2(X, sqrt(Y^2+Z^2)), this tool only */
} pose_t;

typedef struct {
    pose_t pose;
    double firmware_tilt_deg; /* -(pitch - level_pitch); front raised = + */
    double roll_change_deg;
    int largest_axis;
    bool retry; /* tilt too small to judge: stay on this pose */
    const char *verdict;
} result_t;

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

/* Rigid three-point model: left wheel and caster on the floor, right wheel
 * raised by HUMP_HEIGHT_MM, caster on the centreline. Because the tool's pitch
 * and roll are asin() of the gravity components, the expected readings are the
 * asin of the plane's up-vector components (exact, not small-angle). */
static void expected_hump_tilts(double *pitch_deg, double *roll_deg)
{
    const double a = HUMP_HEIGHT_MM / (2.0 * WHEELBASE_MM);
    const double b = HUMP_HEIGHT_MM / TRACK_WIDTH_MM;
    const double norm = sqrt(1.0 + a * a + b * b);
    *pitch_deg = asin(a / norm) * RAD_TO_DEG;
    *roll_deg = asin(b / norm) * RAD_TO_DEG;
}

static result_t results[POSE_COUNT];
static uint next_pose;

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
    i2c_init(i2c0, 100000u);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    uint8_t identity = 0u;
    if (!read_registers(0x0fu, &identity, 1u)) {
        printf("ERROR: accelerometer did not answer on I2C (GP4/GP5).\n");
        return false;
    }
    printf("Accelerometer WHO_AM_I=0x%02x (expected 0x33)\n", identity);
    if (identity != 0x33u) {
        return false;
    }
    /* Identical to terrain.c: 100 Hz, X/Y/Z on, high-resolution, +/-2 g. */
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

/* GP20 is active low (pull-up). Once per press, after three consecutive
 * 10 ms ticks at the new level. */
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

/* One second of stationary samples; applies terrain_calibrate()'s checks. */
static bool capture_pose(pose_t *pose)
{
    double half_sum[2][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
    double square_sum[3] = {0.0, 0.0, 0.0};

    for (uint i = 0u; i < CAPTURE_SAMPLES; ++i) {
        vec3_t r;
        if (!read_acceleration(&r)) {
            printf("CAPTURE FAILED: accelerometer read error.\n");
            return false;
        }
        uint half = i < CAPTURE_SAMPLES / 2u ? 0u : 1u;
        for (int a = 0; a < 3; ++a) {
            half_sum[half][a] += r.v[a];
            square_sum[a] += r.v[a] * r.v[a];
        }
        sleep_ms(TICK_MS);
    }

    vec3_t first;
    vec3_t second;
    for (int a = 0; a < 3; ++a) {
        first.v[a] = half_sum[0][a] / (CAPTURE_SAMPLES / 2.0);
        second.v[a] = half_sum[1][a] / (CAPTURE_SAMPLES / 2.0);
        pose->mean.v[a] = (half_sum[0][a] + half_sum[1][a]) / CAPTURE_SAMPLES;
        double variance = square_sum[a] / CAPTURE_SAMPLES -
                          pose->mean.v[a] * pose->mean.v[a];
        pose->sigma.v[a] = variance > 0.0 ? sqrt(variance) : 0.0;
    }

    double pitch_difference = fabs(pitch_of(&first) - pitch_of(&second));
    double magnitude_difference = fabs(magnitude_of(&first) - magnitude_of(&second));
    if (pitch_difference > STATIONARY_PITCH_LIMIT_DEG ||
        magnitude_difference > STATIONARY_MAGNITUDE_LIMIT) {
        printf("CAPTURE REJECTED: car moved (pitch changed %.2f deg, magnitude %.1f). "
               "Hold it still and press GP20 again.\n",
               pitch_difference, magnitude_difference);
        return false;
    }

    pose->magnitude = magnitude_of(&pose->mean);
    if (pose->magnitude < GRAVITY_MIN || pose->magnitude > GRAVITY_MAX) {
        printf("CAPTURE REJECTED: gravity magnitude %.1f outside %.0f..%.0f.\n",
               pose->magnitude, GRAVITY_MIN, GRAVITY_MAX);
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
            result->verdict = "TOO SMALL: raise the front further, then press GP20 again";
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
            result->verdict = "TOO SMALL: lower the front further, then press GP20 again";
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
            result->verdict = "TOO SMALL: raise the left side further, then press GP20 again";
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
            result->verdict = "TOO SMALL: raise the right side further, then press GP20 again";
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
            result->verdict = "TOO SMALL: right wheel not lifted enough (is it on the block?), then press GP20 again";
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

static void print_result_line(uint index, const result_t *r)
{
    const pose_t *p = &r->pose;
    printf("  X=%8.2f Y=%8.2f Z=%8.2f | |g|=%7.2f | sigma %.2f %.2f %.2f\n",
           p->mean.v[0], p->mean.v[1], p->mean.v[2], p->magnitude,
           p->sigma.v[0], p->sigma.v[1], p->sigma.v[2]);
    printf("  pitch=%+7.3f deg  roll=%+7.3f deg\n", p->pitch_deg, p->roll_deg);
    if (index > 0u) {
        printf("  vs LEVEL: firmware tilt (front raised = +) = %+7.3f deg | "
               "roll change = %+7.3f deg | largest axis change = %s\n",
               r->firmware_tilt_deg, r->roll_change_deg, AXIS_NAME[r->largest_axis]);
    }
    if (index == 6u) {
        double expected_pitch;
        double expected_roll;
        expected_hump_tilts(&expected_pitch, &expected_roll);
        printf("  model (track %.1f, wheelbase %.1f, height %.1f mm): expected |roll| %.3f deg, "
               "|pitch| %.3f deg\n",
               TRACK_WIDTH_MM, WHEELBASE_MM, HUMP_HEIGHT_MM, expected_roll, expected_pitch);
        printf("  measured: |roll| %.3f (error %+.3f)  |tilt| %.3f (error %+.3f) deg; "
               "tilt sign %s\n",
               fabs(r->roll_change_deg), fabs(r->roll_change_deg) - expected_roll,
               fabs(r->firmware_tilt_deg), fabs(r->firmware_tilt_deg) - expected_pitch,
               r->firmware_tilt_deg < 0.0 ? "NEGATIVE (front lowered)" : "POSITIVE (front raised)");
    }
    printf("  => %s\n", r->verdict);
}

static void print_level_analysis(const pose_t *level)
{
    int up = largest_axis(&level->mean);
    printf("  Vertical axis at level: %s (reads %+.1f; a resting accelerometer reads "
           "+1 g on the axis pointing UP).\n",
           AXIS_NAME[up], level->mean.v[up]);
    if (up == 1) {
        printf("  WARN: Y carries gravity at level. The firmware pitch formula assumes "
               "gravity mainly on X/Z with Y along the car. Re-check the mounting.\n");
    }
    printf("  Level pitch %.3f deg, roll %.3f deg (non-zero mostly reflects sensor "
           "mounting, not the car being unlevel).\n",
           level->pitch_deg, level->roll_deg);
}

static void print_summary(void)
{
    printf("\n=========== SUMMARY (copy this block) ===========\n");
    for (uint i = 0u; i < POSE_COUNT; ++i) {
        const result_t *r = &results[i];
        printf("%u: X=%.1f Y=%.1f Z=%.1f pitch=%+.3f roll=%+.3f tilt_fw=%+.3f "
               "roll_chg=%+.3f axis=%s | %s\n",
               i + 1u, r->pose.mean.v[0], r->pose.mean.v[1], r->pose.mean.v[2],
               r->pose.pitch_deg, r->pose.roll_deg, r->firmware_tilt_deg,
               r->roll_change_deg, AXIS_NAME[r->largest_axis], r->verdict);
    }
    print_level_analysis(&results[0].pose);
    printf("=================================================\n");
    printf("Sequence complete. Press GP20 to start again (a new LEVEL baseline).\n\n");
}

static void print_help(void)
{
    printf("\n=== INF2004 IMU ORIENTATION / LEVEL BASELINE | %s %s ===\n", __DATE__, __TIME__);
    printf("MOTORS OFF: GP8-GP12 are held LOW.\n");
    printf("Firmware convention under test: tilt = -(pitch - level_pitch), positive when "
           "the FRONT is raised.\n");
    {
        double expected_pitch;
        double expected_roll;
        expected_hump_tilts(&expected_pitch, &expected_roll);
        printf("Hump layout: only the RIGHT wheel rides the hump, so the signal is mainly ROLL.\n");
        printf("Model for a %.1f mm rise (track %.1f, wheelbase %.1f): roll %.2f deg, pitch %.2f deg.\n",
               HUMP_HEIGHT_MM, TRACK_WIDTH_MM, WHEELBASE_MM, expected_roll, expected_pitch);
    }
    printf("Press GP20 once per pose; hold the car still for about 1 second after each press.\n");
    printf("'Front' = the end that goes over a hump first. 'Left/right' = seen from behind.\n\n");
}

static void print_prompt(void)
{
    printf("NEXT POSE %s\n      Hold still, then press GP20.\n", POSE_PROMPT[next_pose]);
}

int main(void)
{
    hold_outputs_low();
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    stdio_init_all();

    bool usb_was_connected = false;
    bool sensor_ready = false;
    vec3_t window_sum = {{0.0, 0.0, 0.0}};
    uint window_count = 0u;
    bool have_live = false;
    vec3_t live = {{0.0, 0.0, 0.0}};
    uint64_t next_report_ms = 0u;

    while (true) {
        sleep_ms(TICK_MS);
        uint64_t now_ms = to_ms_since_boot(get_absolute_time());

        bool connected = stdio_usb_connected();
        if (connected && !usb_was_connected) {
            print_help();
            sensor_ready = accelerometer_init();
            if (!sensor_ready) {
                printf("STOP: GY-511 accelerometer not initialised. Check Grove 3 wiring.\n");
            } else {
                next_pose = 0u;
                print_prompt();
            }
        }
        usb_was_connected = connected;
        if (!sensor_ready) {
            continue;
        }

        if (button_pressed_event()) {
            printf("\nCAPTURING pose %u... keep the car still.\n", next_pose + 1u);
            result_t *r = &results[next_pose];
            if (capture_pose(&r->pose)) {
                evaluate(next_pose, r, &results[0].pose);
                print_result_line(next_pose, r);
                if (next_pose == 0u) {
                    print_level_analysis(&r->pose);
                }
                if (!r->retry) {
                    ++next_pose;
                }
                if (next_pose >= POSE_COUNT) {
                    print_summary();
                    next_pose = 0u;
                }
                print_prompt();
            } else {
                print_prompt();
            }
            window_count = 0u;
            window_sum = (vec3_t){{0.0, 0.0, 0.0}};
            have_live = false;
            next_report_ms = to_ms_since_boot(get_absolute_time()) + LIVE_REPORT_MS;
            continue;
        }

        vec3_t sample;
        if (read_acceleration(&sample)) {
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
        }

        if (have_live && now_ms >= next_report_ms) {
            next_report_ms = now_ms + LIVE_REPORT_MS;
            double pitch = pitch_of(&live);
            double roll = roll_of(&live);
            if (next_pose > 0u) {
                const pose_t *level = &results[0].pose;
                printf("live X=%7.1f Y=%7.1f Z=%7.1f | tilt_fw=%+6.2f roll_chg=%+6.2f deg\n",
                       live.v[0], live.v[1], live.v[2],
                       -(pitch - level->pitch_deg), roll - level->roll_deg);
            } else {
                printf("live X=%7.1f Y=%7.1f Z=%7.1f | pitch=%+6.2f roll=%+6.2f deg (no baseline yet)\n",
                       live.v[0], live.v[1], live.v[2], pitch, roll);
            }
        }
    }
}
