#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "terrain.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* Exercise the actual driver/state-machine code with deterministic IMU data.
 * Only the standalone test replaces I2C reads and the driver capture clock. */
static uint32_t test_now_ms;
static uint8_t test_register;
static bool test_read_failed;
static int16_t test_y;
static int16_t test_z = 1000;

static int test_write(i2c_inst_t *i2c, uint8_t address, const uint8_t *data,
                      size_t length, bool nostop, uint timeout_us)
{
    (void)i2c; (void)address; (void)nostop; (void)timeout_us;
    if (test_read_failed) { return -1; }
    if (length == 1u) { test_register = data[0]; }
    return (int)length;
}

static int test_read(i2c_inst_t *i2c, uint8_t address, uint8_t *data,
                     size_t length, bool nostop, uint timeout_us)
{
    (void)i2c; (void)address; (void)nostop; (void)timeout_us;
    if (test_read_failed) { return -1; }
    if (test_register == 0x0fu && length == 1u) {
        data[0] = 0x33u;
    } else if (test_register == 0xa8u && length == 6u) {
        int16_t axes[3] = {0, test_y, test_z};
        for (unsigned int i = 0u; i < 3u; ++i) {
            uint16_t raw = (uint16_t)((int32_t)axes[i] * 16);
            data[i * 2u] = (uint8_t)raw;
            data[i * 2u + 1u] = (uint8_t)(raw >> 8u);
        }
    } else { return -1; }
    return (int)length;
}

#define i2c_write_timeout_us test_write
#define i2c_read_timeout_us test_read
#define to_ms_since_boot(ignored) test_now_ms
#define sleep_ms(ignored) ((void)0)
#include "../../app/robot_firmware/terrain.c"
#undef i2c_write_timeout_us
#undef i2c_read_timeout_us
#undef to_ms_since_boot
#undef sleep_ms

static void feed_tilt(float degrees, unsigned int count, bool refresh_motion)
{
    test_y = (int16_t)lroundf(-1000.0f * sinf(degrees * 0.01745329252f));
    test_z = (int16_t)lroundf(1000.0f * cosf(degrees * 0.01745329252f));
    for (unsigned int i = 0u; i < count; ++i) {
        test_now_ms += 20u;
        if (refresh_motion && test_now_ms % 200u == 0u) {
            assert(terrain_set_motion(test_now_ms, 100, 100, true));
        }
        terrain_poll();
    }
}

static void test_terrain_behaviour(void)
{
    assert(terrain_init());
    assert(terrain_calibrate());
    feed_tilt(0.0f, 40u, true);
    feed_tilt(30.0f, 80u, true);
    feed_tilt(-30.0f, 80u, true);
    feed_tilt(0.0f, 40u, true);
    terrain_status_t first = terrain_get_status();
    assert(first.hump_count == 1u && first.highest_height_valid);
    assert(first.highest_hump_mm > 30u && first.highest_hump_mm < 100u);
    assert(first.current_hump_mm == 0u);

    feed_tilt(30.0f, 110u, true);
    feed_tilt(-30.0f, 110u, true);
    feed_tilt(0.0f, 40u, true);
    terrain_status_t second = terrain_get_status();
    assert(second.hump_count == 2u);
    assert(second.highest_hump_mm > first.highest_hump_mm);
    assert(second.highest_height_hump_number == 2u);

    feed_tilt(20.0f, 80u, false);
    feed_tilt(-20.0f, 80u, false);
    feed_tilt(0.0f, 40u, false);
    terrain_status_t stale = terrain_get_status();
    assert(stale.hump_count == 3u && !stale.height_valid);
    assert(stale.highest_hump_mm == second.highest_hump_mm);
    assert(!terrain_set_motion(test_now_ms, 501, 0, true));
    assert(!terrain_set_motion(1800001u, 0, 0, true));
    assert(terrain_set_motion(test_now_ms, -500, 500, true));
    assert(motion_sample.left_mm_s == -500 && motion_sample.right_mm_s == 500);
    assert(!terrain_set_motion(test_now_ms - 1u, 0, 0, true));
    assert(!motion_sample.valid);

    test_y = 0; test_z = 1600;
    test_now_ms += 20u;
    terrain_poll();
    assert(terrain_get_status().motion == TERRAIN_MOTION_IMPACT);
    assert(!terrain_get_status().height_valid);
    assert(terrain_get_status().hump_count == 3u);

    robot_fault_update(ROBOT_FAULT_ENCODER | ROBOT_FAULT_WIFI, true);
    robot_fault_update(ROBOT_FAULT_WIFI, false);
    assert(robot_fault_get() == ROBOT_FAULT_ENCODER);
    test_read_failed = true;
    test_now_ms += 20u;
    terrain_poll();
    terrain_status_t failed = terrain_get_status();
    assert(!failed.calibrated && !failed.sample_valid && !failed.height_valid);
    assert(failed.faults == (ROBOT_FAULT_ENCODER | ROBOT_FAULT_IMU));
    test_read_failed = false;
    test_y = 0; test_z = 1000;
    assert(terrain_calibrate());
    assert(robot_fault_get() == ROBOT_FAULT_ENCODER);
    robot_fault_update(ROBOT_FAULT_ENCODER, false);

    test_now_ms = 1800001u;
    terrain_poll();
    assert(!terrain_get_status().calibrated);
    puts("PASS actual terrain logic: humps, height, stale motion, faults and session bounds");
}

static volatile uint32_t checksum;

int main(void)
{
#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
    for (unsigned int pin = 8u; pin <= 12u; ++pin) {
        gpio_init(pin);
        gpio_put(pin, 0);
        gpio_set_dir(pin, GPIO_OUT);
    }
    stdio_init_all();
    sleep_ms(3000);
#endif
    terrain_status_t s = {
        .available = 1u,
        .calibrated = 1u,
        .sample_valid = 1u,
        .height_valid = 1u,
        .highest_height_valid = 1u,
        .motion = 7u,
        .hump_count = 3000u,
        .highest_hump_number = 2999u,
        .highest_height_hump_number = 3000u,
        .highest_peak_angle_cdeg = 9000u,
        .current_hump_mm = 5000u,
        .highest_hump_mm = 5000u,
        .captured_ms = 1800000u,
        .faults = 31u,
    };
    const uint8_t golden[14] = {0xffu, 0xb8u, 0x7bu, 0xbbu, 0xb8u, 0x8bu, 0x32u, 0x22u, 0x4eu, 0xc4u, 0x09u, 0x74u, 0xb7u, 0x3fu};
    uint8_t output[14];
    assert(sizeof(terrain_status_t) == 14u);
    assert(terrain_encode_status(&s, output, sizeof output) == 14u);
    assert(memcmp(output, golden, sizeof output) == 0);
    assert((output[13] & 0xc0u) == 0u);
    assert(terrain_unpack_bits(output, 8u, 12u) == 3000u);
    assert(terrain_unpack_bits(output, 84u, 21u) == 1800000u);
    assert(terrain_unpack_bits(output, 105u, 5u) == 31u);
    puts("PASS target size, independent golden bytes and padding bits");

    memset(output, 0xa5, sizeof output);
    assert(terrain_encode_status(&s, output, 13u) == 0u);
    for (unsigned int i = 0u; i < sizeof output; ++i) {
        assert(output[i] == 0xa5u);
    }
    assert(terrain_encode_status(NULL, output, sizeof output) == 0u);
    assert(terrain_encode_status(&s, NULL, sizeof output) == 0u);
    s.current_hump_mm = 5001u;
    assert(terrain_encode_status(&s, output, sizeof output) == 0u);
    s.current_hump_mm = 5000u;
    s.captured_ms = 1800001u;
    assert(terrain_encode_status(&s, output, sizeof output) == 0u);
    s.captured_ms = 1800000u;
    s.highest_hump_number = 3001u;
    assert(terrain_encode_status(&s, output, sizeof output) == 0u);
    s.highest_hump_number = 2999u;
    s.highest_peak_angle_cdeg = 9001u;
    assert(terrain_encode_status(&s, output, sizeof output) == 0u);
    s.highest_peak_angle_cdeg = 9000u;
    puts("PASS invalid ranges, short buffers and null arguments");

    /* Every signed 12-bit sensor value at every actual filter axis offset. */
    uint8_t filter[36];
    for (unsigned int axis = 0u; axis < 24u; ++axis) {
        for (int32_t value = -2048; value <= 2047; ++value) {
            memset(filter, 0xa5, sizeof filter);
            terrain_pack_bits(filter, axis * 12u, 12u, (uint32_t)value & 0xfffu);
            int32_t decoded = (int32_t)terrain_unpack_bits(filter, axis * 12u, 12u);
            if (decoded >= 2048) {
                decoded -= 4096;
            }
            assert(decoded == value);
            for (unsigned int bit = 0u; bit < 288u; ++bit) {
                if (bit < axis * 12u || bit >= axis * 12u + 12u) {
                    assert(((filter[bit / 8u] >> (bit % 8u)) & 1u) ==
                           ((0xa5u >> (bit % 8u)) & 1u));
                }
            }
        }
    }
    puts("PASS all raw signed sensor values and neighbouring packed fields");

#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
    uint64_t started = time_us_64();
#endif
    for (uint32_t i = 0u; i < 10000u; ++i) {
        s.captured_ms = i;
        assert(terrain_encode_status(&s, output, sizeof output) == 14u);
        checksum += output[10];
    }
#if defined(PICO_ON_DEVICE) && PICO_ON_DEVICE
    printf("10000 binary encodes: %llu us; checksum=%lu\n",
           (unsigned long long)(time_us_64() - started), (unsigned long)checksum);
#endif
    test_terrain_behaviour();
    puts("ALL TERRAIN REPRESENTATION TESTS PASSED");
    return 0;
}
