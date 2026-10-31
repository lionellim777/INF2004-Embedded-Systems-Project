#ifndef INF2004_MOTION_CONTROL_H
#define INF2004_MOTION_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "encoders.h"

typedef enum {
    MOTION_NONE, MOTION_STRAIGHT_30CM, MOTION_LEFT_90,
    MOTION_RIGHT_90, MOTION_RIGHT_180, MOTION_BARCODE, MOTION_CONTINUOUS, MOTION_COURSE
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
    int32_t target;
    int32_t position;
    int32_t previous;
    int32_t cutoff_position;
    uint32_t cutoff_elapsed_us;
    uint64_t last_progress_us;
    bool finished;
} motion_wheel_t;

/* Intermediate integration format. Native working values retained until the
 * proposed compact motion representation has its own error/range validation. */
typedef struct {
    encoder_sample_t start;
    motion_wheel_t left;
    motion_wheel_t right;
    uint64_t start_us;
    uint64_t control_us;
    int32_t peak_skew;
    motion_kind_t kind;
    motion_phase_t phase;
    motion_fault_t fault;
} motion_control_t;

void motion_init(motion_control_t *motion);
bool motion_begin(motion_control_t *motion, motion_kind_t kind,
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
