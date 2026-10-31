#ifndef BARCODE_ACQUISITION_H
#define BARCODE_ACQUISITION_H
#include <stdbool.h>
#include <stdint.h>
#include "barcode_reader.h"
#include "barcode_motion.h"
#define BARCODE_SAMPLE_US 500u
#define BARCODE_SAMPLE_GAP_US 3000u
#define BARCODE_EDGE_CAPACITY 64u
#define BARCODE_STREAM_EVENTS 16u
#define BARCODE_STREAM_QUIET_US 250000u
/* GP26 barcode only: 28 Oct stationary measurements, narrow black254-260,
 * white182-202. User-requested >250 black; release at220 avoids chatter.
 * The small narrow-black margin still needs full-pass validation. */
#define BARCODE_BLACK_ON 251u
#define BARCODE_BLACK_OFF 220u
typedef struct { uint32_t us; bool black; } barcode_edge_t;
typedef struct {
    uint32_t samples, edges, max_gap_us, last_us;
    uint32_t raw_white, raw_band, raw_black, white_dips, longest_black_us;
    uint32_t position, motion_breaks;
    uint16_t raw_min, raw_max, high_water;
    bool black, gap_fault, overflow;
} barcode_sample_stats_t;
typedef struct {
    barcode_edge_t edges[BARCODE_EDGE_CAPACITY];
    uint32_t positions[BARCODE_EDGE_CAPACITY];
    barcode_sample_stats_t stats;
    uint16_t count;
    uint16_t black_on, black_off;
    bool enabled, candidate;
    uint8_t candidate_samples;
    uint32_t candidate_us, black_since_us, candidate_position;
    barcode_motion_t motion;
} barcode_sampler_t;
typedef struct {
    barcode_edge_t edges[BARCODE_EDGE_CAPACITY];
    uint32_t positions[BARCODE_EDGE_CAPACITY];
    barcode_sample_stats_t stats;
    uint16_t count;
    bool pending_edge;
} barcode_batch_t;
typedef struct {
    barcode_reader_t reader;
    barcode_runs_t last_runs;
    barcode_sample_stats_t stats;
    uint32_t frames, rejected, timeouts;
    uint8_t saved_runs;
    char decoded;
    bool integrity_fault, halt_on_failure;
    bool continuous, waiting_gap, white_tracking, event_overflow;
    uint32_t white_since_us;
    struct { uint32_t us; char letter; } events[BARCODE_STREAM_EVENTS];
    unsigned event_count;
    uint32_t clock_origin;
    uint8_t clock_divisor;
    /* Centred course only: run widths use encoder position; quiet gaps and
     * event timestamps remain wall time. Stationary/legacy tests stay temporal. */
    bool distance_mode;
    bool independent_optical; /* no motion/phase gate; strict framed timestamp decode */
    uint32_t seen_motion_breaks, motion_aborts, pause_timeouts, zero_distance_aborts;
    uint32_t progress_position, progress_us;

} barcode_capture_t;
/* Sample in one IRQ. Reset/take/end only with that IRQ excluded; single core. */
void barcode_sampler_begin(barcode_sampler_t *s);
void barcode_sampler_begin_levels(barcode_sampler_t *s,uint16_t on,uint16_t off);
void barcode_sampler_sample(barcode_sampler_t *s, uint16_t raw, uint32_t now);
/* Call with IRQ excluded, immediately after begin_levels. Positions and edges
 * are then captured in the SAME sampling IRQ; foreground never guesses them. */
void barcode_sampler_enable_motion(barcode_sampler_t *s,uint32_t left_raw,uint32_t right_raw);
void barcode_sampler_sample_motion(barcode_sampler_t *s,uint16_t raw,uint32_t now,uint32_t left_raw,uint32_t right_raw);
void barcode_sampler_take(barcode_sampler_t *s, barcode_batch_t *out);
void barcode_capture_begin(barcode_capture_t *c);
void barcode_capture_begin_drive(barcode_capture_t *c);
void barcode_capture_begin_stream(barcode_capture_t *c,uint32_t origin);
/* Hand scans only: 8 us decoder ticks allow up to 6 s per run. */
void barcode_capture_begin_slow(barcode_capture_t *c, uint32_t origin);
/* Foreground only. Decode timestamps recorded by IRQ, not consumption times. */
void barcode_capture_consume(barcode_capture_t *c, const barcode_batch_t *batch);
#endif
