#ifndef MISSION_MOTION_RUNTIME_H
#define MISSION_MOTION_RUNTIME_H
#include "motion_control.h"
typedef struct {
    motion_control_t motion; /* unchanged validated working controller */
    uint32_t previous_left, previous_right, previous_us, changed_us;
    volatile uint32_t max_gap_us; /* one writer; atomic aligned diagnostic read */
    uint16_t token;
    uint8_t buttons; /* raw:0, stable:1, pending start:2; one task owns all bits */
} motion_runtime_t;
void motion_runtime_init(motion_runtime_t *r, encoder_sample_t enc, uint32_t us, bool down);
void motion_runtime_step(motion_runtime_t *r, encoder_sample_t enc, uint64_t now, bool down);
#endif
