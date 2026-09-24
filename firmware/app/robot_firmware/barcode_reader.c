#include "barcode_reader.h"

#include <string.h>

enum {
    BARCODE_EDGE_TIMEOUT_US = 750000,
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
    memset(reader, 0, sizeof(*reader));
    reader->armed = true;
}

bool barcode_reader_update(barcode_reader_t *reader, bool broad_black,
                           uint32_t now_us, char *letter)
{
    if (reader == NULL || letter == NULL) {
        return false;
    }

    if (!reader->active) {
        if (!broad_black) {
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

    if (broad_black == reader->level_black) {
        return false;
    }

    reader->runs[reader->run_count++] = width_us;
    reader->level_black = broad_black;
    reader->edge_us = now_us;

    if (reader->run_count < BARCODE_CODE39_RUN_COUNT) {
        return false;
    }

    char decoded = '\0';
    bool valid = barcode_decode_code39_letter(reader->runs, &decoded);
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
