#ifndef INF2004_BARCODE_READER_H
#define INF2004_BARCODE_READER_H

#include <stdbool.h>
#include <stdint.h>

#include "barcode_decoder.h"

typedef struct {
    uint32_t edge_us;
    uint32_t last_emit_us;
    barcode_runs_t runs;
    uint8_t run_count;
    char last_letter;
    bool level_black;
    bool active;
    bool armed;
    bool search_frames; /* independent optical stream: slide past rejected prefixes */
} barcode_reader_t;

void barcode_reader_init(barcode_reader_t *reader);
bool barcode_reader_update(barcode_reader_t *reader, bool sensor_black,
                           uint32_t now_us, char *letter);
bool barcode_reader_active(const barcode_reader_t *reader);

#endif
