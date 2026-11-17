#ifndef INF2004_MOTION_CONTROL_H
#define INF2004_MOTION_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "encoders.h"
#include "control_representation.h"

typedef enum {
    MOTION_NONE, MOTION_STRAIGHT_30CM, MOTION_LEFT_90,
    MOTION_RIGHT_90, MOTION_RIGHT_180, MOTION_BARCODE, MOTION_CONTINUOUS, MOTION_COURSE, MOTION_AVOID
} motion_kind_t;

typedef enum {
    MOTION_IDLE, MOTION_ACTIVE, MOTION_DONE, MOTION_ABORTED, MOTION_FAULT
} motion_phase_t;

typedef enum {
    MOTION_OK, MOTION_ENCODER_INVALID, MOTION_WRONG_DIRECTION,
    MOTION_STALLED, MOTION_LATE_POLL, MOTION_TIMED_OUT
} motion_fault_t;

typedef struct {
    float integral;
    float throttle;
    float cps;
    float applied_pwm; /* Last controller output, retained after fault/abort. */
    float requested_cps;
    uint32_t no_progress_ms;
#if ROBOCAR_COMPACT_CONTROL_STATE
    bool held:1,finished:1;
#else
    bool held; /* Deliberately held to let the other wheel catch up. */
#endif
    int32_t target;
    int32_t position;
    int32_t previous;
    int32_t cutoff_position;
    uint32_t cutoff_elapsed_us;
    uint64_t last_progress_us;
#if !ROBOCAR_COMPACT_CONTROL_STATE
    bool finished;
#endif
} motion_wheel_t;

/* Native numerical working values; opt-in packing changes only trusted state
 * and flags, never the precision of a motor-control calculation. */
typedef struct {
    encoder_sample_t start;
    motion_wheel_t left;
    motion_wheel_t right;
    uint64_t start_us;
    uint64_t control_us;
    int32_t peak_skew;
#if ROBOCAR_COMPACT_CONTROL_STATE
    /* kind0..8:4, phase0..4:3, fault0..6 incl RTOS track:3,
     * two stalled-wheel flags:2, four profile flags:4 =16 bits. */
    unsigned short kind:4,phase:3,fault:3,stalled_wheels:2;
    bool slow_distance:1,transit_drive:1,alignment_drive:1,slow_turn:1;
#else
    motion_kind_t kind;
    motion_phase_t phase;
    motion_fault_t fault;
    uint8_t stalled_wheels; /* bit0 left, bit1 right; captured before zeroing outputs. */
    bool slow_distance;
    bool transit_drive; /* Optional clear bypass leg: 800 cps, slowing near endpoint. */
    bool alignment_drive; /* Probe/axle advance: adaptive low-speed effort, max32%. */
    bool slow_turn; /* Bypass only; standalone calibrated turns keep their profile. */
#endif
} motion_control_t;
#if ROBOCAR_COMPACT_CONTROL_STATE
_Static_assert(MOTION_AVOID==8 && MOTION_FAULT==4 && MOTION_TIMED_OUT==5,
               "Review compact state widths when motion domains change");
#endif

void motion_init(motion_control_t *motion);
bool motion_begin(motion_control_t *motion, motion_kind_t kind,
                  encoder_sample_t sample, uint64_t now_us);
/* Nominal wheel geometry, 1..700 mm. Separate from the calibrated 30 cm test.
 * Slow bounded straight legs for bypass; physical distance still needs checking. */
/* Isolated invalid quadrature transitions (a missed edge) tolerated per wheel per run. A missed
 * edge costs about 2 counts (0.2 mm over 300 mm), so a single one must not abort a move. More than
 * this still faults. Stall, wrong-direction, skew and timeout checks are unchanged. */
#define MOTION_ENCODER_INVALID_TOLERANCE 2u
/* Provisional cutoff for the 50 cm hump-test move: scaled from the 30 cm cutoff (3537 counts
 * settles at about 3640 counts = 300 mm, i.e. 12.13 final counts/mm, with about 103 counts of
 * residual rolling): 500 mm -> about 6067 final counts, cutoff 5964. Validate on the floor. */
#define MOTION_STRAIGHT_50CM_STOP_COUNTS 5964
bool motion_begin_distance(motion_control_t *motion, unsigned mm,
                           encoder_sample_t sample, uint64_t now_us);
/* No PWM, GPIO, printing, waiting or counter resets. Main applies outputs and
 * may override them with STOP at any time. Call frequently from the main loop. */
void motion_poll(motion_control_t *motion, encoder_sample_t sample, uint64_t now_us);
void motion_abort(motion_control_t *motion);
int32_t motion_left_position(const motion_control_t *motion, encoder_sample_t sample);
int32_t motion_right_position(const motion_control_t *motion, encoder_sample_t sample);
const char *motion_name(motion_kind_t kind);
const char *motion_fault_name(motion_fault_t fault);

#endif
