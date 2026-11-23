#ifndef RTOS_RUNTIME_MEASURE_H
#define RTOS_RUNTIME_MEASURE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { RUNTIME_MOTION,RUNTIME_DECODER,RUNTIME_NETWORK,RUNTIME_OBSERVER,RUNTIME_TASKS };
#define RUNTIME_PACKET_BYTES 40u
typedef struct { uint32_t us;uint16_t run;bool active; } runtime_span_t;
typedef struct {
    uint32_t iterations,gap_us,work_us,active_gap_us,active_work_us;
} runtime_task_t;
typedef struct { uint16_t requested,allocated,untouched;uint8_t priority,flags; } runtime_stack_t;
enum { RUNTIME_STACK_VALID=1u,RUNTIME_STACK_GUARD=2u,RUNTIME_STACK_OBSERVED=4u };
typedef struct {
    uint32_t ms,stop_sequence,requested_us,disabled_us,guard_gap_us,service_gap_us,monitor_work_us,stack_scans;
    uint16_t run;
    uint8_t source,reason;
    bool armed;
} runtime_summary_t;
void runtime_measure_init(void);
void runtime_measure_run(uint16_t run,bool active);
runtime_span_t runtime_measure_begin(unsigned task,uint32_t now);
void runtime_measure_end(unsigned task,runtime_span_t span,uint32_t now);
void runtime_measure_task(unsigned task,runtime_task_t *out);
void runtime_measure_context(uint16_t *run,bool *active);
size_t runtime_task_encode(uint32_t ms,uint16_t run,unsigned task,const runtime_stack_t *stack,
                           const runtime_task_t *stats,uint8_t out[RUNTIME_PACKET_BYTES]);
size_t runtime_summary_encode(const runtime_summary_t *s,uint8_t out[RUNTIME_PACKET_BYTES]);
#endif
