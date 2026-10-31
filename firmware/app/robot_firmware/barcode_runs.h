#ifndef INF2004_BARCODE_RUNS_H
#define INF2004_BARCODE_RUNS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Packed width limit. Legacy modes use microseconds; centred course uses
 * 1000 units per summed wheel count. Decoder accepts any consistent units. */
enum {
    /* Code 39 *X*: three nine-element symbols plus two separators. */
    BARCODE_CODE39_RUN_COUNT = 29,
    BARCODE_RUN_MAX_US = 750000,
    BARCODE_RUN_BITS = 20,
    BARCODE_RUN_STORAGE_BYTES =
        (BARCODE_CODE39_RUN_COUNT * BARCODE_RUN_BITS + 7) / 8,
};

/* Little-endian bits: run i starts at bit i * 20; final four bits unused.
 * Owned by the barcode task. These multi-byte writes are not atomic. */
typedef struct {
    uint8_t bytes[BARCODE_RUN_STORAGE_BYTES];
} barcode_runs_t;

_Static_assert(sizeof(barcode_runs_t) == 73u, "barcode timing storage size");

static inline bool barcode_runs_set(barcode_runs_t *runs,
                                    unsigned int index, uint32_t width_us)
{
    if (runs == NULL || index >= BARCODE_CODE39_RUN_COUNT ||
        width_us > BARCODE_RUN_MAX_US) {
        return false;
    }

    unsigned int byte = (index * BARCODE_RUN_BITS) / 8u;
    unsigned int shift = (index * BARCODE_RUN_BITS) % 8u;
    uint32_t word = (uint32_t)runs->bytes[byte] |
        ((uint32_t)runs->bytes[byte + 1u] << 8u) |
        ((uint32_t)runs->bytes[byte + 2u] << 16u);
    uint32_t mask = UINT32_C(0xfffff) << shift;
    word = (word & ~mask) | (width_us << shift);
    runs->bytes[byte] = (uint8_t)word;
    runs->bytes[byte + 1u] = (uint8_t)(word >> 8u);
    runs->bytes[byte + 2u] = (uint8_t)(word >> 16u);
    return true;
}

/* Decoder getter; out-of-bounds/corrupt values become an invalid zero run.
 * Zero can be captured at timestamp resolution, but is rejected by decoding. */
static inline uint32_t barcode_runs_get(const barcode_runs_t *runs,
                                        unsigned int index)
{
    if (runs == NULL || index >= BARCODE_CODE39_RUN_COUNT) {
        return 0u;
    }

    unsigned int byte = (index * BARCODE_RUN_BITS) / 8u;
    unsigned int shift = (index * BARCODE_RUN_BITS) % 8u;
    uint32_t word = (uint32_t)runs->bytes[byte] |
        ((uint32_t)runs->bytes[byte + 1u] << 8u) |
        ((uint32_t)runs->bytes[byte + 2u] << 16u);
    uint32_t value = (word >> shift) & UINT32_C(0xfffff);
    return value <= BARCODE_RUN_MAX_US ? value : 0u;
}

#endif
