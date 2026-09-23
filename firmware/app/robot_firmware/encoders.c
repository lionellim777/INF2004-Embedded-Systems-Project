#include "encoders.h"

#include <stdint.h>

#include "pico/stdlib.h"

enum {
    LEFT_A_PIN = 2,
    LEFT_B_PIN = 3,
    RIGHT_A_PIN = 16,
    RIGHT_B_PIN = 17,
};

static volatile int32_t counts[2];
static uint8_t previous_state[2];

static uint8_t read_state(uint pin_a, uint pin_b)
{
    return (uint8_t)((gpio_get(pin_a) ? 2u : 0u) |
                     (gpio_get(pin_b) ? 1u : 0u));
}

static void encoder_gpio_callback(uint gpio, uint32_t events)
{
    (void)events;
    uint index;
    uint pin_a;
    uint pin_b;
    if (gpio == LEFT_A_PIN || gpio == LEFT_B_PIN) {
        index = 0u;
        pin_a = LEFT_A_PIN;
        pin_b = LEFT_B_PIN;
    } else if (gpio == RIGHT_A_PIN || gpio == RIGHT_B_PIN) {
        index = 1u;
        pin_a = RIGHT_A_PIN;
        pin_b = RIGHT_B_PIN;
    } else {
        return;
    }

    static const int8_t transition[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0,
    };
    uint8_t current = read_state(pin_a, pin_b);
    counts[index] += transition[(previous_state[index] << 2u) | current];
    previous_state[index] = current;
}

void encoders_init(void)
{
    const uint pins[4] = {LEFT_A_PIN, LEFT_B_PIN, RIGHT_A_PIN, RIGHT_B_PIN};
    for (uint i = 0u; i < 4u; ++i) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    previous_state[0] = read_state(LEFT_A_PIN, LEFT_B_PIN);
    previous_state[1] = read_state(RIGHT_A_PIN, RIGHT_B_PIN);

    const uint32_t edges = GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL;
    gpio_set_irq_enabled_with_callback(LEFT_A_PIN, edges, true,
                                       encoder_gpio_callback);
    gpio_set_irq_enabled(LEFT_B_PIN, edges, true);
    gpio_set_irq_enabled(RIGHT_A_PIN, edges, true);
    gpio_set_irq_enabled(RIGHT_B_PIN, edges, true);
}

int32_t encoder_left_count(void)
{
    return counts[0];
}

int32_t encoder_right_count(void)
{
    return counts[1];
}
