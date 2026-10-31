#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "barcode_decoder.h"
#include "barcode_reader.h"

/* Array and packed APIs must share acceptance, including optical correction. */

static const uint16_t patterns[26] = {
    0x109, 0x049, 0x148, 0x019, 0x118, 0x058, 0x00D,
    0x10C, 0x04C, 0x01C, 0x103, 0x043, 0x142, 0x013,
    0x112, 0x052, 0x007, 0x106, 0x046, 0x016, 0x181,
    0x0C1, 0x1C0, 0x091, 0x190, 0x0D0,
};

static uint32_t random_value(void)
{
    static uint32_t state = UINT32_C(0x164321);
    state = state * UINT32_C(1664525) + UINT32_C(1013904223);
    return state;
}

static void make_runs(unsigned int letter, uint32_t scale, bool reverse,
                       uint32_t *runs)
{
    const uint16_t symbols[3] = {0x094, patterns[letter], 0x094};
    for (unsigned int i = 0u; i < BARCODE_CODE39_RUN_COUNT; ++i) {
        unsigned int element = i % 10u;
        bool wide = element < 9u &&
            (symbols[i / 10u] & (1u << (8u - element))) != 0u;
        unsigned int destination = reverse
            ? BARCODE_CODE39_RUN_COUNT - 1u - i : i;
        runs[destination] = scale * (wide ? 3u : 1u);
    }
}

static void compare_decoders(const uint32_t *runs, char expected)
{
    barcode_runs_t packed = {{0}};
    for (unsigned int i = 0u; i < BARCODE_CODE39_RUN_COUNT; ++i) {
        assert(barcode_runs_set(&packed, i, runs[i]));
    }
    char reference = '\0';
    char decoded = '\0';
    bool reference_valid = barcode_decode_code39_letter(runs, &reference);
    bool valid = barcode_decode_code39_packed(&packed, &decoded);
    assert(valid == reference_valid);
    if (valid) {
        assert(decoded == reference);
    }
    if (expected != '\0') {
        assert(valid && decoded == expected);
    }
}

static void check_storage(void)
{
    barcode_runs_t packed = {{0}};
    uint32_t expected[BARCODE_CODE39_RUN_COUNT] = {0};
    const uint32_t edges[] = {0, 1, 15, 16, 255, 256, 65535, 65536, 750000};
    for (unsigned int i = 0u; i < BARCODE_CODE39_RUN_COUNT; ++i) {
        for (unsigned int edge = 0u; edge < sizeof(edges) / sizeof(edges[0]);
             ++edge) {
            assert(barcode_runs_set(&packed, i, edges[edge]));
            expected[i] = edges[edge];
            for (unsigned int j = 0u; j < BARCODE_CODE39_RUN_COUNT; ++j) {
                assert(barcode_runs_get(&packed, j) == expected[j]);
            }
        }
    }
    for (unsigned int n = 0u; n < 10000u; ++n) {
        unsigned int index = random_value() % BARCODE_CODE39_RUN_COUNT;
        expected[index] = random_value() % (BARCODE_RUN_MAX_US + 1u);
        assert(barcode_runs_set(&packed, index, expected[index]));
        for (unsigned int j = 0u; j < BARCODE_CODE39_RUN_COUNT; ++j) {
            assert(barcode_runs_get(&packed, j) == expected[j]);
        }
    }
    barcode_runs_t saved = packed;
    assert(!barcode_runs_set(&packed, 29u, 1u));
    assert(!barcode_runs_set(&packed, 0u, 750001u));
    assert(!barcode_runs_set(NULL, 0u, 1u));
    assert(memcmp(&packed, &saved, sizeof(packed)) == 0);
    assert(barcode_runs_get(&packed, 29u) == 0u);
    assert(barcode_runs_get(NULL, 0u) == 0u);
    assert((packed.bytes[72] & 0xf0u) == 0u);
    /* A reserved twenty-bit code cannot become a believable timing. */
    packed.bytes[0] = 0xffu;
    packed.bytes[1] = 0xffu;
    packed.bytes[2] |= 0x0fu;
    assert(barcode_runs_get(&packed, 0u) == 0u);
}

static bool feed(barcode_reader_t *reader, const uint32_t *runs,
                  uint32_t *now, char *decoded)
{
    bool black = true;
    bool result = barcode_reader_update(reader, black, *now, decoded);
    for (unsigned int i = 0u; i < BARCODE_CODE39_RUN_COUNT; ++i) {
        *now += runs[i];
        black = !black;
        result = barcode_reader_update(reader, black, *now, decoded) || result;
    }
    return result;
}

static void check_reader(void)
{
    uint32_t runs[BARCODE_CODE39_RUN_COUNT];
    make_runs(0u, 123u, false, runs);
    barcode_reader_t reader;
    barcode_reader_init(&reader);
    /* Exercises unsigned microsecond-clock wrap, without reducing timestamps. */
    uint32_t now = UINT32_MAX - 1000u;
    char decoded = '\0';
    assert(feed(&reader, runs, &now, &decoded) && decoded == 'A');
    ++now;
    (void)barcode_reader_update(&reader, false, now, &decoded);
    assert(!feed(&reader, runs, &now, &decoded));
    now += 3000000u;
    (void)barcode_reader_update(&reader, false, now, &decoded);
    assert(feed(&reader, runs, &now, &decoded) && decoded == 'A');

    barcode_reader_init(&reader);
    (void)barcode_reader_update(&reader, true, 0u, &decoded);
    assert(!barcode_reader_update(&reader, false, 750000u, &decoded));
    assert(barcode_reader_active(&reader));
    assert(barcode_runs_get(&reader.runs, 0u) == 750000u);
    assert(!barcode_reader_update(&reader, false, 1500001u, &decoded));
    assert(!barcode_reader_active(&reader));
    /* Needs white to re-arm after abort, then black to begin again. */
    (void)barcode_reader_update(&reader, true, 1500002u, &decoded);
    assert(!barcode_reader_active(&reader));
    (void)barcode_reader_update(&reader, false, 1500003u, &decoded);
    (void)barcode_reader_update(&reader, true, 1500004u, &decoded);
    assert(barcode_reader_active(&reader));
    barcode_reader_init(NULL);
    assert(!barcode_reader_update(NULL, false, 0u, &decoded));
    assert(!barcode_reader_update(&reader, false, 0u, NULL));
}

int main(void)
{
    check_storage();
    uint32_t runs[BARCODE_CODE39_RUN_COUNT];
    const uint32_t scales[] = {1u, 123u, 250000u};
    for (unsigned int letter = 0u; letter < 26u; ++letter) {
        for (unsigned int scale = 0u; scale < 3u; ++scale) {
            for (unsigned int reverse = 0u; reverse < 2u; ++reverse) {
                make_runs(letter, scales[scale], reverse != 0u, runs);
                compare_decoders(runs, (char)('A' + letter));
            }
        }
    }
    for (unsigned int trial = 0u; trial < 5000u; ++trial) {
        if (trial % 2u == 0u) {
            make_runs(random_value() % 26u, 10000u, trial % 4u == 0u, runs);
            runs[random_value() % 29u] = random_value() % 750001u;
        } else {
            for (unsigned int i = 0u; i < 29u; ++i) {
                runs[i] = random_value() % 750001u;
            }
        }
        compare_decoders(runs, '\0');
    }
    check_reader();
    printf("PASS packed storage: 10000 neighbour checks, 156 valid barcodes, "
           "5000 array/packed comparisons, wrap/timeout/duplicate checks; "
           "reader=%zu B\n", sizeof(barcode_reader_t));
    return 0;
}
