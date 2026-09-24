#include <stdint.h>
#include <stdio.h>

#include "barcode_decoder.h"
#include "barcode_reader.h"
#include "navigation_commands.h"

static const uint32_t barcode_a[BARCODE_CODE39_RUN_COUNT] = {
    1, 3, 1, 1, 3, 1, 3, 1, 1, 1,
    3, 1, 1, 1, 1, 3, 1, 1, 3, 1,
    1, 3, 1, 1, 3, 1, 3, 1, 1,
};

static const uint32_t barcode_z[BARCODE_CODE39_RUN_COUNT] = {
    1, 3, 1, 1, 3, 1, 3, 1, 1, 1,
    1, 3, 3, 1, 3, 1, 1, 1, 1, 1,
    1, 3, 1, 1, 3, 1, 3, 1, 1,
};

static int check(const uint32_t *runs, char expected)
{
    char decoded = '\0';
    if (!barcode_decode_code39_letter(runs, &decoded) ||
        decoded != expected) {
        printf("FAIL expected=%c decoded=%c\n", expected,
               decoded ? decoded : '?');
        return 1;
    }
    return 0;
}

static void build_barcode(uint16_t letter_pattern, uint32_t scale,
                          uint32_t *runs)
{
    const uint16_t patterns[3] = {0x094, letter_pattern, 0x094};

    for (unsigned int character = 0u; character < 3u; ++character) {
        unsigned int offset = character * 10u;
        for (unsigned int element = 0u; element < 9u; ++element) {
            bool wide = (patterns[character] & (1u << (8u - element))) != 0u;
            runs[offset + element] = scale * (wide ? 3u : 1u);
        }
        if (character < 2u) {
            runs[offset + 9u] = scale;
        }
    }
}

static int check_command(uint16_t pattern, char letter,
                         navigation_command_t expected)
{
    uint32_t runs[BARCODE_CODE39_RUN_COUNT];
    char decoded = '\0';
    build_barcode(pattern, 10u, runs);

    if (!barcode_decode_code39_letter(runs, &decoded) ||
        decoded != letter || navigation_decode_symbol(decoded) != expected) {
        printf("FAIL command %c\n", letter);
        return 1;
    }
    return 0;
}

static bool feed_reader(barcode_reader_t *reader, const uint32_t *runs,
                        uint32_t *now_us, char *decoded)
{
    bool black = true;
    bool ready = barcode_reader_update(reader, black, *now_us, decoded);

    for (unsigned int index = 0u; index < BARCODE_CODE39_RUN_COUNT;
         ++index) {
        *now_us += runs[index] * 100u;
        black = !black;
        ready = barcode_reader_update(reader, black, *now_us, decoded) || ready;
    }
    return ready;
}

int main(void)
{
    uint32_t scaled[BARCODE_CODE39_RUN_COUNT];
    uint32_t reversed[BARCODE_CODE39_RUN_COUNT];
    int failures = check(barcode_a, 'A') + check(barcode_z, 'Z');

    for (unsigned int index = 0u; index < BARCODE_CODE39_RUN_COUNT;
         ++index) {
        scaled[index] = barcode_a[index] * 10u;
        reversed[index] = barcode_a[BARCODE_CODE39_RUN_COUNT - 1u - index];
    }

    failures += check(scaled, 'A');
    failures += check(reversed, 'A');
    failures += check_command(0x109, 'A', NAV_COMMAND_LEFT);
    failures += check_command(0x049, 'B', NAV_COMMAND_RIGHT);
    failures += check_command(0x148, 'C', NAV_COMMAND_STRAIGHT);
    failures += check_command(0x019, 'D', NAV_COMMAND_UTURN);

    scaled[0] = 30u; /* Give the start marker four wide elements. */
    char ignored;
    if (barcode_decode_code39_letter(scaled, &ignored)) {
        puts("FAIL invalid barcode accepted");
        ++failures;
    }

    barcode_reader_t reader;
    barcode_reader_init(&reader);
    uint32_t now_us = 1000u;
    char decoded = '\0';
    if (!feed_reader(&reader, barcode_a, &now_us, &decoded) || decoded != 'A') {
        puts("FAIL live capture");
        ++failures;
    }

    decoded = '\0';
    ++now_us;
    (void)barcode_reader_update(&reader, false, now_us, &decoded);
    if (feed_reader(&reader, barcode_a, &now_us, &decoded)) {
        puts("FAIL duplicate command accepted");
        ++failures;
    }

    barcode_reader_init(&reader);
    decoded = '\0';
    (void)barcode_reader_update(&reader, true, 0u, &decoded);
    if (barcode_reader_update(&reader, true, 750001u, &decoded) ||
        barcode_reader_active(&reader)) {
        puts("FAIL junction rejected as barcode");
        ++failures;
    }

    puts(failures == 0 ? "PASS barcode/navigation pipeline"
                       : "FAIL barcode/navigation pipeline");
    return failures == 0 ? 0 : 1;
}
