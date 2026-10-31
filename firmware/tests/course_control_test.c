#include "course_control.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static course_control_t c;
static uint64_t now;
static int32_t l,r;
static encoder_sample_t enc(void){return (encoder_sample_t){.left_raw=(uint32_t)-l,.right_raw=(uint32_t)r};}
static void reset(char action,unsigned n){now=1000000;l=r=0;char mapped=action=='s'?'s':action=='l'?(n==1?'l':n==2?'a':'c'):(n==1?'r':n==2?'b':'d');course_begin(&c,(course_config_t){mapped,'s',120},enc(),now);course_on_barcode(&c,'Y',enc(),now,false);}
static line_check_output_t step(bool left,bool right,int dl,int dr,unsigned us)
{now+=us;l+=dl;r+=dr;return course_step(&c,(ir_roles_t){false,left,right},enc(),now);}
static void cross(void)
{
    assert(!step(true,true,10,10,10000).stop);
    assert(!step(true,true,20,20,20000).stop);
}
static void leave(void)
{
    for(unsigned i=0;i<8;i++)assert(!step(false,false,100,100,100000).stop);
}
int main(void)
{
    /* A crossing must clear a preceding hard bend/white hold and restore
     * equal targets, including an intentionally stationary inside wheel. */
    reset('s',1);
    for(unsigned i=0;i<15;i++)assert(!step(true,false,0,90,100000).stop);
    assert(c.drive.hard_corner && c.drive.left_pwm==0);
    assert(!step(true,true,0,9,10000).stop);
    assert(!c.drive.hard_corner && !c.drive.corner_hold);
    assert(c.drive.left_target==TRACK_TARGET_CPS && c.drive.right_target==TRACK_TARGET_CPS);
    reset('s',1);
    assert(!step(true,true,10,10,10000).stop);
    assert(!step(false,false,10,10,10000).stop);assert(!c.junctions); /* glitch */
    cross();assert(c.junctions==1);
    for(unsigned i=0;i<5;i++)assert(!step(true,true,50,50,50000).stop);
    assert(c.junctions==1);leave();assert(!c.latched);
    cross();assert(c.junctions==2);leave();
    cross();assert(c.junctions==3 && c.turns==0);
    reset('s',1);cross();
    line_check_output_t o={0};
    for(unsigned i=0;i<20&&!o.stop;i++)o=step(true,true,100,100,100000);
    assert(o.stop && strcmp(o.state,"COURSE_CROSSING_TOO_WIDE")==0);
    for(unsigned ordinal=2;ordinal<=3;ordinal++)for(unsigned side=0;side<2;side++){
        reset(side?'r':'l',ordinal);
        for(unsigned i=1;i<ordinal;i++){cross();assert(c.phase==COURSE_FOLLOW);leave();}
        cross();assert(c.junctions==ordinal && c.phase==COURSE_CENTER && c.remaining_junctions==0);
        for(unsigned i=0;i<20 && c.phase==COURSE_CENTER;i++)assert(!step(false,false,100,100,100000).stop);
        assert(c.phase==COURSE_BRAKE);assert(!step(false,false,0,0,200000).stop);assert(!step(false,false,0,0,50000).stop);
        assert(c.phase==COURSE_TURN);
        /* Exercise actual calibrated controller endpoints without changing its targets. */
        int lt=c.turn.left.target,rt=c.turn.right.target;
        for(unsigned i=0;i<10;i++)assert(!step(false,false,lt/10,rt/10,100000).stop);
        assert(!step(false,false,lt-(lt/10)*10,rt-(rt/10)*10,10000).stop);
        assert(c.phase==COURSE_SETTLE);
        assert(!step(false,false,0,0,200000).stop);assert(!step(false,false,0,0,50000).stop);
        assert(c.phase==COURSE_SEEK);
        assert(!step(false,false,-20,20,100000).stop);
        assert(!step(true,false,-20,20,100000).stop);
        assert(c.phase==COURSE_FOLLOW && c.turns==1);
        leave();cross();assert(c.phase==COURSE_FOLLOW && c.turns==1); /* one-shot */
    }
    reset('s',1);c.phase=COURSE_SEEK;c.phase_us=c.seek_us=now;
    assert(!step(false,false,-220,220,100000).stop && c.seek_reverse);
    o=step(false,false,440,-440,200000);assert(o.stop && !strcmp(o.state,"COURSE_LINE_NOT_REACQUIRED"));
    reset('s',1);c.phase=COURSE_SEEK;c.phase_us=c.seek_us=now;
    o=step(true,true,0,0,10000);assert(o.stop && !strcmp(o.state,"COURSE_REACQUIRE_AMBIGUOUS"));
    reset('s',1);o=step(false,false,1,1,250001);assert(o.stop && !strcmp(o.state,"COURSE_CONTROL_LATE"));
    /* Crossing the former five-minute wall-clock limit must not stop a
     * responsive moving controller. Travel and stall checks remain separate. */
    reset('s',1);now+=UINT64_C(600000000);c.last_us=now;
    c.drive.left_progress=c.drive.right_progress=now-c.segment_us;
    o=step(false,false,100,100,10000);assert(!o.stop);
    reset('s',1);o=step(false,false,COURSE_LIMIT_COUNTS,0,1000);assert(o.stop&&!strcmp(o.state,"COURSE_TRAVEL_LIMIT"));
    reset('s',1);encoder_sample_t e=enc();e.left_invalid=1;
    assert(course_step(&c,(ir_roles_t){0},e,now).stop);
    reset('s',1);for(unsigned i=0;i<9;i++)assert(!step(false,false,0,0,100000).stop);
    assert(step(false,false,0,0,100000).stop); /* no forward encoder progress */
    reset('s',1);o=step(false,false,-65,1,10000);assert(o.stop);
    course_begin(&c,(course_config_t){'x',2,120},enc(),now);assert(c.phase==COURSE_FAILED);
    /* No selection may trigger motion/turn until a mapped barcode arrives. */
    reset('s',1);course_begin(&c,(course_config_t){'b','l',120},enc(),now);
    cross();leave();cross();leave();assert(c.phase==COURSE_FOLLOW&&!c.turns&&!c.route_updates);
    assert(!strcmp(course_on_barcode(&c,'A',enc(),now,false),"IGNORED_UNMAPPED"));assert(!c.remaining_junctions);
    course_on_barcode(&c,'Y',enc(),now,false);assert(c.remaining_junctions==2);
    cross();leave();assert(c.remaining_junctions==1&&c.phase==COURSE_FOLLOW);
    course_on_barcode(&c,'Z',enc(),now,false);assert(c.remaining_junctions==1&&c.replacements==1&&c.pending_source=='Z');
    cross();assert(c.phase==COURSE_CENTER&&c.turn_action=='l'&&c.turn_source=='Z');
    /* Committed centering defers newest, while STOP overrides even a turn. */
    course_on_barcode(&c,'Y',enc(),now,false);assert(c.deferred&&c.deferred_source=='Y'&&c.phase==COURSE_CENTER);
    course_on_barcode(&c,'Z',enc(),now,false);assert(c.deferred_source=='Z'&&c.deferred_replacements==1);
    c.phase=COURSE_SEEK;c.phase_us=now;c.seek_us=now;c.seek_l=l;c.seek_r=r;
    assert(!step(true,false,0,0,10000).stop);assert(c.phase==COURSE_FOLLOW&&c.remaining_junctions==1&&c.pending_source=='Z'&&!c.deferred);
    c.config.z_action='x';c.phase=COURSE_TURN;
    course_on_barcode(&c,'Z',enc(),now,false);assert(c.phase==COURSE_FAILED&&!c.remaining_junctions&&!c.deferred);
    assert(!strcmp(course_on_barcode(&c,'Y',enc(),now,false),"IGNORED_STOPPED"));
    /* Straight cancels a waiting turn, repeated Y restarts the relative count. */
    reset('r',2);cross();leave();assert(c.remaining_junctions==1);
    course_on_barcode(&c,'Y',enc(),now,false);assert(c.remaining_junctions==2&&c.replacements==1);
    course_on_barcode(&c,'Z',enc(),now,false);assert(!c.remaining_junctions&&!c.pending_source&&c.replacements==2);
    cross();assert(c.phase==COURSE_FOLLOW);
    /* A cross already underneath the sensors is not the next future cross. */
    reset('s',1);c.config.y_action='r';
    assert(!step(true,true,10,10,10000).stop);
    course_on_barcode(&c,'Y',enc(),now,true);
    assert(!step(true,true,20,20,20000).stop);assert(c.remaining_junctions==1&&c.phase==COURSE_FOLLOW);
    leave();cross();assert(c.phase==COURSE_CENTER);
    /* Immediate actions never use the forward centering distance. */
    const char immediate[]={'L','R','U'};
    for(unsigned i=0;i<3;i++){
        reset('s',1);c.config.y_action=immediate[i];
        course_on_barcode(&c,'Y',enc(),now,false);assert(c.phase==COURSE_BRAKE&&!c.remaining_junctions);
        assert(!step(false,false,0,0,200000).stop);assert(!step(false,false,0,0,50000).stop);
        assert(c.phase==COURSE_TURN);
        assert(c.turn.kind==(i==0?MOTION_LEFT_90:i==1?MOTION_RIGHT_90:MOTION_RIGHT_180));
    }
    /* Every documented action is valid and maps to the correct future count. */
    const char all[]="sLRUlrabcdx";
    for(unsigned i=0;i<sizeof(all)-1;i++)assert(course_config_valid((course_config_t){all[i],all[i],120}));
    assert(!course_config_valid((course_config_t){'Q','s',120}));
    assert(!course_config_valid((course_config_t){'s','s',0}));
    puts("Course tests passed: debouncing, encounter latch, straight crossings, barcode-relative left/right, replacements, deferral/STOP, immediate L/R/U, centering, calibrated turn, bounded reacquire, one-shot and faults.");
}
