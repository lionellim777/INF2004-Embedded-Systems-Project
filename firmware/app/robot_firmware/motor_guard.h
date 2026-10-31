#ifndef MOTOR_GUARD_H
#define MOTOR_GUARD_H
#include <stdbool.h>
#include <stdint.h>
/* Shared only between the main loop and its alarm IRQ on one core.
 * Main refresh/reset must run with interrupts disabled. No printing in IRQ. */
typedef struct {uint32_t refreshed_us; bool armed; bool tripped;} motor_guard_t;
#define MOTOR_GUARD_TIMEOUT_US 150000u
static inline bool motor_guard_refresh(volatile motor_guard_t *g,uint32_t now,bool enabled)
{
    if(g->tripped) return false;
    g->refreshed_us=now;g->armed=enabled;return true;
}
static inline bool motor_guard_tick(volatile motor_guard_t *g,uint32_t now)
{
    if(g->armed && (uint32_t)(now-g->refreshed_us)>=MOTOR_GUARD_TIMEOUT_US) {
        g->armed=false;g->tripped=true;return true;
    }
    return false;
}
static inline void motor_guard_reset(volatile motor_guard_t *g)
{g->armed=false;g->tripped=false;}
#endif
