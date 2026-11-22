#ifndef RTOS_SCAN_DIAGNOSTIC_H
#define RTOS_SCAN_DIAGNOSTIC_H
#include "barcode_waveform.h"
#include <stddef.h>

/* scan_diag/v1: little-endian bit fields, zero final-byte padding. These
 * diagnostic records supplement, never replace, the established telemetry.
 * The network task exports the frozen last substantial waveform after stop. */
#define SCAN_PACKET_BYTES 192u
#define SCAN_CHUNK_WORDS 64u
enum
{
    SCAN_STATE,
    SCAN_ATTEMPT,
    SCAN_SNAPSHOT,
    SCAN_PROFILE,
    SCAN_WORDS,
    SCAN_DONE
};
typedef struct
{
    uint32_t ms;
    uint32_t samples;
    uint32_t frames;
    uint32_t rejected;
    uint32_t decode_us;
    uint32_t adc_us;
    uint32_t attempt_overflow;
    uint32_t recoveries;
    uint32_t stop_us;
    uint16_t run;
    uint16_t events;
    uint16_t queue_peak;
    uint16_t stop_discarded;
    uint8_t faults;
    uint8_t attempts;
    bool enabled ROBOCAR_BIT1;
    bool recording ROBOCAR_BIT1;
    bool sealed ROBOCAR_BIT1;
    bool ready ROBOCAR_BIT1;
} scan_state_t;
typedef struct
{
    uint32_t start_us;
    uint32_t leading_us;
    uint32_t white_us;
    uint32_t duration_us;
    uint32_t checksum;
    uint16_t count;
    uint16_t paper;
    uint16_t signal_range;
    uint8_t reason;
    uint8_t coherent_mask;
    uint8_t geometry_mask;
    char letter;
    bool motion;
} scan_snapshot_t;
size_t scan_state_encode(const scan_state_t *s, uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_attempt_encode(uint16_t run, unsigned index,
                           const barcode_wave_attempt_t *a,
                           uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_snapshot_encode(uint16_t run, const scan_snapshot_t *s,
                            uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_profile_encode(uint16_t run, unsigned index,
                           const barcode_wave_profile_t *p,
                           uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_words_encode(uint16_t run, unsigned index, const uint16_t *words,
                         unsigned count, uint8_t out[SCAN_PACKET_BYTES]);
size_t scan_done_encode(uint16_t run, unsigned attempts, bool snapshot,
                        uint8_t out[SCAN_PACKET_BYTES]);
#endif
