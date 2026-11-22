#ifndef RTOS_TRACK_SENSOR_H
#define RTOS_TRACK_SENSOR_H
#include <stdbool.h>
#include <stdint.h>
#include "scan_diagnostic.h"

/* One ADC IRQ producer, one decoder-task consumer. IRQ code never logs,
 * decodes, waits on Wi-Fi or calls the kernel. Tokens isolate stopped runs. */
#define TRACK_SAMPLE_US 500u
#define TRACK_FRESH_US 3000u
enum
{
    TRACK_QUEUE_FAULT = 1u,
    TRACK_TIMING_FAULT = 2u,
    TRACK_EVENT_FAULT = 4u
};
typedef struct
{
    uint32_t us;
    uint32_t age_us;
    uint16_t raw[3];
    bool ready;
} track_ir_t;
typedef struct
{
    uint32_t us;
    char letter;
    bool crossing;
} track_event_t;
typedef struct
{
    uint32_t samples;
    uint32_t frames;
    uint32_t rejected;
    uint32_t decode_max_us;
    uint32_t adc_max_us;
    uint16_t queue_peak;
    uint16_t events;
    uint16_t ack;
    uint8_t faults;
    bool recording;
} track_diagnostics_t;
bool track_sensor_init(void);
void track_sensor_read(track_ir_t *out);
void track_request(uint16_t token);
void track_stop(void);
bool track_ready(uint16_t token);
bool track_faulted(void);
bool track_recording(void);
bool track_event_take(uint16_t token, track_event_t *out);
void track_diagnostics(track_diagnostics_t *out);
/* Decoder task only; bounded queue drain. Motion may preempt any decode. */
void track_service(void);
void track_recoveries(uint32_t count);
uint16_t track_scan_run(void);
size_t track_scan_state(uint8_t out[SCAN_PACKET_BYTES]);
/* Only sealed/stopped captures can be exported. Copy native data under a
 * short IRQ lock, serialize outside it. Run token prevents mixed captures. */
size_t track_scan_export(uint16_t run, unsigned index,
                         uint8_t out[SCAN_PACKET_BYTES]);
#endif
