#ifndef INF2004_BARCODE_DECODER_H
#define INF2004_BARCODE_DECODER_H

#include <stdbool.h>
#include <stdint.h>

#include "barcode_runs.h"

/*
 * Runs must alternate black/white, beginning and ending with black.
 * Widths may be time, encoder counts or any consistent non-zero unit.
 * Decodes one Code 39 letter (A-Z) between '*' start/stop characters.
 * Corrects an additive black/white crossing bias per symbol using narrow
 * medians; requires corrected wide/narrow separation >=1.8 and coherent
 * width classes. A bounded fallback permits ONE expanded outer narrow guard
 * bar, <=3x the other narrow median and <=80% of the shortest wide element;
 * all other elements remain strict. No missing-edge recovery or expected-letter
 * guessing. This tolerance is provisional until physically validated.
 */
bool barcode_decode_code39_letter(
    const uint32_t runs[BARCODE_CODE39_RUN_COUNT], char *letter);

/* Reads packed capture directly, without a second 29-element array. */
bool barcode_decode_code39_packed(const barcode_runs_t *runs, char *letter);

/* Diagnostic inspection of the strict path. Does not change acceptance, and
 * may report an outer-bar width failure for a frame the bounded fallback reads. */
enum { BARCODE_WIDTH_OK, BARCODE_WIDTH_RANGE, BARCODE_WIDTH_TIE,
    BARCODE_WIDTH_OFFSET, BARCODE_WIDTH_SEPARATION, BARCODE_WIDTH_NARROW_SPREAD,
    BARCODE_WIDTH_WIDE_SPREAD, BARCODE_WIDTH_OUTER };
typedef struct {
    uint16_t patterns[3];
    uint8_t width_reason[3], guards, separators, reversed, score;
} barcode_decode_diagnostic_t;
void barcode_inspect_code39(const uint32_t runs[29],barcode_decode_diagnostic_t *out);

#endif
