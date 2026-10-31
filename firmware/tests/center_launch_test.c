#include "barcode_drive.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* A simple unequal-friction wheel plant. This exercises the missing startup
 * case; it is not a physical model or proof of course/heading acceptance. */
static barcode_progress_t run_plant(barcode_progress_t p,float left_dead,float right_dead)
{
    float l=0,r=0,lv=0,rv=0,max_launch_skew=0;
    line_check_output_t o={0};
    for(unsigned t=0;t<4000000;t+=10000){
        bool launching=!p.launch_complete;
        o=barcode_drive_step_profile((ir_roles_t){0},t,(int)l,(int)r,false,&p,true,true);
        assert(!o.stop && o.left>=0 && o.right>=0 && o.left<=0.5f && o.right<=0.5f);
        if(launching){
            assert(o.left<=0.26f && o.right<=0.26f);
            if(!p.launch_complete && (int)l-(int)r>=12)assert(o.left==0);
            if(!p.launch_complete && (int)r-(int)l>=12)assert(o.right==0);
            if(fabsf(l-r)>max_launch_skew)max_launch_skew=fabsf(l-r);
        }
        lv+=0.2f*(fmaxf(0,8000*(o.left-left_dead))-lv);
        rv+=0.2f*(fmaxf(0,8000*(o.right-right_dead))-rv);
        l+=lv*0.01f;r+=rv*0.01f;
    }
    assert(p.launch_complete && p.speed_valid && p.speed_samples>20);
    assert(max_launch_skew<35); /* includes inertia after the leading wheel is held */
    assert(fabsf(l-r)<25 && lv>500 && lv<700 && rv>500 && rv<700);
    printf("Unequal friction %.2f/%.2f: launch peak skew %.1f, final skew %.1f, speed %.1f/%.1f cps.\n",
           left_dead,right_dead,max_launch_skew,l-r,lv,rv);
    return p;
}
/* Command a gentle bend under unequal motor friction, then leave the bend.
 * This tests wheel response, not optical geometry or physical barcode success. */
static void steering_plant(float ld,float rd,float steer)
{
    barcode_progress_t p={.center_profile=true};
    float l=0,r=0,lv=0,rv=0,entry=0,exit=0;
    int sign=steer>0?1:-1;bool responded=false;
    for(unsigned t=0;t<6000000;t+=10000){
        if(t==2000000)entry=l-r;
        if(t==3000000){
            assert(responded);exit=l-r;
            printf("Steer%+.0f friction %.2f/%.2f: 1s differential %.1f counts.\n",steer,ld,rd,(exit-entry)*sign);
        }
        p.center_steer=t>=2000000 && t<3000000?steer:0;
        line_check_output_t o=barcode_drive_step_profile((ir_roles_t){0},t,(int)l,(int)r,false,&p,true,true);
        assert(!o.stop && o.left>=0 && o.right>=0 && o.left<=0.5f && o.right<=0.5f);
        assert(fabsf(p.yaw_error)<=40.01f && fabsf(p.yaw_pwm)<=0.0601f);
        if(t>=2000000 && t<3000000 && (l-r-entry)*sign>=20)responded=true;
        lv+=0.2f*(fmaxf(0,8000*(o.left-ld))-lv);
        rv+=0.2f*(fmaxf(0,8000*(o.right-rd))-rv);
        l+=lv*0.01f;r+=rv*0.01f;
    }
    /* Old turn demand must not keep accumulating after strong tape returns. */
    assert(fabsf(l-r-exit)<25 && fabsf(lv-rv)<20);
}
int main(void)
{
    for(int swap=0;swap<2;swap++)for(int dir=-1;dir<=1;dir+=2){
        steering_plant(swap?0.13f:0.08f,swap?0.08f:0.13f,75.0f*dir);
        steering_plant(swap?0.13f:0.08f,swap?0.08f:0.13f,220.0f*dir);
    }
    barcode_progress_t p=run_plant((barcode_progress_t){.center_profile=true},0.08f,0.13f);
    /* Stop/restart with learned effort, as after a recovery. */
    run_plant((barcode_progress_t){.center_profile=true,
        .launch_left_pwm=barcode_clamp(0.157f*0.5f+p.left_integral,0.12f,0.22f),
        .launch_right_pwm=barcode_clamp(0.142f*0.5f+p.right_integral,0.14f,0.22f)},0.08f,0.13f);
    run_plant((barcode_progress_t){.center_profile=true},0.13f,0.08f);
    /* No encoder feedback from one wheel must not permit an endless boost. */
    for(int side=0;side<2;side++){
        p=(barcode_progress_t){.center_profile=true};line_check_output_t o={0};
        for(unsigned t=0;t<=1100000;t+=10000){
            int progress=t?20:0;
            o=barcode_drive_step_profile((ir_roles_t){0},t,side?0:progress,side?progress:0,false,&p,true,true);
            if(o.stop)break;
            assert((side?o.right:o.left)==(t?0:(side?0.14f:0.12f)));
        }
        assert(o.stop && !o.left && !o.right && !p.launch_complete);
        assert(p.stall_mask&(side?1:2));
    }
    p=(barcode_progress_t){.center_profile=true};
    barcode_drive_step_profile((ir_roles_t){0},0,0,0,false,&p,true,true);
    assert(barcode_drive_step_profile((ir_roles_t){0},250001,0,0,false,&p,true,true).stop);
    puts("PASS balanced launch, unequal wheel friction, warm restart, blocked wheel and timing limits.");
}
