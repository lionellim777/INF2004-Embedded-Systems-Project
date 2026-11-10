#ifndef RTOS_SCAN_DIAGNOSTIC_H
#define RTOS_SCAN_DIAGNOSTIC_H
#include "barcode_waveform.h"
#include <stddef.h>

/* scan_diag/v1: little-endian bit fields, zero final-byte padding. These
 * diagnostic records supplement, never replace, the established telemetry.
 * The network task exports the frozen last substantial waveform after stop. */
#define SCAN_PACKET_BYTES 192u
#define SCAN_CHUNK_WORDS 64u
enum { SCAN_STATE, SCAN_ATTEMPT, SCAN_SNAPSHOT, SCAN_PROFILE, SCAN_WORDS, SCAN_DONE };
typedef struct {
    uint32_t ms,samples,frames,rejected,decode_us,adc_us,attempt_overflow,recoveries,stop_us;
    uint16_t run,events,queue_peak,stop_discarded;
    uint8_t faults,attempts;
    bool enabled,recording,sealed,ready;
} scan_state_t;
typedef struct {
    uint32_t start_us,leading_us,white_us,duration_us,checksum;
    uint16_t count,paper,signal_range;
    uint8_t reason,coherent_mask,geometry_mask;
    char letter;
    bool motion;
} scan_snapshot_t;
size_t scan_state_encode(const scan_state_t *s,uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_attempt_encode(uint16_t run,unsigned index,const barcode_wave_attempt_t *a,uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_snapshot_encode(uint16_t run,const scan_snapshot_t *s,uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_profile_encode(uint16_t run,unsigned index,const barcode_wave_profile_t *p,uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_words_encode(uint16_t run,unsigned index,const uint16_t *words,unsigned count,uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_done_encode(uint16_t run,unsigned attempts,bool snapshot,uint8_t out[SCAN_PACKET_BYTES]);
#endif
