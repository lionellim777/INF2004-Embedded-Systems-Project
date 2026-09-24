#ifndef INF2004_BARCODE_READER_H
#define INF2004_BARCODE_READER_H

#include <stdbool.h>
#include <stdint.h>

#include "barcode_decoder.h"

typedef struct {
    uint32_t runs[BARCODE_CODE39_RUN_COUNT];
    uint32_t edge_us;
    uint32_t last_emit_us;
    uint8_t run_count;
    char last_letter;
    bool level_black;
    bool active;
    bool armed;
} barcode_reader_t;

void barcode_reader_init(barcode_reader_t *reader);
bool barcode_reader_update(barcode_reader_t *reader, bool broad_black,
                           uint32_t now_us, char *letter);
bool barcode_reader_active(const barcode_reader_t *reader);

#endif
