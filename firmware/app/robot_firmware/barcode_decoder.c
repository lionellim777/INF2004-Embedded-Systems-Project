#include "barcode_decoder.h"

#include <stddef.h>

enum {
    CODE39_ELEMENTS = 9,
    CODE39_WIDE_ELEMENTS = 3,
    CODE39_CHARACTER_STRIDE = 10,
    CODE39_CHARACTER_COUNT = 3,
    CODE39_ASTERISK = 0x094,
};

static const uint16_t letter_patterns[26] = {
    0x109, 0x049, 0x148, 0x019, 0x118, 0x058, 0x00D,
    0x10C, 0x04C, 0x01C, 0x103, 0x043, 0x142, 0x013,
    0x112, 0x052, 0x007, 0x106, 0x046, 0x016, 0x181,
    0x0C1, 0x1C0, 0x091, 0x190, 0x0D0,
};

static bool classify_pattern(const uint32_t widths[CODE39_ELEMENTS],
                             uint16_t *pattern,
                             uint32_t *wide_threshold)
{
    uint32_t sorted[CODE39_ELEMENTS];

    for (unsigned int index = 0u; index < CODE39_ELEMENTS; ++index) {
        if (widths[index] == 0u) {
            return false;
        }

        unsigned int insert = index;
        while (insert > 0u && sorted[insert - 1u] > widths[index]) {
            sorted[insert] = sorted[insert - 1u];
            --insert;
        }
        sorted[insert] = widths[index];
    }

    uint32_t max_narrow = sorted[CODE39_ELEMENTS -
                                 CODE39_WIDE_ELEMENTS - 1u];
    uint32_t min_wide = sorted[CODE39_ELEMENTS - CODE39_WIDE_ELEMENTS];
    if ((uint64_t)min_wide * 10u < (uint64_t)max_narrow * 18u) {
        return false;
    }

    *wide_threshold = max_narrow + ((min_wide - max_narrow) / 2u);
    *pattern = 0u;
    unsigned int wide_count = 0u;

    for (unsigned int index = 0u; index < CODE39_ELEMENTS; ++index) {
        *pattern <<= 1u;
        if (widths[index] >= *wide_threshold) {
            *pattern |= 1u;
            ++wide_count;
        }
    }

    return wide_count == CODE39_WIDE_ELEMENTS;
}

static char pattern_to_letter(uint16_t pattern)
{
    for (unsigned int index = 0u; index < 26u; ++index) {
        if (letter_patterns[index] == pattern) {
            return (char)('A' + index);
        }
    }
    return '\0';
}

static bool decode_direction(
    const uint32_t runs[BARCODE_CODE39_RUN_COUNT], bool reverse,
    char *letter)
{
    uint16_t patterns[CODE39_CHARACTER_COUNT];
    uint32_t thresholds[CODE39_CHARACTER_COUNT];

    for (unsigned int character = 0u; character < CODE39_CHARACTER_COUNT;
         ++character) {
        uint32_t widths[CODE39_ELEMENTS];
        unsigned int offset = character * CODE39_CHARACTER_STRIDE;

        for (unsigned int element = 0u; element < CODE39_ELEMENTS;
             ++element) {
            unsigned int index = offset + element;
            widths[element] = reverse
                ? runs[BARCODE_CODE39_RUN_COUNT - 1u - index]
                : runs[index];
        }

        if (!classify_pattern(widths, &patterns[character],
                              &thresholds[character])) {
            return false;
        }
    }

    if (patterns[0] != CODE39_ASTERISK ||
        patterns[2] != CODE39_ASTERISK) {
        return false;
    }

    /* Inter-character separators must be narrow. */
    uint32_t first_gap = reverse ? runs[19] : runs[9];
    uint32_t second_gap = reverse ? runs[9] : runs[19];
    if (first_gap >= thresholds[0] || second_gap >= thresholds[1]) {
        return false;
    }

    *letter = pattern_to_letter(patterns[1]);
    return *letter != '\0';
}

bool barcode_decode_code39_letter(
    const uint32_t runs[BARCODE_CODE39_RUN_COUNT], char *letter)
{
    if (runs == NULL || letter == NULL) {
        return false;
    }

    return decode_direction(runs, false, letter) ||
           decode_direction(runs, true, letter);
}
