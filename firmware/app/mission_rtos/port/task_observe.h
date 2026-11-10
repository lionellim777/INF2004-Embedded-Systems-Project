#ifndef RTOS_TASK_OBSERVE_H
#define RTOS_TASK_OBSERVE_H
#include "runtime_measure.h"
bool rtos_observed_start(unsigned id,void (*entry)(int,void *),int priority,int requested);
/* Observer task only. No lock during the scan; watermarks are observations,
 * not a guaranteed worst-case bound. Markers are never repainted at runtime. */
bool rtos_observed_poll(void);
void rtos_observed_stack(unsigned id,runtime_stack_t *out);
#endif
