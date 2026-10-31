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

#endif
