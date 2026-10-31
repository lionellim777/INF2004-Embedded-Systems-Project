/*
 * INF2004 wheel encoder wiring test for Cytron Robo Pico + Pico W.
 *
 * Left:  Grove 2, encoder A=GP2,  B=GP3.
 * Right: Grove 4, encoder A=GP16, B=GP17.
 *
 * The four motor-driver inputs are held low. No motor is commanded to move.
 * Turn one raised wheel at a time by hand and watch the USB serial output.
 */

#include <stdint.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

enum {
    LEFT_A_PIN = 2,
    LEFT_B_PIN = 3,
    RIGHT_A_PIN = 16,
    RIGHT_B_PIN = 17,
    REPORT_INTERVAL_MS = 500,
    ZERO_BUTTON_PIN = 20,
};

typedef struct {
    volatile uint32_t a_edges;
    volatile uint32_t b_edges;
    volatile uint32_t invalid_transitions;
    volatile int32_t position;
    volatile uint8_t previous_ab;
} encoder_t;

typedef struct {
    uint32_t a_edges;
    uint32_t b_edges;
    uint32_t invalid_transitions;
    int32_t position;
    uint8_t ab;
} snapshot_t;

static encoder_t left_encoder;
static encoder_t right_encoder;
static uint8_t read_ab(uint a_pin, uint b_pin);

static void print_help(void)
{
    printf("\n=== AAI10 ENCODER INSPECTION v2 | %s %s ===\n", __DATE__, __TIME__);
    printf("MOTOR OUTPUTS OFF: GP8/9/10/11 are held LOW. No powered movement.\n");
    printf("Left encoder: Grove 2 A=GP2 B=GP3. Right: Grove 4 A=GP16 B=GP17.\n");
    printf("1. Raise wheels. Turn LEFT wheel forward by hand; only LEFT should count.\n");
    printf("2. Turn it backward; the count direction should reverse. Repeat RIGHT.\n");
    printf("3. Mark a wheel, press GP20 to zero, turn ONE full revolution slowly.\n");
    printf("4. Record the count magnitude; repeat each wheel three times.\n");
    printf("Previous calibration reference: about 2543 quadrature counts/revolution.\n");
    printf("Raw polarity reference: forward LEFT negative; forward RIGHT positive.\n");
    printf("These are references to check, not measured results from this test.\n");
    printf("A/B=channel edge totals; d=new position counts since previous report.\n");
    printf("pins=instantaneous A/B levels, not a revolution counter.\n");
    printf("invalid=skipped A/B transitions; a rising value needs investigation.\n");
    printf("No edges can mean no movement OR missing encoder power/wiring.\n");
    printf("GP20 ONLY zeros diagnostics; it never starts a motor in this firmware.\n");
    printf("USB serial: 115200 baud. Reports every 500 ms, even while idle.\n\n");
}

static void reset_encoders(void)
{
    uint32_t saved = save_and_disable_interrupts();
    left_encoder.a_edges = left_encoder.b_edges = 0u;
    right_encoder.a_edges = right_encoder.b_edges = 0u;
    left_encoder.invalid_transitions = right_encoder.invalid_transitions = 0u;
    left_encoder.position = right_encoder.position = 0;
    left_encoder.previous_ab = read_ab(LEFT_A_PIN, LEFT_B_PIN);
    right_encoder.previous_ab = read_ab(RIGHT_A_PIN, RIGHT_B_PIN);
    restore_interrupts(saved);
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

    if (gpio == LEFT_A_PIN || gpio == LEFT_B_PIN) {
        encoder = &left_encoder;
        a_pin = LEFT_A_PIN;
        b_pin = LEFT_B_PIN;
    } else if (gpio == RIGHT_A_PIN || gpio == RIGHT_B_PIN) {
        encoder = &right_encoder;
        a_pin = RIGHT_A_PIN;
        b_pin = RIGHT_B_PIN;
    } else {
        return;
    }

    if (gpio == a_pin) {
        ++encoder->a_edges;
    } else {
        ++encoder->b_edges;
    }

    static const int8_t quadrature_step[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0,
    };
    uint8_t current_ab = read_ab(a_pin, b_pin);
    uint8_t old_ab = encoder->previous_ab;
    uint8_t changed = old_ab ^ current_ab;
    if (changed == 3u) {
        ++encoder->invalid_transitions;
    }
    encoder->position += quadrature_step[(old_ab << 2u) | current_ab];
    encoder->previous_ab = current_ab;
}

static snapshot_t take_snapshot(encoder_t *encoder, uint a_pin, uint b_pin)
{
    uint32_t saved_interrupts = save_and_disable_interrupts();
    snapshot_t snapshot = {
        .a_edges = encoder->a_edges,
        .b_edges = encoder->b_edges,
        .invalid_transitions = encoder->invalid_transitions,
        .position = encoder->position,
        .ab = read_ab(a_pin, b_pin),
    };
    restore_interrupts(saved_interrupts);
    return snapshot;
}

static void hold_motors_stopped(void)
{
    const uint motor_pins[] = {8u, 9u, 10u, 11u};
    for (uint i = 0u; i < 4u; ++i) {
        gpio_init(motor_pins[i]);
        gpio_put(motor_pins[i], 0);
        gpio_set_dir(motor_pins[i], GPIO_OUT);
    }
}

static void init_encoders(void)
{
    const uint encoder_pins[] = {
        LEFT_A_PIN, LEFT_B_PIN, RIGHT_A_PIN, RIGHT_B_PIN,
    };
    for (uint i = 0u; i < 4u; ++i) {
        gpio_init(encoder_pins[i]);
        gpio_set_dir(encoder_pins[i], GPIO_IN);
        gpio_pull_up(encoder_pins[i]);
    }

    left_encoder.previous_ab = read_ab(LEFT_A_PIN, LEFT_B_PIN);
    right_encoder.previous_ab = read_ab(RIGHT_A_PIN, RIGHT_B_PIN);

    const uint32_t both_edges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
    gpio_set_irq_enabled_with_callback(LEFT_A_PIN, both_edges, true,
                                       encoder_edge_callback);
    gpio_set_irq_enabled(LEFT_B_PIN, both_edges, true);
    gpio_set_irq_enabled(RIGHT_A_PIN, both_edges, true);
    gpio_set_irq_enabled(RIGHT_B_PIN, both_edges, true);
}

int main(void)
{
    hold_motors_stopped();
    init_encoders();
    gpio_init(ZERO_BUTTON_PIN);
    gpio_set_dir(ZERO_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(ZERO_BUTTON_PIN);
    stdio_init_all();

    snapshot_t last_left = take_snapshot(&left_encoder, LEFT_A_PIN, LEFT_B_PIN);
    snapshot_t last_right = take_snapshot(&right_encoder, RIGHT_A_PIN,
                                          RIGHT_B_PIN);
    bool usb_was_connected = false;
    bool button_candidate = false;
    bool button_pressed = false;
    uint64_t button_changed_us = time_us_64();
    uint64_t next_report_us = time_us_64();
    uint32_t report_number = 0u;

    while (true) {
        sleep_ms(10u);
        bool connected = stdio_usb_connected();
        if (connected && !usb_was_connected) {
            print_help();
        }
        usb_was_connected = connected;

        uint64_t now_us = time_us_64();
        bool pressed = !gpio_get(ZERO_BUTTON_PIN);
        if (pressed != button_candidate) {
            button_candidate = pressed;
            button_changed_us = now_us;
        }
        if (now_us - button_changed_us >= 30000u &&
            button_pressed != button_candidate) {
            button_pressed = button_candidate;
            if (button_pressed) {
                reset_encoders();
                last_left = take_snapshot(&left_encoder, LEFT_A_PIN, LEFT_B_PIN);
                last_right = take_snapshot(&right_encoder, RIGHT_A_PIN, RIGHT_B_PIN);
                printf("\n[ZERO] Both diagnostic counts cleared. Motors remain OFF.\n");
            }
        }
        if (now_us < next_report_us) {
            continue;
        }
        next_report_us = now_us + REPORT_INTERVAL_MS * 1000u;
        snapshot_t left = take_snapshot(&left_encoder, LEFT_A_PIN, LEFT_B_PIN);
        snapshot_t right = take_snapshot(&right_encoder, RIGHT_A_PIN,
                                          RIGHT_B_PIN);

        uint32_t left_a_new = left.a_edges - last_left.a_edges;
        uint32_t left_b_new = left.b_edges - last_left.b_edges;
        uint32_t right_a_new = right.a_edges - last_right.a_edges;
        uint32_t right_b_new = right.b_edges - last_right.b_edges;
        const char *activity = "IDLE";
        if ((left_a_new | left_b_new) && (right_a_new | right_b_new)) {
            activity = "BOTH";
        } else if (left_a_new | left_b_new) {
            activity = "LEFT";
        } else if (right_a_new | right_b_new) {
            activity = "RIGHT";
        }

        printf("\n[%llu ms #%lu] MOTOR=OFF Activity=%s\n",
               (unsigned long long)(now_us / 1000u),
               (unsigned long)++report_number, activity);
        printf(" LEFT  A/B edges=%lu/%lu (+%lu/+%lu) raw_count=%ld d=%lld invalid=%lu pins=%u%u\n",
               (unsigned long)left.a_edges, (unsigned long)left.b_edges,
               (unsigned long)left_a_new, (unsigned long)left_b_new,
               (long)left.position, (long long)left.position - last_left.position,
               (unsigned long)left.invalid_transitions,
               (unsigned)(left.ab >> 1u), (unsigned)(left.ab & 1u));
        printf(" RIGHT A/B edges=%lu/%lu (+%lu/+%lu) raw_count=%ld d=%lld invalid=%lu pins=%u%u\n",
               (unsigned long)right.a_edges, (unsigned long)right.b_edges,
               (unsigned long)right_a_new, (unsigned long)right_b_new,
               (long)right.position, (long long)right.position - last_right.position,
               (unsigned long)right.invalid_transitions,
               (unsigned)(right.ab >> 1u), (unsigned)(right.ab & 1u));

        if (left.invalid_transitions != last_left.invalid_transitions ||
            right.invalid_transitions != last_right.invalid_transitions) {
            printf(" CHECK: new invalid transitions. Turn slowly; inspect signal/power/wiring.\n");
        }
        if ((left.a_edges >= 8u && left.b_edges == 0u) ||
            (left.b_edges >= 8u && left.a_edges == 0u) ||
            (right.a_edges >= 8u && right.b_edges == 0u) ||
            (right.b_edges >= 8u && right.a_edges == 0u)) {
            printf(" CHECK: one channel has edges while its partner has none. Check A/B wiring.\n");
        }
        if (report_number % 20u == 0u) {
            printf(" TIP: GP20 zeroes counts. One marked wheel revolution, then read raw_count.\n");
        }

        last_left = left;
        last_right = right;
    }
}
