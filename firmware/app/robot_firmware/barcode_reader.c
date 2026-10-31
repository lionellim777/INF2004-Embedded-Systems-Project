#include "barcode_reader.h"

#include <string.h>

enum {
    BARCODE_EDGE_TIMEOUT_US = BARCODE_RUN_MAX_US,
    BARCODE_DUPLICATE_TIMEOUT_US = 3000000,
};

static void stop_capture(barcode_reader_t *reader)
{
    reader->run_count = 0u;
    reader->active = false;
    reader->armed = false;
}

void barcode_reader_init(barcode_reader_t *reader)
{
    if (reader == NULL) {
        return;
    }
    memset(reader, 0, sizeof(*reader));
    reader->armed = true;
}

bool barcode_reader_update(barcode_reader_t *reader, bool sensor_black,
                           uint32_t now_us, char *letter)
{
    if (reader == NULL || letter == NULL) {
        return false;
    }

    if (!reader->active) {
        if (!sensor_black) {
            reader->armed = true;
        } else if (reader->armed) {
            reader->active = true;
            reader->level_black = true;
            reader->edge_us = now_us;
            reader->run_count = 0u;
        }
        return false;
    }

    uint32_t width_us = now_us - reader->edge_us;
    if (width_us > BARCODE_EDGE_TIMEOUT_US) {
        stop_capture(reader);
        return false;
    }

    if (sensor_black == reader->level_black) {
        return false;
    }

    /* In stream mode retain29 complete intervals until the next edge, so the
     * caller can report the rejected frame. Slide by two to preserve BLACK
     * alignment; a spurious prefix must not hide a later valid start marker. */
    if (reader->search_frames && reader->run_count == BARCODE_CODE39_RUN_COUNT) {
        for (unsigned i=0;i<BARCODE_CODE39_RUN_COUNT-2;i++)
            barcode_runs_set(&reader->runs,i,barcode_runs_get(&reader->runs,i+2));
        reader->run_count-=2;
    }
    if (!barcode_runs_set(&reader->runs, reader->run_count, width_us)) {
        stop_capture(reader);
        return false;
    }
    ++reader->run_count;
    reader->level_black = sensor_black;
    reader->edge_us = now_us;

    if (reader->run_count < BARCODE_CODE39_RUN_COUNT) {
        return false;
    }

    char decoded = '\0';
    bool valid = barcode_decode_code39_packed(&reader->runs, &decoded);
    if (!valid && reader->search_frames) return false;
    stop_capture(reader);

    if (!valid) {
        return false;
    }

    bool duplicate = decoded == reader->last_letter &&
        (now_us - reader->last_emit_us) < BARCODE_DUPLICATE_TIMEOUT_US;
    if (duplicate) {
        return false;
    }

    reader->last_letter = decoded;
    reader->last_emit_us = now_us;
    *letter = decoded;
    return true;
}

bool barcode_reader_active(const barcode_reader_t *reader)
{
    return reader != NULL && reader->active;
}
