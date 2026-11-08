#ifndef MISSION_MOTION_RECORD_H
#define MISSION_MOTION_RECORD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Binary schema motion/v1, LSB-first fields. Working arithmetic is not packed.
 * ms:0..31; relative left/right counts:32..47/48..63 signed16;
 * signed left/right mm/s:64..73/74..83; kind:84..86; phase:87..89;
 * motion fault:90..92; stop reason:93..95; valid:96; stop latched:97;
 * bits98..103 are zero padding for bench kinds, or the track extension below.
 * No raw structs on the wire.
 * 32-bit ms supports wrap at 49.7 days; consumers use bounded unsigned ages.
 * Relative counts cover -30000..30000 per explicitly started manoeuvre. Raw
 * cumulative encoder counts stay modulo32 in the existing driver.
 * Invalid records retain time/state but must zero numeric count/speed fields. */
/* Track mode kind7 extends both counts to signed19 using formerly reserved
 * tail bits98..100 (left high3) and101..103 (right high3). The 13-byte size,
 * all other offsets and all kind0..4 encodings are unchanged. Track bounds
 * are +/-160000 counts. Kinds5/6 remain unsupported. Old receivers reject
 * unknown kind7 rather than silently reading its extended counts. */
#define MOTION_TRACK_KIND 7u
#define MOTION_TRACK_COUNTS 160000
#define MOTION_TRACK_FAULT 6u /* Track supervisor; see TRACK status detail. */
#define MOTION_RECORD_BITS 98u
#define MOTION_TRACK_RECORD_BITS 104u
#define MOTION_RECORD_BYTES 13u
typedef struct {
    uint32_t captured_ms;
    int32_t left_counts, right_counts, left_mm_s, right_mm_s;
    uint8_t kind, phase, fault, stop_reason;
    bool valid, stop_latched;
} motion_record_t;
bool motion_record_encode(const motion_record_t *record, uint8_t *output, size_t size);
bool motion_record_decode(const uint8_t *input, size_t size, motion_record_t *record);
#endif
