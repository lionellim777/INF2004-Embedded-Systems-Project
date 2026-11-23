#ifndef MISSION_ADC12_FILTER_H
#define MISSION_ADC12_FILTER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Twelve ADC12 readings:144 bits. Sum0..49140:16 bits. Next0..11 and
 * count0..12:4 bits each. Exactly168 bits/21 bytes, no padding.
 * The ADC IRQ is the sole owner after zero-initialization before enabling it;
 * adjacent samples share nibbles, so do not add a concurrent writer. */
typedef struct { uint8_t history[18],sum[2],state; } adc12_filter_t;
_Static_assert(sizeof(adc12_filter_t)==21,"ADC filter representation changed");

static inline uint16_t adc12_filter_sample(const adc12_filter_t *f,uint8_t slot)
{
    uint8_t at=(uint8_t)(slot*3u/2u);
    uint16_t value=(uint16_t)(f->history[at]|((uint16_t)f->history[at+1u]<<8u));
    return (slot&1u)?(uint16_t)(value>>4u):(uint16_t)(value&4095u);
}
static inline bool adc12_filter_update(adc12_filter_t *f,uint16_t raw,uint16_t *mean)
{
    if(!f || !mean || raw>4095u)return false;
    uint8_t next=f->state&15u,count=f->state>>4u;
    uint16_t sum=(uint16_t)(f->sum[0]|((uint16_t)f->sum[1]<<8u));
    if(next>=12u || count>12u || sum>49140u)return false;
    if(count==12u){
        uint16_t old=adc12_filter_sample(f,next);
        if(old>sum)return false;
        sum=(uint16_t)(sum-old);
    }else count++;
    if((uint32_t)sum+raw>49140u)return false;
    sum=(uint16_t)(sum+raw);
    uint8_t at=(uint8_t)(next*3u/2u);
    if(next&1u){
        f->history[at]=(uint8_t)((f->history[at]&15u)|(raw<<4u));
        f->history[at+1u]=(uint8_t)(raw>>4u);
    }else{
        f->history[at]=(uint8_t)raw;
        f->history[at+1u]=(uint8_t)((f->history[at+1u]&240u)|(raw>>8u));
    }
    f->sum[0]=(uint8_t)sum;f->sum[1]=(uint8_t)(sum>>8u);
    next=(uint8_t)(next==11u?0u:next+1u);
    f->state=(uint8_t)(next|(count<<4u));*mean=(uint16_t)(sum/count);
    return true;
}
#endif
