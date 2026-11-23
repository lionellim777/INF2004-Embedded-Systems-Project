#include "obstacle_io.h"
#include "obstacle_trigger.pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/time.h"

enum { TRIG = 0, ECHO = 1, SERVO = 12, SERVO_CENTER = 90,
       TRIGGER_PERIOD_US = 60000, RISE_TIMEOUT_US = 30000, FALL_TIMEOUT_US = 45000 };
enum { IDLE, WAIT_RISE, WAIT_FALL, PULSE_READY, BAD_EDGES };
static const uint32_t edges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
static PIO trigger_pio;
static uint trigger_sm;
static uint32_t last_trigger_us;
static unsigned servo_angle;
static bool active;
/* Raw IRQ and Motion share these fields only under local IRQ exclusion. */
static volatile uint32_t edge_us;
static volatile uint16_t pulse_us;
static volatile uint8_t state;

static void echo_irq(void)
{
    uint32_t events = gpio_get_irq_event_mask(ECHO) & edges;
    if (!events) return;
    gpio_acknowledge_irq(ECHO, events);
    uint32_t now = time_us_32();
    if (state == WAIT_RISE && events == GPIO_IRQ_EDGE_RISE) {
        if (now - edge_us >= RISE_TIMEOUT_US) return;
        edge_us = now;
        state = WAIT_FALL;
    } else if (state == WAIT_FALL && events == GPIO_IRQ_EDGE_FALL) {
        uint32_t elapsed = now - edge_us;
        pulse_us = (uint16_t)(elapsed < FALL_TIMEOUT_US ? elapsed : FALL_TIMEOUT_US);
        state = PULSE_READY;
    } else if (state == WAIT_RISE || state == WAIT_FALL) {
        state = BAD_EDGES;
    }
}

static void set_servo(unsigned degrees)
{
    pwm_set_gpio_level(SERVO, (uint16_t)(500u + degrees * 2000u / 180u));
    servo_angle = degrees;
}

bool obstacle_io_init(void)
{
    uint offset;
    if (!pio_claim_free_sm_and_add_program(&obstacle_trigger_program,
                                           &trigger_pio, &trigger_sm, &offset)) return false;
    pio_sm_config trigger = obstacle_trigger_program_get_default_config(offset);
    sm_config_set_set_pins(&trigger, TRIG, 1);
    sm_config_set_clkdiv(&trigger, (float)clock_get_hz(clk_sys) / 1000000.0f);
    pio_sm_init(trigger_pio, trigger_sm, offset, &trigger);
    pio_sm_exec(trigger_pio, trigger_sm, pio_encode_set(pio_pins, 0));
    pio_sm_set_consecutive_pindirs(trigger_pio, trigger_sm, TRIG, 1, true);
    pio_gpio_init(trigger_pio, TRIG);
    pio_sm_set_enabled(trigger_pio, trigger_sm, true);

    gpio_init(ECHO);
    gpio_set_dir(ECHO, GPIO_IN);
    gpio_add_raw_irq_handler(ECHO, echo_irq);

    gpio_set_function(SERVO, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(SERVO);
    pwm_set_clkdiv(slice, (float)clock_get_hz(clk_sys) / 1000000.0f);
    pwm_set_wrap(slice, 19999);
    set_servo(SERVO_CENTER);
    pwm_set_enabled(slice, true);
    return true;
}

void obstacle_io_begin(void)
{
    obstacle_io_end();
    last_trigger_us = time_us_32();
    gpio_set_irq_enabled(ECHO, edges, true);
    active = true;
}

void obstacle_io_end(void)
{
    active = false;
    uint32_t irq = save_and_disable_interrupts();
    gpio_set_irq_enabled(ECHO, edges, false);
    gpio_acknowledge_irq(ECHO, edges);
    state = IDLE;
    restore_interrupts(irq);
    set_servo(SERVO_CENTER);
}

void obstacle_io_angle(unsigned degrees)
{
    if (degrees < 50) degrees = 50;
    if (degrees > 125) degrees = 125;
    if (degrees == servo_angle) return;
    /* Drop a capture from the old ray. The controller owns servo settling. */
    uint32_t irq = save_and_disable_interrupts();
    state = IDLE;
    gpio_acknowledge_irq(ECHO, edges);
    last_trigger_us = time_us_32();
    restore_interrupts(irq);
    set_servo(degrees);
}

obstacle_range_t obstacle_io_poll(void)
{
    obstacle_range_t result = {0};
    if (!active) return result;
    uint16_t width = 0;
    bool measured = false;
    uint32_t irq = save_and_disable_interrupts();
    uint32_t now = time_us_32();
    if (state == PULSE_READY || state == BAD_EDGES ||
        (state == WAIT_RISE && now - edge_us >= RISE_TIMEOUT_US) ||
        (state == WAIT_FALL && now - edge_us >= FALL_TIMEOUT_US)) {
        result.fresh = true;
        if (state == PULSE_READY) {
            width = pulse_us;
            measured = true;
        }
        if (state == WAIT_FALL) result.no_return = true;
        if (state == BAD_EDGES) result.error = "AVOID_SONAR_BAD_EDGES";
        state = IDLE;
    } else if (state == IDLE && now - last_trigger_us >= TRIGGER_PERIOD_US) {
        last_trigger_us = now;
        gpio_acknowledge_irq(ECHO, edges);
        if (gpio_get(ECHO)) {
            result.fresh = true;
            result.error = "AVOID_SONAR_ECHO_HIGH_BEFORE_TRIGGER";
        } else if (!pio_sm_is_tx_fifo_full(trigger_pio, trigger_sm)) {
            edge_us = now;
            state = WAIT_RISE;
            pio_sm_put(trigger_pio, trigger_sm, 0);
        } else {
            result.fresh = true;
            result.error = "AVOID_SONAR_TRIGGER_BUSY";
        }
    }
    restore_interrupts(irq);
    if (measured) {
        result.cm = width * 0.01715f;
        result.valid = result.cm >= 2.0f && result.cm <= 400.0f;
        result.no_return = result.cm > 400.0f;
        if (result.cm < 2.0f) result.error = "AVOID_SONAR_TOO_CLOSE_OR_BAD_PULSE";
    }
    return result;
}
