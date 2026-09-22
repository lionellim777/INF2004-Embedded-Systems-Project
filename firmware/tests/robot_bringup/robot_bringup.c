/*
 * INF2004 robot bring-up test
 *
 * Current wiring:
 *   Ultrasonic HC-SR04+ : TRIG GP0, ECHO GP1
 *   GY-511 / LSM303DLHC: SDA GP4, SCL GP5
 *   Motor 1             : M1A GP8, M1B GP9
 *   Motor 2             : M2A GP10, M2B GP11
 *   Servo 1             : signal GP12
 *   IR line sensor 1    : AO GP27 (Grove 6)
 *   IR line sensor 2    : AO GP28 (Grove 7)
 *   IR barcode sensor   : DO GP2  (Grove 2)
 *   Robo Pico buttons   : GP20 motor test, GP21 servo test
 *
 * IMPORTANT: Raise the driven wheels off the table before pressing GP20.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

enum {
    ULTRASONIC_TRIG_PIN = 0,
    ULTRASONIC_ECHO_PIN = 1,
    IR_3_DIGITAL_PIN = 2,
    I2C_SDA_PIN = 4,
    I2C_SCL_PIN = 5,
    MOTOR_1_A_PIN = 8,
    MOTOR_1_B_PIN = 9,
    MOTOR_2_A_PIN = 10,
    MOTOR_2_B_PIN = 11,
    SERVO_1_PIN = 12,
    MOTOR_TEST_BUTTON_PIN = 20,
    SERVO_TEST_BUTTON_PIN = 21,
    IR_1_ANALOG_PIN = 27,
    IR_2_ANALOG_PIN = 28,
};

enum {
    LSM303_ACCEL_ADDRESS = 0x19,
    LSM303_MAG_ADDRESS = 0x1e,
};

typedef struct {
    uint slice;
    uint channel_a;
    uint channel_b;
} motor_t;

static motor_t motor_1;
static motor_t motor_2;
static uint16_t motor_wrap;

static uint servo_slice;
static uint servo_channel;

static void motor_stop(motor_t *motor)
{
    pwm_set_chan_level(motor->slice, motor->channel_a, 0);
    pwm_set_chan_level(motor->slice, motor->channel_b, 0);
}

static void motor_set(motor_t *motor, float throttle)
{
    if (throttle > 1.0f) {
        throttle = 1.0f;
    } else if (throttle < -1.0f) {
        throttle = -1.0f;
    }

    uint16_t level = (uint16_t)((float)(motor_wrap + 1u) *
                                (throttle < 0.0f ? -throttle : throttle));

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
    /* 125 MHz / (6249 + 1) = 20 kHz at the default system clock. */
    motor_wrap = (uint16_t)(clock_get_hz(clk_sys) / 20000u - 1u);
    motor_init_one(&motor_1, MOTOR_1_A_PIN, MOTOR_1_B_PIN);
    motor_init_one(&motor_2, MOTOR_2_A_PIN, MOTOR_2_B_PIN);
}

static void ir_sensors_init(void)
{
    adc_init();
    adc_gpio_init(IR_1_ANALOG_PIN);
    adc_gpio_init(IR_2_ANALOG_PIN);

    gpio_init(IR_3_DIGITAL_PIN);
    gpio_set_dir(IR_3_DIGITAL_PIN, GPIO_IN);
}

static uint16_t ir_sensor_read(uint gpio_pin)
{
    adc_select_input(gpio_pin - 26u);
    sleep_us(5);

    uint32_t total = 0;
    for (uint sample = 0; sample < 16u; ++sample) {
        total += adc_read();
    }
    return (uint16_t)(total / 16u);
}

static void servo_set_angle(uint angle_degrees)
{
    if (angle_degrees > 180u) {
        angle_degrees = 180u;
    }

    /* Map 0..180 degrees to a conservative 500..2500 us pulse. */
    uint16_t pulse_us = (uint16_t)(500u + (angle_degrees * 2000u) / 180u);
    pwm_set_chan_level(servo_slice, servo_channel, pulse_us);
}

static void servo_init(void)
{
    gpio_set_function(SERVO_1_PIN, GPIO_FUNC_PWM);
    servo_slice = pwm_gpio_to_slice_num(SERVO_1_PIN);
    servo_channel = pwm_gpio_to_channel(SERVO_1_PIN);

    /* Divide the system clock to 1 MHz, then use 20,000 ticks for 50 Hz. */
    float divider = (float)clock_get_hz(clk_sys) / 1000000.0f;
    pwm_set_clkdiv(servo_slice, divider);
    pwm_set_wrap(servo_slice, 19999u);
    pwm_set_chan_level(servo_slice, servo_channel, 1500u);
    pwm_set_enabled(servo_slice, true);
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

    uint64_t pulse_start = time_us_64();
    deadline = pulse_start + 30000u;
    while (gpio_get(ULTRASONIC_ECHO_PIN)) {
        if (time_us_64() >= deadline) {
            return false;
        }
        tight_loop_contents();
    }

    uint64_t pulse_width_us = time_us_64() - pulse_start;
    *distance_cm = (float)pulse_width_us * 0.0343f / 2.0f;
    return true;
}

static bool i2c_write_register(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t message[2] = {reg, value};
    return i2c_write_blocking(i2c0, address, message, 2, false) == 2;
}

static bool i2c_read_registers(uint8_t address, uint8_t reg,
                               uint8_t *data, size_t length)
{
    if (i2c_write_blocking(i2c0, address, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(i2c0, address, data, length, false) == (int)length;
}

static bool gy511_init(void)
{
    i2c_init(i2c0, 100000u);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    uint8_t who_am_i = 0;
    uint8_t magnetic_id[3] = {0};

    bool accel_found = i2c_read_registers(LSM303_ACCEL_ADDRESS, 0x0f,
                                          &who_am_i, 1);
    bool mag_found = i2c_read_registers(LSM303_MAG_ADDRESS, 0x0a,
                                        magnetic_id, sizeof magnetic_id);

    printf("GY-511 accelerometer: %s, WHO_AM_I=0x%02x (expected 0x33)\n",
           accel_found ? "responding" : "NOT FOUND", who_am_i);
    printf("GY-511 magnetometer:  %s, ID=%c%c%c (expected H43)\n",
           mag_found ? "responding" : "NOT FOUND",
           magnetic_id[0] ? magnetic_id[0] : '?',
           magnetic_id[1] ? magnetic_id[1] : '?',
           magnetic_id[2] ? magnetic_id[2] : '?');

    if (!accel_found || who_am_i != 0x33u || !mag_found) {
        return false;
    }

    /* Accelerometer: 100 Hz, all axes, high-resolution, +/-2 g. */
    bool ok = i2c_write_register(LSM303_ACCEL_ADDRESS, 0x20, 0x57);
    ok = i2c_write_register(LSM303_ACCEL_ADDRESS, 0x23, 0x08) && ok;

    /* Magnetometer: 30 Hz, +/-1.3 gauss, continuous conversion. */
    ok = i2c_write_register(LSM303_MAG_ADDRESS, 0x00, 0x14) && ok;
    ok = i2c_write_register(LSM303_MAG_ADDRESS, 0x01, 0x20) && ok;
    ok = i2c_write_register(LSM303_MAG_ADDRESS, 0x02, 0x00) && ok;
    return ok;
}

static bool gy511_read_acceleration(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t data[6];
    /* Bit 7 enables register-address auto-increment. */
    if (!i2c_read_registers(LSM303_ACCEL_ADDRESS, 0x28u | 0x80u,
                            data, sizeof data)) {
        return false;
    }

    *x = (int16_t)((uint16_t)data[1] << 8 | data[0]) >> 4;
    *y = (int16_t)((uint16_t)data[3] << 8 | data[2]) >> 4;
    *z = (int16_t)((uint16_t)data[5] << 8 | data[4]) >> 4;
    return true;
}

static void run_motor_test(void)
{
    printf("Motor 1: direction A at 35%%\n");
    motor_set(&motor_1, 0.35f);
    sleep_ms(700);
    motor_stop(&motor_1);
    sleep_ms(500);

    printf("Motor 1: direction B at 35%%\n");
    motor_set(&motor_1, -0.35f);
    sleep_ms(700);
    motor_stop(&motor_1);
    sleep_ms(500);

    printf("Motor 2: direction A at 35%%\n");
    motor_set(&motor_2, 0.35f);
    sleep_ms(700);
    motor_stop(&motor_2);
    sleep_ms(500);

    printf("Motor 2: direction B at 35%%\n");
    motor_set(&motor_2, -0.35f);
    sleep_ms(700);
    motor_stop(&motor_2);
    printf("Both motors: stopped\n");
}

static void run_servo_test(void)
{
    printf("Servo: left -> centre -> right -> centre\n");
    servo_set_angle(45u);
    sleep_ms(700);
    servo_set_angle(90u);
    sleep_ms(700);
    servo_set_angle(135u);
    sleep_ms(700);
    servo_set_angle(90u);
}

int main(void)
{
    stdio_init_all();

    motors_init();
    servo_init();
    ultrasonic_init();
    ir_sensors_init();

    gpio_init(MOTOR_TEST_BUTTON_PIN);
    gpio_set_dir(MOTOR_TEST_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(MOTOR_TEST_BUTTON_PIN);

    gpio_init(SERVO_TEST_BUTTON_PIN);
    gpio_set_dir(SERVO_TEST_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(SERVO_TEST_BUTTON_PIN);

    sleep_ms(2500);
    printf("\nINF2004 robot bring-up test\n");
    printf("Raise the wheels before pressing GP20.\n");
    printf("GP20: test both motors; GP21: sweep servo.\n");
    printf("IR readings are raw ADC values from 0 to 4095.\n\n");

    bool gy511_ready = gy511_init();
    bool previous_motor_button = true;
    bool previous_servo_button = true;

    while (true) {
        bool motor_button = gpio_get(MOTOR_TEST_BUTTON_PIN);
        bool servo_button = gpio_get(SERVO_TEST_BUTTON_PIN);

        if (previous_motor_button && !motor_button) {
            run_motor_test();
        }
        if (previous_servo_button && !servo_button) {
            run_servo_test();
        }

        previous_motor_button = motor_button;
        previous_servo_button = servo_button;

        float distance_cm = 0.0f;
        if (ultrasonic_read_cm(&distance_cm)) {
            printf("Distance: %6.1f cm", distance_cm);
        } else {
            printf("Distance: no echo");
        }

        if (gy511_ready) {
            int16_t x, y, z;
            if (gy511_read_acceleration(&x, &y, &z)) {
                printf(" | Accel raw: X=%5d Y=%5d Z=%5d", x, y, z);
            } else {
                printf(" | GY-511 read failed");
            }
        } else {
            printf(" | GY-511 unavailable");
        }

        uint16_t ir_1 = ir_sensor_read(IR_1_ANALOG_PIN);
        uint16_t ir_2 = ir_sensor_read(IR_2_ANALOG_PIN);
        bool ir_3 = gpio_get(IR_3_DIGITAL_PIN);
        printf(" | IR1=%4u IR2=%4u IR3=%u", ir_1, ir_2, ir_3 ? 1u : 0u);
        printf("\n");

        sleep_ms(500);
    }
}
