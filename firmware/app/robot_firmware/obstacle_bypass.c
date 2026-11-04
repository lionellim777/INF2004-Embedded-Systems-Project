#include "obstacle_bypass.h"
#include <math.h>
#include <string.h>

/* Initial calibration values, not a guarantee of chassis clearance. Geometry
 * uses a parallel return as fallback; confirmed tape can interrupt the detour. Unknown object
 * length is inspected in steps; lateral extension is limited to 600 mm. */
enum { SETTLE_US=300000, RANGE_STALE_US=400000, EPISODE_US=240000000,
       STEP_MM=100, EDGE_SHIFT_MM=60, MAX_EDGE_SHIFT_MM=300,
       ALONG_STEP_MM=200, MAX_ALONG_MM=1200, MAX_OFFSET_MM=600, MIN_ALONG_MM=400,
       SENSOR_LEAD_MM=120, MAX_EPISODES=5, INITIAL_OFFSET_MM=250,
       WARNING_CM=45, APPROACH_CM=20, MIN_PIVOT_CM=12, APPROACH_CONFIRM_US=600000 };
static const unsigned angles[]={50,70,90,110,125};
/* Provisional bypass-only trims from the 2 Nov overhead video. Encoder travel
 * is not yaw feedback: surface slip and camera perspective limit accuracy.
 * Keep standalone calibration and the tape-guided alignment sweep unchanged. */
enum { BYPASS_RIGHT_COUNTS=1100, BYPASS_LEFT_COUNTS=1090 };
static const float counts_per_mm=2543.0f/(3.14159265f*60.0f);

const char *obstacle_bypass_phase(avoid_phase_t p)
{
    const char *names[]={"FOLLOW","SETTLE","TURN_RIGHT_OUT","SCAN_RIGHT_PATH","MOVE_OUT",
        "FACE_FORWARD","SCAN_FORWARD","MOVE_ALONG","FACE_OBJECT","SCAN_OBJECT_SIDE",
        "FACE_ALONG","SEEK_RETURN_LINE","VERIFY_LINE","CHECK_FOUND_TAPE",
        "PROBE_TAPE_HEADING","ADVANCE_AXLE_TO_TAPE","ALIGN_WITH_TAPE","CONFIRM_TAPE_HEADING","SCAN_OBJECT_EDGE","CHECK_LEFT_EDGE","CLEAR_REAR_MARGIN",
        "TURN_TO_TRACK","SCAN_TO_TRACK","TURN_FURTHER_RIGHT","TURN_TOWARD_LINE","FAILED"};
    return p<=AVOID_FAILED?names[p]:"INVALID";
}
static line_check_output_t quiet(obstacle_bypass_t *a)
{return (line_check_output_t){0,0,false,obstacle_bypass_phase(a->phase)};}
static line_check_output_t fail(obstacle_bypass_t *a,const char *why)
{a->phase=AVOID_FAILED;a->fault=why;a->servo_angle=90;motion_abort(&a->leg);return (line_check_output_t){0,0,true,why};}
/* A diagonal quadrature transition has no known direction: the ISR already
 * discards it. Do not invent counts. Permit at most two isolated events per
 * wheel for the entire run, never a burst or two within 250 ms. Raw counters
 * remain available to diagnostics; only downstream fault metadata is rebased. */
static bool encoder_health_ok(obstacle_bypass_t *a,encoder_sample_t enc,uint64_t now)
{
    const uint32_t values[]={enc.left_invalid,enc.right_invalid};
    for(unsigned i=0;i<2;i++){
        uint32_t extra=values[i]-a->encoder_seen[i];
        if(!extra)continue;
        if(extra!=1||a->encoder_skips[i]>=2||
           (a->encoder_skips[i]&&now-a->encoder_error_us[i]<250000))return false;
        a->encoder_seen[i]=values[i];a->encoder_error_us[i]=now;a->encoder_skips[i]++;
    }
    return true;
}
static bool scan_phase(avoid_phase_t p)
{return p==AVOID_SCAN_INITIAL||p==AVOID_SCAN_OUT||p==AVOID_SCAN_FORWARD||p==AVOID_SCAN_SIDE||p==AVOID_SCAN_RETURN;}
static bool forward_phase(avoid_phase_t p)
{return p==AVOID_MOVE_OUT||p==AVOID_MOVE_ALONG||p==AVOID_RETURN_LINE||p==AVOID_TAPE_PROBE||p==AVOID_AXLE_ADVANCE||p==AVOID_CLEAR_TAIL;}
static void wait_for(obstacle_bypass_t *a,avoid_phase_t next,uint64_t now)
{
    a->phase=AVOID_WAIT;a->next=next;a->phase_us=now;a->servo_angle=90;
    a->close_count=a->moving_invalid=0;a->range_us=0;a->hit_us=0;
    a->fast_clear=0;
}
static void enter(obstacle_bypass_t *a,avoid_phase_t p,encoder_sample_t enc,uint64_t now)
{
    a->phase=p;a->phase_us=now;a->hit_us=0;a->close_count=0;
    motion_init(&a->leg);
    if(p==AVOID_CHECK_LEFT){a->servo_angle=125;a->left_samples=a->left_far=0;a->left_near=false;return;}
    if(scan_phase(p)){
        if(p==AVOID_SCAN_INITIAL){for(unsigned i=0;i<OBSTACLE_PLAN_RAYS;i++){
            a->plan_cm[i]=401;a->plan_near[i]=a->plan_far[i]=a->plan_no_return[i]=0;
        }}
        a->scan_index=a->samples=a->scan_no_echo=a->scan_center_clear=a->scan_center_no_return=0;a->scan_min_cm=401;
        a->scan_ray_far=a->scan_far_rays=a->scan_far_samples=0;
        a->scan_nearest_angle=a->scan_blocked_angle=0;
        a->scan_blocked=a->scan_core_blocked=false;
        a->servo_angle=p==AVOID_SCAN_INITIAL?obstacle_plan_angles[0]:angles[0];return;
    }
    a->servo_angle=90;
    motion_kind_t turn=MOTION_NONE;
    if(p==AVOID_TURN_OUT||p==AVOID_TURN_ALONG)turn=MOTION_RIGHT_90;
    if(p==AVOID_TURN_FORWARD||p==AVOID_TURN_LOOK||p==AVOID_TURN_TO_LINE)turn=MOTION_LEFT_90;
    if(p==AVOID_TURN_RETURN||p==AVOID_TURN_CLEARANCE)turn=MOTION_RIGHT_90;
    if(p==AVOID_ALIGN_TURN)turn=a->align_direction>0?MOTION_RIGHT_90:MOTION_LEFT_90;
    if(turn!=MOTION_NONE){
        motion_begin(&a->leg,turn,enc,now);a->leg.slow_turn=true;
        if(turn==MOTION_RIGHT_90){a->leg.left.target=BYPASS_RIGHT_COUNTS;a->leg.right.target=-BYPASS_RIGHT_COUNTS;}
        else {a->leg.left.target=-BYPASS_LEFT_COUNTS;a->leg.right.target=BYPASS_LEFT_COUNTS;}
        if(p==AVOID_TURN_OUT||p==AVOID_TURN_FORWARD){
            unsigned degrees=a->departure_deg?a->departure_deg:90;
            a->leg.left.target=a->leg.left.target*(int)degrees/90;
            a->leg.right.target=a->leg.right.target*(int)degrees/90;
        }
        if(p==AVOID_TURN_RETURN){
            a->leg.left.target=a->leg.left.target*(int)(90-a->return_deg)/90;
            a->leg.right.target=a->leg.right.target*(int)(90-a->return_deg)/90;
        }
        if(p==AVOID_TURN_TO_LINE){
            a->leg.left.target=a->leg.left.target*(int)a->return_deg/90;
            a->leg.right.target=a->leg.right.target*(int)a->return_deg/90;
        }
        if(p==AVOID_TURN_CLEARANCE){
            a->leg.left.target=a->leg.left.target*10/90;
            a->leg.right.target=a->leg.right.target*10/90;
        }
        if(p==AVOID_ALIGN_TURN){
            if(!a->align_reversed)a->align_origin.start=enc;
            a->align_hit_us=0;a->align_armed=true;
            /* Operator-selected right-first rejoin. The opposite sweep is
             * only a bounded fallback if the right sweep finds no tape. */
            int degrees=a->align_reversed?220:110;
            a->leg.left.target=a->align_direction*1000*degrees/90;
            a->leg.right.target=-a->leg.left.target;
        }
    }
    else if(forward_phase(p)){
        unsigned mm=p==AVOID_MOVE_OUT?a->departure_step_mm:p==AVOID_MOVE_ALONG?(a->angled_probe?STEP_MM:ALONG_STEP_MM):p==AVOID_CLEAR_TAIL?100:
            p==AVOID_TAPE_PROBE?60:p==AVOID_AXLE_ADVANCE?a->axle_mm:
            (unsigned)ceilf((a->offset_mm-SENSOR_LEAD_MM+60)/sinf((a->return_deg?a->return_deg:90)*0.01745329252f));
        motion_begin_distance(&a->leg,mm,enc,now);
        a->leg.alignment_drive=p==AVOID_TAPE_PROBE||p==AVOID_AXLE_ADVANCE;
    }else if(p==AVOID_VERIFY){
        course_begin(&a->line,(course_config_t){'s','s',SENSOR_LEAD_MM},enc,now);
        course_set_center_reference(&a->line,a->reference);
        a->line.recovery_limit_counts=240;
    }
}
void obstacle_bypass_begin(obstacle_bypass_t *a,encoder_sample_t enc,uint64_t now,int32_t reference)
{
    memset(a,0,sizeof(*a));a->phase=AVOID_FOLLOW;a->servo_angle=90;a->last_us=now;
    a->origin.start=enc;a->reference=reference;a->wall_us=a->active_us=now;
    a->encoder_seen[0]=enc.left_invalid;a->encoder_seen[1]=enc.right_invalid;
    course_begin(&a->line,(course_config_t){'s','s',SENSOR_LEAD_MM},enc,now);
    course_set_center_reference(&a->line,reference);
}
static int32_t mean_mm(const motion_control_t *m,encoder_sample_t enc)
{return (int32_t)(((int64_t)motion_left_position(m,enc)+motion_right_position(m,enc))/(2*counts_per_mm));}
/* Pause a detour without discarding its encoder target. On rejection, wait
 * again for the restored sonar direction and restart PI from zero output. */
static void resume_detour(obstacle_bypass_t *a,encoder_sample_t enc,uint64_t now)
{
    uint64_t pause=now-a->candidate_us;
    if(a->phase==AVOID_TAPE_PROBE)a->leg=a->saved_leg;
    a->phase=a->resume_phase;a->phase_us=now;a->servo_angle=a->resume_servo;
    a->rejoin_rejected++;a->range_us=0;a->close_count=0;
    a->leg.start_us+=pause;a->leg.control_us=now;
    a->leg.left.last_progress_us=a->leg.right.last_progress_us=now;
    a->leg.left.previous=motion_left_position(&a->leg,enc);
    a->leg.right.previous=motion_right_position(&a->leg,enc);
    a->leg.left.integral=a->leg.right.integral=0;
    a->leg.left.throttle=a->leg.right.throttle=0;
}
static bool may_find_tape(avoid_phase_t p)
{
    /* Only moving straight legs can be paused and resumed after a spatial
     * ink test. Black glimpses during chassis/sonar turns are not route proof. */
    return p==AVOID_MOVE_ALONG||p==AVOID_CLEAR_TAIL||p==AVOID_RETURN_LINE;
}
/* Encoder-estimated heading relative to the axle-centred crossing. Use both
 * wheels and retain the origin across the opposite sweep. This is a geometric
 * gate, not proof that black ink is route tape or that tyres did not slip. */
static int32_t alignment_heading(const obstacle_bypass_t *a,encoder_sample_t enc)
{
    int64_t differential=(int64_t)motion_left_position(&a->align_origin,enc)-
        motion_right_position(&a->align_origin,enc);
    return (int32_t)(differential*90/2000);
}
static int32_t route_heading(const obstacle_bypass_t *a,encoder_sample_t enc)
{
    int64_t differential=(int64_t)motion_left_position(&a->episode_origin,enc)-
        motion_right_position(&a->episode_origin,enc);
    int32_t degrees=(int32_t)(differential*90/2000)%360;
    if(degrees>180)degrees-=360;
    if(degrees< -180)degrees+=360;
    return degrees;
}
static bool route_direction_ok(const obstacle_bypass_t *a,encoder_sample_t enc)
{
    /* Odometry is a prior, not compass truth. Allow bends, reject a heading
     * that points predominantly back toward the approach/bottle. */
    int h=route_heading(a,enc);return h>=-120&&h<=120;
}
static bool alignment_window(const obstacle_bypass_t *a,encoder_sample_t enc)
{
    int32_t heading=alignment_heading(a,enc)*a->align_direction;
    /* Straight tape seeking no longer approaches at the diagonal angle A.
     * Inspect the bounded sweep instead of using A as the tape bearing. */
    if(a->angled_probe)return heading>=25&&heading<=115;
    int expected=a->return_deg?a->return_deg:90;
    int lower=expected-20>25?expected-20:25;
    int upper=expected+25<115?expected+25:115;
    return heading>=lower&&heading<=upper;
}
static void resume_alignment(obstacle_bypass_t *a,encoder_sample_t enc,uint64_t now)
{
    /* Keep the remaining target and original sweep origin; a false black
     * glimpse must not restart a full turn or consume the stopped stall timer. */
    a->leg.start_us+=now-a->phase_us;a->leg.control_us=now;
    a->leg.left.last_progress_us=a->leg.right.last_progress_us=now;
    a->leg.left.previous=motion_left_position(&a->leg,enc);
    a->leg.right.previous=motion_right_position(&a->leg,enc);
    a->leg.left.integral=a->leg.right.integral=0;
    a->leg.left.throttle=a->leg.right.throttle=0;
    a->phase=AVOID_ALIGN_TURN;a->phase_us=now;
    a->align_hit_us=0;a->align_armed=false;a->align_rejected++;
}
line_check_output_t obstacle_bypass_step(obstacle_bypass_t *a,ir_roles_t ir,
    int32_t center,encoder_sample_t enc,uint64_t now,bool fresh,bool valid,float cm)
{
    if(a->phase==AVOID_FAILED)return fail(a,a->fault);
    if(now<a->last_us||now-a->last_us>250000)return fail(a,"AVOID_CONTROL_LATE");
    a->last_us=now;
    if(!encoder_health_ok(a,enc,now))return fail(a,"AVOID_ENCODER_INVALID");
    enc.left_invalid=a->origin.start.left_invalid;
    enc.right_invalid=a->origin.start.right_invalid;
    int32_t l=motion_left_position(&a->origin,enc),r=motion_right_position(&a->origin,enc);
    if(l<=-COURSE_LIMIT_COUNTS||l>=COURSE_LIMIT_COUNTS||r<=-COURSE_LIMIT_COUNTS||r>=COURSE_LIMIT_COUNTS)
        return fail(a,"AVOID_RUN_TRAVEL_LIMIT");
    if(a->phase!=AVOID_FOLLOW&&now-a->episode_us>=EPISODE_US)return fail(a,"AVOID_EPISODE_TIMEOUT");
    if(fresh){
        a->range_us=now;valid=valid&&isfinite(cm)&&cm>=2&&cm<=400;a->echo_valid=valid;
        a->fast_clear=valid&&cm>60?(a->fast_clear<2?a->fast_clear+1:2):0;
        if(!valid)a->no_echo++;
    }
    /* This mode never feeds GP26 into course navigation or line recovery. */
    ir.barcode=false;
    if(a->phase==AVOID_CHECK_TAPE){
        /* Side-facing measurements do not establish forward clearance. */
        if(now-a->candidate_us<SETTLE_US)return quiet(a);
        bool tape=center>=a->reference*70/100;
        if(!tape||(fresh&&valid&&cm<=30)){
            a->tape_decision=!tape?"TAPE_DISAPPEARED":"NEAR_OBSTACLE";
            resume_detour(a,enc,now);return quiet(a);
        }
        if(fresh){
            bool observed=(valid&&cm>30)||(!valid&&a->no_return);
            a->rejoin_good=observed?a->rejoin_good+1:0;
            a->tape_decision=valid?"FAR_ECHO":a->no_return?"NO_RETURN_MONITORED":"NO_RANGE";
        }
        if(a->rejoin_good>=2){
            a->saved_leg=a->leg;
            motion_abort(&a->leg);a->tape_origin.start=enc;a->align_reversed=false;
            a->route_heading_deg=route_heading(a,enc);
            a->align_direction=1; /* explicit operator right-first preference */
            a->probe_black=true;a->probe_gap=false;a->probe_first_black_mm=0;
            a->tape_decision="SPATIAL_INK_CHECK";
            enter(a,AVOID_TAPE_PROBE,enc,now);
            return quiet(a);
        }
        if(now-a->candidate_us>=1500000){a->tape_decision="NO_FRESH_RANGE";return fail(a,"AVOID_REJOIN_RANGE_UNAVAILABLE");}
        return quiet(a);
    }
    if(may_find_tape(a->phase)&&!(a->phase==AVOID_WAIT&&
        (a->next==AVOID_AXLE_ADVANCE||a->next==AVOID_ALIGN_TURN))){
        bool tape=center>=a->reference*80/100||ir.right;
        if(!tape){
            if(!a->white_us)a->white_us=now;
            if(now-a->white_us>=100000)a->rejoin_armed=true;
        }else a->white_us=0;
        /* Do not immediately reacquire the starting tape under the bottle.
         * First leave it and move at least 80 mm laterally. */
        bool departed=a->offset_mm>=80||
            (a->phase==AVOID_MOVE_OUT&&mean_mm(&a->leg,enc)>=80);
        if(tape&&departed&&a->rejoin_armed){
            a->resume_phase=a->phase;a->resume_servo=a->servo_angle;
            a->candidate_us=now;a->rejoin_attempts++;a->rejoin_good=0;a->tape_decision="CHECKING";
            a->rejoin_armed=false;a->white_us=0;a->range_us=0;
            a->phase=AVOID_CHECK_TAPE;a->servo_angle=90;
            return quiet(a);
        }
    }
    if(a->phase==AVOID_ALIGN_CONFIRM){
        if(fresh&&valid&&cm<=30)return fail(a,"AVOID_ALIGNED_PATH_BLOCKED");
        if(center<a->reference*70/100||!alignment_window(a,enc)){
            resume_alignment(a,enc,now);return quiet(a);
        }
        if(now-a->phase_us<SETTLE_US)return quiet(a);
        if(fresh){
            a->rejoin_good=((valid&&cm>30)||(!valid&&a->no_return))?a->rejoin_good+1:0;
            a->tape_decision=valid?"FAR_ECHO":a->no_return?"NO_RETURN_MONITORED":"NO_RANGE";
        }
        if(a->rejoin_good>=2)enter(a,AVOID_VERIFY,enc,now);
        else if(now-a->phase_us>=1500000)return fail(a,"AVOID_ALIGNED_PATH_UNCONFIRMED");
        return quiet(a);
    }
    if(a->phase==AVOID_TAPE_PROBE){
        int mm=mean_mm(&a->tape_origin,enc);
        if(a->probe_black&&center<200){
            a->probe_black=false;a->probe_gap=true;
            a->probe_first_black_mm=mm>0?(unsigned)mm:0;
            if(mm<8){
                a->tape_decision="INK_TOO_NARROW";
                resume_detour(a,enc,now);return quiet(a);
            }
        }else if(!a->probe_black&&center>=a->reference*80/100){
            /* Multiple black bands over 60 mm are consistent with barcode or
             * printed text. Resume the saved detour; do not steer along ink. */
            a->tape_decision="REPEATED_INK";
            resume_detour(a,enc,now);return quiet(a);
        }
    }
    if(a->phase==AVOID_ALIGN_TURN){
        if(!a->range_us||now-a->range_us>RANGE_STALE_US)return fail(a,"AVOID_RANGE_STALE");
        if(fresh&&valid&&cm<=12)return fail(a,"AVOID_ALIGN_OBSTACLE_STOP");
        if(center<200)a->align_armed=true;
        if(a->align_armed&&alignment_window(a,enc)&&center>=a->reference*80/100){
            if(!a->align_hit_us)a->align_hit_us=now;
            if(a->angled_probe||now-a->align_hit_us>=80000){
                /* Pause rather than abort: confirmation may reject this ink. */
                a->leg.left.throttle=a->leg.right.throttle=0;
                a->phase=AVOID_ALIGN_CONFIRM;a->phase_us=now;
                a->rejoin_good=0;return quiet(a);
            }
        }else a->align_hit_us=0;
    }
    if(a->phase==AVOID_FOLLOW||a->phase==AVOID_VERIFY){
        if(a->phase==AVOID_VERIFY){
            if(fresh)a->close_count=valid&&cm<=30?a->close_count+1:0;
            if(a->close_count>=2)return fail(a,"AVOID_RETURN_BLOCKED");
        }else{
            /* Retain a near target when a narrow bottle drops out of the beam.
             * This is a conservative travel budget, not a new sonar measurement.
             * Estimate sensor displacement from the fixed warning origin, not
             * per-tick absolute wheel differences (asynchronous encoder edges
             * would inflate that sum). Use translation plus front-tip rotation;
             * add 20% odometry margin. Large heading change invalidates pursuit. */
            float approach_travel_cm=0;
            if(a->approach_warning&&!a->approach_pending){
                float dl=motion_left_position(&a->approach_origin,enc)/counts_per_mm;
                float dr=motion_right_position(&a->approach_origin,enc)/counts_per_mm;
                float yaw=fabsf(dl-dr)/115.0f;
                a->approach_yaw_rad=yaw;
                approach_travel_cm=1.2f*(fabsf(dl+dr)*0.5f+130.0f*yaw)/10.0f;
                float budget=a->approach_anchor_cm-approach_travel_cm;
                if(budget<a->approach_estimate_cm)a->approach_estimate_cm=budget;
            }
            if(fresh&&valid&&cm<=WARNING_CM){
                if(!a->approach_warning){
                    a->approach_warning=true;a->approach_origin.start=enc;
                    a->approach_estimate_cm=a->approach_anchor_cm=cm;a->approach_min_cm=cm;
                    a->approach_yaw_rad=0;
                }
                if(cm<a->approach_min_cm)a->approach_min_cm=cm;
                if(cm<a->approach_estimate_cm){
                    a->approach_estimate_cm=cm;a->approach_anchor_cm=cm+approach_travel_cm;
                }
            }
            if(a->approach_warning&&a->approach_min_cm<MIN_PIVOT_CM)
                return fail(a,"AVOID_APPROACH_TOO_CLOSE");
            if(a->approach_warning&&!a->approach_pending&&a->approach_yaw_rad>0.35f)
                return fail(a,"AVOID_APPROACH_HEADING_UNCERTAIN");
            if(a->approach_warning&&a->approach_estimate_cm<=APPROACH_CM&&!a->approach_pending){
                a->approach_pending=true;a->approach_us=now;a->approach_hits=0;
            }
            if(a->approach_pending&&fresh&&valid&&cm<=APPROACH_CM&&a->approach_hits<2)
                a->approach_hits++;
        }
        /* A warning slows the follower but does not disable a measured bend
         * direction. A committed obstacle stop and rejoin verification still
         * use precise recovery rather than rolling away from their target. */
        a->line.bend_disabled=a->phase==AVOID_VERIFY||a->approach_pending;
        course_set_cruise_limit(&a->line,a->phase==AVOID_VERIFY||a->approach_warning?450:
            a->phase==AVOID_FOLLOW&&a->fast_clear>=2?1000:600);
        if(a->phase==AVOID_FOLLOW&&a->approach_pending){
            if(a->approach_hits<2&&now-a->approach_us<APPROACH_CONFIRM_US)
                return (line_check_output_t){0,0,false,"OBSTACLE_CONFIRM_STOPPED"};
            if(a->detected>=MAX_EPISODES)return fail(a,"AVOID_EPISODE_LIMIT");
            a->detected++;a->episode_us=now;a->episode_origin.start=enc;a->offset_mm=a->along_mm=0;
            /* Front tip is 130mm ahead of axle, half-width <=60mm: pivot
             * radius ~=143mm before margins and object width. 200mm lateral
             * offset allowed the tip to touch a bottle during side inspection.
             * Start at the requested 250mm compromise; physical clearance
             * remains to be checked. Extensions and path scans stay bounded. */
            a->outward_mm=INITIAL_OFFSET_MM;a->departure_deg=a->return_deg=0;
            a->departure_travel_mm=INITIAL_OFFSET_MM;a->departure_forward_mm=0;
            a->departure_done_mm=a->departure_step_mm=a->edge_shift_count=a->clearance_adjustments=0;
            a->edge_shift_active=a->angled_probe=false;
            a->plan=(obstacle_plan_t){0};a->tail_margin_done=false;a->side_seen=false;a->clear_checks=0;a->last_side_seen_mm=0;
            a->approach_pending=false;a->approach_warning=false;
            a->rejoin_armed=false;a->white_us=0;
            wait_for(a,AVOID_SCAN_INITIAL,now);return quiet(a);
        }
        if(!a->range_us){return quiet(a);}
        if(now-a->range_us>RANGE_STALE_US)return fail(a,"AVOID_RANGE_STALE");
        course_center_level(&a->line,center);
        line_check_output_t out=course_step(&a->line,ir,enc,now);
        if(out.stop)return fail(a,out.state);
        if(a->phase==AVOID_VERIFY){
            if(center>=a->reference*80/100 && a->line.phase==COURSE_FOLLOW){
                if(!a->hit_us)a->hit_us=now;
                if(now-a->hit_us>=300000&&
                   motion_left_position(&a->line.reference,enc)>=1080&&
                   motion_right_position(&a->line.reference,enc)>=1080){
                    a->completed++;a->phase=AVOID_FOLLOW;
                    a->line.recovery_limit_counts=0;
                }
            }else a->hit_us=0;
            if(now-a->phase_us>10000000)return fail(a,"AVOID_LINE_ALIGNMENT_FAILED");
        }
        return out;
    }
    if(a->phase==AVOID_WAIT){
        if((a->next==AVOID_TURN_OUT||a->next==AVOID_SCAN_INITIAL)&&a->offset_mm==0&&fresh&&valid&&cm<MIN_PIVOT_CM)
            return fail(a,"AVOID_APPROACH_TOO_CLOSE");
        if(now-a->phase_us>=SETTLE_US){
            /* Wait for a NEW forward-facing sample, not the last side ray. */
            if(!a->range_us||now-a->range_us>RANGE_STALE_US)return fail(a,"AVOID_RANGE_STALE");
            if(a->next==AVOID_AXLE_ADVANCE){
                int32_t remaining=SENSOR_LEAD_MM+8-mean_mm(&a->tape_origin,enc);
                if(remaining<40||remaining>140)return fail(a,"AVOID_TAPE_POSITION_UNCERTAIN");
                a->axle_mm=(unsigned)remaining; /* include coast during settling */
            }
            enter(a,a->next,enc,now);
        }
        return quiet(a);
    }
    if(a->phase==AVOID_CHECK_LEFT){
        /* Fixed leftmost servo ray is oblique, not a true side-looking sensor.
         * Inspect while stopped so forward coverage is not silently disabled. */
        if(now-a->phase_us>1500000)return fail(a,"AVOID_LEFT_CHECK_STALE");
        if(now-a->phase_us<SETTLE_US||!fresh)return quiet(a);
        float limit=a->offset_mm/10.0f+20;
        if(valid&&cm<=limit){a->left_near=true;a->side_seen=true;a->last_side_seen_mm=a->along_mm;a->tail_margin_done=false;}
        if(valid&&cm>limit)a->left_far++;
        if(++a->left_samples>=3){
            if(!a->left_near&&a->left_far>=2&&(a->side_seen||a->plan.right_edge_known)&&!a->tail_margin_done)
                wait_for(a,AVOID_CLEAR_TAIL,now);
            else wait_for(a,AVOID_TURN_LOOK,now);
        }
        return quiet(a);
    }
    if(scan_phase(a->phase)){
        if(now-a->phase_us>2500000)return fail(a,"AVOID_SCAN_STALE");
        if(!fresh||now-a->phase_us<SETTLE_US)return quiet(a);
        float limit=a->phase==AVOID_SCAN_SIDE?a->offset_mm/10.0f+20.0f:
            a->phase==AVOID_SCAN_OUT?(a->departure_travel_mm-a->departure_done_mm)/10.0f+20.0f:
            a->phase==AVOID_SCAN_RETURN?(a->offset_mm-60)/10.0f/sinf((a->return_deg?a->return_deg:90)*0.01745329252f)+20.0f:25.0f;
        if(valid){
            if(a->phase==AVOID_SCAN_INITIAL){
                float edge_limit=fminf(45.0f,fmaxf(30.0f,a->approach_min_cm+15.0f));
                if(cm<=edge_limit){a->plan_near[a->scan_index]++;
                    if(cm<a->plan_cm[a->scan_index])a->plan_cm[a->scan_index]=cm;
                }else a->plan_far[a->scan_index]++;
            }
            if(cm<a->scan_min_cm){a->scan_min_cm=cm;a->scan_nearest_angle=a->servo_angle;}
            bool in_corridor=true;
            if(a->phase==AVOID_SCAN_OUT){
                /* 12 cm body + 4 cm clearance each side. Widen each measured
                 * ray by a provisional 15-degree beam/heading uncertainty.
                 * A 34 cm echo at 50deg is beside this corridor, not ahead.
                 * Very close echoes still veto regardless of angle. */
                float bearing=fabsf((float)a->servo_angle-90.0f);
                float inner=fmaxf(0,bearing-15.0f)*3.14159265f/180.0f;
                in_corridor=cm*sinf(inner)<=10.0f||cm<=12.0f;
            }
            if(cm<=limit&&in_corridor){
                a->scan_blocked=true;a->scan_blocked_angle=a->servo_angle;
                if(a->phase==AVOID_SCAN_OUT&&a->servo_angle!=125)a->scan_core_blocked=true;
            }
            if(a->servo_angle==90&&cm>limit)a->scan_center_clear++;
            if(cm>limit){a->scan_ray_far++;a->scan_far_samples++;}
        }
        else{
            a->scan_no_echo++;
            if(a->servo_angle==90&&a->no_return)
                a->scan_center_no_return++;
            if(a->phase==AVOID_SCAN_INITIAL&&a->no_return)a->plan_no_return[a->scan_index]++;
        }
        if(++a->samples<3)return quiet(a);
        if(a->scan_ray_far>=2)a->scan_far_rays++;
        a->samples=a->scan_ray_far=0;
        unsigned count=a->phase==AVOID_SCAN_INITIAL?OBSTACLE_PLAN_RAYS:sizeof(angles)/sizeof(angles[0]);
        if(++a->scan_index<count){
            a->servo_angle=a->phase==AVOID_SCAN_INITIAL?obstacle_plan_angles[a->scan_index]:angles[a->scan_index];
            a->phase_us=now;return quiet(a);
        }
        a->scans++;a->scan_serial++;
        if(a->phase==AVOID_SCAN_INITIAL){
            a->plan=obstacle_plan(a->plan_cm,a->plan_near,a->plan_far,a->plan_no_return);
            if(a->scan_min_cm<MIN_PIVOT_CM)return fail(a,"AVOID_APPROACH_TOO_CLOSE");
            if(!a->plan.right_edge_known&&!a->plan.edge_from_no_return)
                return fail(a,"AVOID_RIGHT_EDGE_UNKNOWN");
            if(a->offset_mm+a->plan.lateral_mm>MAX_OFFSET_MM)
                return fail(a,"AVOID_OBJECT_TOO_WIDE_OR_PATH_BLOCKED");
            a->departure_deg=a->plan.angle_deg;a->outward_mm=a->plan.lateral_mm;
            a->return_deg=a->plan.angle_deg;a->departure_done_mm=0;
            a->clearance_adjustments=0;
            a->departure_travel_mm=a->plan.travel_mm;a->departure_forward_mm=a->plan.forward_mm;
            wait_for(a,AVOID_TURN_OUT,now);
        }else if(a->phase==AVOID_SCAN_OUT){
            if(a->scan_blocked){
                /* A single oblique echo on the object-facing boundary can
                 * overlap the conservative corridor estimate. Aim farther
                 * right and measure again before allowing any travel. */
                if(a->scan_core_blocked||(a->scan_center_clear<2&&a->scan_center_no_return<3)||
                    a->scan_min_cm<=12||
                    a->departure_done_mm||a->departure_deg>70||a->clearance_adjustments>=2)
                    return fail(a,"AVOID_RIGHT_PATH_BLOCKED");
                a->departure_deg+=10;a->return_deg=a->departure_deg;
                float rad=a->departure_deg*0.01745329252f;
                a->departure_travel_mm=(unsigned)ceilf(a->outward_mm/sinf(rad));
                a->departure_forward_mm=(unsigned)fmaxf(0,floorf(a->departure_travel_mm*cosf(rad)));
                a->plan.angle_deg=a->departure_deg;
                a->plan.travel_mm=a->departure_travel_mm;
                a->plan.forward_mm=a->departure_forward_mm;
                a->clearance_adjustments++;
                wait_for(a,AVOID_TURN_CLEARANCE,now);return quiet(a);
            }
            if(a->scan_center_clear<2&&a->scan_center_no_return<3)
                return fail(a,"AVOID_OUTWARD_RANGE_UNKNOWN");
            unsigned remaining=a->departure_travel_mm-a->departure_done_mm;
            a->departure_step_mm=remaining<STEP_MM?remaining:STEP_MM;
            wait_for(a,AVOID_MOVE_OUT,now);
        }else if(a->phase==AVOID_SCAN_FORWARD){
            if(a->scan_center_clear<2&&a->scan_center_no_return<3&&!a->scan_blocked)
                return fail(a,"AVOID_FORWARD_RANGE_UNKNOWN");
            if(a->angled_probe){
                /* A clear original-heading scan continues straight to tape. */
                if(a->scan_blocked){
                    unsigned step_offset=(unsigned)ceilf(STEP_MM*sinf(a->return_deg*0.01745329252f));
                    if(a->offset_mm+step_offset>MAX_OFFSET_MM)
                        return fail(a,"AVOID_OBJECT_TOO_WIDE_OR_PATH_BLOCKED");
                    a->departure_deg=a->return_deg;a->outward_mm=step_offset;
                    a->departure_travel_mm=STEP_MM;
                    a->departure_forward_mm=(unsigned)floorf(STEP_MM*cosf(a->departure_deg*0.01745329252f));
                    a->departure_done_mm=0;
                    wait_for(a,AVOID_TURN_OUT,now);
                }else{
                    if(a->along_mm>=MAX_ALONG_MM)return fail(a,"AVOID_RETURN_LINE_NOT_FOUND");
                    wait_for(a,AVOID_MOVE_ALONG,now);
                }
                return quiet(a);
            }
            if(a->scan_blocked){
                if(a->offset_mm>=MAX_OFFSET_MM)return fail(a,"AVOID_OBJECT_TOO_WIDE_OR_PATH_BLOCKED");
                a->departure_deg=90;a->departure_forward_mm=0;
                a->outward_mm=MAX_OFFSET_MM-a->offset_mm<STEP_MM?MAX_OFFSET_MM-a->offset_mm:STEP_MM;
                a->departure_travel_mm=a->outward_mm;a->departure_done_mm=0;
                wait_for(a,AVOID_TURN_OUT,now);
            }else{
                if(a->along_mm>=MAX_ALONG_MM)return fail(a,"AVOID_OBJECT_END_NOT_CONFIRMED");
                wait_for(a,AVOID_MOVE_ALONG,now);
            }
        }else if(a->phase==AVOID_SCAN_RETURN){
            if(a->scan_blocked)return fail(a,"AVOID_RETURN_BLOCKED");
            if(a->scan_center_clear<2&&a->scan_center_no_return<3)
                return fail(a,"AVOID_RETURN_RANGE_UNKNOWN");
            wait_for(a,AVOID_RETURN_LINE,now);
        }else{
            if(a->scan_blocked){a->side_seen=true;a->last_side_seen_mm=a->along_mm;a->clear_checks=0;a->tail_margin_done=false;}
            if(a->edge_shift_count&&a->along_mm==0&&!a->side_seen)
                return fail(a,"AVOID_OBJECT_SIDE_UNKNOWN");
            /* Open space need not echo on precisely the central ray. Use two
             * distinct angles with repeated valid far echoes (or the original
             * repeated central echoes), no close ray, and an observed object
             * at least 200 mm behind the latest along checkpoint. Confirm with
             * a second stationary sweep instead of driving another 200 mm.
             * This authorises a cautious monitored return, not proof of an
             * empty swept volume; all movement obstacle/stale guards remain. */
            else if((a->side_seen||a->plan.right_edge_known)&&a->along_mm>=MIN_ALONG_MM&&
                a->along_mm>=a->last_side_seen_mm+ALONG_STEP_MM&&
                (a->scan_center_clear>=2||a->scan_far_rays>=2||a->scan_center_no_return>=3))a->clear_checks++;
            else a->clear_checks=0;
            if((a->side_seen||a->plan.right_edge_known)&&a->clear_checks>=2&&a->along_mm>=MIN_ALONG_MM)
                wait_for(a,a->return_deg<90?AVOID_TURN_RETURN:AVOID_SCAN_RETURN,now);
            else if(a->clear_checks==1)wait_for(a,AVOID_SCAN_SIDE,now);
            else wait_for(a,AVOID_TURN_ALONG,now);
        }
        return quiet(a);
    }
    if(forward_phase(a->phase)){
        a->leg.transit_drive=(a->phase==AVOID_MOVE_OUT||a->phase==AVOID_MOVE_ALONG)&&a->fast_clear>=2;
        if(!a->range_us){
            if(now-a->phase_us>RANGE_STALE_US)return fail(a,"AVOID_RANGE_STALE");
            return quiet(a);
        }
        if(now-a->range_us>RANGE_STALE_US)return fail(a,"AVOID_RANGE_STALE");
        if(fresh){
            a->close_count=valid&&cm<=20?a->close_count+1:0;
            a->moving_invalid=(valid||a->no_return)?0:a->moving_invalid+1;
        }
        if((fresh&&valid&&cm<=12)||a->close_count>=2)return fail(a,"AVOID_PATH_OBSTACLE_STOP");
        if(a->moving_invalid>=3&&(a->phase==AVOID_MOVE_OUT||a->phase==AVOID_MOVE_ALONG||
            a->phase==AVOID_RETURN_LINE||a->phase==AVOID_CLEAR_TAIL))
            return fail(a,"AVOID_MOVING_RANGE_UNKNOWN");

    }
    motion_poll(&a->leg,enc,now);
    if(a->leg.phase==MOTION_FAULT)return fail(a,motion_fault_name(a->leg.fault));
    if(a->leg.phase==MOTION_DONE){
        switch(a->phase){
        case AVOID_TURN_OUT:
            a->departure_step_mm=a->edge_shift_active?EDGE_SHIFT_MM:STEP_MM;
            wait_for(a,AVOID_MOVE_OUT,now);break;
        case AVOID_MOVE_OUT:{
            unsigned previous=a->departure_done_mm;
            a->departure_done_mm+=a->departure_step_mm;
            float rad=a->departure_deg*0.01745329252f;
            a->offset_mm+=(unsigned)floorf(fmaxf(0,a->departure_done_mm*sinf(rad)+0.001f))-
                (unsigned)floorf(fmaxf(0,previous*sinf(rad)+0.001f));
            a->along_mm+=(unsigned)floorf(fmaxf(0,a->departure_done_mm*cosf(rad)+0.001f))-
                (unsigned)floorf(fmaxf(0,previous*cosf(rad)+0.001f));
            /* Recheck from the original heading after every completed
             * angled step, rather than finishing the whole diagonal first. */
            wait_for(a,AVOID_TURN_FORWARD,now);
            break;
        }
        case AVOID_TURN_FORWARD:
            if(a->edge_shift_active){a->edge_shift_active=false;wait_for(a,AVOID_SCAN_INITIAL,now);}
            else {a->angled_probe=true;wait_for(a,AVOID_SCAN_FORWARD,now);}
            break;
        case AVOID_TURN_ALONG:wait_for(a,AVOID_SCAN_FORWARD,now);break;
        case AVOID_TURN_RETURN:wait_for(a,AVOID_SCAN_RETURN,now);break;
        case AVOID_TURN_TO_LINE:wait_for(a,AVOID_SCAN_RETURN,now);break;
        case AVOID_TURN_CLEARANCE:wait_for(a,AVOID_SCAN_OUT,now);break;
        case AVOID_MOVE_ALONG:
            a->along_mm+=a->angled_probe?STEP_MM:ALONG_STEP_MM;
            if(a->angled_probe){
                if(a->along_mm>=MAX_ALONG_MM)return fail(a,"AVOID_RETURN_LINE_NOT_FOUND");
                wait_for(a,AVOID_MOVE_ALONG,now);
            }else wait_for(a,AVOID_CHECK_LEFT,now);
            break;
        case AVOID_CLEAR_TAIL:a->along_mm+=100;a->tail_margin_done=true;wait_for(a,AVOID_TURN_LOOK,now);break;
        case AVOID_TURN_LOOK:wait_for(a,AVOID_SCAN_SIDE,now);break;
        case AVOID_TAPE_PROBE:
            if(a->probe_gap){
                int remaining=SENSOR_LEAD_MM+8-mean_mm(&a->tape_origin,enc);
                if(remaining<40||remaining>140)return fail(a,"AVOID_TAPE_POSITION_UNCERTAIN");
                a->axle_mm=(unsigned)remaining;
                a->tape_decision="SINGLE_CROSSING";
                wait_for(a,AVOID_AXLE_ADVANCE,now);
            }else if(route_direction_ok(a,enc)){
                a->tape_decision="CONTINUOUS_INK";enter(a,AVOID_VERIFY,enc,now);
            }else return fail(a,"AVOID_REJOIN_BACKWARD_HEADING");
            break;
        case AVOID_AXLE_ADVANCE:wait_for(a,AVOID_ALIGN_TURN,now);break;
        case AVOID_ALIGN_TURN:
            if(a->align_reversed)return fail(a,"AVOID_TAPE_HEADING_NOT_FOUND");
            a->align_reversed=true;a->align_direction=-a->align_direction;
            wait_for(a,AVOID_ALIGN_TURN,now);break;
        case AVOID_RETURN_LINE:return fail(a,"AVOID_RETURN_LINE_NOT_FOUND");
        default:return fail(a,"AVOID_BAD_PHASE");
        }
        return quiet(a);
    }
    return (line_check_output_t){a->leg.left.throttle,a->leg.right.throttle,false,obstacle_bypass_phase(a->phase)};
}

line_check_output_t obstacle_bypass_guarded_step(obstacle_bypass_t *a,ir_roles_t ir,
    int32_t center,encoder_sample_t enc,uint64_t wall,bool fresh,bool valid,float cm,
    const char *error,bool completed_no_return)
{
    if(a->phase==AVOID_FAILED)return fail(a,a->fault);
    if(wall<a->wall_us||wall-a->wall_us>250000)return fail(a,"AVOID_CONTROL_LATE");
    uint64_t delta=wall-a->wall_us;a->wall_us=wall;
    if(!a->sonar_wait)a->active_us+=delta;
    if(!encoder_health_ok(a,enc,a->active_us))return fail(a,"AVOID_ENCODER_INVALID");
    if(fresh)a->no_return=completed_no_return&&!error&&!valid;
    if(fresh&&error){
        bool recoverable=!strcmp(error,"AVOID_SONAR_ECHO_HIGH_BEFORE_TRIGGER");
        if(!recoverable)return fail(a,error);
        if(!a->sonar_wait){a->sonar_wait=true;a->sonar_wait_us=wall;a->sonar_retries++;a->sonar_good=0;}
    }
    if(a->sonar_wait){
        if(wall-a->sonar_wait_us>=1000000)return fail(a,"AVOID_SONAR_RECOVERY_FAILED");
        if(fresh){
            bool good=!error&&valid&&isfinite(cm)&&cm>=2&&cm<=400;
            a->sonar_good=good?a->sonar_good+1:0;
            /* Preserve consecutive close observations across the pause. */
            a->close_count=good&&cm<=30?a->close_count+1:0;
        }
        if(a->sonar_good<2)return (line_check_output_t){0,0,false,"AVOID_SONAR_RETRY_STOPPED"};
        a->sonar_wait=false;a->sonar_resumes++;
        /* Do not double count the second close sample in the controller. */
        if(a->close_count)a->close_count--;
    }
    return obstacle_bypass_step(a,ir,center,enc,a->active_us,fresh,valid,cm);
}
