#include <assert.h>
#include <stdio.h>
#include "barcode_acquisition.h"
static barcode_sampler_t sampler;
static barcode_capture_t capture;
static uint32_t now,left,right;
static unsigned take_every=1;
static void tick(unsigned raw,int dl,int dr){
    left-=(uint32_t)dl;right+=(uint32_t)dr;now+=1000;
    barcode_sampler_sample_motion(&sampler,(uint16_t)raw,now,left,right);
    if(now/1000u%take_every==0){
        barcode_batch_t batch;barcode_sampler_take(&sampler,&batch);
        barcode_capture_consume(&capture,&batch);
    }
}
static void white(unsigned ms){while(ms--)tick(190,0,0);}
static void begin(void){
    now=0;left=right=0;take_every=1;
    barcode_sampler_begin_levels(&sampler,226,220);
    barcode_sampler_enable_motion(&sampler,left,right);
    barcode_capture_begin_stream(&capture,0);capture.distance_mode=true;capture.waiting_gap=true;
    white(300);
}
static void run_symbol(unsigned pattern,unsigned narrow,bool reversed,bool pause){
    unsigned widths[29],n=0,pats[]={0x094,pattern,0x094};
    for(unsigned ch=0;ch<3;ch++){
        for(int bit=8;bit>=0;bit--)widths[n++]=(pats[ch]&(1u<<bit))?narrow*3:narrow;
        if(ch<2)widths[n++]=narrow;
    }
    for(unsigned i=0;i<29;i++){
        unsigned raw=i%2?190:2100;
        unsigned goal=widths[reversed?28-i:i],moved=0,steps=0;
        while(moved<goal){
            /* Unequal forward wheel rates model steering; vary speed per bar.
             * Optical widths follow AXLE path here, not full sensor geometry. */
            unsigned stride=i%3==0?1:i%3==1?2:4;
            int dl=0,dr=0;
            if(steps++%stride==0){dl=i%2?1:2;dr=i%2?2:1;moved+=3;}
            tick(raw,dl,dr);
            if(pause && i==14 && moved==goal/2 && dl){
                for(unsigned k=0;k<1000;k++)tick(raw,0,0);
            }
        }
    }
    white(300);
}
int main(void){
    static const unsigned patterns[]={0x109,0x049,0x148,0x019,0x118,0x058,0x00d,0x10c,0x04c,0x01c,0x103,0x043,0x142,0x013,0x112,0x052,0x007,0x106,0x046,0x016,0x181,0x0c1,0x1c0,0x091,0x190,0x0d0};
    for(unsigned l=0;l<26;l++)for(unsigned narrow=48;narrow<=96;narrow+=48)for(unsigned reverse=0;reverse<2;reverse++){
        begin();run_symbol(patterns[l],narrow,reverse,true);
        assert(capture.event_count==1 && capture.events[0].letter==(char)('A'+l));
        assert(!capture.integrity_fault && !capture.timeouts && !capture.rejected && !capture.motion_aborts);
    }
    /* Queue many sample/edge timestamps between foreground drains. */
    for(unsigned batch=20;batch<=100;batch+=20){
        begin();take_every=batch;run_symbol(0x109,48,false,true);run_symbol(0x0d0,96,false,false);
        assert(capture.event_count==2 && capture.events[0].letter=='A' && capture.events[1].letter=='Z');
        assert(!capture.integrity_fault);
    }
    /* Same variable-speed signal fails a TIME-only reader; distance resolves it. */
    begin();capture.distance_mode=false;run_symbol(0x0d0,60,false,true);assert(!capture.event_count);
    begin();run_symbol(0x109,60,false,false);run_symbol(0x0d0,60,false,true);
    assert(capture.event_count==2 && capture.events[0].letter=='A' && capture.events[1].letter=='Z');
    /* A pivot breaks a partial frame; white gap plus a fresh full scan recovers. */
    begin();for(unsigned i=0;i<20;i++)tick(2100,1,1);assert(capture.reader.active);
    for(unsigned i=0;i<20;i++)tick(i%4?2100:190,-1,1);
    assert(capture.motion_aborts && !capture.event_count && capture.waiting_gap);
    white(300);run_symbol(0x0d0,60,false,false);assert(capture.event_count==1&&capture.events[0].letter=='Z');
    /* Stationary optical flicker cannot invent nonzero spatial bars. */
    begin();for(unsigned i=0;i<10;i++)tick(2100,1,1);
    for(unsigned i=0;i<5;i++)tick(190,0,0); /* completes current bar */
    for(unsigned i=0;i<5;i++)tick(2100,0,0);
    assert(capture.zero_distance_aborts==1 && !capture.event_count);
    /* Stationary holds bounded to2s, not an infinite partial-frame lifetime. */
    begin();for(unsigned i=0;i<20;i++)tick(2100,1,1);
    for(unsigned i=0;i<2100;i++)tick(2100,0,0);
    assert(capture.pause_timeouts==1 && !capture.event_count);
    /* A physical counter wrap and measurement wrap are not reset faults. */
    begin();left=2;right=UINT32_MAX-2;barcode_motion_begin(&sampler.motion,left,right);
    sampler.motion.position=UINT32_MAX-30000;
    run_symbol(0x0d0,60,false,true);assert(capture.event_count==1&&!capture.motion_aborts);
    /* Small bounce does not add fictitious travel. A reset/jump breaks scan. */
    barcode_motion_t m;barcode_motion_begin(&m,0,0);
    barcode_motion_step(&m,(uint32_t)-10,10);assert(m.position==20000);
    barcode_motion_step(&m,(uint32_t)-9,9);barcode_motion_step(&m,(uint32_t)-10,10);
    assert(m.position==20000&&!m.breaks);
    barcode_motion_step(&m,(uint32_t)-100,100);assert(m.breaks==1);
    puts("PASS distance-width A-Z both sizes/directions, unequal forward rates, variable speed, 1s pauses, A/Z sequence, reverse reset/rearm, zero-travel edges, 2s hold bound, bounce and counter wrap. Not a physical curved-sensor-path simulation.");
}
