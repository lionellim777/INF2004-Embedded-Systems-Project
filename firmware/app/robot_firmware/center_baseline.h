#ifndef CENTER_BASELINE_H
#define CENTER_BASELINE_H
#include <stdbool.h>
#include <stdint.h>
/* Last 31 spaced observations while stopped. Median resists isolated spikes;
 * IQR rejects an unsettled placement. Never adapt to maxima while driving. */
#define CENTER_BASELINE_SAMPLES 31u
#ifndef CENTER_BASELINE_COMPACT
#define CENTER_BASELINE_COMPACT 0
#endif
#if CENTER_BASELINE_COMPACT
/* Motion-owned while stopped:31 clamped10-bit levels, count5, next5 =320
 * bits/40 bytes, followed by the unchanged64-bit capture time. No task/IRQ
 * shares this storage. The full timer domain is retained. */
typedef struct { uint8_t packed[40];uint64_t last_us; } center_baseline_t;
_Static_assert(sizeof(center_baseline_t)==48,"Centre baseline layout changed");
static inline uint16_t center_level_get(const uint8_t *p,uint8_t slot)
{
    uint16_t bit=(uint16_t)(slot*10u);uint8_t at=(uint8_t)(bit/8u);
    uint16_t word=(uint16_t)(p[at]|((uint16_t)p[at+1u]<<8u));
    return (uint16_t)((word>>(bit%8u))&1023u);
}
static inline void center_level_set(uint8_t *p,uint8_t slot,uint16_t level)
{
    uint16_t bit=(uint16_t)(slot*10u);uint8_t at=(uint8_t)(bit/8u),shift=(uint8_t)(bit%8u);
    uint16_t word=(uint16_t)(p[at]|((uint16_t)p[at+1u]<<8u));
    word=(uint16_t)((word&~(1023u<<shift))|(level<<shift));
    p[at]=(uint8_t)word;p[at+1u]=(uint8_t)(word>>8u);
}
static inline void center_baseline_add(center_baseline_t *b,int32_t level,uint64_t now)
{
    uint8_t count=(uint8_t)((b->packed[38]>>6u)|((b->packed[39]&7u)<<2u));
    if(count && now>=b->last_us && now-b->last_us<10000u)return;
    if(level<0)level=0;
    if(level>1000)level=1000;
    uint8_t next=b->packed[39]>>3u;
    center_level_set(b->packed,next,(uint16_t)level);
    next=(uint8_t)(next==30u?0u:next+1u);if(count<31u)count++;
    b->packed[38]=(uint8_t)((b->packed[38]&63u)|((count&3u)<<6u));
    b->packed[39]=(uint8_t)((count>>2u)|(next<<3u));b->last_us=now;
}
static inline bool center_baseline_value(const center_baseline_t *b,uint64_t now,
                                         int32_t *median,int32_t *spread)
{
    *median=*spread=0;
    uint8_t count=(uint8_t)((b->packed[38]>>6u)|((b->packed[39]&7u)<<2u));
    if(count<31u || now<b->last_us || now-b->last_us>100000u)return false;
    uint8_t sorted[39]={0}; /*31*10 bits, two final padding bits. */
    for(uint8_t i=0;i<31u;i++){
        uint8_t j=i;uint16_t value=center_level_get(b->packed,i);
        while(j && center_level_get(sorted,(uint8_t)(j-1u))>value){
            center_level_set(sorted,j,center_level_get(sorted,(uint8_t)(j-1u)));j--;
        }
        center_level_set(sorted,j,value);
    }
    *median=center_level_get(sorted,15);*spread=center_level_get(sorted,23)-center_level_get(sorted,7);
    return *median>=400 && *spread<=*median/5;
}
#else
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
#endif
