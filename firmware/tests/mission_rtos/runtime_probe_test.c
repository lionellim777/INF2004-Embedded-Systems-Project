#include "runtime_report.h"
#include "board.h"
#include "runtime_test_kernel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "port/task_observe.c"

static uint8_t storage[4][8448];static int sizes[4];
static unsigned irq_depth,created,started,deleted,stops;
static int create_result,ref_result,start_result;
static uint32_t clock_us;
uint32_t save_and_disable_interrupts(void){return irq_depth++;}
void restore_interrupts(uint32_t n){assert(irq_depth>0);irq_depth=n;}
uint32_t time_us_32(void){return clock_us;}
ID tk_cre_tsk(const T_CTSK *c)
{
    assert(c->tskatr==(TA_HLNG|TA_RNG3) && c->bufptr==NULL && c->exinf==NULL);
    if(create_result)return create_result;
    assert(created<4);sizes[created]=c->stksz+128;memset(storage[created],0xcc,sizeof storage[created]);return (int)++created;
}
ER td_ref_tsk(ID id,TD_RTSK *p)
{
    if(ref_result)return ref_result;
    assert(id>0 && id<=4);*p=(TD_RTSK){TTS_DMT,sizes[id-1],storage[id-1]+sizes[id-1]};return E_OK;
}
ER tk_sta_tsk(ID id,int code)
{
    assert(code==0);unsigned n=(unsigned)(id-1),painted=(unsigned)sizes[n]-128;
    for(unsigned i=0;i<painted;i++)assert(storage[n][i]==0xa5);
    for(unsigned i=painted;i<(unsigned)sizes[n];i++)assert(storage[n][i]==0xcc);
    started++;return start_result;
}
ER tk_del_tsk(ID id){assert(id>0);deleted++;return E_OK;}
void board_stop(uint8_t why){assert(why==STOP_INIT);stops++;}
void board_runtime_trace(board_runtime_trace_t *out)
{*out=(board_runtime_trace_t){.sequence=7,.requested_us=UINT32_MAX-5u,.disabled_us=4,.guard_gap_us=5001,.service_gap_us=10002,.reason=STOP_BUTTON,.source=STOP_SOURCE_API};}
static void entry(int n,void *p){(void)n;(void)p;}
static void reset_observer(void)
{memset(observed,0,sizeof observed);created=started=deleted=stops=0;create_result=ref_result=start_result=0;}
static void emit(size_t n,const uint8_t *b)
{assert(n);printf("RUNTIME_FIXTURE ");for(size_t i=0;i<n;i++)printf("%02x",b[i]);puts("");}
static void stacks(void)
{
    reset_observer();create_result=-1;assert(!rtos_observed_start(0,entry,2,4096) && !started);
    reset_observer();ref_result=-1;assert(!rtos_observed_start(0,entry,2,4096) && deleted==1 && !started);
    reset_observer();start_result=-1;assert(!rtos_observed_start(0,entry,2,4096) && deleted==1);
    reset_observer();assert(!rtos_observed_start(0,entry,2,2147483647));assert(!rtos_observed_start(4,entry,2,4096));
    assert(!rtos_observed_start(0,entry,2,129));assert(!created);
    assert(rtos_observed_start(0,entry,2,4096));assert(rtos_observed_start(1,entry,4,8192));
    assert(rtos_observed_start(2,entry,8,4096));assert(rtos_observed_start(3,entry,9,4096));
    assert(!rtos_observed_start(0,entry,2,4096));assert(started==4);
    runtime_stack_t s;rtos_observed_stack(0,&s);assert(s.allocated==4224 && s.requested==4096 && s.untouched==4096 && !(s.flags&4u));
    storage[0][3896]=0;assert(rtos_observed_poll());rtos_observed_stack(0,&s);
    assert(s.untouched==3896 && s.flags==7 && s.allocated-s.untouched==328);
    storage[0][3896]=0xa5;assert(rtos_observed_poll());rtos_observed_stack(0,&s);assert(s.untouched==3896);
    storage[2][0]=0;runtime_report_poll();assert(stops==1);rtos_observed_stack(2,&s);assert(s.untouched==0 && !(s.flags&2u));
    puts("PASS stack observer: same kernel allocations, dormant context preserved, pre-start paint, monotonic watermark, guard failure and create/ref/start cleanup");
}
static void timing(void)
{
    runtime_measure_init();runtime_span_t a=runtime_measure_begin(0,100);runtime_measure_end(0,a,120);
    a=runtime_measure_begin(0,2000100);runtime_measure_end(0,a,2000120);
    runtime_measure_run(1,true);a=runtime_measure_begin(0,2000200);runtime_measure_end(0,a,2000230);
    a=runtime_measure_begin(0,2005200);runtime_measure_end(0,a,2005250);
    runtime_task_t s;runtime_measure_task(0,&s);assert(s.gap_us==2000000 && s.active_gap_us==5000 && s.active_work_us==50);
    runtime_measure_run(1,false);a=runtime_measure_begin(0,4005200);runtime_measure_end(0,a,4005300);
    runtime_measure_task(0,&s);assert(s.active_gap_us==5000 && s.active_work_us==50 && s.work_us==100);
    runtime_measure_run(2,true);a=runtime_measure_begin(0,4005400);runtime_measure_end(0,a,4005410);
    runtime_measure_task(0,&s);assert(!s.active_gap_us && s.active_work_us==10);
    runtime_span_t old=a;runtime_measure_run(3,true);runtime_measure_end(0,old,4505400);
    runtime_measure_task(0,&s);assert(!s.active_work_us);
    runtime_measure_init();runtime_measure_run(4,true);
    a=runtime_measure_begin(0,UINT32_MAX-10u);runtime_measure_end(0,a,4);
    a=runtime_measure_begin(0,9);runtime_measure_end(0,a,19);
    runtime_measure_task(0,&s);assert(s.active_gap_us==20 && s.active_work_us==15);
    puts("PASS observed timing: lifetime vs armed scopes, fresh-run reset, old-span isolation, uint32 clock wrap and preemption-inclusive elapsed time");
}
static void codecs(void)
{
    uint8_t b[RUNTIME_PACKET_BYTES];runtime_task_t t={.iterations=UINT32_MAX,.gap_us=2000000,.work_us=75000,.active_gap_us=5000,.active_work_us=70000};
    runtime_stack_t s={4096,4224,3896,2,7};size_t n=runtime_task_encode(UINT32_MAX,65535,0,&s,&t,b);assert(n==34);emit(n,b);
    s.allocated=16384;assert(!runtime_task_encode(0,0,0,&s,&t,b));s.allocated=4224;s.untouched=4225;assert(!runtime_task_encode(0,0,0,&s,&t,b));
    runtime_summary_t summary={.ms=123,.run=4,.stop_sequence=7,.requested_us=UINT32_MAX-5u,.disabled_us=4,.guard_gap_us=5001,.service_gap_us=10002,.source=1,.reason=1,.stack_scans=1};
    n=runtime_summary_encode(&summary,b);assert(n==36);emit(n,b);
    summary.reason=7;assert(!runtime_summary_encode(&summary,b));
    n=runtime_report_packet(0,100,b);assert(n==36);emit(n,b);
    n=runtime_report_packet(1,100,b);assert(n==34);emit(n,b);
    assert(!runtime_report_packet(5,100,b));assert(!irq_depth);
    puts("PASS compact runtime records: bounded stack fields, stop-source timestamps and rejection, consistent report snapshots");
}
int main(void){stacks();timing();codecs();return 0;}
