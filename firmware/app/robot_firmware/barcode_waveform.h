#ifndef BARCODE_WAVEFORM_H
#define BARCODE_WAVEFORM_H
#include <stdbool.h>
#include <stdint.h>
#include "control_representation.h"
#include "barcode_decoder.h"

/* Foreground-only record-then-decide reader. Feed the 6 ms filtered GP26 ADC
 * at 2 ms intervals, retaining grey levels rather than fixed black/white.
 * Time-only tests use ADC12 + elapsed-ms4. Production also retains signed
 * GP26 displacement along its heading at capture start (mean-count units).
 * Frozen production exports interleave ADC/time word then position word.
 * No allocation or expected letter.
 * 8192 readings cover 16.384 seconds at the intended rate. */
#define BARCODE_WAVE_SAMPLES 8192u
#define BARCODE_WAVE_PERIOD_US 2000u
#define BARCODE_WAVE_GAP_US 3000u
#ifndef BARCODE_WAVE_LAST_PASS
#define BARCODE_WAVE_LAST_PASS 0
#endif
enum
{
    WAVE_NONE,
    WAVE_ACCEPTED,
    WAVE_TOO_FEW,
    WAVE_TOO_MANY,
    WAVE_RUN_SHORT,
    WAVE_RUN_LONG,
    WAVE_CODE39,
    WAVE_AMBIGUOUS,
    WAVE_FRAME_LIMIT,
    WAVE_BUFFER_FULL,
    WAVE_TIMING,
    WAVE_STOPPED_PARTIAL,
    WAVE_NO_FRAME,
    WAVE_MOTION
};
typedef struct
{
    uint16_t extrema;
    uint16_t windows;
    uint16_t best_window;
    uint8_t reason;
    uint8_t bad_run;
    uint32_t bad_width;
    barcode_decode_diagnostic_t code39;
    uint32_t runs[29];
} barcode_wave_profile_t;
typedef struct
{
    uint16_t sample[2u * BARCODE_WAVE_SAMPLES];
    uint16_t count;
    uint16_t paper;
    bool motion;
    uint8_t coherent_mask;
    uint8_t geometry_mask;
    uint16_t signal_range;
    uint32_t start_us;
    uint32_t leading_us;
    uint32_t white_us;
    uint32_t duration_us;
    uint32_t checksum;
    uint8_t reason;
    char letter;
    barcode_wave_profile_t profile[6];
} barcode_wave_snapshot_t;
typedef struct
{
    uint32_t start_us;
    uint32_t duration_us;
    uint16_t samples;
    uint16_t raw_min;
    uint16_t raw_max;
    uint16_t lateral_mm;
    uint16_t yaw_deg;
    uint8_t reason;
    uint8_t coherent_mask;
    uint8_t geometry_mask;
    char letter;
    barcode_wave_profile_t best;
} barcode_wave_attempt_t;
#define BARCODE_WAVE_ATTEMPTS 32u
typedef struct
{
    uint16_t sample[BARCODE_WAVE_SAMPLES];
    int16_t position[BARCODE_WAVE_SAMPLES];
#if BARCODE_WAVE_LAST_PASS
    /* Per-instance decode scratch: latest forward crossing at each coordinate.
     * Raw ADC/position words remain unchanged for frozen diagnostics. */
    uint8_t latest_mask[(BARCODE_WAVE_SAMPLES + 7u) / 8u];
#endif
    uint32_t edge_work[95];
    uint32_t fit_work[29];
    uint32_t geometry_work[29];
    bool motion ROBOCAR_BIT1;
    bool motion_seen ROBOCAR_BIT1;
    bool motion_fault ROBOCAR_BIT1;
    bool odometry_fault ROBOCAR_BIT1;
    bool recrossing ROBOCAR_BIT1;
    uint32_t left_raw;
    uint32_t right_raw;
    uint32_t motion_faults;
    uint32_t recrosses;
    uint32_t stationary_samples;
    uint32_t white_counts;
    uint32_t lateral_recrosses;
    uint8_t frame_reason;
    bool partial_saved;
    float start_yaw;
    float pose_x;
    float pose_y;
    float yaw;
    float sensor_x;
    float sensor_y;
    float axis_cos;
    float axis_sin;
    float origin_x;
    float origin_y;
    float frontier_y;
    float max_lateral;
    float max_yaw;
    int32_t progress;
    int32_t frontier;
    int32_t white_frontier;
    barcode_wave_attempt_t attempts[BARCODE_WAVE_ATTEMPTS];
    unsigned attempt_count;
    unsigned attempt_overflow;
    uint16_t count;
    uint16_t previous;
    uint16_t paper;
    uint32_t last_us;
    uint32_t residual_us;
    uint32_t white_us;
    uint32_t leading_us;
    uint32_t frame_us;
    uint32_t frames;
    uint32_t rejected;
    uint32_t overflows;
    uint32_t timing_faults;
    uint32_t ambiguous;
    uint32_t last_runs[BARCODE_CODE39_RUN_COUNT];
    uint16_t last_prominence;
    uint16_t last_extrema;
    uint16_t profile_extrema[6];
    uint8_t complete_profiles;
    uint8_t coherent_mask;
    uint8_t geometry_mask;
    uint32_t capture_start_us;
    barcode_wave_profile_t profile[6];
    barcode_wave_snapshot_t saved;
    bool seen ROBOCAR_BIT1;
    bool armed ROBOCAR_BIT1;
    bool recording ROBOCAR_BIT1;
    bool attempted ROBOCAR_BIT1;
    bool last_conflict ROBOCAR_BIT1;
} barcode_waveform_t;
void barcode_waveform_begin(barcode_waveform_t *w);
bool barcode_waveform_update(barcode_waveform_t *w, uint16_t raw,
                             uint32_t now_us, char *letter);
bool barcode_waveform_update_motion(barcode_waveform_t *w, uint16_t raw,
                                    uint32_t now_us, uint32_t left_raw,
                                    uint32_t right_raw, char *letter);
/* Independent batch interface for captured waveform/reconstruction checks. */
bool barcode_waveform_decode(barcode_waveform_t *w, char *letter);
void barcode_waveform_save_partial(barcode_waveform_t *w);
const char *barcode_waveform_reason(unsigned reason);
unsigned barcode_waveform_details(const barcode_wave_snapshot_t *s, char *out,
                                  unsigned capacity);
#endif
