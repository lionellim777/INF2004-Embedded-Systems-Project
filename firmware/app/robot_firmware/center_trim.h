#ifndef CENTER_TRIM_H
#define CENTER_TRIM_H
#include <stdbool.h>
#include <stdint.h>
/* A centre reading has no left/right sign. Trial a hinted direction, retain
 * measurable improvement, reverse once on deterioration or stalled progress.
 * Absolute time/yaw budgets are never renewed by optical noise or progress. */
#define CENTER_TRIM_LIMIT_US 1500000u
#define CENTER_TRIM_LIMIT_COUNTS 240u
#define CENTER_TRIM_STALL_US 500000u
typedef struct {
    bool active,reversed,exhausted,no_response;
    int direction,last_good;
    int32_t entry_level,best_level,progress_level;
    int64_t entry_delta,previous_delta;
    uint64_t since,started,progress_since,total_travel;
} center_trim_t;
static inline void center_trim_clear(center_trim_t *t)
{int last=t->last_good;*t=(center_trim_t){.last_good=last};}
static inline float center_trim_step(center_trim_t *t,int32_t level,int32_t reference,
                                    bool right,int32_t l,int32_t r,uint64_t now)
{
    if(reference<400)reference=400;
    if(reference>1000)reference=1000;
    int32_t enter=reference*85/100,leave=reference*92/100;
    if(level>=leave){
        if(t->active && !t->exhausted)t->last_good=t->direction;
        center_trim_clear(t);return 0;
    }
    if(!t->active && level>=enter)return 0;
    int64_t delta=(int64_t)l-r;
    if(!t->active){
        t->active=true;t->direction=right?1:t->last_good?t->last_good:-1;
        t->entry_level=t->best_level=t->progress_level=level;
        t->entry_delta=t->previous_delta=delta;
        t->since=t->started=t->progress_since=now;
    }
    int64_t moved=delta-t->previous_delta;
    t->total_travel+=(uint64_t)(moved<0?-moved:moved);t->previous_delta=delta;
    if(t->exhausted || now<t->since || now-t->started>=CENTER_TRIM_LIMIT_US ||
       t->total_travel>=CENTER_TRIM_LIMIT_COUNTS){t->exhausted=true;return 0;}
    if(level>t->best_level)t->best_level=level;
    /* A real gain of20/1000 renews only the progress timer, never the limits. */
    if(level>=t->progress_level+20){t->progress_level=level;t->progress_since=now;}
    int64_t directed=(delta-t->entry_delta)*t->direction;
    int64_t travel=delta-t->entry_delta;if(travel<0)travel=-travel;
    /* Do not judge a requested left turn when encoders show a right turn.
     * Motor response must precede optical-direction judgement. */
    bool executed=directed>=20;
    if(!executed && now-t->since>=1000000u){t->no_response=true;t->exhausted=true;return 0;}
    bool worsening=executed && now-t->since>=80000u && level<t->best_level-40;
    bool improving=t->best_level>=t->entry_level+20 && now-t->progress_since<300000u;
    bool stalled=executed && (now-t->progress_since>=CENTER_TRIM_STALL_US || (travel>=120 && !improving));
    if(worsening || stalled){
        if(t->reversed){t->exhausted=true;return 0;}
        t->direction=-t->direction;t->reversed=true;
        t->entry_level=t->best_level=t->progress_level=level;
        t->entry_delta=delta;t->since=t->progress_since=now;
    }
    /* Taper differential speed near recovered tape; never a fixed full turn. */
    float amount=60.0f+160.0f*(float)(leave-level)/(float)(leave-200);
    if(amount>220)amount=220;
    if(amount<60)amount=60;
    return amount*t->direction;
}
#endif
