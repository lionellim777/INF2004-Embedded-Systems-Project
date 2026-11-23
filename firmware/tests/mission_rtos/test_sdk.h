#ifndef TEST_MISSION_SDK_H
#define TEST_MISSION_SDK_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef unsigned int uint;
typedef struct repeating_timer repeating_timer_t;
struct repeating_timer { bool (*callback)(repeating_timer_t *); };
enum { GPIO_IN, GPIO_FUNC_PWM, clk_sys };
uint32_t save_and_disable_interrupts(void);
void restore_interrupts(uint32_t previous);
uint32_t time_us_32(void);
uint32_t clock_get_hz(int clock);
void gpio_init(uint pin);
void gpio_set_dir(uint pin, bool output);
void gpio_pull_up(uint pin);
void gpio_set_function(uint pin, int function);
bool gpio_get(uint pin);
uint pwm_gpio_to_slice_num(uint pin);
void pwm_set_both_levels(uint slice, uint16_t left, uint16_t right);
void pwm_set_enabled(uint slice, bool enabled);
void pwm_set_clkdiv(uint slice, float divider);
void pwm_set_wrap(uint slice, uint16_t wrap);
bool add_repeating_timer_us(int64_t period, bool (*callback)(repeating_timer_t *),
    void *context, repeating_timer_t *timer);
#endif
