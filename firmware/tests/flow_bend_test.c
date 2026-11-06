#define ROBOCAR_CENTER_LINE 1
#include "../app/robot_firmware/course_control.c"
#include <assert.h>
#include <stdio.h>
static course_control_t c;
static int32_t l,r;
static uint64_t now;
static line_check_output_t step(int center,int side,int dl,int dr,unsigned dt)
{
    now+=dt;l+=dl;r+=dr;
    course_center_level(&c,center);course_right_level(&c,side);
    return course_step(&c,(ir_roles_t){false,center>=250,side>=250},
        (encoder_sample_t){.left_raw=(uint32_t)-l,.right_raw=(uint32_t)r},now);
}
static void setup(void)
{
    now=1000000;l=r=0;course_begin(&c,(course_config_t){'s','s',120},(encoder_sample_t){0},now);
    course_enable_flow_follow(&c,true);course_set_center_reference(&c,800);c.drive.launch_complete=true;
    for(unsigned i=0;i<12;i++)assert(!step(800,0,60,60,100000).stop);
    assert(c.bend_ready);
}
static void right_entry(void)
{
    for(unsigned i=0;i<4;i++)step(500,160,6,6,10000);
    assert(c.right_preview && c.right_previews==1 && c.bend_active && c.bend_direction==1);
    assert(!c.recoveries && c.phase==COURSE_FOLLOW);
}
int main(void)
{
    line_check_output_t o={0};
    setup();course_note_barcode_activity(&c,now);right_entry();
    for(unsigned i=0;i<8;i++){
        line_check_output_t o=step(i>=4?800:0,160,10,3,20000);
        assert(!o.stop && o.left>=0 && o.right>=0);
        assert(c.drive.left_target<=600 && c.drive.right_target>=120);
    }
    assert(!c.bend_active && c.bend_completed==1 && !c.recoveries);
    setup();right_entry();
    for(unsigned i=0;i<3;i++)o=step(800,400,10,3,20000);
    assert(!c.bend_active && c.bend_completed==1 && !c.recoveries && o.left>=0 && o.right>=0);
    /* History can guide a gentle probe, not a hard arc with both side
     * readings absent. Reproduces the v5.59 straight-drift confidence error. */
    setup();c.bend_direction=-1;c.bend_hint_us=now;c.bend_source="CONFIRMED_TEST";
    o=step(500,0,6,6,10000);
    assert(!c.bend_active && !c.bend_attempts && c.drive.center_steer>=-150 && o.left>=0 && o.right>=0);
    for(int direction=-1;direction<=1;direction+=2){
        setup();c.bend_direction=direction;c.bend_hint_us=now;c.bend_source="RECOVERY_PRIOR";
        c.center_trim.last_good=direction;
        o=step(450,0,6,6,10000);
        assert(!c.bend_active && !c.bend_attempts && c.drive.center_steer<=150 && c.drive.center_steer>=-150);
        o=step(0,0,6,6,10000);
        assert(!c.bend_active && !c.bend_attempts);
    }
    /* A small wrong-direction trial reverses after measured yaw and optical
     * deterioration, then tapers out as GP28 improves. No bend classification. */
    setup();c.center_trim.last_good=1;
    o=step(700,0,6,6,10000);assert(c.drive.center_steer>0 && c.drive.center_steer<=150);
    o=step(690,0,40,10,100000);assert(!c.center_trim.reversed);
    o=step(620,0,40,10,100000);assert(c.center_trim.reversed && c.drive.center_steer<0);
    o=step(740,0,10,40,100000);assert(!c.bend_active && c.drive.center_steer<0);
    o=step(790,0,10,40,100000);assert(!c.center_trim.active && !c.bend_attempts);
    setup();o=step(0,0,6,6,10000);
    assert(!strcmp(o.state,"CENTER_EDGE_PREVIEW") && !c.bend_active);
    for(unsigned i=0;i<7;i++)o=step(0,0,6,6,10000);
    o=step(0,0,6,6,10000);
    assert(c.phase==COURSE_LOST && !o.left && !o.right && c.loss_coasts==1);
    /* Right-side confirmation can arrive during the short forward grace. */
    setup();step(0,0,5,5,10000);
    for(unsigned i=0;i<3;i++)o=step(0,160,6,6,20000);
    assert(c.bend_active && c.bend_direction==1 && !o.stop);
    /* Strong centre ignores side-grey background; a short spike is not a hint. */
    setup();for(unsigned i=0;i<10;i++)step(800,160,6,6,10000);
    assert(!c.right_preview && !c.right_previews && !c.bend_attempts);
    step(600,160,6,6,10000);step(600,0,6,6,20000);
    assert(!c.right_preview && !c.right_previews);
    setup();o=step(450,400,6,6,10000); /* Even full-black needs timed evidence. */
    assert(!c.bend_attempts && !c.right_preview);
    step(450,0,6,6,10000);assert(!c.bend_attempts && !c.right_previews);
    /* Existing absolute time/travel/yaw/wrong-motion bounds remain. */
    for(unsigned mode=0;mode<5;mode++){
        setup();right_entry();
        if(mode==0)for(unsigned i=0;i<7;i++)o=step(0,160,10,3,100000);
        if(mode==1)o=step(0,160,550,10,10000);
        if(mode==2)o=step(0,160,410,0,10000);
        if(mode==3)o=step(0,160,-30,0,10000);
        if(mode==4){c.bend_disabled=true;o=step(0,160,1,1,10000);}
        assert(!c.bend_active && c.bend_fallbacks==1 && c.phase==COURSE_LOST && !o.left && !o.right);
        assert(c.center_search.limit==120); /* No barcode-only 60-count cap. */
    }
    setup();right_entry();c.config.z_action='R';
    assert(!strcmp(course_on_barcode(&c,'Z',(encoder_sample_t){0},now,false),"DEFERRED_UNTIL_MANOEUVRE_COMPLETE"));
    for(unsigned i=0;i<3;i++)o=step(800,0,10,3,20000);
    assert(c.phase==COURSE_BRAKE && !c.deferred && !o.left && !o.right);
    setup();right_entry();c.config.z_action='b';
    course_on_barcode(&c,'Z',(encoder_sample_t){0},now,false);
    for(unsigned i=0;i<5;i++)o=step(800,400,10,3,20000);
    assert(c.phase==COURSE_FOLLOW && !c.deferred && c.junctions==1 &&
        c.remaining_junctions==course_action_junctions('b'));
    setup();right_entry();
    c.config.z_action='x';assert(!strcmp(course_on_barcode(&c,'Z',(encoder_sample_t){0},now,false),"STOP_NOW"));
    o=step(0,160,1,1,10000);assert(o.stop && !o.left && !o.right);
    setup();right_entry();o=step(0,160,0,0,250001);assert(o.stop && !strcmp(o.state,"COURSE_CONTROL_LATE"));
    puts("PASS flow: live right-side ink gates hard arcs; recovery history alone does not; early gentle drift probe reverses on measured deterioration; timed preview, bounds, moving rejoin and STOP guards.");
}
