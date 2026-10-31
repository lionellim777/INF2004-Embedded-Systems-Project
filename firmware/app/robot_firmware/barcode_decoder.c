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

typedef uint32_t (*run_getter_t)(const void *runs, unsigned int index);

static uint32_t array_run_get(const void *runs, unsigned int index)
{
    return ((const uint32_t *)runs)[index];
}

static uint32_t packed_run_get(const void *runs, unsigned int index)
{
    return barcode_runs_get((const barcode_runs_t *)runs, index);
}

/* A-Z and '*' each contain two wide black bars and one wide white gap.
 * Threshold crossing can shorten all black runs and lengthen all white runs.
 * Estimate that additive offset from the median of the three narrow runs of
 * each colour. Use doubled units to retain half-unit offsets without floats.
 * Candidate ranks alone never accept a symbol: corrected wide/narrow runs
 * must still satisfy the original1.8 separation, plus within-class coherence.
 */
static bool classify_pattern(const uint32_t widths[CODE39_ELEMENTS],
                             uint16_t *pattern,
                             uint32_t *white_threshold, int outer_narrow)
{
    uint32_t sorted[2][5] = {{0}};
    unsigned count[2] = {0};
    for (unsigned i=0;i<CODE39_ELEMENTS;i++) {
        if (!widths[i] || widths[i]>BARCODE_RUN_MAX_US) return false;
        unsigned colour=i%2, insert=count[colour]++;
        while (insert && sorted[colour][insert-1]>widths[i]) {
            sorted[colour][insert]=sorted[colour][insert-1];--insert;
        }
        sorted[colour][insert]=widths[i];
    }
    /* Three narrow elements of each colour. Ties at the split are ambiguous. */
    if (sorted[0][3]<=sorted[0][2] || sorted[1][3]<=sorted[1][2]) return false;
    int64_t offset=(int64_t)sorted[1][1]-sorted[0][1];
    int64_t min_narrow=INT64_MAX,max_narrow=0,min_wide=INT64_MAX,max_wide=0;
    int64_t outer_width=0, narrow[6];
    unsigned narrow_count=0;
    *pattern=0;
    for (unsigned i=0;i<CODE39_ELEMENTS;i++) {
        unsigned colour=i%2;
        bool wide=widths[i]>=sorted[colour][3];
        int64_t corrected=2*(int64_t)widths[i]+(colour ? -offset : offset);
        if (corrected<=0) return false;
        *pattern=(uint16_t)((*pattern<<1) | (wide?1u:0u));
        /* Only the physical first/last narrow BLACK guard element may use
         * bounded expansion. Never reinterpret a ranked wide bar as narrow. */
        if ((int)i == outer_narrow) {
            if (wide || colour) return false;
            outer_width=corrected;
            continue;
        }
        if(wide){
            if(corrected<min_wide)min_wide=corrected;
            if(corrected>max_wide)max_wide=corrected;
        }else{
            unsigned insert=narrow_count++;
            while(insert && narrow[insert-1]>corrected){
                narrow[insert]=narrow[insert-1];--insert;
            }
            narrow[insert]=corrected;
            if(corrected<min_narrow)min_narrow=corrected;
            if(corrected>max_narrow)max_narrow=corrected;
        }
    }
    if(min_wide*10<max_narrow*18 || max_narrow>2*min_narrow || max_wide>2*min_wide)
        return false;
    if (outer_narrow >= 0) {
        /* End-of-frame optics/speed can expand a terminal narrow bar. Permit
         * ONE such bar per frame, with all other elements strict. It must be
         * an expansion, <=3x the other narrow median and at least 20% shorter
         * than every corrected wide element. Missing/extra runs are not fixed.
         * These are provisional acceptance bounds, not proof of the cause. */
        if(narrow_count!=5 || outer_width<=max_narrow ||
           outer_width>3*narrow[2] || outer_width*5>min_wide*4) return false;
    }
    /* Separator is white: compare in its original measurement domain. */
    *white_threshold=sorted[1][2]+(sorted[1][3]-sorted[1][2])/2u;
    return true;
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
    const void *runs, run_getter_t get_run, bool reverse,
    char *letter, int physical_outer)
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
            widths[element] = get_run(runs, reverse
                ? BARCODE_CODE39_RUN_COUNT - 1u - index : index);
        }

        int outer=-1;
        int logical_outer=physical_outer;
        if(reverse && physical_outer>=0) logical_outer=28-physical_outer;
        if(logical_outer>=0 && (unsigned)logical_outer/10u==character)
            outer=logical_outer%10;
        if (!classify_pattern(widths, &patterns[character],
                              &thresholds[character], outer)) {
            return false;
        }
    }

    if (patterns[0] != CODE39_ASTERISK ||
        patterns[2] != CODE39_ASTERISK) {
        return false;
    }

    /* Inter-character separators must be narrow. */
    uint32_t first_gap = get_run(runs, reverse ? 19u : 9u);
    uint32_t second_gap = get_run(runs, reverse ? 9u : 19u);
    if (!first_gap || !second_gap || first_gap >= thresholds[0] ||
        first_gap >= thresholds[1] || second_gap >= thresholds[1] ||
        second_gap >= thresholds[2]) {
        return false;
    }

    *letter = pattern_to_letter(patterns[1]);
    return *letter != '\0';
}

/* Try strict first. A bounded fallback can affect only one physical outer
 * guard bar, never a payload or interior marker element. Conflicting candidates
 * are rejected, even if a future pattern set makes reverse decoding ambiguous. */
static bool decode(const void *runs, run_getter_t get_run, char *letter)
{
    static const int outer_modes[]={-1,0,28};
    for(unsigned mode=0;mode<3;mode++) {
        char accepted=0;
        /* Evaluate both boundary alternatives together before accepting. */
        unsigned last=mode?3:1;
        for(unsigned m=mode;m<last;m++)for(unsigned reverse=0;reverse<2;reverse++) {
            char candidate=0;
            if(decode_direction(runs,get_run,reverse,&candidate,outer_modes[m])) {
                if(accepted && accepted!=candidate)return false;
                accepted=candidate;
            }
        }
        if(accepted){*letter=accepted;return true;}
        if(mode)break;
    }
    return false;
}

bool barcode_decode_code39_letter(
    const uint32_t runs[BARCODE_CODE39_RUN_COUNT], char *letter)
{
    if (runs == NULL || letter == NULL) return false;
    return decode(runs,array_run_get,letter);
}

bool barcode_decode_code39_packed(const barcode_runs_t *runs, char *letter)
{
    if (runs == NULL || letter == NULL) return false;
    return decode(runs,packed_run_get,letter);
}
