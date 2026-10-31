#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "barcode_acquisition.h"
#include "barcode_drive.h"
static barcode_sampler_t s;
static barcode_capture_t c;
static barcode_batch_t b;
static uint32_t now;
static unsigned ticks;
static bool measured_levels;
static void drain(void){barcode_sampler_take(&s,&b);barcode_capture_consume(&c,&b);}
static void sample(uint16_t raw,unsigned interval,unsigned drain_ticks){
    now+=interval;barcode_sampler_sample(&s,raw,now);
    if(drain_ticks && ++ticks % drain_ticks==0)drain();
}
static void pattern(char ch,unsigned narrow,bool reverse,uint32_t start,unsigned delayed){
    unsigned widths[29],n=0;unsigned symbols[3]={0x094,ch=='A'?0x109:0x0d0,0x094};
    for(unsigned j=0;j<3;j++){for(int bit=8;bit>=0;bit--)widths[n++]=(symbols[j]&(1u<<bit))?3*narrow:narrow;if(j<2)widths[n++]=narrow;}
    barcode_sampler_begin(&s);barcode_capture_begin(&c);now=start;ticks=0;
    for(unsigned i=0;i<10;i++)sample(200,1000,delayed);
    for(unsigned j=0;j<29;j++)for(unsigned k=0;k<widths[reverse?28-j:j];k++){
        unsigned width=widths[reverse?28-j:j];
        uint16_t raw=j%2?200:3000;
        if(measured_levels)raw=j%2 ? (width==narrow?196+k%7:182+k%6) :
                                   (width==narrow?254+k%7:1992+k%11);
        sample(raw,k%2?900:1100,delayed);
    }
    for(unsigned i=0;i<10;i++)sample(200,1000,delayed);
    drain();assert(c.decoded==ch);assert(c.frames==1);assert(c.rejected==0);
    assert(!c.integrity_fault && c.stats.edges==30);assert(c.saved_runs==29);
}
/* Replay observed edge timing: retain valid letters, latch failed captures.
 * This tests recorded thresholded evidence, not optical detection or steering. */
static void recorded_regressions(void){
    const uint32_t t0[]={171000u,230000u,296000u,495000u,571000u,744000u,819000u,878000u,1078000u,1125000u,1177000u,1350000u,1424000u,1481000u,1645000u,1694000u,1762000u,1807000u,1872000u,2048000u,2116000u,2171000u,2229000u,2409000u,2473000u,2663000u,2717000u,2765000u,2958000u,3003000u};
    const uint32_t t1[]={157000u,214000u,287000u,484000u,563000u,768000u,838000u,891000u,1088000u,1150000u,1212000u,1405000u,1465000u,1505000u,1682000u,1729000u,1782000u,1830000u,1888000u,2012000u,2051000u,2089000u,2145000u,2319000u,2398000u,2586000u,2631000u,2663000u,2796000u,2833000u,3859000u,4603000u,4938000u};
    const uint32_t t2[]={360000u,1124000u,1267000u,2114000u,2268000u,3098000u,3248000u,3354000u,4025000u,5207000u,5241000u};
    const uint32_t t3[]={286000u,333000u,413000u,599000u,685000u,870000u,953000u,997000u,1217000u,1264000u,1346000u,1393000u,1472000u,1517000u,1595000u,1766000u,1843000u,2014000u,2220000u,2263000u,2342000u,2388000u,2464000u,2634000u,2709000u,2879000u,2956000u,2999000u,3206000u,3249000u};
    const uint32_t t4[]={356000u,400000u,482000u,665000u,747000u,930000u,1013000u,1063000u,1277000u,1322000u,1404000u,1450000u,1531000u,1578000u,1655000u,1823000u,1900000u,2075000u,2286000u,2330000u,2407000u,2452000u,2524000u,2689000u,2765000u,2924000u,2999000u,3043000u,3246000u,3289000u};
    const uint32_t *times[]={t0,t1,t2,t3,t4};
    const unsigned lengths[]={30,33,11,30,30};
    const char expected[]={'A',0,0,'Z','Z'};
    for(unsigned run=0;run<5;run++){
        barcode_capture_t capture;barcode_capture_begin_drive(&capture);
        barcode_batch_t batch={0};
        for(unsigned i=0;i<lengths[run];i++){
            batch.count=1;batch.edges[0]=(barcode_edge_t){times[run][i],i%2==0};
            batch.stats.samples=i+1;batch.stats.last_us=times[run][i];
            batch.stats.black=i%2==0;
            barcode_capture_consume(&capture,&batch);
        }
        assert(capture.decoded==expected[run]);
        line_check_output_t out=barcode_capture_stop((line_check_output_t){.left=0.2f,.right=0.2f},&capture);
        assert(out.stop && out.left==0 && out.right==0);
        if(expected[run])assert(!strcmp(out.state,"BARCODE_READ"));
        if(run==1){
            assert(!strcmp(out.state,"BARCODE_REJECTED"));
            assert(capture.rejected==1 && capture.saved_runs==29 && capture.timeouts==0);
            assert(barcode_runs_get(&capture.last_runs,27)==133000);
        }
        if(run==2)assert(!strcmp(out.state,"BARCODE_CAPTURE_TIMEOUT") && capture.timeouts==1);
    }
    /* A stuck-black timeout fires even without any later edge. */
    barcode_capture_t capture;barcode_capture_begin_drive(&capture);
    barcode_batch_t batch={.count=1,.edges={{10000,true}},.stats={.samples=1,.last_us=10000,.black=true}};
    barcode_capture_consume(&capture,&batch);
    batch.count=0;batch.stats.last_us=760000;barcode_capture_consume(&capture,&batch);
    assert(!capture.timeouts);
    batch.stats.last_us++;barcode_capture_consume(&capture,&batch);
    assert(capture.timeouts==1);
    /* An existing drive fault retains priority; sampling corruption overrides. */
    line_check_output_t stopped={0,0,true,"BARCODE_ENCODER_FAULT"};
    assert(!strcmp(barcode_capture_stop(stopped,&capture).state,"BARCODE_ENCODER_FAULT"));
    capture.integrity_fault=true;
    assert(!strcmp(barcode_capture_stop(stopped,&capture).state,"BARCODE_SAMPLE_FAULT"));
}

int main(void){
    recorded_regressions();
    for(unsigned size=0;size<2;size++)for(unsigned rev=0;rev<2;rev++)for(unsigned delay=1;delay<=40;delay+=39){
        pattern('A',size?10:20,rev,0,delay);pattern('Z',size?10:20,rev,UINT32_MAX-200000,delay);
    }
    pattern('A',10,false,0,0); /* whole frame buffered while foreground delayed */
    measured_levels=true;
    for(unsigned rev=0;rev<2;rev++){
        pattern('A',20,rev,0,40);pattern('Z',20,rev,0,40);
    }
    measured_levels=false;
    /* Strict >250 entry, <=220 release, retained state in the middle;
     * a single outlier must not create an edge. */
    barcode_sampler_begin(&s);barcode_capture_begin(&c);now=0;ticks=0;
    for(unsigned i=0;i<5;i++)sample(250,1000,1);
    assert(s.stats.edges==0 && !s.stats.black);
    sample(251,1000,1);sample(200,1000,1);assert(s.stats.edges==0);
    sample(251,1000,1);sample(251,1000,1);assert(s.stats.edges==1 && s.stats.black);
    for(unsigned i=0;i<5;i++)sample(i%2?250:221,1000,1);
    assert(s.stats.edges==1 && s.stats.black);
    sample(220,1000,1);sample(251,1000,1);assert(s.stats.edges==1);
    sample(220,1000,1);sample(220,1000,1);assert(s.stats.edges==2 && !s.stats.black);
    assert(s.stats.white_dips==1 && s.stats.raw_band>0 && s.stats.raw_white>0);
    assert(s.stats.raw_white+s.stats.raw_band+s.stats.raw_black==s.stats.samples);
    assert(s.stats.longest_black_us>0);
    barcode_sampler_begin(&s);barcode_capture_begin(&c);now=0;ticks=0;
    sample(200,1000,1);sample(3000,1000,1);sample(200,1000,1);assert(s.stats.edges==0);
    sample(3000,1000,1);sample(3000,900,1);assert(c.reader.active && c.timeouts==0);
    sample(3000,4000,1);assert(c.integrity_fault && !c.decoded);
    barcode_sampler_begin(&s);barcode_capture_begin(&c);now=0;
    for(unsigned i=0;i<140;i++){sample(i%4<2?3000:200,1000,0);}drain();assert(c.integrity_fault && s.stats.overflow);
    barcode_sampler_begin(&s);barcode_capture_begin(&c);now=0;
    for(unsigned i=0;i<760;i++)sample(3000,1000,1);assert(c.timeouts==1);
    barcode_progress_t p={0};ir_roles_t ir={0};
    assert(!barcode_drive_step(ir,500000,100,100,false,&p).stop);
    assert(barcode_drive_step(ir,1500000,101,100,false,&p).stop);
    p=(barcode_progress_t){0};assert(barcode_drive_step(ir,0,-65,0,false,&p).stop);
    assert(barcode_drive_step(ir,0,8000,0,false,&p).stop);
    assert(barcode_drive_step(ir,8000000,100,100,false,&p).stop);
    assert(barcode_drive_step(ir,0,0,0,true,&p).stop);
    ir.left=ir.right=true;assert(barcode_drive_step(ir,0,0,0,false,&p).stop);
    puts("Barcode acquisition passed: A/Z both sizes/directions, measured narrow/wide levels, 250/251 and 220/221 boundaries, hysteresis, delayed foreground, jitter/wrap, bounce, gap/overflow, timeout and drive bounds.");
}
