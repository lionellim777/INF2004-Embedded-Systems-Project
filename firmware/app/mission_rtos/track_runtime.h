#ifndef RTOS_TRACK_RUNTIME_H
#define RTOS_TRACK_RUNTIME_H
#include "course_control.h"
#include "center_baseline.h"
typedef struct {
    course_control_t course;
    center_baseline_t baseline;
    uint64_t requested_us;
    uint16_t token;
    uint8_t state; /* waiting0,ready1; Motion task owns state */
    uint16_t reads;
    char letter;
    const char *status;
#ifdef RTOS_OBSTACLE
    bool obstacle_enabled;
#endif
} track_runtime_t;
void track_runtime_idle(track_runtime_t *t,uint64_t now);
void track_runtime_observe(uint64_t now);
bool track_runtime_begin(track_runtime_t *t,uint16_t token,encoder_sample_t enc,uint64_t now);
#ifdef RTOS_OBSTACLE
bool track_runtime_begin_obstacle(track_runtime_t *t,uint16_t token,encoder_sample_t enc,uint64_t now);
#endif
line_check_output_t track_runtime_step(track_runtime_t *t,encoder_sample_t enc,uint64_t now);
void track_runtime_end(void);
#endif
