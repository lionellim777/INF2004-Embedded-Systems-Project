#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "barcode_drive.h"
static barcode_capture_t capture;
static uint32_t now;
static unsigned narrow_us=20000;
static void level(bool black,uint32_t duration){
    now+=duration;
    barcode_batch_t b={.count=1,.edges={{now,black}},.stats={.samples=1,.last_us=now,.black=black}};
    barcode_capture_consume(&capture,&b);
}
static void idle_white(uint32_t duration){
    now+=duration;
    barcode_batch_t b={.stats={.samples=1,.last_us=now,.black=false}};
    barcode_capture_consume(&capture,&b);
}
static void symbol(char letter,bool corrupt){
    unsigned patterns[3]={0x094,letter=='A'?0x109:0x0d0,0x094};
    level(true,1000);
    for(unsigned i=0;i<29;i++){
        unsigned k=i%10;
        bool wide=k<9 && (patterns[i/10]&(1u<<(8-k)));
        uint32_t width=wide?3*narrow_us:narrow_us;
        if(corrupt && i==0)width=60000;
        level(i%2!=0,width);
    }
}
static line_check_output_t moving(void){return (line_check_output_t){.left=.2f,.right=.2f};}
int main(void){
    now=UINT32_MAX-100000;barcode_capture_begin_stream(&capture,now);
    symbol('A',false);assert(capture.event_count==1 && capture.events[0].letter=='A');
    assert(!barcode_capture_stop(moving(),&capture).stop);
    idle_white(300000);symbol('Z',false);
    assert(capture.event_count==2 && capture.events[1].letter=='Z');
    assert(capture.events[1].us>capture.events[0].us);
    /* Same letter at a distinct quiet gap is another event, even within3s. */
    idle_white(300000);symbol('Z',false);assert(capture.event_count==3);
    idle_white(300000);symbol('A',true);assert(capture.rejected==1 && capture.event_count==3);
    assert(!barcode_capture_stop(moving(),&capture).stop);
    idle_white(300000);symbol('A',false);assert(capture.event_count==4);
    /* Stuck level timeout is recoverable; it cannot emit a guessed letter. */
    idle_white(300000);level(true,1000);level(true,750001);
    assert(capture.timeouts==1 && !barcode_capture_stop(moving(),&capture).stop);
    level(false,1000);idle_white(300000);symbol('Z',false);assert(capture.event_count==5);
    while(capture.event_count<BARCODE_STREAM_EVENTS){idle_white(300000);symbol('A',false);}
    idle_white(300000);symbol('Z',false);
    assert(capture.event_overflow && capture.integrity_fault && barcode_capture_stop(moving(),&capture).stop);
    /* A short white gap must not rearm or duplicate a completed scan. */
    now=0;barcode_capture_begin_stream(&capture,0);symbol('A',false);
    idle_white(100000);level(true,1000);level(false,1000);
    assert(capture.waiting_gap && capture.event_count==1);
    idle_white(250000);symbol('Z',false);assert(capture.event_count==2);
    /* Full A then half A are distinct events with a real quiet gap. Size
     * changes timing, not the symbol; this is synthetic, not optical proof. */
    now=0;barcode_capture_begin_stream(&capture,0);narrow_us=40000;
    symbol('A',false);idle_white(300000);narrow_us=20000;symbol('A',false);
    assert(capture.event_count==2 && capture.events[0].letter=='A' && capture.events[1].letter=='A');
    assert(!capture.rejected && !capture.timeouts);
    /* Sample integrity remains fatal in stream mode. */
    barcode_batch_t bad={.stats={.gap_fault=true}};barcode_capture_consume(&capture,&bad);
    assert(barcode_capture_stop(moving(),&capture).stop);
    barcode_progress_t p={0};ir_roles_t clear={0},both={.left=true,.right=true};
    line_check_output_t o=barcode_drive_step_mode(clear,0,0,0,false,&p,true);assert(!o.stop);
    o=barcode_drive_step_mode(both,100000,120,120,false,&p,true);
    assert(!o.stop && !strcmp(o.state,"CONTINUOUS_LINE_BRIDGE"));
    o=barcode_drive_step_mode(both,200000,220,220,false,&p,true);assert(!o.stop);
    o=barcode_drive_step_mode(clear,250000,270,270,false,&p,true);assert(!o.stop && p.both_active);
    o=barcode_drive_step_mode(clear,300000,320,320,false,&p,true);assert(!o.stop && !p.both_active);
    /* Persistent ambiguity and brief clear flicker share one bounded budget. */
    o=barcode_drive_step_mode(both,400000,420,420,false,&p,true);assert(!o.stop);
    o=barcode_drive_step_mode(clear,500000,520,520,false,&p,true);assert(!o.stop);
    o=barcode_drive_step_mode(both,520000,540,540,false,&p,true);assert(!o.stop);
    o=barcode_drive_step_mode(both,700000,700,700,false,&p,true);
    assert(o.stop && o.left==0 && !strcmp(o.state,"CONTINUOUS_LINE_AMBIGUOUS"));
    p=(barcode_progress_t){0};(void)barcode_drive_step_mode(both,0,0,0,false,&p,true);
    o=barcode_drive_step_mode(both,100000,360,360,false,&p,true);assert(o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step_mode(clear,20000000,100,100,false,&p,true);assert(o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step_mode(clear,100000,24000,100,false,&p,true);assert(o.stop);
    /* Continuous run passes old8s bound; original diagnostic still stops. */
    p=(barcode_progress_t){0};p.left_progress=p.right_progress=8000000;
    o=barcode_drive_step_mode(clear,8000000,100,100,false,&p,true);assert(!o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step(clear,8000000,100,100,false,&p);assert(o.stop);
    puts("PASS continuous A/Z/repeated-letter sequence, reject/timeout recovery, quiet-gap gate, wrap, bounded events, sample faults and line ambiguity bounds.");
}
