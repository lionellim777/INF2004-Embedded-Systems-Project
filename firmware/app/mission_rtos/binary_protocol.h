#ifndef RTOS_BINARY_PROTOCOL_H
#define RTOS_BINARY_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Transport topics carry version v1. Byte padding is zero and validated.
 * Barcode: capture_ms32, code5 (0 none,1..26 A..Z,27 unreadable), valid1,
 * simulated1, padding1. Status: capture_ms32, boot_id32, available5,
 * simulated5, connection_state3, padding3. Simulation is a subset of available. */
#define BARCODE_RECORD_BYTES 5u
#define STATUS_RECORD_BYTES 10u
/* Line: time32, ADC12x3 in GP26/GP28/GP27 order, pattern3, valid1. */
#define LINE_RECORD_BYTES 9u
bool line_record_encode(uint32_t ms,const uint16_t raw[3],uint8_t pattern,
                        bool valid,uint8_t out[LINE_RECORD_BYTES]);
bool barcode_record_encode(uint32_t ms, char letter, bool valid, bool simulated,
                           uint8_t out[BARCODE_RECORD_BYTES]);
bool barcode_record_valid(const uint8_t *bytes, size_t size);
bool status_record_encode(uint32_t ms, uint32_t boot_id, uint8_t available,
    uint8_t simulated, uint8_t state, uint8_t out[STATUS_RECORD_BYTES]);

enum { BINARY_COMMAND_INVALID, BINARY_COMMAND_STATUS, BINARY_COMMAND_PERIOD, BINARY_COMMAND_STOP };
/* Shared command state is network-task-owned. Maximum two payload bytes.
 * state bits: kind0..1, active2, expected3..4, received5..6, padding7. */
typedef struct { uint8_t payload[2], state; } binary_command_frame_t;
typedef struct { uint16_t period_ms; uint8_t kind; } binary_command_t;
bool binary_command_begin(binary_command_frame_t *f, uint8_t kind, size_t length);
bool binary_command_append(binary_command_frame_t *f, const uint8_t *bytes,
                           size_t length, bool last, binary_command_t *out);
#endif
