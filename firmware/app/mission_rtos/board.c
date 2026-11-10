#include "board.h"
#include <math.h>
#include <string.h>
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

enum { BUTTON_PIN = 20, LEFT_A = 8, LEFT_B = 9, RIGHT_A = 10, RIGHT_B = 11 };
static safety_gate_t gate;
static repeating_timer_t guard_timer;
static uint16_t wrap;
enum { SELECT_MASK = 7u, READY = 8u, LAST_REASON_MASK = 112u };
static uint8_t control; /* selection0..2, ready3, last stop reason4..6 */
static uint8_t latest[MOTION_RECORD_BYTES];
static uint32_t stop_sequence, stop_requested_us, stop_disabled_us, alarm_gap_us, last_alarm_us;
static uint32_t armed_alarm_gap_us,service_gap_us;
static uint8_t stop_source;

static void zero_outputs(void)
{
    pwm_set_both_levels(pwm_gpio_to_slice_num(LEFT_A), 0, 0);
    pwm_set_both_levels(pwm_gpio_to_slice_num(RIGHT_A), 0, 0);
}
static void trip_outputs(uint8_t reason,uint8_t source,uint32_t requested)
{
    stop_requested_us=requested;stop_source=source;
    zero_outputs();
    stop_disabled_us=time_us_32();
    ++stop_sequence;
    control=(uint8_t)((control&~LAST_REASON_MASK) | (reason<<4u));
}
static bool guard_callback(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t irq = save_and_disable_interrupts();
    uint32_t now=time_us_32(), gap=now-last_alarm_us; last_alarm_us=now;
    if (gap>alarm_gap_us) alarm_gap_us=gap;
    if(safety_gate_armed(&gate)){
        if(gap>armed_alarm_gap_us)armed_alarm_gap_us=gap;
        uint32_t age=now-gate.serviced_us;if(age>service_gap_us)service_gap_us=age;
    }
    if (safety_gate_tick(&gate, now, board_button_down())) trip_outputs(safety_gate_reason(&gate),STOP_SOURCE_GUARD,now);
    restore_interrupts(irq);
    return true;
}
bool board_init(void)
{
    control = 0;
    stop_sequence=stop_requested_us=stop_disabled_us=alarm_gap_us=0;
    armed_alarm_gap_us=service_gap_us=0;stop_source=STOP_SOURCE_BOOT;
    last_alarm_us=time_us_32();
    safety_gate_init(&gate);
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    wrap = (uint16_t)(clock_get_hz(clk_sys) / 20000u - 1u);
    const uint pins[] = {LEFT_A, LEFT_B, RIGHT_A, RIGHT_B};
    for (unsigned i = 0; i < 4; ++i) gpio_set_function(pins[i], GPIO_FUNC_PWM);
    const uint slices[] = {pwm_gpio_to_slice_num(LEFT_A), pwm_gpio_to_slice_num(RIGHT_A)};
    for (unsigned i = 0; i < 2; ++i) {
        pwm_set_enabled(slices[i], false);
        pwm_set_clkdiv(slices[i], 1.0f);
        pwm_set_wrap(slices[i], wrap);
        pwm_set_both_levels(slices[i], 0, 0);
        pwm_set_enabled(slices[i], true);
    }
    motion_record_t initial = {.stop_latched = true};
    (void)motion_record_encode(&initial, latest, sizeof latest);
    bool initialized = add_repeating_timer_us(-5000, guard_callback, NULL, &guard_timer);
    if (initialized) control |= READY;
    else board_stop(STOP_INIT);
    return initialized;
}
bool board_button_down(void) { return !gpio_get(BUTTON_PIN); }
void board_stop(uint8_t reason)
{
    uint32_t requested=time_us_32();
    uint32_t irq = save_and_disable_interrupts();
    bool was_armed=safety_gate_armed(&gate);
    if (reason == STOP_INIT) control &= (uint8_t)~READY;
    safety_gate_stop(&gate, reason);
    if (was_armed) trip_outputs(safety_gate_reason(&gate),STOP_SOURCE_API,requested);
    else zero_outputs();
    restore_interrupts(irq);
}
void board_note_release(void)
{
    uint32_t irq = save_and_disable_interrupts();
    safety_gate_release(&gate);
    restore_interrupts(irq);
}
bool board_ready_to_start(void)
{
    uint32_t irq = save_and_disable_interrupts();
    bool result = (control & READY) && (control & SELECT_MASK) != 0 && safety_gate_ready(&gate);
    restore_interrupts(irq);
    return result;
}
bool board_start(uint16_t *token)
{
    uint32_t irq = save_and_disable_interrupts();
    bool result = (control & READY) && (control & SELECT_MASK) != 0 && !board_button_down() &&
        safety_gate_arm(&gate, time_us_32(), token);
    if (result) { alarm_gap_us=armed_alarm_gap_us=service_gap_us=0; last_alarm_us=time_us_32(); }
    restore_interrupts(irq);
    return result;
}
static void drive(uint pin, float throttle)
{
    if (throttle > 0.65f) throttle = 0.65f;
    if (throttle < -0.65f) throttle = -0.65f;
    float magnitude = throttle < 0 ? -throttle : throttle;
    uint16_t level = (uint16_t)((float)(wrap + 1u) * magnitude);
    pwm_set_both_levels(pwm_gpio_to_slice_num(pin), throttle > 0 ? level : 0,
                        throttle < 0 ? level : 0);
}
bool board_apply(uint16_t token, float left, float right)
{
    uint32_t irq = save_and_disable_interrupts();
    bool was_armed=safety_gate_armed(&gate);
    uint32_t now=time_us_32();
    if(was_armed){uint32_t age=now-gate.serviced_us;if(age>service_gap_us)service_gap_us=age;}
    bool result = (control & READY) && isfinite(left) && isfinite(right) &&
        !board_button_down() && safety_gate_service(&gate, token, time_us_32());
    if (result) {
        drive(LEFT_A, left); drive(RIGHT_A, right);
    } else {
        /* An old token may remove output, but can never revive a newer run. */
        if (was_armed) {
            if (safety_gate_armed(&gate))
                safety_gate_stop(&gate, board_button_down() ? STOP_BUTTON : STOP_MOTION);
            trip_outputs(safety_gate_reason(&gate),STOP_SOURCE_APPLY,now);
        } else zero_outputs();
    }
    restore_interrupts(irq);
    return result;
}
void board_gate_snapshot(bool *armed, uint8_t *reason)
{
    uint32_t irq = save_and_disable_interrupts();
    *armed = safety_gate_armed(&gate); *reason = safety_gate_reason(&gate);
    restore_interrupts(irq);
}
bool board_select(uint8_t kind)
{
    uint32_t irq = save_and_disable_interrupts();
    bool supported=kind>=1 && kind<=4;
#ifdef RTOS_TRACK
    supported=supported || kind==MOTION_TRACK_KIND;
#endif
#ifdef RTOS_OBSTACLE
    supported = supported || kind == BOARD_OBSTACLE_SELECT;
#endif
    bool result = supported && !safety_gate_armed(&gate);
    if (result) control = (uint8_t)((control & (uint8_t)~SELECT_MASK) | kind);
    restore_interrupts(irq);
    return result;
}
uint8_t board_selected(void)
{
    uint32_t irq = save_and_disable_interrupts();
    uint8_t value = control & SELECT_MASK;
    restore_interrupts(irq);
    return value;
}
void board_publish(const motion_record_t *record)
{
    uint8_t encoded[MOTION_RECORD_BYTES];
    if (!motion_record_encode(record, encoded, sizeof encoded)) {
        board_stop(STOP_RANGE);
        motion_record_t invalid = {.captured_ms = record->captured_ms,
            .phase = 4, .stop_reason = STOP_RANGE, .stop_latched = true};
        (void)motion_record_encode(&invalid, encoded, sizeof encoded);
    }
    uint32_t irq = save_and_disable_interrupts();
    memcpy(latest, encoded, sizeof latest);
    restore_interrupts(irq);
}
void board_read(uint8_t output[MOTION_RECORD_BYTES])
{
    uint32_t irq = save_and_disable_interrupts();
    memcpy(output, latest, sizeof latest);
    restore_interrupts(irq);
}
void board_stop_trace(board_stop_trace_t *trace)
{
    uint32_t irq=save_and_disable_interrupts();
    *trace=(board_stop_trace_t){.sequence=stop_sequence,.requested_us=stop_requested_us,
        .disabled_us=stop_disabled_us,.alarm_gap_us=alarm_gap_us,.reason=(control>>4u)&7u};
    restore_interrupts(irq);
}
void board_runtime_trace(board_runtime_trace_t *out)
{
    uint32_t irq=save_and_disable_interrupts();
    *out=(board_runtime_trace_t){stop_sequence,stop_requested_us,stop_disabled_us,armed_alarm_gap_us,service_gap_us,
        (uint8_t)((control>>4u)&7u),stop_source,safety_gate_armed(&gate)};
    restore_interrupts(irq);
}
