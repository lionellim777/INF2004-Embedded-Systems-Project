#include "safety_gate.h"
#include <stddef.h>
enum { ARMED = 1u, RELEASED = 2u, REASON_SHIFT = 2u };
void safety_gate_init(safety_gate_t *g)
{
    *g = (safety_gate_t){0};
}
bool safety_gate_armed(const safety_gate_t *g) { return (g->state & ARMED) != 0; }
bool safety_gate_ready(const safety_gate_t *g) { return !safety_gate_armed(g) && (g->state & RELEASED) != 0; }
uint8_t safety_gate_reason(const safety_gate_t *g) { return (uint8_t)(g->state >> REASON_SHIFT); }
void safety_gate_stop(safety_gate_t *g, uint8_t reason)
{
    if (reason > STOP_COMPLETE) reason = STOP_INIT;
    g->state = (uint8_t)(reason << REASON_SHIFT);
}
void safety_gate_release(safety_gate_t *g)
{
    if (!safety_gate_armed(g)) g->state |= RELEASED;
}
bool safety_gate_arm(safety_gate_t *g, uint32_t now, uint16_t *token)
{
    if (token == NULL || safety_gate_armed(g) || !(g->state & RELEASED)) return false;
    if (g->epoch == UINT16_MAX) {
        safety_gate_stop(g, STOP_INIT);
        return false;
    }
    ++g->epoch;
    g->serviced_us = now;
    g->state = ARMED;
    *token = g->epoch;
    return true;
}
bool safety_gate_service(safety_gate_t *g, uint16_t token, uint32_t now)
{
    if (!safety_gate_armed(g) || token != g->epoch) return false;
    if ((uint32_t)(now - g->serviced_us) >= SAFETY_SERVICE_US) {
        safety_gate_stop(g, STOP_SERVICE);
        return false;
    }
    g->serviced_us = now;
    return true;
}
bool safety_gate_tick(safety_gate_t *g, uint32_t now, bool button_down)
{
    if (!safety_gate_armed(g)) return false;
    if (button_down) safety_gate_stop(g, STOP_BUTTON);
    else if ((uint32_t)(now - g->serviced_us) >= SAFETY_SERVICE_US) safety_gate_stop(g, STOP_SERVICE);
    else return false;
    return true;
}
