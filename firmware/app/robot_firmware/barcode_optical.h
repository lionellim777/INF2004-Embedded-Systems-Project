#ifndef BARCODE_OPTICAL_H
#define BARCODE_OPTICAL_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Diagnostic only: retain extrema in 10 ms windows of the existing 1 ms ADC
 * stream. No threshold, motor command or decoder state is changed. */
#define OPTICAL_WINDOW_US 10000u
#define OPTICAL_BINS 500u
#define OPTICAL_DURATION_US (OPTICAL_WINDOW_US * OPTICAL_BINS)
typedef struct {
    uint16_t low[OPTICAL_BINS], high[OPTICAL_BINS];
    uint32_t start_us, last_us, samples, max_gap_us;
    bool active, armed;
} barcode_optical_t;
/* Foreground begin/end with IRQ excluded. sample has the ADC IRQ as owner. */
static inline void barcode_optical_begin(barcode_optical_t *o,uint32_t now)
{
    memset(o,0,sizeof(*o));
    for(unsigned i=0;i<OPTICAL_BINS;i++)o->low[i]=UINT16_MAX;
    o->start_us=o->last_us=now;o->active=true;
}
/* Pre-clear in foreground; triggering in the ADC IRQ is constant-time. */
static inline void barcode_optical_arm(barcode_optical_t *o)
{
    barcode_optical_begin(o,0);o->active=false;o->armed=true;
}
static inline void barcode_optical_trigger(barcode_optical_t *o,uint32_t now)
{
    if(!o->armed)return;
    o->armed=false;o->active=true;o->start_us=o->last_us=now;
}
static inline void barcode_optical_sample(barcode_optical_t *o,uint16_t raw,uint32_t now)
{
    if(!o->active)return;
    uint32_t elapsed=now-o->start_us;
    if(elapsed>=OPTICAL_DURATION_US){o->active=false;return;}
    uint32_t gap=now-o->last_us;
    if(gap>o->max_gap_us)o->max_gap_us=gap;
    o->last_us=now;o->samples++;
    unsigned bin=elapsed/OPTICAL_WINDOW_US;
    if(raw<o->low[bin])o->low[bin]=raw;
    if(raw>o->high[bin])o->high[bin]=raw;
}
static inline void barcode_optical_end(barcode_optical_t *o,uint32_t now)
{
    uint32_t elapsed=now-o->start_us;
    if(elapsed>OPTICAL_DURATION_US)elapsed=OPTICAL_DURATION_US;
    uint32_t tail=elapsed-(uint32_t)(o->last_us-o->start_us);
    if(tail>o->max_gap_us)o->max_gap_us=tail;
    o->active=false;o->armed=false;
}
#endif
