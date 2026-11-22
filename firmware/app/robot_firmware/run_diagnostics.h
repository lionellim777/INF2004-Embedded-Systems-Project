#ifndef RUN_DIAGNOSTICS_H
#define RUN_DIAGNOSTICS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct
{
    bool active;
    uint32_t samples;
    uint32_t sonar_valid;
    uint32_t sonar_no_echo;
    uint32_t pitch_samples;
    uint32_t last_pitch_sequence;
    uint16_t ir_min[3];
    uint16_t ir_max[3];
    uint32_t ir_edges[3];
    bool ir_previous[3];
    float sonar_min;
    float sonar_max;
    float pitch_min;
    float pitch_max;
    uint32_t start_humps;
    uint32_t start_errors;
} run_diagnostics_t;
void run_diagnostics_begin(run_diagnostics_t *d, uint32_t humps,
                           uint32_t errors, uint32_t pitch_sequence);
void run_diagnostics_sample(run_diagnostics_t *d, const uint16_t raw[3],
                            const bool black[3], bool sonar_fresh,
                            bool sonar_valid, float sonar_cm, bool pitch_valid,
                            uint32_t pitch_sequence, float pitch_deg);
#endif
