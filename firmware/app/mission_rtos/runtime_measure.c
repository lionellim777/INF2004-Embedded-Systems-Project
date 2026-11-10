#include "runtime_measure.h"
#include "hardware/sync.h"
#include <string.h>
typedef struct {
    runtime_task_t stats;uint32_t last_us;uint16_t last_run;uint8_t flags;
} task_state_t;
static task_state_t task[RUNTIME_TASKS];
static uint16_t current_run;
static bool moving;
void runtime_measure_init(void)
{memset(task,0,sizeof task);current_run=0;moving=false;}
void runtime_measure_run(uint16_t run,bool active)
{
    uint32_t irq=save_and_disable_interrupts();
    if(run!=current_run){
        for(unsigned i=0;i<RUNTIME_TASKS;i++)task[i].stats.active_gap_us=task[i].stats.active_work_us=0;
        current_run=run;
    }
    moving=active;restore_interrupts(irq);
}
runtime_span_t runtime_measure_begin(unsigned index,uint32_t now)
{
    uint32_t irq=save_and_disable_interrupts();runtime_span_t span={now,current_run,moving};
    if(index<RUNTIME_TASKS){
        task_state_t *t=&task[index];
        if(t->flags&1u){
            uint32_t gap=now-t->last_us;
            if(gap>t->stats.gap_us)t->stats.gap_us=gap;
            if(moving && (t->flags&2u) && t->last_run==current_run && gap>t->stats.active_gap_us)t->stats.active_gap_us=gap;
        }
        t->last_us=now;t->last_run=current_run;t->flags=(uint8_t)(1u|(moving?2u:0u));t->stats.iterations++;
    }
    restore_interrupts(irq);return span;
}
void runtime_measure_end(unsigned index,runtime_span_t span,uint32_t now)
{
    uint32_t irq=save_and_disable_interrupts();
    if(index<RUNTIME_TASKS){
        runtime_task_t *t=&task[index].stats;uint32_t elapsed=now-span.us;
        if(elapsed>t->work_us)t->work_us=elapsed;
        if(span.active && span.run==current_run && elapsed>t->active_work_us)t->active_work_us=elapsed;
    }
    restore_interrupts(irq);
}
void runtime_measure_task(unsigned index,runtime_task_t *out)
{
    uint32_t irq=save_and_disable_interrupts();
    *out=index<RUNTIME_TASKS?task[index].stats:(runtime_task_t){0};restore_interrupts(irq);
}
void runtime_measure_context(uint16_t *run,bool *active)
{
    uint32_t irq=save_and_disable_interrupts();*run=current_run;*active=moving;restore_interrupts(irq);
}
typedef struct {uint8_t *out;unsigned bit;bool valid;} writer_t;
static void put(writer_t *w,uint32_t value,unsigned bits)
{
    if((bits<32 && value>=(UINT32_C(1)<<bits)) || w->bit+bits>8u*RUNTIME_PACKET_BYTES){w->valid=false;return;}
    for(unsigned i=0;i<bits;i++,w->bit++)if(value&(UINT32_C(1)<<i))w->out[w->bit/8u]|=(uint8_t)(1u<<(w->bit%8u));
}
static writer_t header(uint8_t *out,unsigned kind,uint16_t run,uint32_t ms)
{memset(out,0,RUNTIME_PACKET_BYTES);writer_t w={out,0,true};put(&w,kind,8);put(&w,run,16);put(&w,ms,32);return w;}
size_t runtime_task_encode(uint32_t ms,uint16_t run,unsigned id,const runtime_stack_t *s,const runtime_task_t *t,uint8_t out[RUNTIME_PACKET_BYTES])
{
    if(id>=RUNTIME_TASKS || s->flags>7 || s->untouched>s->allocated ||
       ((s->flags&1u) && (!s->priority || s->priority>16 || s->requested<128 || s->requested>s->allocated)) ||
       (!(s->flags&1u) && (s->priority || s->requested || s->allocated || s->untouched || s->flags)))return 0;
    writer_t w=header(out,1,run,ms);put(&w,id,2);put(&w,s->priority,5);put(&w,(s->flags&1u)!=0,1);
    put(&w,s->requested,14);put(&w,s->allocated,14);put(&w,s->untouched,14);put(&w,(s->flags&2u)!=0,1);put(&w,(s->flags&4u)!=0,1);put(&w,0,4);
    put(&w,t->iterations,32);put(&w,t->gap_us,32);put(&w,t->work_us,32);put(&w,t->active_gap_us,32);put(&w,t->active_work_us,32);
    return w.valid?(w.bit+7u)/8u:0;
}
size_t runtime_summary_encode(const runtime_summary_t *s,uint8_t out[RUNTIME_PACKET_BYTES])
{
    if(s->source>3 || s->reason>6)return 0;
    writer_t w=header(out,0,s->run,s->ms);put(&w,s->armed,1);put(&w,s->source,2);put(&w,s->reason,3);put(&w,0,2);
    put(&w,s->stop_sequence,32);put(&w,s->requested_us,32);put(&w,s->disabled_us,32);
    put(&w,s->guard_gap_us,32);put(&w,s->service_gap_us,32);put(&w,s->monitor_work_us,32);put(&w,s->stack_scans,32);
    return w.valid?(w.bit+7u)/8u:0;
}
