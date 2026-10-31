#include "barcode_optical.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    barcode_optical_t o;
    barcode_optical_begin(&o,UINT32_MAX-2000000u);
    uint32_t origin=o.start_us;
    for(unsigned t=0;t<5000;t++)
        barcode_optical_sample(&o,t%10==5?219:205,origin+t*1000u);
    barcode_optical_sample(&o,4000,origin+5000000u);
    barcode_optical_end(&o,origin+5001000u);
    assert(!o.active&&o.samples==5000&&o.max_gap_us==1000);
    for(unsigned i=0;i<500;i++)assert(o.low[i]==205&&o.high[i]==219);
    /* A one-sample pulse is retained, even when a slower screen misses it. */
    barcode_optical_begin(&o,0);
    barcode_optical_sample(&o,190,1000);
    barcode_optical_sample(&o,1000,2000);
    barcode_optical_sample(&o,190,3000);
    barcode_optical_sample(&o,200,20000);
    assert(o.low[0]==190&&o.high[0]==1000&&o.low[1]==UINT16_MAX);
    assert(o.max_gap_us==17000);
    barcode_optical_end(&o,5000000);
    assert(o.max_gap_us==4980000);
    barcode_optical_begin(&o,100);
    barcode_optical_end(&o,5000100);
    assert(o.samples==0&&o.max_gap_us==5000000);
    barcode_optical_begin(&o,0);
    barcode_optical_sample(&o,250,1000);
    barcode_optical_end(&o,2000);
    barcode_optical_sample(&o,900,3000);
    assert(o.samples==1&&o.high[0]==250);
    /* Auto-arm waits without sampling, triggers once, and cannot retrigger. */
    barcode_optical_arm(&o);
    barcode_optical_sample(&o,190,50000);
    assert(o.armed&&!o.active&&!o.samples);
    barcode_optical_trigger(&o,100000);
    barcode_optical_sample(&o,240,100000);
    barcode_optical_trigger(&o,200000);
    assert(o.start_us==100000&&o.samples==1&&!o.armed);
    barcode_optical_end(&o,101000);
    barcode_optical_trigger(&o,300000);
    barcode_optical_sample(&o,900,301000);
    assert(!o.active&&!o.armed&&o.samples==1&&o.high[0]==240);
    barcode_optical_arm(&o);
    barcode_optical_trigger(&o,500000);
    assert(o.samples==0&&o.low[0]==UINT16_MAX);
    for(unsigned t=0;t<5000;t++)barcode_optical_sample(&o,200,500000+t*1000u);
    barcode_optical_sample(&o,200,5500000);
    barcode_optical_end(&o,9000000);
    assert(!o.active&&o.samples==5000&&o.max_gap_us==1000);
    puts("Optical capture: brief peaks, missing samples, clock wrap and cancellation passed.");
}
