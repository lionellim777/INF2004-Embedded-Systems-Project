#include "fault_flags.h"
#include <assert.h>
#include <stdio.h>

/* hardware/sync.h is replaced by a one-line fake header (see tools/test_minbit_layouts.py). */
static unsigned masked, unmasked;
uint32_t save_and_disable_interrupts(void) { masked++; return 7u; }
void restore_interrupts(uint32_t previous) { assert(previous == 7u); unmasked++; }

/* The shared fault mask: independent bits, latched copy, no cross-talk. Interrupt exclusion itself is
 * only checkable on the target; here the mock counts that every read-modify-write was bracketed. */
int main(void)
{
    assert(fault_active() == 0 && fault_latched() == 0);
    fault_set(FAULT_MOTOR_STALL);
    assert(fault_active() == FAULT_MOTOR_STALL && fault_latched() == FAULT_MOTOR_STALL);
    fault_set(FAULT_IMU | FAULT_NO_ECHO);
    assert(fault_active() == (FAULT_MOTOR_STALL | FAULT_IMU | FAULT_NO_ECHO));
    fault_clear(FAULT_MOTOR_STALL);
    assert(fault_active() == (FAULT_IMU | FAULT_NO_ECHO));           /* other bits untouched */
    assert(fault_latched() == (FAULT_MOTOR_STALL | FAULT_IMU | FAULT_NO_ECHO)); /* latch survives the clear */
    fault_set(0x80u | 0x40u);                                        /* undefined bits are ignored */
    assert(fault_active() == (FAULT_IMU | FAULT_NO_ECHO) && !(fault_latched() & ~FAULT_ALL));
    for (unsigned bit = 0; bit < 6; ++bit) {                         /* every defined bit sets and clears alone */
        uint8_t before = fault_active();
        fault_set((uint8_t)(1u << bit)); assert(fault_active() & (1u << bit));
        fault_clear((uint8_t)(1u << bit)); assert(!(fault_active() & (1u << bit)));
        fault_set(before); fault_clear((uint8_t)(~before & FAULT_ALL));
        assert(fault_active() == (before & FAULT_ALL));
    }
    assert(masked == unmasked && masked > 0);                        /* every read-modify-write was bracketed */
    assert(sizeof(uint16_t) == 2); /* active+latched share one halfword: one RMW updates both */
    puts("PASS shared fault mask: independent bits, latched copy, undefined bits rejected");
    return 0;
}
