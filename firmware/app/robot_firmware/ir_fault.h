#ifndef INF2004_IR_FAULT_H
#define INF2004_IR_FAULT_H

#include <stddef.h>
#include <stdint.h>

#include "barcode_acquisition.h"

/*
 * IR fault bit in the shared fault mask. Bits 0-5 belong to motion
 * (Buddy 2); bit 6 is the IR/barcode sensing fault.
 *
 * The IR fault is raised when barcode sampling integrity is lost: a sample
 * gap above BARCODE_SAMPLE_GAP_US, an edge-buffer overflow or an event-queue
 * overflow. In each case the capture has already discarded its letter.
 */
enum { FAULT_BIT_IR = 6 };

#define FAULT_MASK_IR ((uint16_t)(1u << FAULT_BIT_IR))

static inline uint16_t ir_fault_bits(const barcode_capture_t *capture)
{
    return ((capture != NULL) && capture->integrity_fault) ? FAULT_MASK_IR : 0u;
}

#endif
