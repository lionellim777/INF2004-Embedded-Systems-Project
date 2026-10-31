#ifndef INF2004_BARCODE_RUNS_H
#define INF2004_BARCODE_RUNS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Packed bar/space widths, 10 bits each (minimum-bit representation).
 *
 * Callers still pass and receive widths in their capture unit (microseconds
 * for optical timing, 1000 units per summed wheel count in distance mode).
 * Storage keeps one step per BARCODE_RUN_QUANTUM units, rounded to nearest:
 * 1 ms for optical timing. The 750 ms run limit is 750 steps, so 10 bits
 * (0..1023) are sufficient. Timestamps and decoder arithmetic stay 32-bit.
 * The decoder only uses width ratios, so the common x1000 scale is harmless.
 */
enum {
    /* Code 39 *X*: three nine-element symbols plus two separators. */
    BARCODE_CODE39_RUN_COUNT = 29,
    BARCODE_RUN_MAX_US = 750000,
    BARCODE_RUN_QUANTUM = 1000,
    BARCODE_RUN_BITS = 10,
    BARCODE_RUN_STORAGE_BYTES =
        (BARCODE_CODE39_RUN_COUNT * BARCODE_RUN_BITS + 7) / 8,
};

_Static_assert(BARCODE_RUN_MAX_US / BARCODE_RUN_QUANTUM <
                   (1 << BARCODE_RUN_BITS),
               "maximum run does not fit the packed field");

/* Little-endian bits: run i starts at bit i * 10; final six bits unused.
 * A 10-bit field starts at bit offset 0, 2, 4 or 6, so it always lies within
 * two bytes. Owned by the barcode task; multi-byte writes are not atomic. */
typedef struct {
    uint8_t bytes[BARCODE_RUN_STORAGE_BYTES];
} barcode_runs_t;

_Static_assert(sizeof(barcode_runs_t) == 37u, "barcode timing storage size");

static inline bool barcode_runs_set(barcode_runs_t *runs,
                                    unsigned int index, uint32_t width_us)
{
    if (runs == NULL || index >= BARCODE_CODE39_RUN_COUNT ||
        width_us > BARCODE_RUN_MAX_US) {
        return false;
    }

    uint32_t steps = (width_us + (BARCODE_RUN_QUANTUM / 2u)) /
                     BARCODE_RUN_QUANTUM;
    unsigned int byte = (index * BARCODE_RUN_BITS) / 8u;
    unsigned int shift = (index * BARCODE_RUN_BITS) % 8u;
    uint32_t word = (uint32_t)runs->bytes[byte] |
        ((uint32_t)runs->bytes[byte + 1u] << 8u);
    uint32_t mask = UINT32_C(0x3ff) << shift;

    word = (word & ~mask) | (steps << shift);
    runs->bytes[byte] = (uint8_t)word;
    runs->bytes[byte + 1u] = (uint8_t)(word >> 8u);
    return true;
}

/* Returns the stored width in capture units (a multiple of the quantum).
 * Out-of-bounds/corrupt values become an invalid zero run. A run shorter
 * than half a quantum is stored as zero, which the decoder rejects. */
static inline uint32_t barcode_runs_get(const barcode_runs_t *runs,
                                        unsigned int index)
{
    if (runs == NULL || index >= BARCODE_CODE39_RUN_COUNT) {
        return 0u;
    }

    unsigned int byte = (index * BARCODE_RUN_BITS) / 8u;
    unsigned int shift = (index * BARCODE_RUN_BITS) % 8u;
    uint32_t word = (uint32_t)runs->bytes[byte] |
        ((uint32_t)runs->bytes[byte + 1u] << 8u);
    uint32_t steps = (word >> shift) & UINT32_C(0x3ff);
    uint32_t value = steps * BARCODE_RUN_QUANTUM;

    return value <= BARCODE_RUN_MAX_US ? value : 0u;
}

#endif
