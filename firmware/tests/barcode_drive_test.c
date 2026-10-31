#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "barcode_drive.h"

static void bounded(line_check_output_t o)
{
    assert(isfinite(o.left)&&isfinite(o.right));
    if(o.stop){assert(o.left==0 && o.right==0);return;}
    assert(o.left>=0.0f&&o.left<=0.55f);
    assert(o.right>=0.0f&&o.right<=0.55f);
}
int main(void)
{
    /* Repeated short edge sightings must not erase the power learned for a
     * slow inside wheel. Model no inside progress, with the outside moving;
     * recovery effort should rise, but the one-second fault must still stop. */
    for(unsigned side=0;side<2;side++){
        barcode_progress_t gentle={0};line_check_output_t out={0};
        float early=0;
        for(unsigned t=0;t<=1000000;t+=10000){
            bool edge=t%100000<60000;
            ir_roles_t ir={.left=edge&&!side,.right=edge&&side};
            out=barcode_drive_step_mode(ir,t,side?(int32_t)(t/1000):0,
                side?0:(int32_t)(t/1000),false,&gentle,true);
            bounded(out);
            assert(out.stop==(t==1000000));
            if(t==100000)early=side?out.right:out.left;
            if(t==900000){
                assert(!gentle.hard_corner);
                assert((side?out.right:out.left)>early+0.025f);
            }
        }
        assert(gentle.stall_mask==(side?2u:1u));
        assert((side?gentle.stall_right_ms:gentle.stall_left_ms)==1000u);
        assert((side?gentle.stall_left_ms:gentle.stall_right_ms)==0u);
    }
    /* Straight acceleration requires a clear interval; either edge immediately
     * restores the slow outside target, and a crossing never cruises. */
    barcode_progress_t cruise={0};line_check_output_t cruise_out;
    for(unsigned t=0;t<=500000;t+=10000){
        cruise_out=barcode_drive_step_mode((ir_roles_t){0},t,t/1000,t/1000,false,&cruise,true);
        assert(!cruise_out.stop);
        if(t<250000)assert(cruise.left_target<=900 && cruise.right_target<=900);
    }
    assert(cruise.left_target==1200 && cruise.right_target==1200);
    cruise_out=barcode_drive_step_mode((ir_roles_t){.right=true},510000,510,510,false,&cruise,true);
    assert(!cruise_out.stop && cruise.left_target==900 && cruise.right_target==250);
    assert(!cruise.cruise_clear_tracking);
    cruise_out=barcode_drive_step_mode((ir_roles_t){0},520000,520,520,false,&cruise,true);
    assert(!cruise_out.stop && cruise.left_target<=900 && cruise.right_target<=900);
    cruise_out=barcode_drive_step_mode((ir_roles_t){.left=true,.right=true},530000,530,530,false,&cruise,true);
    assert(!cruise_out.stop && !cruise.cruise_clear_tracking && cruise.left_target<=900 && cruise.right_target<=900);
    /* Track correction: both polarities, immediate slowdown, intentional
     * inside stop, finite white hold, reversal and actual outside stall. */
    for(unsigned side=0;side<2;side++){
        barcode_progress_t track={0};
        ir_roles_t edge={.left=!side,.right=side},white={0};
        line_check_output_t out;
        int32_t outer=0;
        for(unsigned t=0;t<=1500000;t+=10000){
            outer=(int32_t)(t*0.0009);
            out=barcode_drive_step_mode(edge,t,side?outer:0,side?0:outer,false,&track,true);
            bounded(out);assert(!out.stop);
            assert((side?track.right_target:track.left_target)==(t<80000?250:0));
            if(t>=80000)assert((side?out.right:out.left)==0);
        }
        assert(track.hard_corner);
        out=barcode_drive_step_mode(white,1510000,side?1360:0,side?0:1360,false,&track,true);
        assert(!out.stop && track.corner_hold && (side?out.right:out.left)==0);
        out=barcode_drive_step_mode(white,1600000,side?1440:0,side?0:1440,false,&track,true);
        assert(!out.stop && !track.corner_hold && !track.hard_corner && (side?track.right_target:track.left_target)>0);
        /* Opposite edge replaces direction immediately, never carries hold. */
        out=barcode_drive_step_mode((ir_roles_t){.left=side,.right=!side},1610000,side?1450:10,side?10:1450,false,&track,true);
        assert(!out.stop && !track.hard_corner && track.edge_direction==(side?-1:1));
        track=(barcode_progress_t){0};
        for(unsigned t=0;t<=1000000;t+=10000){
            out=barcode_drive_step_mode(edge,t,0,0,false,&track,true);
            assert(out.stop==(t==1000000));
        }
        assert(!strcmp(out.state,"BARCODE_ENCODER_STALL"));
        assert(track.stall_mask==(side?1u:2u)); /* driven outside wheel only */
    }
    /* Recorded clear-line overspeed must be correctable below the old20% floor. */
    float integral=0;
    float duty=barcode_speed_pwm(1200,2400,0.1f,&integral,false);
    assert(duty<0.20f && integral<0);
    float learned=integral;
    (void)barcode_speed_pwm(1200,1200,0.1f,&integral,false);
    assert(integral==learned);
    /* Saturation must not wind the integral farther into either limit. */
    integral=0.34f;
    assert(barcode_speed_pwm(1200,0,0.1f,&integral,false)==0.50f);
    assert(integral==0.34f);
    (void)barcode_speed_pwm(1200,2400,0.1f,&integral,false);
    assert(integral<0.34f);
    integral=-0.19f;
    assert(barcode_speed_pwm(1200,2400,0.1f,&integral,false)==0);
    assert(integral==-0.19f);
    (void)barcode_speed_pwm(1200,0,0.1f,&integral,false);
    assert(integral>-0.19f);
    ir_roles_t clear={0};barcode_progress_t p={0};
    line_check_output_t o=barcode_drive_step(clear,0,0,0,false,&p);
    bounded(o);assert(!o.stop);
    /* At a fixed sample interval, slow wheels get more drive than fast wheels. */
    barcode_progress_t slow=p,fast=p;
    line_check_output_t a=barcode_drive_step(clear,100000,60,60,false,&slow);
    line_check_output_t b=barcode_drive_step(clear,100000,200,200,false,&fast);
    bounded(a);bounded(b);assert(a.left>b.left && a.right>b.right);
    barcode_progress_t l=p,r=p;
    ir_roles_t left={.left=true},right={.right=true};
    a=barcode_drive_step(left,100000,120,120,false,&l);
    b=barcode_drive_step(right,100000,120,120,false,&r);
    assert(a.left<b.left && a.right>b.right);
    assert(l.left_target==1050 && l.right_target==1200);
    assert(r.left_target==1200 && r.right_target==1050);
    /* Targets change between ticks, but ramps prevent a sudden speed jump. */
    float previous=l.left_target;
    o=barcode_drive_step(left,150000,180,180,false,&l);
    assert(l.left_target==previous-75 && l.right_target==1200);
    assert(o.left<o.right && !o.stop);
    o=barcode_drive_step(clear,150001,180,180,true,&l);bounded(o);assert(o.stop);
    /* Measured overspeed during an edge now receives feedback, not55% power. */
    p=(barcode_progress_t){0};
    for(unsigned t=0;t<=500000;t+=100000){
        o=barcode_drive_step(left,t,(int32_t)(t*0.0023),(int32_t)(t*0.0024),false,&p);
        bounded(o);assert(!o.stop && o.right<0.30f);
        assert(p.left_target>=900 && p.left_target<=1200 && p.right_target==1200);
    }
    assert(p.left_integral<0 && p.right_integral<0);
    assert(p.balance_offset==-50 && p.speed_samples==0 && p.steering_intervals==5);
    o=barcode_drive_step(clear,500001,1150,1200,false,&p);
    assert(!o.stop && p.left_target<901); /* no instantaneous return-to-straight */
    o=barcode_drive_step((ir_roles_t){.left=true,.right=true},500002,1150,1200,false,&p);
    assert(o.stop && o.left==0 && o.right==0);
    p=(barcode_progress_t){0};o=barcode_drive_step(clear,0,-65,0,false,&p);bounded(o);assert(o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step(clear,8000000,100,100,false,&p);bounded(o);assert(o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step(clear,100000,8000,100,false,&p);bounded(o);assert(o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step(clear,100000,100,8000,false,&p);bounded(o);assert(o.stop);
    p=(barcode_progress_t){0};o=barcode_drive_step((ir_roles_t){.left=true,.right=true},0,0,0,false,&p);bounded(o);assert(o.stop);
    p=(barcode_progress_t){0};(void)barcode_drive_step(clear,0,0,0,false,&p);
    o=barcode_drive_step(clear,250001,200,200,false,&p);bounded(o);assert(o.stop);
    p=(barcode_progress_t){0};(void)barcode_drive_step(clear,1000,0,0,false,&p);
    o=barcode_drive_step(clear,999,1,1,false,&p);bounded(o);assert(o.stop);
    /* A single stalled wheel must stop even when the other keeps progressing. */
    p=(barcode_progress_t){0};
    for(unsigned t=0;t<=1000000;t+=100000){
        o=barcode_drive_step(clear,t,0,(int32_t)(t/1000),false,&p);bounded(o);
        assert(o.stop==(t==1000000));
    }
    /* Exercise the loop against two simple plant gains and a load step.
     * This checks response and bounds only; it does not predict real speed. */
    p=(barcode_progress_t){0};float lp=0,rp=0,lv=0,rv=0;
    for(unsigned t=0;t<6000000;t+=10000){
        o=barcode_drive_step(clear,t,(int32_t)lp,(int32_t)rp,false,&p);
        bounded(o);assert(!o.stop);
        float gain=t<3000000?6000.0f:4500.0f;
        lv+=0.1f*(fmaxf(0,gain*(o.left-0.15f))-lv);
        rv+=0.1f*(fmaxf(0,(gain+300)*(o.right-0.15f))-rv);
        lp+=lv*0.01f;rp+=rv*0.01f;
    }
    assert(p.left_cps>900&&p.left_cps<1500);
    assert(p.right_cps>900&&p.right_cps<1500);
    assert(p.speed_samples>40 && p.steering_intervals==0);
    /* Hypothetical faster plant: old20% floor cannot reach1200 at gain8000.
     * Verify convergence across a load change with lag, not physical accuracy. */
    p=(barcode_progress_t){0};lp=rp=lv=rv=0;
    for(unsigned t=0;t<5000000;t+=10000){
        o=barcode_drive_step(clear,t,(int32_t)lp,(int32_t)rp,false,&p);
        bounded(o);assert(!o.stop);
        float gain=t<2500000?8000.0f:6500.0f;
        lv+=0.1f*(gain*o.left-lv);
        rv+=0.1f*((gain+500)*o.right-rv);
        lp+=lv*0.01f;rp+=rv*0.01f;
    }
    assert(p.left_cps>1100&&p.left_cps<1300);
    assert(p.right_cps>1100&&p.right_cps<1300);
    /* Steering at a hypothetical observed-like plant gain retains feedback.
     * Check both polarities and return to straight without an outside boost. */
    p=(barcode_progress_t){0};lp=rp=lv=rv=0;
    for(unsigned t=0;t<5000000;t+=10000){
        ir_roles_t sensed=t<1500000?clear:t<3000000?left:t<4200000?right:clear;
        o=barcode_drive_step(sensed,t,(int32_t)lp,(int32_t)rp,false,&p);
        bounded(o);assert(!o.stop);
        assert(p.left_target<=1200 && p.right_target<=1200);
        lv+=0.1f*(8000*o.left-lv);rv+=0.1f*(8500*o.right-rv);
        lp+=lv*0.01f;rp+=rv*0.01f;
        if(t==2800000)assert(lv<1050 && rv>1100 && rv<1400);
        if(t==4000000)assert(rv<1050 && lv>1100 && lv<1400);
    }
    p=(barcode_progress_t){.center_profile=true,.launch_complete=true};
    for(unsigned t=0;t<=1000000;t+=100000){
        o=barcode_drive_step_profile(clear,t,t*6/10000,t*6/10000,false,&p,true,true);
        assert(!o.stop && p.left_target==600 && p.right_target==600);
    }
    assert(p.speed_samples>=5 && p.steering_intervals==0);
    p.center_steer=-150;
    o=barcode_drive_step_profile(clear,1100000,660,660,false,&p,true,true);
    assert(!o.stop && p.left_target==450 && p.right_target==600 && p.steering_intervals==1);
    puts("PASS barcode speed, ramped steering, PWM bounds, control timing, fault stops and synthetic load/steering scenarios.");
}
