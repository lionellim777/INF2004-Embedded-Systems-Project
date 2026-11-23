#include "task_observe.h"
#include <tk/tkernel.h>
#include <tk/dbgspt.h>
#include <string.h>
#include "hardware/sync.h"
typedef struct {const volatile uint8_t *base;runtime_stack_t record;} observed_t;
static observed_t observed[RUNTIME_TASKS];
bool rtos_observed_start(unsigned index,void (*entry)(int,void *),int priority,int requested)
{
    if(index>=RUNTIME_TASKS || observed[index].record.flags || !entry || priority<1 || priority>16 ||
       requested<MIN_SYS_STACK_SIZE || requested>=16384-DEFAULT_SYS_STKSZ || (requested&7))return false;
    T_CTSK config={0};config.task=entry;config.itskpri=priority;config.stksz=requested;config.tskatr=TA_HLNG|TA_RNG3;
    ID id=tk_cre_tsk(&config);if(id<=E_OK)return false;
    TD_RTSK info={0};
    if(td_ref_tsk(id,&info)!=E_OK || info.tskstat!=TTS_DMT || !info.istack ||
       info.stksz!=requested+DEFAULT_SYS_STKSZ){(void)tk_del_tsk(id);return false;}
    uint8_t *base=(uint8_t *)info.istack-info.stksz;
    /* The pinned ARMv6-M port reserves MIN_SYS_STACK_SIZE >= SStackFrame.
     * Leave that entire top region untouched so its dormant CPU context is
     * preserved. Count it conservatively as used from the first observation. */
    unsigned painted=(unsigned)info.stksz-MIN_SYS_STACK_SIZE;memset(base,0xa5,painted);
    observed[index]=(observed_t){base,{(uint16_t)requested,(uint16_t)info.stksz,(uint16_t)painted,(uint8_t)priority,3}};
    if(tk_sta_tsk(id,0)!=E_OK){observed[index]=(observed_t){0};(void)tk_del_tsk(id);return false;}
    return true;
}
bool rtos_observed_poll(void)
{
    bool guards=true;
    for(unsigned i=0;i<RUNTIME_TASKS;i++){
        observed_t *o=&observed[i];if(!(o->record.flags&RUNTIME_STACK_VALID))continue;
        unsigned n=0;while(n<o->record.untouched && o->base[n]==0xa5)n++;
        bool intact=true;for(unsigned j=0;j<16;j++)if(o->base[j]!=0xa5)intact=false;
        uint32_t irq=save_and_disable_interrupts();o->record.untouched=(uint16_t)n;
        o->record.flags|=RUNTIME_STACK_OBSERVED;if(!intact)o->record.flags&=(uint8_t)~RUNTIME_STACK_GUARD;
        if(!(o->record.flags&RUNTIME_STACK_GUARD))guards=false;
        restore_interrupts(irq);
    }
    return guards;
}
void rtos_observed_stack(unsigned index,runtime_stack_t *out)
{uint32_t irq=save_and_disable_interrupts();*out=index<RUNTIME_TASKS?observed[index].record:(runtime_stack_t){0};restore_interrupts(irq);}
