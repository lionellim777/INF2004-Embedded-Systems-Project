#include "obstacle_bypass.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Deterministic controller simulation. This does not model tyre slip, beam
 * width, mounting or ground clearance; those require the supervised run. */
static obstacle_bypass_t a;
static encoder_sample_t enc;
static uint64_t now;
static double left,right;
static line_check_output_t out;
static unsigned seen_phases;
static void cautious_speed_tests(void){
    /* Both a sluggish and a fast plant: speed feedback must work below the
     * old 25% floor, obey the ramp/cap and stop at the commanded endpoint. */
    const float gains[]={2500,9000};
    for(unsigned g=0;g<2;g++){
        motion_control_t m={0};encoder_sample_t e={0};double l=0,r=0;
        float prev_l=0,prev_r=0;
        assert(motion_begin_distance(&m,200,e,0));
        for(unsigned i=1;i<=3000&&m.phase==MOTION_ACTIVE;i++){
            l+=m.left.throttle*gains[g]*.01;r+=m.right.throttle*gains[g]*.01;
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            assert(m.left.throttle>=0&&m.left.throttle<=.24001f);
            assert(m.right.throttle>=0&&m.right.throttle<=.24001f);
            assert(m.left.throttle-prev_l<=.04001f&&m.right.throttle-prev_r<=.04001f);
            prev_l=m.left.throttle;prev_r=m.right.throttle;
        }
        assert(m.phase==MOTION_DONE&&!m.left.throttle&&!m.right.throttle);
    }
    motion_control_t m={0};encoder_sample_t e={0};
    assert(motion_begin_distance(&m,200,e,0));motion_poll(&m,e,100000);
    e.left_raw=(uint32_t)-300;e.right_raw=300;motion_poll(&m,e,200000);
    assert(m.left.throttle==0&&m.right.throttle==0); /* brake drive on overspeed */
    puts("PASS cautious speed ramp, cap, overspeed and endpoint on two motor responses");
}
static void init(void){
    enc=(encoder_sample_t){0};now=1000000;left=right=0;
    obstacle_bypass_begin(&a,enc,now,700);out=(line_check_output_t){0};seen_phases=0;
}
static void adaptive_cruise_tests(void){
    /* Actual course/bypass controller with moving encoders: grow only after
     * sustained strong tape; every optical/sonar uncertainty removes boost. */
    for(unsigned mode=0;mode<6;mode++){
        init();float previous=600;bool boosted=false;
        for(unsigned i=0;i<350&&!out.stop;i++){
            now+=10000;left+=out.left*4000*.01;right+=out.right*4000*.01;
            enc.left_raw=(uint32_t)-(int32_t)left;enc.right_raw=(uint32_t)(int32_t)right;
            bool valid=true,right_black=false;int center=700;float cm=100;
            if(i>=300){
                if(mode==1)center=620; /* weaken before trim threshold */
                if(mode==2)right_black=true;
                if(mode==3)valid=false;
                if(mode==4)cm=50;
                if(mode==5)center=550; /* steering active */
            }
            out=obstacle_bypass_step(&a,(ir_roles_t){false,center>=250,right_black},center,enc,now,true,valid,cm);
            float cruise=a.line.drive.center_cruise_cps;
            assert(!out.stop&&a.phase==AVOID_FOLLOW&&cruise<=800);
            if(i<100)assert(cruise<=600);
            if(cruise>600)assert(cruise-previous<=2.01f);
            if(i==299){assert(cruise==800);boosted=true;}
            if(i>=300&&mode){
                assert(cruise==600&&a.line.drive.left_target<=600&&a.line.drive.right_target<=600);
            }
            previous=cruise;
        }
        assert(boosted);
    }
    /* Transit profile is faster on a responsive plant and still stops at
     * its encoder endpoint; ramp, effort and stall limits remain bounded. */
    uint64_t durations[2]={0};
    for(unsigned fast=0;fast<2;fast++){
        motion_control_t m={0};encoder_sample_t e={0};double l=0,r=0;
        assert(motion_begin_distance(&m,200,e,0));m.transit_drive=fast;
        float prior=0;
        for(unsigned i=1;i<3000&&m.phase==MOTION_ACTIVE;i++){
            l+=m.left.throttle*4000*.01;r+=m.right.throttle*4000*.01;
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            assert(m.left.throttle<= (fast?.28001f:.24001f));
            assert(m.left.throttle-prior<=.04001f);prior=m.left.throttle;
            durations[fast]=i*10000u;
        }
        assert(m.phase==MOTION_DONE&&!m.left.throttle&&!m.right.throttle);
    }
    assert(durations[1]<durations[0]);
    puts("PASS adaptive cruise ramp/cap, weak tape, right hint, no echo, nearer object, trim, faster bounded transit");
}
static void tick(bool fresh,bool valid,float cm,int center,bool stall){
    now+=10000;
    if(!stall){left+=out.left*2500*.01;right+=out.right*2500*.01;}
    enc.left_raw=(uint32_t)-(int32_t)left;enc.right_raw=(uint32_t)(int32_t)right;
    out=obstacle_bypass_step(&a,(ir_roles_t){true,center>=250,false},center,enc,now,fresh,valid,cm);
    seen_phases|=1u<<a.phase;
    assert(a.servo_angle>=50&&a.servo_angle<=125);
    if(a.phase==AVOID_WAIT||a.phase==AVOID_SCAN_OUT||a.phase==AVOID_SCAN_SIDE||a.phase==AVOID_SCAN_FORWARD||a.phase==AVOID_CHECK_TAPE||a.phase==AVOID_ALIGN_CONFIRM||out.stop)
        assert(!out.left&&!out.right);
}
static void run(unsigned scenario){
    init();
    for(unsigned i=0;i<25000&&!out.stop&&!a.completed;i++){
        bool fresh=i%12==0,valid=true,stall=false;float cm=180;int center=(a.phase==AVOID_FOLLOW||a.phase==AVOID_VERIFY||a.phase==AVOID_CHECK_TAPE||a.phase==AVOID_TAPE_PROBE)?700:0;
        if(a.phase==AVOID_FOLLOW)cm=25;
        if(a.phase==AVOID_SCAN_SIDE)cm=a.along_mm<=300?28:180;
        if(scenario==1&&a.phase==AVOID_SCAN_OUT)cm=25;
        if(scenario==2&&a.phase==AVOID_SCAN_SIDE)cm=28;
        if(scenario==3&&a.phase==AVOID_MOVE_OUT)stall=true;
        if(scenario==4&&a.phase==AVOID_MOVE_OUT)fresh=false;
        if(scenario==5&&a.phase==AVOID_MOVE_ALONG)cm=10;
        if(scenario==6&&a.phase==AVOID_SCAN_SIDE)cm=180; /* never saw the object alongside */
        if(scenario==7&&a.phase==AVOID_MOVE_OUT)enc.left_invalid=2;
        if(scenario==17&&a.phase==AVOID_TURN_OUT&&motion_left_position(&a.leg,enc)>400)enc.right_invalid=1;
        if(scenario==8&&a.phase==AVOID_SCAN_FORWARD&&a.offset_mm<450)cm=22;
        if(scenario==9&&a.phase==AVOID_SCAN_FORWARD)cm=22;
        if(scenario==10&&a.phase==AVOID_SCAN_SIDE&&a.along_mm>300)valid=false;
        if(scenario==11&&a.phase==AVOID_SCAN_OUT&&a.samples==0)cm=20; /* one blocked ray vetoes scan */
        if(scenario==12&&a.phase==AVOID_SCAN_SIDE){cm=28;if(a.along_mm==400)cm=180;} /* only one clear checkpoint */
        if(a.phase==AVOID_RETURN_LINE){
            float mm=(motion_left_position(&a.leg,enc)+motion_right_position(&a.leg,enc))/(2*13.491f);
            center=mm>a.offset_mm-120?700:0;
            if(scenario==13)center=0;
            if(scenario==14)center=700;
        }
        if(scenario==15&&a.phase==AVOID_CHECK_TAPE)center=0;
        if(scenario==16&&a.phase==AVOID_VERIFY)center=0;
        if(a.phase==AVOID_TURN_OUT||a.phase==AVOID_TURN_ALONG)
            assert(a.leg.left.target==1045&&a.leg.right.target==-1045);
        tick(fresh,valid,cm,center,stall);
    }
    const char *reasons[]={NULL,"AVOID_RIGHT_PATH_BLOCKED","AVOID_OBJECT_END_NOT_CONFIRMED",
        "NO_ENCODER_PROGRESS_1S","AVOID_RANGE_STALE","AVOID_PATH_OBSTACLE_STOP",
        "AVOID_OBJECT_END_NOT_CONFIRMED","AVOID_ENCODER_INVALID",NULL,
        "AVOID_OBJECT_TOO_WIDE_OR_PATH_BLOCKED","AVOID_OBJECT_END_NOT_CONFIRMED","AVOID_RIGHT_PATH_BLOCKED",
        "AVOID_OBJECT_END_NOT_CONFIRMED","AVOID_RETURN_LINE_NOT_FOUND",
        NULL,"AVOID_RETURN_LINE_NOT_FOUND",NULL,NULL};
    if(scenario==16){assert(out.stop&&!a.completed);}
    else if(reasons[scenario]){
        if(!out.stop||strcmp(out.state,reasons[scenario])){printf("scenario%u stopped=%d reason=%s phase=%s\n",scenario,out.stop,out.state,obstacle_bypass_phase(a.phase));assert(0);}
        assert(!a.completed);
    }else{
        if(out.stop||a.completed!=1){printf("scenario%u completed=%u stopped=%d reason=%s phase=%s\n",scenario,a.completed,out.stop,out.state,obstacle_bypass_phase(a.phase));assert(0);}
        assert(a.phase==AVOID_FOLLOW&&a.detected==1&&a.side_seen&&a.along_mm>=400);
        assert(seen_phases&(1u<<AVOID_VERIFY));
        if(scenario==8)assert(a.offset_mm==500);
        if(scenario==10)assert(a.no_echo>0);
        if(scenario==17)assert(a.encoder_skips[1]==1&&enc.right_invalid==1&&(seen_phases&(1u<<AVOID_SCAN_OUT)));
        /* A stale course timestamp after the bypass must not immediately fault. */
        tick(true,true,180,700,false);assert(!out.stop);
    }
    printf("PASS obstacle scenario %u: %s completed=%u\n",scenario,out.state,a.completed);
}
static void retry_tests(void){
    init();
    /* Without an obstacle/fault, output must match the existing course
     * controller for identical sensor and encoder inputs. */
    course_control_t original;
    course_begin(&original,(course_config_t){'s','s',120},enc,now);
    course_set_center_reference(&original,700);
    for(unsigned i=0;i<150;i++){
        now+=10000;enc.left_raw=(uint32_t)-(int32_t)(i*6);enc.right_raw=i*6;
        int level=i<80?700:400;
        ir_roles_t input={false,true,false};
        course_center_level(&original,level);
        line_check_output_t expected=course_step(&original,input,enc,now);
        out=obstacle_bypass_guarded_step(&a,input,level,enc,now,true,true,80,NULL);
        assert(out.left==expected.left&&out.right==expected.right&&out.stop==expected.stop&&!strcmp(out.state,expected.state));
    }
    init();
    const char *err="AVOID_SONAR_ECHO_HIGH_BEFORE_TRIGGER";
    ir_roles_t ir={false,true,false};
    /* Fault while an existing line search is underway: preserve its origin,
     * direction and remaining budget; stationary waiting must not time it out. */
    course_center_level(&a.line,0);
    now+=10000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,true,80,NULL);
    assert(a.line.phase==COURSE_LOST);
    center_search_t search=a.line.center_search;
    now+=10000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,false,0,err);
    assert(!out.stop&&!out.left&&!out.right&&a.sonar_wait);
    uint64_t active=a.active_us;
    for(unsigned i=0;i<7;i++){
        now+=100000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,false,0,err);
        assert(!out.stop&&!out.left&&!out.right&&a.active_us==active);
        assert(!memcmp(&search,&a.line.center_search,sizeof(search)));
    }
    now+=100000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,true,80,NULL);
    assert(a.sonar_wait&&!out.left&&!out.right);
    now+=100000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,true,80,NULL);
    assert(!out.stop&&!a.sonar_wait&&a.sonar_resumes==1&&a.line.phase==COURSE_LOST);
    assert(a.active_us==active);
    /* NO_ECHO is not sufficient to release a fault pause. */
    init();now+=10000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,false,0,err);
    for(unsigned i=0;i<10;i++){now+=100000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,false,0,NULL);assert(!out.left&&!out.right);}
    assert(out.stop&&!strcmp(out.state,"AVOID_SONAR_RECOVERY_FAILED"));
    /* Obstacle discovered on retry must enter bypass before any forward PWM. */
    init();now+=10000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,false,0,err);
    for(unsigned i=0;i<2;i++){now+=100000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,true,20,NULL);assert(!out.left&&!out.right);}
    assert(!out.stop&&a.detected==1&&a.phase==AVOID_WAIT);
    puts("PASS stopped sonar retry, preserved line-search state, persistent failure and obstacle on resume");
}
static void encoder_error_tests(void){
    init();enc.right_invalid=1;tick(true,true,100,700,false);assert(!out.stop&&a.encoder_skips[1]==1);
    enc.right_invalid=2;tick(true,true,100,700,false);assert(out.stop&&!strcmp(out.state,"AVOID_ENCODER_INVALID"));
    init();enc.left_invalid=2;tick(true,true,100,700,false);assert(out.stop);
    init();enc.right_invalid=1;tick(true,true,100,700,false);
    for(unsigned i=0;i<30;i++)tick(true,true,100,700,false);
    enc.right_invalid=2;tick(true,true,100,700,false);assert(!out.stop&&a.encoder_skips[1]==2);
    for(unsigned i=0;i<30;i++)tick(true,true,100,700,false);
    enc.right_invalid=3;tick(true,true,100,700,false);assert(out.stop);
    init();enc.right_invalid=7;obstacle_bypass_begin(&a,enc,now,700);tick(true,true,100,700,false);assert(!out.stop&&!a.encoder_skips[1]);
    enc.right_invalid=0;tick(true,true,100,700,false);assert(out.stop);
    puts("PASS isolated encoder transition, burst/rate/run budget, initial counters and counter reset");
}
static void early_rejoin_tests(void){
    const avoid_phase_t phases[]={AVOID_MOVE_OUT,AVOID_MOVE_ALONG,AVOID_TURN_FORWARD,
        AVOID_TURN_LOOK,AVOID_TURN_ALONG,AVOID_SCAN_SIDE,AVOID_SCAN_FORWARD,AVOID_WAIT,AVOID_RETURN_LINE};
    for(unsigned k=0;k<sizeof(phases)/sizeof(phases[0]);k++){
        init();a.phase=phases[k];a.episode_us=now;a.offset_mm=200;a.rejoin_armed=true;
        a.next=AVOID_TURN_LOOK;a.servo_angle=50;
        tick(true,true,180,700,false);
        assert(a.phase==AVOID_CHECK_TAPE&&a.servo_angle==90&&a.rejoin_attempts==1);
        for(unsigned i=0;i<25;i++)tick(true,true,180,700,false);
        assert(a.phase==AVOID_CHECK_TAPE&&!a.completed); /* ignore settling rays */
        for(unsigned i=0;i<200&&a.phase!=AVOID_VERIFY;i++)tick(true,true,180,700,false);
        assert(a.phase==AVOID_VERIFY&&!a.completed);
        for(unsigned i=0;i<35;i++)tick(true,true,180,700,true);
        assert(!a.completed); /* stationary black is not proof of following */
        for(unsigned i=0;i<400&&!a.completed&&!out.stop;i++)tick(true,true,180,700,false);
        assert(a.completed==1&&!out.stop);
    }
    /* Right sensor can request checking even while centre is white. */
    init();a.phase=AVOID_MOVE_ALONG;a.offset_mm=200;a.episode_us=now;a.rejoin_armed=true;
    now+=10000;out=obstacle_bypass_step(&a,(ir_roles_t){false,false,true},0,enc,now,true,true,180);
    assert(a.phase==AVOID_CHECK_TAPE&&!out.left&&!out.right);
    /* Starting line is not an escape; barcode alone never triggers a rejoin. */
    init();a.phase=AVOID_TURN_OUT;a.episode_us=now;a.rejoin_armed=true;
    motion_begin(&a.leg,MOTION_RIGHT_90,enc,now);
    tick(true,true,180,700,false);assert(a.phase==AVOID_TURN_OUT);
    a.offset_mm=200;tick(true,true,180,0,false);assert(a.phase==AVOID_TURN_OUT);
    /* Blocked, vanished and unknown candidates restore the original target. */
    for(unsigned mode=0;mode<3;mode++){
        init();a.phase=AVOID_MOVE_ALONG;a.offset_mm=200;a.episode_us=now;a.rejoin_armed=true;
        motion_begin_distance(&a.leg,100,enc,now);encoder_sample_t origin=a.leg.start;
        int target=a.leg.left.target;
        tick(true,true,180,700,false);
        for(unsigned i=0;i<170&&a.phase==AVOID_CHECK_TAPE;i++)
            tick(i%12==0,mode!=2,mode==0?20:180,mode==1?0:700,false);
        assert(a.phase==AVOID_MOVE_ALONG&&a.rejoin_rejected==1&&!out.stop);
        assert(a.leg.left.target==target&&!memcmp(&a.leg.start,&origin,sizeof(origin)));
        assert(!a.rejoin_armed&&!a.completed);
        tick(true,true,180,700,false);assert(a.phase==AVOID_MOVE_ALONG&&!out.stop);
    }
    /* A bottle after the trial rejoin must stop the car, not report success. */
    init();a.phase=AVOID_VERIFY;a.phase_us=a.episode_us=now;
    tick(true,true,20,700,false);tick(true,true,20,700,false);
    assert(out.stop&&!a.completed&&!strcmp(out.state,"AVOID_RETURN_BLOCKED"));
    puts("PASS early tape capture, settling, forward proof, right hint, starting tape exclusion and rejection resume");
}
static void turn_assist_tests(void){
    motion_control_t m={0};encoder_sample_t e={0};
    assert(motion_begin(&m,MOTION_LEFT_90,e,0));m.slow_turn=true;
    bool assist=false;
    for(unsigned i=1;i<=10;i++){
        motion_poll(&m,e,i*100000u);
        if(fabsf(m.left.throttle)>.24f)assist=true;
        assert(fabsf(m.left.throttle)<=.30001f&&fabsf(m.right.throttle)<=.30001f);
    }
    assert(assist&&m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED);
    /* A wheel that needs >24% to restart can now finish; no permanent floor. */
    assert(motion_begin(&m,MOTION_LEFT_90,e,0));m.slow_turn=true;
    double l=0,r=0;
    for(unsigned i=1;i<=1200&&m.phase==MOTION_ACTIVE;i++){
        if(fabsf(m.left.throttle)>.24f)l+=m.left.throttle*2500*.01;
        if(fabsf(m.right.throttle)>.24f)r+=m.right.throttle*2500*.01;
        e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
        motion_poll(&m,e,i*10000u);
    }
    assert(m.phase==MOTION_DONE);
    puts("PASS turn-only assist, deadband recovery and retained hard-stall stop");
}
static void alignment_drive_tests(void){
    for(unsigned profile=0;profile<2;profile++){
        motion_control_t m={0};encoder_sample_t e={0};double l=0,r=0;
        assert(motion_begin_distance(&m,119,e,0));m.alignment_drive=profile;
        float pl=0,pr=0;
        for(unsigned i=1;i<=3000&&m.phase==MOTION_ACTIVE;i++){
            /* Same mid-advance failure seen at 300-400 counts in the logs.
             * Old 24% profile cannot overcome this load; new PI must recover. */
            l+=(l<300?m.left.throttle*2500:fmaxf(0,m.left.throttle-.25f)*10000)*.01;
            r+=(r<300?m.right.throttle*2500:fmaxf(0,m.right.throttle-.265f)*12000)*.01;
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            assert(m.left.throttle<=.32001f&&m.right.throttle<=.32001f);
            assert(m.left.throttle-pl<=.04001f&&m.right.throttle-pr<=.04001f);
            pl=m.left.throttle;pr=m.right.throttle;
        }
        if(profile)assert(m.phase==MOTION_DONE);
        else assert(m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED);
    }
    for(unsigned block=0;block<2;block++){
        motion_control_t m={0};encoder_sample_t e={0};double l=0;
        assert(motion_begin_distance(&m,119,e,0));m.alignment_drive=true;
        for(unsigned i=1;i<=100&&m.phase==MOTION_ACTIVE;i++){
            if(block)l+=m.left.throttle*9000*.01;
            e.left_raw=(uint32_t)-(int32_t)l;
            motion_poll(&m,e,i*10000u);
        }
        assert(m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED);
        assert(!m.left.throttle&&!m.right.throttle);
        assert(l<300); /* leading wheel held while the right wheel is blocked */
    }
    motion_control_t m={0};encoder_sample_t e={0};
    assert(motion_begin_distance(&m,119,e,0));m.alignment_drive=true;
    motion_poll(&m,e,100000);e.left_raw=(uint32_t)-300;e.right_raw=300;
    motion_poll(&m,e,200000);assert(!m.left.throttle&&!m.right.throttle);
    puts("PASS old alignment stall reproduced; loaded advance recovery, ramp, overspeed, single/both wheel stall and skew hold");
}
static void alignment_tests(void){
    /* Geometric plant: sensor 120 mm ahead of axle; tape at x=0 extending
     * along y. A perpendicular approach cannot succeed by a small pivot at
     * the detection point x=-120. Count-to-yaw uses 1000 counts per 90 deg.
     * This validates geometry/control logic, not tyre slip or physical optics. */
    for(unsigned mode=0;mode<10;mode++){
        init();a.phase=AVOID_CHECK_TAPE;a.candidate_us=a.phase_us=a.episode_us=now;
        a.offset_mm=200;a.resume_phase=AVOID_MOVE_ALONG;a.rejoin_attempts=1;
        /* Prior wheel imbalance must not change the explicit right-first policy. */
        a.episode_origin.start.left_raw=600;
        double x=-120,y=0,heading=0;
        unsigned visited=0;bool saw_lost=false;
        for(unsigned i=0;i<5000&&!out.stop&&!a.completed;i++){
            double dl=out.left*2500*.01,dr=out.right*2500*.01;
            if((mode==5||mode==6)&&a.phase==AVOID_AXLE_ADVANCE){
                if(motion_left_position(&a.leg,enc)>=300)dl=fmaxf(0,out.left-.25f)*10000*.01;
                if(motion_right_position(&a.leg,enc)>=300)dr=fmaxf(0,out.right-.25f)*10000*.01;
            }
            left+=dl;right+=dr;
            double dh=-(dl-dr)/2000.0*1.57079632679;
            double ds=(dl+dr)/(2*13.491);
            x+=ds*cos(heading+dh/2);y+=ds*sin(heading+dh/2);heading+=dh;
            enc.left_raw=(uint32_t)-(int32_t)left;enc.right_raw=(uint32_t)(int32_t)right;
            double sx=x+120*cos(heading);
            int center=fabs(sx)<=10?700:0;
            if(a.phase==AVOID_TAPE_PROBE&&!center)saw_lost=true;
            if(mode==1&&(a.phase==AVOID_ALIGN_TURN||a.phase==AVOID_ALIGN_CONFIRM))center=0;
            /* Force preferred sweep to miss, exercise opposite full sweep. */
            if(mode==2&&a.phase==AVOID_ALIGN_TURN&&!a.align_reversed)center=0;
            double degrees=-heading*180/3.14159265359;
            /* Barcode-like ink at 20..40deg must not end the turn. */
            if((mode==7||mode==9)&&a.phase==AVOID_ALIGN_TURN){
                if(mode==9)center=0;
                if(degrees>20&&degrees<40)center=700;
            }
            /* A candidate inside the angle window vanishes while stopped.
             * Preserve the remaining sweep, then find the real line at 90deg. */
            if(mode==8&&!a.align_rejected){
                if(a.phase==AVOID_ALIGN_TURN&&degrees>65&&degrees<75)center=700;
                if(a.phase==AVOID_ALIGN_CONFIRM)center=0;
            }
            bool fresh=i%12==0;float cm=180;
            if(mode==3&&a.phase==AVOID_AXLE_ADVANCE)cm=10;
            if(mode==4&&a.phase==AVOID_ALIGN_CONFIRM)cm=20;
            now+=10000;
            out=obstacle_bypass_step(&a,(ir_roles_t){false,center>=250,false},center,enc,now,fresh,true,cm);
            if(mode==5&&a.phase==AVOID_AXLE_ADVANCE)a.leg.alignment_drive=false;
            visited|=1u<<a.phase;
            if(a.phase==AVOID_ALIGN_TURN&&!a.align_reversed)assert(a.align_direction==1);
            if(a.phase==AVOID_ALIGN_CONFIRM){
                assert(!out.left&&!out.right);
                assert(fabs(degrees)>=59&&fabs(degrees)<=116);
            }
        }
        assert(saw_lost&&(visited&(1u<<AVOID_AXLE_ADVANCE)));
        if(mode==0||mode==2||mode==6||mode==7||mode==8){
            if(out.stop||!a.completed)printf("geometry mode=%u phase=%s x=%.2f y=%.2f heading=%.2f stop=%s\n",mode,obstacle_bypass_phase(a.phase),x,y,heading,out.state);
            assert(!out.stop&&a.completed==1);
            assert(visited&(1u<<AVOID_ALIGN_CONFIRM));
            if(mode==2)assert(a.align_reversed);
            if(mode==8)assert(a.align_rejected==1);
        }else{
            assert(out.stop&&!a.completed);
            const char *reason=(mode==1||mode==9)?"AVOID_TAPE_HEADING_NOT_FOUND":mode==3?"AVOID_PATH_OBSTACLE_STOP":mode==5?"NO_ENCODER_PROGRESS_1S":"AVOID_ALIGNED_PATH_BLOCKED";
            assert(!strcmp(out.state,reason));
        }
    }
    puts("PASS 10 perpendicular geometry cases: right-first, both sweeps, early false ink, rejected confirmation recovery, absent tape, load and obstacles");
}
int main(void){
    adaptive_cruise_tests();
    cautious_speed_tests();
    turn_assist_tests();
    early_rejoin_tests();
    alignment_drive_tests();
    alignment_tests();
    encoder_error_tests();
    retry_tests();
    for(unsigned s=0;s<18;s++)run(s);
    init();tick(true,true,25,700,false);tick(true,true,100,700,false);assert(!a.detected);
    init();now+=300000;tick(false,false,0,700,false);assert(out.stop&&!strcmp(out.state,"AVOID_CONTROL_LATE"));
    init();a.phase=AVOID_WAIT;a.episode_us=now-240000000u;tick(true,true,100,700,false);assert(out.stop);
    motion_control_t m={0};assert(!motion_begin_distance(&m,701,enc,now));assert(!motion_begin_distance(&m,0,enc,now));
    assert(motion_begin_distance(&m,150,enc,now)&&m.slow_distance&&m.left.target==2024);
    puts("PASS debounce, late control, episode timeout and distance bounds");
}
