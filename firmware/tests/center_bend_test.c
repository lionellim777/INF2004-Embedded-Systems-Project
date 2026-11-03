#define ROBOCAR_CENTER_LINE 1
#include "../app/robot_firmware/course_control.c"
#include <assert.h>
#include <stdio.h>
static course_control_t c;
static int32_t l,r;
static uint64_t now;
static line_check_output_t step(int level,bool right,int dl,int dr,unsigned dt){
    now+=dt;l+=dl;r+=dr;
    course_center_level(&c,level);
    return course_step(&c,(ir_roles_t){false,level>=250,right},
        (encoder_sample_t){.left_raw=(uint32_t)-l,.right_raw=(uint32_t)r},now);
}
static void setup(void){
    now=1000000;l=r=0;course_begin(&c,(course_config_t){'s','s',120},(encoder_sample_t){0},now);
    course_set_center_reference(&c,800);c.drive.launch_complete=true;
    for(int i=0;i<12;i++)assert(!step(800,false,60,60,100000).stop);
    assert(c.bend_ready);
}
static void hint(int direction){
    if(direction>0){step(500,true,60,60,100000);}
    else{step(500,false,60,60,100000);step(550,false,30,60,100000);}
    assert(c.bend_direction==direction&&c.bend_hint_us);
}
static line_check_output_t lose(int direction){
    line_check_output_t o=step(0,direction>0,5,5,10000);
    assert(c.bend_active&&c.bend_attempts==1&&c.phase==COURSE_FOLLOW&&!o.stop);
    assert(!course_reads_barcodes(&c));
    assert(c.drive.center_steer*direction>0);
    return o;
}
int main(void){
    for(int dir=-1;dir<=1;dir+=2){
        setup();hint(dir);line_check_output_t o=lose(dir);
        for(int i=0;i<6;i++){
            o=step(i>=2?800:0,false,dir>0?10:3,dir>0?3:10,20000);
            assert(!o.stop&&o.left>=0&&o.right>=0);
        }
        assert(!c.bend_active&&c.bend_completed==1&&c.phase==COURSE_FOLLOW);
        assert(c.drive.launch_complete&&c.recoveries==0&&course_reads_barcodes(&c));
        /* No immediate second blind excursion after only a black glimpse. */
        o=step(0,false,2,2,10000);
        assert(!o.stop&&c.phase==COURSE_LOST&&!o.left&&!o.right&&c.bend_attempts==1);
    }
    /* Unknown/stale direction, startup, barcode proximity, obstacle override,
     * and a crossing do not authorize blind continuation. */
    for(int mode=0;mode<6;mode++){
        setup();if(mode!=0)hint(-1);
        if(mode==1)for(int i=0;i<7;i++)step(800,false,60,60,100000);
        if(mode==2)c.drive.launch_complete=false;
        if(mode==3)course_note_barcode_activity(&c,now);
        if(mode==4)c.bend_disabled=true;
        if(mode==5)step(800,true,10,10,10000);
        line_check_output_t o=step(0,false,0,0,10000);
        assert(!c.bend_active&&!c.bend_attempts&&c.phase==COURSE_LOST&&!o.left&&!o.right);
    }
    /* Absolute bounds cannot be renewed by weak tape flicker. */
    for(int mode=0;mode<5;mode++){
        setup();hint(-1);lose(-1);line_check_output_t o;
        if(mode==0){for(int i=0;i<5;i++)o=step(i%2?650:0,false,5,10,100000);}
        else if(mode==1)o=step(0,false,200,280,10000);
        else if(mode==2)o=step(0,false,0,190,10000);
        else if(mode==3)o=step(0,false,80,0,10000); /* wrong yaw */
        else o=step(0,false,-30,0,10000); /* reverse encoder motion */
        assert(!c.bend_active&&c.bend_fallbacks==1&&c.phase==COURSE_LOST&&!o.left&&!o.right);
    }
    setup();hint(-1);lose(-1);
    line_check_output_t o=step(0,false,0,0,300000);
    assert(o.stop&&!o.left&&!o.right&&!strcmp(o.state,"COURSE_CONTROL_LATE"));
    setup();hint(-1);lose(-1);
    now+=10000;
    o=course_step(&c,(ir_roles_t){0},(encoder_sample_t){.left_raw=(uint32_t)-l,.right_raw=(uint32_t)r,.right_invalid=1},now);
    assert(o.stop&&!o.left&&!o.right&&!strcmp(o.state,"COURSE_ENCODER_FAULT"));
    puts("PASS rolling left/right rejoin; fresh evidence, arming, no rearming on flicker; time/travel/yaw bounds; unknown, barcode, obstacle, crossing and fault guards");
}
