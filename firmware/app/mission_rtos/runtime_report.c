#include "runtime_report.h"
#include "port/task_observe.h"
#include "board.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
static uint32_t scans,monitor_max_us;
void runtime_report_poll(void)
{
    uint32_t start=time_us_32();bool intact=rtos_observed_poll();uint32_t elapsed=time_us_32()-start;
    scans++;if(elapsed>monitor_max_us)monitor_max_us=elapsed;
    if(!intact)board_stop(STOP_INIT);
}
size_t runtime_report_packet(unsigned slot,uint32_t ms,uint8_t out[RUNTIME_PACKET_BYTES])
{
    if(slot>RUNTIME_TASKS)return 0;
    uint16_t run;bool active;runtime_task_t stats;runtime_stack_t stack;board_runtime_trace_t trace;
    uint32_t irq=save_and_disable_interrupts();runtime_measure_context(&run,&active);
    if(slot){runtime_measure_task(slot-1u,&stats);rtos_observed_stack(slot-1u,&stack);}
    else board_runtime_trace(&trace);
    runtime_summary_t summary={.ms=ms,.run=run,.armed=active,.stack_scans=scans,.monitor_work_us=monitor_max_us};
    restore_interrupts(irq);
    if(slot)return runtime_task_encode(ms,run,slot-1u,&stack,&stats,out);
    summary.stop_sequence=trace.sequence;summary.requested_us=trace.requested_us;summary.disabled_us=trace.disabled_us;
    summary.guard_gap_us=trace.guard_gap_us;summary.service_gap_us=trace.service_gap_us;
    summary.source=trace.source;summary.reason=trace.reason;summary.armed=trace.armed;
    return runtime_summary_encode(&summary,out);
}
