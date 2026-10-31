#include <assert.h>
#include <stdio.h>
#include "barcode_signal.h"
#include "barcode_acquisition.h"
#include "barcode_optical_fixtures.h"
static barcode_sampler_t sampler;
static barcode_capture_t capture;
static barcode_signal_t filter;
static uint32_t now;
static void begin(void){
    filter=(barcode_signal_t){0};now=0;
    barcode_sampler_begin_levels(&sampler,BARCODE_DRIVE_BLACK_ON,BARCODE_DRIVE_BLACK_OFF);
    barcode_capture_begin_stream(&capture,0);capture.waiting_gap=true;
}
static void sample(uint16_t raw){
    now+=BARCODE_SAMPLE_US;barcode_sampler_sample(&sampler,barcode_signal_update(&filter,raw),now);
    barcode_batch_t batch;barcode_sampler_take(&sampler,&batch);barcode_capture_consume(&capture,&batch);
}
static void level(uint16_t raw,unsigned ms){for(unsigned i=0;i<ms*1000u/BARCODE_SAMPLE_US;i++)sample(raw);}
int main(void){
    barcode_sampler_begin_levels(&sampler,220,226);assert(!sampler.enabled);
    barcode_sampler_begin_levels(&sampler,4096,220);assert(!sampler.enabled);
    filter=(barcode_signal_t){0};
    for(unsigned i=0;i<100;i++)assert(barcode_signal_update(&filter,4095)==4095);
    for(unsigned i=0;i<BARCODE_FILTER_SAMPLES;i++)barcode_signal_update(&filter,0);
    assert(filter.sum==0);
    for(unsigned t=0;t<6;t++)for(unsigned mode=0;mode<4;mode++){
        begin();
        for(unsigned i=0;i<500;i++)for(unsigned j=0;j<20;j++){
            uint16_t lo=optical_fixtures[t][i][0],hi=optical_fixtures[t][i][1];
            uint16_t raw=mode==0?(lo+hi)/2:mode==1?(j%2?lo:hi):lo+(hi-lo)*(mode==2?j:19-j)/19;
            sample(raw);
        }
        level(190,800);
        assert(!capture.integrity_fault);
        if(t==5){assert(capture.event_count==1&&capture.events[0].letter=='A');}
        else assert(!capture.event_count); /* partial sequences are never letters */
    }
    static const uint16_t letters[]={0x109,0x049,0x148,0x019,0x118,0x058,0x00d,0x10c,0x04c,0x01c,0x103,0x043,0x142,0x013,0x112,0x052,0x007,0x106,0x046,0x016,0x181,0x0c1,0x1c0,0x091,0x190,0x0d0};
    for(unsigned l=0;l<26;l++)for(unsigned rev=0;rev<2;rev++)for(unsigned narrow=30;narrow<=60;narrow+=30){
        unsigned widths[29],n=0;uint16_t pats[]={0x094,letters[l],0x094};
        for(unsigned ch=0;ch<3;ch++){
            for(int bit=8;bit>=0;bit--)widths[n++]=(pats[ch]&(1u<<bit))?3*narrow:narrow;
            if(ch<2)widths[n++]=narrow;
        }
        begin();level(190,400);
        for(unsigned i=0;i<29;i++)level(i%2?190:3000,widths[rev?28-i:i]);
        level(190,400);
        assert(capture.event_count==1&&capture.events[0].letter==(char)('A'+l)&&!capture.integrity_fault);
    }
    /* White noise and a single short spike must not fabricate a symbol. */
    begin();for(unsigned i=0;i<10000;i++)sample(210+i%21);
    sample(4095);level(210,2000);assert(!capture.event_count);
    puts("PASS filtered low-contrast half-A reconstructions (not raw replay), partial-sequence rejection, A-Z both directions at 30/60ms narrow widths, white noise, filter bounds and invalid calibration.");
}
