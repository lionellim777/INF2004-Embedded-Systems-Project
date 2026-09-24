#ifndef INF2004_BARCODE_DECODER_H
#define INF2004_BARCODE_DECODER_H

#include <stdbool.h>
#include <stdint.h>

/* Code 39 *X*: 3 characters x 9 elements plus 2 narrow separators. */
enum { BARCODE_CODE39_RUN_COUNT = 29 };

/*
 * Runs must alternate black/white, beginning and ending with black.
 * Widths may be time, encoder counts or any consistent non-zero unit.
 * Decodes one Code 39 letter (A-Z) between '*' start/stop characters.
 */
bool barcode_decode_code39_letter(
    const uint32_t runs[BARCODE_CODE39_RUN_COUNT], char *letter);

#endif
