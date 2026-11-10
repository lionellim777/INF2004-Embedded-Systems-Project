#ifndef MISSION_SAFETY_GATE_H
#define MISSION_SAFETY_GATE_H
#include <stdbool.h>
#include <stdint.h>

/* Local-core owner serializes every call against timer IRQ and task switches.
 * Only the debounced physical-button path may call note_release/arm.
 * 16-bit tokens never wrap: the 65,536th arm requires a cold restart. */
#define SAFETY_SERVICE_US 40000u
enum { STOP_BOOT, STOP_BUTTON, STOP_SERVICE, STOP_MOTION, STOP_INIT,
       STOP_RANGE, STOP_COMPLETE };
typedef struct {
    uint32_t serviced_us; /* modulo microseconds, compared within 40 ms */
    uint16_t epoch;
    uint8_t state; /* armed:0, release:1, stop reason:2..4; 5..7 zero */
} safety_gate_t;
void safety_gate_init(safety_gate_t *gate);
void safety_gate_release(safety_gate_t *gate);
bool safety_gate_arm(safety_gate_t *gate, uint32_t now, uint16_t *token);
bool safety_gate_service(safety_gate_t *gate, uint16_t token, uint32_t now);
void safety_gate_stop(safety_gate_t *gate, uint8_t reason);
bool safety_gate_tick(safety_gate_t *gate, uint32_t now, bool button_down);
bool safety_gate_armed(const safety_gate_t *gate);
bool safety_gate_ready(const safety_gate_t *gate);
uint8_t safety_gate_reason(const safety_gate_t *gate);
#endif
