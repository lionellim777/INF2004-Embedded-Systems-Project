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
            assert(!out.stop&&a.phase==AVOID_FOLLOW&&cruise<=1000);
            if(i<60)assert(cruise<=600);
            if(cruise>600)assert(cruise-previous<=3.01f);
            if(i==299){assert(cruise==1000);boosted=true;}
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
    assert(a.offset_mm<=600);
    if(a.phase==AVOID_MOVE_OUT)assert(a.offset_mm<=600&&a.departure_step_mm<=100);
    if(a.phase==AVOID_WAIT||a.phase==AVOID_SCAN_INITIAL||a.phase==AVOID_CHECK_LEFT||a.phase==AVOID_SCAN_OUT||a.phase==AVOID_SCAN_SIDE||a.phase==AVOID_SCAN_FORWARD||a.phase==AVOID_SCAN_RETURN||a.phase==AVOID_CHECK_TAPE||a.phase==AVOID_ALIGN_CONFIRM||out.stop)
        assert(!out.left&&!out.right);
}
static void corridor_scan_tests(void){
    for(unsigned mode=0;mode<4;mode++){
        init();a.phase=AVOID_WAIT;a.next=AVOID_SCAN_OUT;a.phase_us=now;
        a.episode_us=now;a.outward_mm=a.departure_travel_mm=200;
        bool finished=false;
        for(unsigned i=0;i<1000&&!out.stop;i++){
            float cm=100;
            if((mode==0||mode==1)&&a.servo_angle==50)cm=mode?20:34.4f;
            if(mode==2&&a.servo_angle==90)cm=34.4f;
            if(mode==3&&a.servo_angle==125)cm=10;
            tick(i%12==0,true,cm,0,false);
            if(a.scan_serial){finished=true;break;}
        }
        assert(finished);
        if(!mode){assert(!out.stop&&a.phase==AVOID_WAIT&&a.next==AVOID_MOVE_OUT);assert(a.scan_nearest_angle==50);}
        else {assert(out.stop&&!strcmp(out.state,"AVOID_RIGHT_PATH_BLOCKED"));assert(a.scan_blocked_angle);}
    }
    puts("PASS angled 34.4cm side echo excluded; near/central/corridor obstacles still block");
}
static void side_angle_adjustment_test(void){
    init();a.phase=AVOID_WAIT;a.next=AVOID_SCAN_OUT;a.phase_us=now;
    a.episode_us=now;a.outward_mm=300;a.departure_deg=a.return_deg=60;
    a.departure_travel_mm=347;a.departure_forward_mm=173;
    a.plan.angle_deg=60;a.plan.travel_mm=347;a.plan.forward_mm=173;
    for(unsigned i=0;i<1000&&!a.scan_serial&&!out.stop;i++)
        tick(i%12==0,true,a.servo_angle==125?28.2f:110,0,false);
    assert(a.scan_serial==1&&!out.stop);
    assert(a.phase==AVOID_WAIT&&a.next==AVOID_TURN_CLEARANCE);
    assert(a.departure_deg==70&&a.clearance_adjustments==1);
    assert(a.departure_done_mm==0&&a.plan.travel_mm==a.departure_travel_mm);
    bool saw_turn=false;
    for(unsigned i=0;i<4000&&a.scan_serial<2&&!out.stop;i++){
        tick(i%12==0,true,110,0,false);
        if(a.phase==AVOID_TURN_CLEARANCE){
            saw_turn=true;
            assert(a.leg.left.target==1100*10/90);
            assert(a.leg.right.target==-1100*10/90);
        }
    }
    assert(saw_turn&&a.scan_serial==2&&!out.stop);
    assert(a.phase==AVOID_WAIT&&a.next==AVOID_MOVE_OUT);
    puts("PASS isolated side echo causes 10-degree turn and clear rescan before outward move");
}
static void original_heading_check_test(void){
    init();a.phase=AVOID_WAIT;a.next=AVOID_SCAN_FORWARD;a.phase_us=a.episode_us=now;
    a.angled_probe=true;a.offset_mm=300;a.return_deg=60;
    for(unsigned i=0;i<1000&&!a.scan_serial&&!out.stop;i++){
        a.no_return=true;tick(i%12==0,false,0,0,false);
    }
    assert(!out.stop&&a.scan_center_no_return==3);
    assert(a.phase==AVOID_WAIT&&a.next==AVOID_MOVE_ALONG);
    for(unsigned i=0;i<3000&&a.along_mm<200&&!out.stop;i++){
        a.no_return=true;tick(i%12==0,false,0,0,false);
        assert(a.scan_serial==1&&a.servo_angle==90);
    }
    assert(!out.stop&&a.along_mm>=200);
    puts("PASS clear original-heading scan continues straight without a return turn");
}
static void no_return_path_tests(void){
    for(unsigned mode=0;mode<2;mode++){
        init();a.phase=AVOID_WAIT;a.next=mode?AVOID_SCAN_RETURN:AVOID_SCAN_OUT;
        a.phase_us=a.episode_us=now;a.departure_deg=a.return_deg=60;
        a.departure_travel_mm=100;a.offset_mm=250;
        for(unsigned i=0;i<1000&&!a.scan_serial&&!out.stop;i++){
            a.no_return=true;tick(i%12==0,false,0,0,false);
        }
        assert(!out.stop&&a.scan_serial==1&&a.scan_center_no_return==3);
        assert(a.phase==AVOID_WAIT&&a.next==(mode?AVOID_RETURN_LINE:AVOID_MOVE_OUT));
    }
    init();a.phase=AVOID_MOVE_OUT;a.episode_us=now;a.departure_step_mm=100;
    a.departure_travel_mm=200;a.departure_deg=60;
    motion_begin_distance(&a.leg,100,enc,now);
    for(unsigned i=0;i<3;i++){
        a.no_return=true;tick(true,false,0,0,false);
        assert(!out.stop&&a.moving_invalid==0);
    }
    puts("PASS three completed centre no-returns permit checked path; completed no-return does not trip moving invalid guard");
}
static void approach_clearance_tests(void){
    /* 45cm is only a warning: stationary readings cannot start a bypass. */
    init();for(unsigned i=0;i<100;i++)tick(true,true,43,700,true);
    assert(a.approach_warning&&!a.approach_pending&&!a.detected);
    assert(a.line.drive.center_cruise_cps==450);
    /* Reproduce the near bottle disappearing into background/no-return.
     * Encoder travel must consume the retained distance, not forget it. */
    for(unsigned mode=0;mode<3;mode++){
        init();tick(true,true,34,700,false);
        assert(a.approach_warning&&!a.approach_pending&&!a.detected);
        for(unsigned i=0;i<1500&&!a.detected&&!out.stop;i++){
            tick(true,mode!=0,mode==2?34:125,700,false);
            if(a.approach_pending)assert(!out.left&&!out.right);
        }
        assert(!out.stop&&a.detected==1&&a.phase==AVOID_WAIT);
        assert(a.outward_mm==250&&a.approach_estimate_cm<=20);
        float mm=(left+right)/(2*13.491f);
        assert(mm>80&&mm<135); /* 140mm budget with conservative odometry margin */
    }
    /* Direct 20cm reading brakes on its FIRST sample, no driving debounce. */
    init();tick(true,true,20,700,false);
    assert(a.approach_pending&&!out.left&&!out.right&&!a.detected);
    tick(true,true,20,700,false);assert(a.detected==1&&a.phase==AVOID_WAIT);
    init();tick(true,true,10,700,false);
    assert(out.stop&&!a.detected&&!strcmp(out.state,"AVOID_APPROACH_TOO_CLOSE"));
    init();tick(true,true,20,700,false);tick(true,true,20,700,false);tick(true,true,10,700,false);
    assert(out.stop&&!out.left&&!out.right&&!strcmp(out.state,"AVOID_APPROACH_TOO_CLOSE"));
    /* Large search rotation cannot turn a remembered target into an invented
     * front clearance; stop with an explicit uncertainty reason instead. */
    init();tick(true,true,40,700,false);left=400;right=-400;
    tick(true,false,0,700,true);
    assert(out.stop&&!strcmp(out.state,"AVOID_APPROACH_HEADING_UNCERTAIN"));
    init();for(unsigned i=0;i<80;i++)tick(true,false,0,700,false);
    assert(!a.approach_warning&&!a.detected&&!out.stop);
    puts("PASS 45cm warning only, 450cps approach, 20cm immediate brake, retained travel budget, close/heading guards");
}
static void run(unsigned scenario){
    init();
    for(unsigned i=0;i<25000&&!out.stop&&!a.completed;i++){
        bool fresh=i%12==0,valid=true,stall=false;float cm=180;int center=(a.phase==AVOID_FOLLOW||a.phase==AVOID_VERIFY||a.phase==AVOID_CHECK_TAPE||a.phase==AVOID_TAPE_PROBE)?700:0;
        if(a.phase==AVOID_FOLLOW)cm=25;
        if(a.phase==AVOID_SCAN_INITIAL)cm=a.servo_angle<=100?20:100;
        if(a.phase==AVOID_SCAN_SIDE||a.phase==AVOID_CHECK_LEFT)cm=a.along_mm<=300?28:180;
        if(scenario==1&&a.phase==AVOID_MOVE_OUT)cm=10;
        if(scenario==2&&a.phase==AVOID_SCAN_FORWARD)cm=20;
        if(scenario==3&&a.phase==AVOID_MOVE_OUT)stall=true;
        if(scenario==4&&a.phase==AVOID_MOVE_OUT)fresh=false;
        if(scenario==5&&a.phase==AVOID_MOVE_ALONG)cm=10;
        if(scenario==6&&a.phase==AVOID_SCAN_FORWARD)cm=180; /* clear original heading needs no side observation */
        if(scenario==7&&a.phase==AVOID_MOVE_OUT)enc.left_invalid=2;
        if(scenario==17&&a.phase==AVOID_TURN_OUT&&motion_left_position(&a.leg,enc)<-400)enc.right_invalid=1;
        if(scenario==8&&a.phase==AVOID_SCAN_FORWARD&&a.offset_mm<450)cm=22;
        if(scenario==9&&a.phase==AVOID_SCAN_FORWARD&&a.along_mm>300)cm=22;
        if(scenario==10&&a.phase==AVOID_SCAN_FORWARD&&a.servo_angle==90)valid=false;
        if(scenario==11&&a.phase==AVOID_MOVE_OUT)cm=10;
        if(scenario==12&&a.phase==AVOID_SCAN_FORWARD&&a.offset_mm<340)cm=20;
        if(scenario>=18&&a.phase==AVOID_SCAN_FORWARD){
            if(scenario==18){valid=a.servo_angle!=90;a.no_return=!valid;}
            if(scenario==19)valid=a.servo_angle==125;
            if(scenario==20)valid=a.samples==0;
        }
        if(a.phase==AVOID_RETURN_LINE||a.phase==AVOID_MOVE_ALONG){
            float mm=(motion_left_position(&a.leg,enc)+motion_right_position(&a.leg,enc))/(2*13.491f);
            center=mm>35?700:0;
            if(scenario==13)center=0;
            if(scenario==9)center=0;
            if(scenario==14)center=700;
        }
        if(scenario==15&&a.phase==AVOID_CHECK_TAPE)center=0;
        if(scenario==16&&a.phase==AVOID_VERIFY)center=0;
        if(a.phase==AVOID_TURN_OUT)
            assert(a.leg.left.target==-1090*(int)a.departure_deg/90&&a.leg.right.target==1090*(int)a.departure_deg/90);
        if(a.phase==AVOID_TURN_ALONG)
            assert(a.leg.left.target==1100&&a.leg.right.target==-1100);
        tick(fresh,valid,cm,center,stall);
    }
    const char *reasons[]={NULL,"AVOID_PATH_OBSTACLE_STOP","AVOID_OBJECT_TOO_WIDE_OR_PATH_BLOCKED",
        "NO_ENCODER_PROGRESS_1S","AVOID_RANGE_STALE","AVOID_PATH_OBSTACLE_STOP",
        NULL,"AVOID_ENCODER_INVALID",NULL,
        "AVOID_RETURN_LINE_NOT_FOUND","AVOID_FORWARD_RANGE_UNKNOWN","AVOID_PATH_OBSTACLE_STOP",
        NULL,"AVOID_RETURN_LINE_NOT_FOUND",
        "AVOID_RETURN_LINE_NOT_FOUND","AVOID_RETURN_LINE_NOT_FOUND",NULL,NULL,NULL,"AVOID_FORWARD_RANGE_UNKNOWN","AVOID_FORWARD_RANGE_UNKNOWN"};
    if(scenario==16){assert(out.stop&&!a.completed);}
    else if(reasons[scenario]){
        if(!out.stop||strcmp(out.state,reasons[scenario])){fprintf(stderr,"scenario%u stopped=%d reason=%s phase=%s\n",scenario,out.stop,out.state,obstacle_bypass_phase(a.phase));assert(0);}
        assert(!a.completed);
    }else{
        if(out.stop||a.completed!=1){fprintf(stderr,"scenario%u completed=%u stopped=%d reason=%s phase=%s\n",scenario,a.completed,out.stop,out.state,obstacle_bypass_phase(a.phase));assert(0);}
        assert(a.phase==AVOID_FOLLOW&&a.detected==1&&(a.plan.edge_from_no_return||a.plan.left_edge_known));
        assert(seen_phases&(1u<<AVOID_VERIFY));
        if(scenario==18)assert(a.no_echo>0);
        if(scenario==8)assert(a.offset_mm>=450&&a.offset_mm<=550);
        if(scenario==10)assert(a.no_echo>0);
        if(scenario==17)assert(a.encoder_skips[1]==1&&enc.right_invalid==1&&(seen_phases&(1u<<AVOID_SCAN_FORWARD)));
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
        course_set_cruise_limit(&original,i?1000:600);
        course_center_level(&original,level);
        line_check_output_t expected=course_step(&original,input,enc,now);
        out=obstacle_bypass_guarded_step(&a,input,level,enc,now,true,true,80,NULL,false);
        assert(out.left==expected.left&&out.right==expected.right&&out.stop==expected.stop&&!strcmp(out.state,expected.state));
    }
    init();
    const char *err="AVOID_SONAR_ECHO_HIGH_BEFORE_TRIGGER";
    ir_roles_t ir={false,true,false};
    /* Fault while an existing line search is underway: preserve its origin,
     * direction and remaining budget; stationary waiting must not time it out. */
    course_center_level(&a.line,0);
    now+=10000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,true,80,NULL,false);
    assert(a.line.phase==COURSE_LOST);
    center_search_t search=a.line.center_search;
    now+=10000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,false,0,err,false);
    assert(!out.stop&&!out.left&&!out.right&&a.sonar_wait);
    uint64_t active=a.active_us;
    for(unsigned i=0;i<7;i++){
        now+=100000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,false,0,err,false);
        assert(!out.stop&&!out.left&&!out.right&&a.active_us==active);
        assert(!memcmp(&search,&a.line.center_search,sizeof(search)));
    }
    now+=100000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,true,80,NULL,false);
    assert(a.sonar_wait&&!out.left&&!out.right);
    now+=100000;out=obstacle_bypass_guarded_step(&a,ir,0,enc,now,true,true,80,NULL,false);
    assert(!out.stop&&!a.sonar_wait&&a.sonar_resumes==1&&a.line.phase==COURSE_LOST);
    assert(a.active_us==active);
    /* NO_ECHO is not sufficient to release a fault pause. */
    init();now+=10000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,false,0,err,false);
    for(unsigned i=0;i<10;i++){now+=100000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,false,0,NULL,false);assert(!out.left&&!out.right);}
    assert(out.stop&&!strcmp(out.state,"AVOID_SONAR_RECOVERY_FAILED"));
    /* Obstacle discovered on retry must enter bypass before any forward PWM. */
    init();now+=10000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,false,0,err,false);
    for(unsigned i=0;i<2;i++){now+=100000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,true,20,NULL,false);assert(!out.left&&!out.right);}
    assert(!out.stop&&a.approach_pending&&a.phase==AVOID_FOLLOW);
    now+=10000;out=obstacle_bypass_guarded_step(&a,ir,700,enc,now,true,true,20,NULL,false);
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
    const avoid_phase_t phases[]={AVOID_MOVE_ALONG,AVOID_RETURN_LINE};
    for(unsigned k=0;k<sizeof(phases)/sizeof(phases[0]);k++){
        init();a.phase=phases[k];a.episode_us=now;a.offset_mm=200;a.rejoin_armed=true;
        a.next=AVOID_TURN_LOOK;a.servo_angle=50;
        tick(true,true,180,700,false);
        assert(a.phase==AVOID_CHECK_TAPE&&a.servo_angle==90&&a.rejoin_attempts==1);
        for(unsigned i=0;i<25;i++)tick(true,true,180,700,false);
        assert(a.phase==AVOID_CHECK_TAPE&&!a.completed); /* ignore settling rays */
        for(unsigned i=0;i<450&&a.phase!=AVOID_VERIFY;i++)tick(true,true,180,700,false);
        assert(a.phase==AVOID_VERIFY&&!a.completed);
        for(unsigned i=0;i<35;i++)tick(true,true,180,700,true);
        assert(!a.completed); /* stationary black is not proof of following */
        for(unsigned i=0;i<400&&!a.completed&&!out.stop;i++)tick(true,true,180,700,false);
        assert(a.completed==1&&!out.stop);
    }
    /* Crossing the original tape during the outward diagonal is not rejoin. */
    init();a.phase=AVOID_MOVE_OUT;a.episode_us=now;a.rejoin_armed=true;
    a.departure_step_mm=100;a.departure_travel_mm=200;a.departure_deg=55;
    motion_begin_distance(&a.leg,100,enc,now);
    tick(true,true,180,700,false);
    assert(!out.stop&&a.phase==AVOID_MOVE_OUT&&a.rejoin_attempts==0);
    /* Right sensor can request checking even while centre is white. */
    init();a.phase=AVOID_MOVE_ALONG;a.offset_mm=200;a.episode_us=now;a.rejoin_armed=true;
    now+=10000;out=obstacle_bypass_step(&a,(ir_roles_t){false,false,true},0,enc,now,true,true,180);
    assert(a.phase==AVOID_CHECK_TAPE&&!out.left&&!out.right);
    /* Starting line is not an escape; barcode alone never triggers a rejoin. */
    init();a.phase=AVOID_TURN_OUT;a.episode_us=now;a.rejoin_armed=true;
    motion_begin(&a.leg,MOTION_RIGHT_90,enc,now);
    tick(true,true,180,700,false);assert(a.phase==AVOID_TURN_OUT);
    a.offset_mm=200;tick(true,true,180,0,false);assert(a.phase==AVOID_TURN_OUT);
    /* Blocked/vanished candidates restore the target; unknown range holds tape. */
    for(unsigned mode=0;mode<3;mode++){
        init();a.phase=AVOID_MOVE_ALONG;a.offset_mm=200;a.episode_us=now;a.rejoin_armed=true;
        motion_begin_distance(&a.leg,100,enc,now);encoder_sample_t origin=a.leg.start;
        int target=a.leg.left.target;
        tick(true,true,180,700,false);
        for(unsigned i=0;i<170&&a.phase==AVOID_CHECK_TAPE;i++)
            tick(i%12==0,mode!=2,mode==0?20:180,mode==1?0:700,false);
        if(mode==2){assert(out.stop&&!strcmp(out.state,"AVOID_REJOIN_RANGE_UNAVAILABLE"));continue;}
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
static void no_return_rejoin_tests(void){
    for(unsigned mode=0;mode<5;mode++){
        init();a.phase=AVOID_CHECK_TAPE;a.candidate_us=a.phase_us=a.episode_us=now;
        a.offset_mm=250;a.resume_phase=AVOID_MOVE_ALONG;
        motion_begin_distance(&a.leg,200,enc,now);
        for(unsigned i=0;i<20&&a.phase==AVOID_CHECK_TAPE&&!out.stop;i++){
            now+=100000;
            out=obstacle_bypass_guarded_step(&a,(ir_roles_t){false,true,true},700,enc,now,
                mode!=3,mode==2,mode==2?20:180,
                mode==4?"AVOID_SONAR_TOO_CLOSE_OR_BAD_PULSE":NULL,mode!=1);
        }
        if(mode==0){assert(a.phase==AVOID_TAPE_PROBE&&!out.stop);assert(!strcmp(a.tape_decision,"SPATIAL_INK_CHECK"));}
        else if(mode==2){assert(a.phase==AVOID_MOVE_ALONG&&a.rejoin_rejected==1);}
        else {assert(out.stop);assert(!strcmp(out.state,mode==4?"AVOID_SONAR_TOO_CLOSE_OR_BAD_PULSE":"AVOID_REJOIN_RANGE_UNAVAILABLE"));}
    }
    puts("PASS confirmed tape plus completed no-return enters rejoin; NO_RISE/stale/error/near obstacle cannot grant it");
}
static void turn_assist_tests(void){
    motion_control_t m={0};encoder_sample_t e={0};
    assert(motion_begin(&m,MOTION_LEFT_90,e,0));m.slow_turn=true;
    bool assist=false;
    for(unsigned i=1;i<=10;i++){
        motion_poll(&m,e,i*100000u);
        if(fabsf(m.left.throttle)>.24f)assist=true;
        assert(fabsf(m.left.throttle)<=.60001f&&fabsf(m.right.throttle)<=.60001f);
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
    /* Foam-like asymmetric load: right needs sustained >26.5% PWM.
     * Check both directions, turn completion, bounded lead and hard stalls. */
    for(unsigned mode=0;mode<3;mode++){
        e=(encoder_sample_t){0};l=r=0;
        assert(motion_begin(&m,mode==1?MOTION_LEFT_90:MOTION_RIGHT_90,e,0));m.slow_turn=true;
        bool sustained=false;unsigned powered=0;float peak_skew=0;
        for(unsigned i=1;i<=1200&&m.phase==MOTION_ACTIVE;i++){
            l+=m.left.throttle*2500*.01;
            double effort=fmaxf(0,fabsf(m.right.throttle)-.265f)*12000;
            if(mode!=2)r+=copysign(effort,m.right.throttle)*.01;
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            float skew=fabsf(fabsf((float)l)-fabsf((float)r));if(skew>peak_skew)peak_skew=skew;
            if(fabsf(m.right.throttle)>.265f){if(++powered>20)sustained=true;}else powered=0;
            assert(fabsf(m.left.throttle)<=.60001f&&fabsf(m.right.throttle)<=.60001f);
        }
        if(mode==2)assert(m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED);
        else assert(m.phase==MOTION_DONE&&sustained);
        assert(peak_skew<210); /* 120-count hold plus <=one 100ms update/coast */
    }
    /* The old 32% cap cannot move this loaded wheel. Exercise each wheel,
     * both turn directions, and retained diagnostics for a hard stall. */
    for(unsigned loaded=0;loaded<2;loaded++)for(unsigned dir=0;dir<2;dir++)for(unsigned old_cap=0;old_cap<2;old_cap++){
        e=(encoder_sample_t){0};l=r=0;
        assert(motion_begin(&m,dir?MOTION_LEFT_90:MOTION_RIGHT_90,e,0));m.slow_turn=true;
        bool boosted=false;
        for(unsigned i=1;i<=1200&&m.phase==MOTION_ACTIVE;i++){
            float pwm[2]={m.left.throttle,m.right.throttle};
            for(unsigned w=0;w<2;w++){
                float effort=pwm[w];
                if(old_cap&&fabsf(effort)>.32f)effort=copysignf(.32f,effort);
                float load=w==loaded?.35f:.15f;
                double step=copysignf(fmaxf(0,fabsf(effort)-load)*8000,effort)*.01;
                if(w==0)l+=step;else r+=step;
            }
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            if(fabsf(m.left.applied_pwm)>.32f||fabsf(m.right.applied_pwm)>.32f)boosted=true;
            assert(fabsf(m.left.throttle)<=.60001f&&fabsf(m.right.throttle)<=.60001f);
        }
        if(old_cap){assert(m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED);assert(m.stalled_wheels==(1u<<loaded));}
        else assert(m.phase==MOTION_DONE&&boosted);
    }
    e=(encoder_sample_t){0};assert(motion_begin(&m,MOTION_RIGHT_90,e,0));m.slow_turn=true;
    for(unsigned i=1;i<=10;i++)motion_poll(&m,e,i*100000u);
    assert(m.phase==MOTION_FAULT&&m.stalled_wheels==3);
    assert(m.left.no_progress_ms==1000&&m.right.no_progress_ms==1000);
    assert(m.left.applied_pwm>.32f&&m.right.applied_pwm<-.32f);
    assert(m.left.throttle==0&&m.right.throttle==0);
    /* Replay load appearing near the first run's 1057-count endpoint or
     * halfway through the second run. Old 45% fails; bounded 60% completes. */
    for(unsigned loaded=0;loaded<2;loaded++)for(unsigned dir=0;dir<2;dir++)for(unsigned late=0;late<2;late++)for(unsigned old=0;old<2;old++){
        e=(encoder_sample_t){0};l=r=0;
        assert(motion_begin(&m,dir?MOTION_LEFT_90:MOTION_RIGHT_90,e,0));m.slow_turn=true;
        m.left.target=dir?-1090:1100;m.right.target=-m.left.target;
        for(unsigned i=1;i<=1200&&m.phase==MOTION_ACTIVE;i++){
            float pwm[2]={m.left.throttle,m.right.throttle};
            for(unsigned w=0;w<2;w++){
                float effort=pwm[w];if(old&&fabsf(effort)>.45f)effort=copysignf(.45f,effort);
                double position=w?r:l;
                float load=(w==loaded&&fabs(position)>(late?1050:400))?.48f:.15f;
                double step=copysignf(fmaxf(0,fabsf(effort)-load)*8000,effort)*.01;
                if(w)r+=step;else l+=step;
            }
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            assert(fabsf(m.left.throttle)<=.60001f&&fabsf(m.right.throttle)<=.60001f);
        }
        if(old)assert(m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED&&m.stalled_wheels==(1u<<loaded));
        else assert(m.phase==MOTION_DONE);
    }
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
static void endpoint_assist_tests(void){
    /* Reproduce the recorded39-count shortfall with the other wheel done.
     * Each wheel must restart above21.1%, finish exactly, and a hard block
     * must still terminate at the existing one-second deadline. */
    for(unsigned w=0;w<2;w++)for(unsigned blocked=0;blocked<2;blocked++){
        motion_control_t m={0};encoder_sample_t e={0};
        assert(motion_begin_distance(&m,200,e,0));
        double l=m.left.target-(w==0?39:0),r=m.right.target-(w==1?39:0);
        bool assist=false;
        for(unsigned i=1;i<=500&&m.phase==MOTION_ACTIVE;i++){
            if(!blocked){
                if(w==0)l+=fmaxf(0,m.left.throttle-.24f)*8000*.01;
                else r+=fmaxf(0,m.right.throttle-.24f)*8000*.01;
            }
            e.left_raw=(uint32_t)-(int32_t)l;e.right_raw=(uint32_t)(int32_t)r;
            motion_poll(&m,e,i*10000u);
            if(m.left.throttle>.24f||m.right.throttle>.24f)assist=true;
            assert(m.left.throttle<=.32001f&&m.right.throttle<=.32001f);
            if(w==0)assert(m.right.throttle==0);else assert(m.left.throttle==0);
        }
        assert(assist);
        if(blocked)assert(m.phase==MOTION_FAULT&&m.fault==MOTION_STALLED);
        else assert(m.phase==MOTION_DONE);
    }
    puts("PASS 39-count endpoint shortfall: each wheel restarts, finished wheel held, hard stall retained");
}
static void spatial_ink_tests(void){
    for(unsigned mode=0;mode<4;mode++){
        init();a.phase=AVOID_MOVE_ALONG;a.episode_us=now;a.offset_mm=250;
        a.rejoin_armed=true;motion_begin_distance(&a.leg,200,enc,now);
        int target=a.leg.left.target;encoder_sample_t start=a.leg.start;
        tick(true,true,180,700,false);
        while(a.phase==AVOID_CHECK_TAPE)tick(true,true,180,700,false);
        assert(a.phase==AVOID_TAPE_PROBE);
        for(unsigned i=0;i<600&&!out.stop&&a.phase==AVOID_TAPE_PROBE;i++){
            double mm=(obstacle_encoder_delta(a.tape_origin.left_raw,enc.left_raw)+obstacle_encoder_delta(enc.right_raw,a.tape_origin.right_raw))/(2*13.491);
            int center=700;
            if(mode==0)center=mm<3?700:0; /* half narrow bar */
            if(mode==1)center=mm<18||mm>=30?700:0; /* full wide bar + gap + next ink */
            if(mode==2)center=mm<18?700:0; /* single route crossing */
            tick(true,true,180,center,false);
        }
        assert(!out.stop);
        if(mode<2){
            assert(a.phase==AVOID_MOVE_ALONG&&a.rejoin_rejected==1&&!a.completed);
            assert(a.leg.left.target==target&&!memcmp(&start,&a.leg.start,sizeof(start)));
            assert(!strcmp(a.tape_decision,mode==0?"INK_TOO_NARROW":"REPEATED_INK"));
        }else if(mode==2)assert(a.phase==AVOID_WAIT&&a.next==AVOID_AXLE_ADVANCE);
        else assert(a.phase==AVOID_VERIFY&&!a.completed);
    }
    /* Black during a pivot is never accepted as a route candidate. */
    init();a.phase=AVOID_TURN_LOOK;a.offset_mm=250;a.episode_us=now;a.rejoin_armed=true;
    motion_begin(&a.leg,MOTION_LEFT_90,enc,now);a.leg.slow_turn=true;
    tick(true,true,180,700,false);assert(a.phase==AVOID_TURN_LOOK&&!a.rejoin_attempts);
    puts("PASS spatial ink check: narrow/repeated ink rejected with saved leg; single crossing/continuous tape qualified; pivot ink ignored");
}
static void straight_seek_alignment_test(void){
    for(unsigned transient=0;transient<2;transient++){
        init();a.phase=AVOID_ALIGN_TURN;a.episode_us=now;
        a.angled_probe=true;a.return_deg=65;a.align_direction=1;a.align_armed=true;
        a.align_origin=obstacle_encoder_origin(enc);
        motion_begin(&a.leg,MOTION_RIGHT_90,enc,now);a.leg.slow_turn=true;
        left=1222;right=-1222; /* approximately 110 degrees, beyond old A window */
        tick(true,true,180,700,false);
        assert(a.phase==AVOID_ALIGN_CONFIRM&&!out.left&&!out.right);
        if(transient){
            tick(true,true,180,0,false);
            assert(a.phase==AVOID_ALIGN_TURN&&a.align_rejected==1);
        }else{
            for(unsigned i=0;i<40&&a.phase==AVOID_ALIGN_CONFIRM;i++)tick(true,true,180,700,false);
            assert(a.phase==AVOID_VERIFY&&!out.stop);
        }
    }
    puts("PASS straight-seek tape at 110deg pauses for confirmation; transient ink resumes alignment");
}
static void alignment_tests(void){
    /* Geometric plant: sensor 120 mm ahead of axle; tape at x=0 extending
     * along y. A perpendicular approach cannot succeed by a small pivot at
     * the detection point x=-120. Count-to-yaw uses 1000 counts per 90 deg.
     * This validates geometry/control logic, not tyre slip or physical optics. */
    for(unsigned mode=0;mode<14;mode++){
        init();a.phase=AVOID_CHECK_TAPE;a.candidate_us=a.phase_us=a.episode_us=now;
        a.offset_mm=200;a.resume_phase=AVOID_MOVE_ALONG;a.rejoin_attempts=1;
        /* A small odometry bias still prefers the left path toward the approach. */
        a.episode_origin.left_raw=(uint32_t)-600;
        if(mode==11||mode==13)a.episode_origin.left_raw=2000; /* positive approach bias must still choose left */
        if(mode==12)a.episode_origin.left_raw=(uint32_t)-2000; /* negative approach bias must still choose left */
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
            if((mode==2||mode==13)&&a.phase==AVOID_ALIGN_TURN&&!a.align_reversed)center=0;
            double degrees=heading*180/3.14159265359;
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
            a.no_return=mode==10;
            out=obstacle_bypass_step(&a,(ir_roles_t){false,center>=250,false},center,enc,now,fresh,mode!=10,cm);
            if(mode==5&&a.phase==AVOID_AXLE_ADVANCE)a.leg.alignment_drive=false;
            visited|=1u<<a.phase;
            if(a.phase==AVOID_ALIGN_TURN&&!a.align_reversed)assert(a.align_direction==-1);
            if(a.phase==AVOID_ALIGN_CONFIRM){
                assert(!out.left&&!out.right);
                assert(fabs(degrees)>=59&&fabs(degrees)<=116);
            }
        }
        assert(saw_lost&&(visited&(1u<<AVOID_AXLE_ADVANCE)));
        if(mode==0||mode==2||mode==6||mode==7||mode==8||mode==10||mode==11||mode==12||mode==13){
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
    puts("PASS 14 perpendicular geometry cases: left-first regardless of odometry bias, both sweeps, early false ink, rejected confirmation recovery, absent tape, load and obstacles");
}
static void planned_detour_tests(void){
    float narrow[OBSTACLE_PLAN_RAYS]={20,100,100,100,100,100,100,100};
    obstacle_ray_counts_t narrow_counts[OBSTACLE_PLAN_RAYS]={{.near=3},{.far=3}};
    obstacle_plan_t p=obstacle_plan(narrow,narrow_counts);
    assert(p.left_edge_known&&p.angle_deg==50&&p.edge_servo_deg==90&&p.clear_servo_deg==95);
    assert(p.lateral_mm>=220&&p.lateral_mm<=225&&p.travel_mm>=285&&p.travel_mm<=295);
    float wide[OBSTACLE_PLAN_RAYS]={20,20,20,20,20,100,100,100};
    obstacle_ray_counts_t wide_counts[OBSTACLE_PLAN_RAYS]={
        {.near=3},{.near=3},{.near=3},{.near=3},{.near=3},{.far=3},{.far=3},{.far=3}
    };
    obstacle_plan_t w=obstacle_plan(wide,wide_counts);
    assert(w.left_edge_known&&w.angle_deg>p.angle_deg&&w.lateral_mm>p.lateral_mm);
    for(unsigned i=0;i<OBSTACLE_PLAN_RAYS;i++)wide_counts[i]=(obstacle_ray_counts_t){.near=3};
    w=obstacle_plan(wide,wide_counts);assert(!w.left_edge_known&&!w.edge_from_no_return);
    narrow_counts[1].far=1;w=obstacle_plan(narrow,narrow_counts);assert(!w.left_edge_known&&!w.edge_from_no_return);
    narrow_counts[1].far=0;w=obstacle_plan(narrow,narrow_counts);assert(!w.left_edge_known&&!w.edge_from_no_return);
    narrow_counts[1].no_return=2;w=obstacle_plan(narrow,narrow_counts);assert(!w.edge_from_no_return);
    narrow_counts[1].no_return=3;w=obstacle_plan(narrow,narrow_counts);
    assert(!w.left_edge_known&&w.edge_from_no_return&&w.angle_deg>=45&&w.travel_mm>200);
    narrow_counts[0].near=0;w=obstacle_plan(narrow,narrow_counts);assert(!w.edge_from_no_return);
    /* After the first angled departure, face the original route heading,
     * repeat A once when the forward scan is blocked, then seek the tape. */
    init();bool planned=false,saw_forward=false,saw_repeat=false,saw_return=false;
    for(unsigned i=0;i<25000&&!out.stop&&!a.completed;i++){
        float cm=a.phase==AVOID_FOLLOW?20:180;
        if(a.phase==AVOID_SCAN_INITIAL)cm=a.servo_angle<=100?20:100;
        if(a.phase==AVOID_SCAN_FORWARD&&a.offset_mm<340)cm=20;
        int center=(a.phase==AVOID_FOLLOW||a.phase==AVOID_VERIFY||a.phase==AVOID_CHECK_TAPE||a.phase==AVOID_TAPE_PROBE)?700:0;
        if(a.phase==AVOID_MOVE_ALONG)center=((motion_left_position(&a.leg,enc)+motion_right_position(&a.leg,enc))/(2*13.491f))>35?700:0;
        if(a.phase==AVOID_TURN_OUT){planned=true;if(a.offset_mm)saw_repeat=true;assert(a.departure_deg==55&&a.leg.left.target==-666&&a.leg.right.target==666);}
        if(a.phase==AVOID_TURN_FORWARD){
            if(!saw_forward)assert(a.departure_done_mm==100&&a.offset_mm>=80&&a.offset_mm<=100);
            saw_forward=true;assert(a.leg.left.target==672&&a.leg.right.target==-672);
        }
        assert(a.phase!=AVOID_SCAN_OUT&&a.phase!=AVOID_SCAN_SIDE);
        assert(a.phase!=AVOID_TURN_TO_LINE&&a.phase!=AVOID_TURN_LOOK);
        if(a.phase==AVOID_MOVE_ALONG)saw_return=true;
        tick(i%12==0,true,cm,center,false);
    }
    if(out.stop||!a.completed)printf("planned route ended phase=%s reason=%s along=%u\n",obstacle_bypass_phase(a.phase),out.state,a.along_mm);
    assert(!out.stop&&a.completed==1&&planned&&saw_forward&&saw_repeat&&saw_return);
    /* No-return, a single far echo, or a mixed near/far ray do not
     * authorize the extra rear-clearance step. Two/three far samples do. */
    for(unsigned mode=0;mode<4;mode++){
        init();a.phase=AVOID_WAIT;a.next=AVOID_CHECK_LEFT;a.episode_us=now;a.phase_us=now;a.offset_mm=250;a.side_seen=true;
        for(unsigned i=0;i<150&&!out.stop;i++){
            a.no_return=true;
            bool valid=mode!=0&&(mode!=1||a.left_samples==0);
            float cm=mode==2&&a.left_samples==0?20:180;
            tick(i%12==0,valid,cm,0,false);
            if(a.phase==AVOID_WAIT&&a.next!=AVOID_CHECK_LEFT)break;
        }
        assert(!out.stop&&a.phase==AVOID_WAIT&&a.next==(mode==3?AVOID_CLEAR_TAIL:AVOID_TURN_LOOK));
    }
    puts("PASS 5deg edge scan, angled checked legs, original-heading forward check, repeat and return");
}
static void unknown_scan_tests(void){
    for(unsigned mode=0;mode<4;mode++){
        init();a.phase=AVOID_WAIT;a.episode_us=a.phase_us=now;
        a.next=mode==0?AVOID_SCAN_INITIAL:mode==1?AVOID_SCAN_OUT:
            mode==2?AVOID_SCAN_FORWARD:AVOID_SCAN_RETURN;
        a.departure_travel_mm=a.outward_mm=100;a.offset_mm=mode==0?300:250;a.return_deg=55;
        for(unsigned i=0;i<1200&&!out.stop;i++){
            bool valid=!(mode==0?a.servo_angle==105:a.servo_angle==90);
            float cm=mode==0&&a.servo_angle<=100?20:100;
            tick(i%12==0,valid,cm,0,false);
        }
        const char *reason=mode==0?"AVOID_LEFT_EDGE_UNKNOWN":mode==1?"AVOID_OUTWARD_RANGE_UNKNOWN":
            mode==2?"AVOID_FORWARD_RANGE_UNKNOWN":"AVOID_RETURN_RANGE_UNKNOWN";
        assert(out.stop&&!strcmp(out.state,reason));
    }
    init();a.phase=AVOID_MOVE_OUT;a.episode_us=now;a.departure_step_mm=100;
    a.departure_travel_mm=200;a.departure_deg=55;
    motion_begin_distance(&a.leg,100,enc,now);
    for(unsigned i=0;i<3&&!out.stop;i++)tick(true,false,0,0,false);
    assert(out.stop&&!strcmp(out.state,"AVOID_MOVING_RANGE_UNKNOWN"));
    puts("PASS missing edge, scan clearance or moving range stops unverified travel");
}
static void edge_shift_tests(void){
    init();a.phase=AVOID_WAIT;a.next=AVOID_SCAN_INITIAL;
    a.phase_us=a.episode_us=now;a.approach_min_cm=20;
    for(unsigned i=0;i<1000&&!out.stop;i++)tick(i%12==0,true,20,0,false);
    assert(out.stop&&!strcmp(out.state,"AVOID_LEFT_EDGE_UNKNOWN"));
    assert(a.edge_shift_count==0&&a.offset_mm==0&&a.scan_serial==1);
    puts("PASS inconclusive initial edge stops without extra scan manoeuvres");
}
static void echo_edge_tests(void){
    init();a.phase=AVOID_WAIT;a.next=AVOID_SCAN_INITIAL;
    a.phase_us=a.episode_us=now;a.approach_min_cm=20;
    bool planned=false;
    for(unsigned i=0;i<3000&&!out.stop;i++){
        bool initial=a.phase==AVOID_SCAN_INITIAL;
        bool valid=!(initial&&a.servo_angle>=125);
        a.no_return=initial&&!valid;
        float cm=initial?20:100;
        tick(i%12==0,valid,cm,0,false);
        if(a.plan.edge_from_no_return)planned=true;
        if(a.phase==AVOID_MOVE_OUT)break;
    }
    assert(planned&&a.plan.edge_from_no_return&&!a.plan.left_edge_known);
    assert(a.departure_deg>=45&&a.edge_shift_count==0&&a.offset_mm==0);
    assert(!out.stop&&a.phase==AVOID_MOVE_OUT&&a.departure_step_mm==100&&a.scan_serial==1);
    puts("PASS initial no-return edge plan leads directly to 100mm angled move without another sweep");
}

int main(void){
    planned_detour_tests();unknown_scan_tests();edge_shift_tests();echo_edge_tests();
    endpoint_assist_tests();spatial_ink_tests();
    corridor_scan_tests();side_angle_adjustment_test();original_heading_check_test();no_return_path_tests();
    approach_clearance_tests();
    adaptive_cruise_tests();
    cautious_speed_tests();
    no_return_rejoin_tests();turn_assist_tests();
    early_rejoin_tests();
    alignment_drive_tests();
    alignment_tests();straight_seek_alignment_test();
    encoder_error_tests();
    retry_tests();
    for(unsigned s=0;s<21;s++)run(s);
    init();tick(true,true,25,700,false);tick(true,true,100,700,false);assert(!a.detected);
    init();now+=300000;tick(false,false,0,700,false);assert(out.stop&&!strcmp(out.state,"AVOID_CONTROL_LATE"));
    init();a.phase=AVOID_WAIT;a.episode_us=now-240000000u;tick(true,true,100,700,false);assert(out.stop);
    motion_control_t m={0};assert(!motion_begin_distance(&m,701,enc,now));assert(!motion_begin_distance(&m,0,enc,now));
    assert(motion_begin_distance(&m,150,enc,now)&&m.slow_distance&&m.left.target==2024);
    puts("PASS debounce, late control, episode timeout and distance bounds");
}
