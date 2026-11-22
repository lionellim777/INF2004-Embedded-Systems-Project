/* fault_flags.c - shared fault bitmask (see fault_flags.h for the ownership
 * rules). */
#include "fault_flags.h"
#include "hardware/sync.h"

/* latched copy lives in the high byte of the word */
#define FAULT_LATCH_SHIFT 8u
#define FAULT_ACTIVE_MASK 0xFFu

static volatile uint16_t
    fault_word; /* bits 0..7 active, bits 8..15 latched since boot */

void fault_set(uint8_t bits)
{
    bits &= FAULT_ALL;
    uint32_t irq = save_and_disable_interrupts();
    fault_word =
        (uint16_t)(fault_word | bits | ((uint16_t)bits << FAULT_LATCH_SHIFT));
    restore_interrupts(irq);
}

void fault_clear(uint8_t bits)
{
    bits &= FAULT_ALL;
    uint32_t irq = save_and_disable_interrupts();
    fault_word = (uint16_t)(fault_word & ~(uint16_t)bits);
    restore_interrupts(irq);
}

uint8_t fault_active(void)
{
    return (uint8_t)(fault_word & FAULT_ACTIVE_MASK);
}

uint8_t fault_latched(void)
{
    return (uint8_t)(fault_word >> FAULT_LATCH_SHIFT);
}
