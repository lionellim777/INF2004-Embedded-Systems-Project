#ifndef RUN_DIAGNOSTICS_H
#define RUN_DIAGNOSTICS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool active;
    uint32_t samples, sonar_valid, sonar_no_echo, pitch_samples, last_pitch_sequence;
    uint16_t ir_min[3], ir_max[3];
    uint32_t ir_edges[3];
    bool ir_previous[3];
    float sonar_min, sonar_max, pitch_min, pitch_max;
    uint32_t start_humps, start_errors;
} run_diagnostics_t;
void run_diagnostics_begin(run_diagnostics_t *d,uint32_t humps,uint32_t errors,uint32_t pitch_sequence);
void run_diagnostics_sample(run_diagnostics_t *d,const uint16_t raw[3],const bool black[3],
 bool sonar_fresh,bool sonar_valid,float sonar_cm,bool pitch_valid,uint32_t pitch_sequence,float pitch_deg);
#endif
