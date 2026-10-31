#ifndef BARCODE_SIGNAL_H
#define BARCODE_SIGNAL_H
#include <stdint.h>
/* Provisional moving-scan calibration from 30 Oct half-A optical capture.
 * Raw optical diagnostics remain unfiltered. No changes to GP28/GP27. */
#define BARCODE_DRIVE_BLACK_ON 226u
#define BARCODE_DRIVE_BLACK_OFF 220u
#ifndef BARCODE_FILTER_SAMPLES
#define BARCODE_FILTER_SAMPLES 12u
#endif
typedef struct {
    uint16_t samples[BARCODE_FILTER_SAMPLES];
    uint32_t sum;
    unsigned next,count;
} barcode_signal_t;
static inline uint16_t barcode_signal_update(barcode_signal_t *s,uint16_t raw)
{
    if(s->count==BARCODE_FILTER_SAMPLES)s->sum-=s->samples[s->next];
    else s->count++;
    s->samples[s->next]=raw;s->sum+=raw;
    s->next=(s->next+1u)%BARCODE_FILTER_SAMPLES;
    return (uint16_t)(s->sum/s->count);
}
#endif
