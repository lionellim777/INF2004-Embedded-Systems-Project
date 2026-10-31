#ifndef CENTER_BASELINE_H
#define CENTER_BASELINE_H
#include <stdbool.h>
#include <stdint.h>
/* Last 31 spaced observations while stopped. Median resists isolated spikes;
 * IQR rejects an unsettled placement. Never adapt to maxima while driving. */
#define CENTER_BASELINE_SAMPLES 31u
typedef struct {
    uint16_t samples[CENTER_BASELINE_SAMPLES];
    unsigned count,next;
    uint64_t last_us;
} center_baseline_t;
static inline void center_baseline_add(center_baseline_t *b,int32_t level,uint64_t now)
{
    if(b->count && now>=b->last_us && now-b->last_us<10000u)return;
    if(level<0)level=0;
    if(level>1000)level=1000;
    b->samples[b->next]=(uint16_t)level;b->next=(b->next+1)%CENTER_BASELINE_SAMPLES;
    if(b->count<CENTER_BASELINE_SAMPLES)b->count++;
    b->last_us=now;
}
static inline bool center_baseline_value(const center_baseline_t *b,uint64_t now,
                                         int32_t *median,int32_t *spread)
{
    *median=*spread=0;
    if(b->count<CENTER_BASELINE_SAMPLES || now<b->last_us || now-b->last_us>100000u)return false;
    uint16_t sorted[CENTER_BASELINE_SAMPLES];
    for(unsigned i=0;i<CENTER_BASELINE_SAMPLES;i++){
        unsigned j=i;uint16_t v=b->samples[i];
        while(j && sorted[j-1]>v){sorted[j]=sorted[j-1];j--;}
        sorted[j]=v;
    }
    *median=sorted[15];*spread=sorted[23]-sorted[7];
    return *median>=400 && *spread<=*median/5;
}
#endif
