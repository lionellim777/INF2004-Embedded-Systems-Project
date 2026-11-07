#include "course_control.h"
#include <string.h>
static line_check_output_t stopped(course_control_t *c,const char *reason)
{c->phase=COURSE_FAILED;c->fault=reason;return (line_check_output_t){0,0,true,reason};}
static line_check_output_t hold(const char *reason)
{return (line_check_output_t){0,0,false,reason};}
static int32_t magnitude(int32_t v){return v<0?-v:v;}
static void new_segment(course_control_t *c,int32_t l,int32_t r,uint64_t now)
{
    /* Preserve bounded learned forward effort, never old speed/count history.
     * The new segment still has to verify that BOTH wheels start moving. */
    float left=barcode_clamp(0.157f*0.5f+c->drive.left_integral,0.12f,0.22f);
    float right=barcode_clamp(0.142f*0.5f+c->drive.right_integral,0.14f,0.22f);
    c->segment_l=l;c->segment_r=r;c->segment_us=now;
    c->steady_line_us=0;
    c->drive=(barcode_progress_t){.center_profile=ROBOCAR_CENTER_LINE,
        .launch_left_pwm=left,.launch_right_pwm=right};
}
void course_begin(course_control_t *c,course_config_t config,encoder_sample_t enc,uint64_t now)
{
    memset(c,0,sizeof(*c));c->config=config;c->reference.start=enc;
    c->start_us=c->last_us=c->segment_us=now;c->phase=COURSE_FOLLOW;
    c->drive.center_profile=ROBOCAR_CENTER_LINE;c->center_reference=500;
    if(!course_config_valid(config)){c->phase=COURSE_FAILED;c->fault="COURSE_BAD_CONFIG";}
}
void course_center_level(course_control_t *c,int32_t level)
{
    c->center_level_valid=true;c->center_level=level;
    /* Keep following through the narrow edge band, but do not use that weak
     * reading to finish a search. Based on GP28 white178/black2908: enter861,
     * leave724 raw ADC. Reacquisition uses85 percent of the strong run reference,
     * with a lower hold threshold during stopped confirmation. All remain hardware-test candidates. */
    c->center_visible=level >= (c->center_visible?200:250);
}
void course_set_center_reference(course_control_t *c,int32_t level)
{c->center_reference=level<400?400:level>1000?1000:level;}
void course_set_cruise_limit(course_control_t *c,unsigned cps)
{c->cruise_limit_cps=cps<450?450:cps>1000?1000:cps;}
void course_note_barcode_activity(course_control_t *c,uint64_t now)
{c->barcode_seen=true;c->barcode_activity_us=now;}
void course_enable_flow_follow(course_control_t *c,bool enabled)
{c->flow_follow=enabled;}
void course_right_level(course_control_t *c,int32_t level)
{c->right_level_valid=true;c->right_level=level;}
#if ROBOCAR_CENTER_LINE
static bool near_barcode(const course_control_t *c,uint64_t now)
{return c->barcode_seen && (now<c->barcode_activity_us || now-c->barcode_activity_us<2000000u);}
static void front_preview(course_control_t *c,uint64_t now)
{
    /* GP27 raw ~211 enters; ~183 releases under the existing 141/842
     * calibration. Logged bend-side rises 210..321 were often below the
     * old raw316 binary threshold. Require 30ms, not one reflective spike. */
    bool weak=c->center_level<c->center_reference*85/100;
    bool side=c->right_level_valid && c->right_level>=(c->right_preview?60:100);
    if(c->phase!=COURSE_FOLLOW || !weak || !side){
        c->right_preview=false;c->right_preview_tracking=false;return;
    }
    if(!c->right_preview_tracking){c->right_preview_tracking=true;c->right_preview_since=now;}
    if(now-c->right_preview_since>=30000u){
        if(!c->right_preview)c->right_previews++;
        c->right_preview=true;
        if(c->bend_active)return; /* Never flip a committed arc's budget/sign. */
        c->bend_direction=1;c->bend_hint_us=now;c->bend_source="RIGHT_PREVIEW";
        /* An observed right edge supersedes a guessed left trim. */
        if(c->center_trim.active && c->center_trim.direction<0){
            center_trim_clear(&c->center_trim);c->center_trim.last_good=1;
        }
    }
}
#endif
static void apply_action(course_control_t *c,char action,char source,uint64_t now,bool on_crossing)
{
    c->pending_action=action;c->pending_source=source;
    c->remaining_junctions=course_action_junctions(action);
    /* Exclude a crossing already underneath the sensors when the frame ends. */
    c->ignore_current_crossing=on_crossing&&!c->latched;
    if(action=='L'||action=='R'||action=='U'){
        c->turn_action=action;c->turn_source=source;c->pending_action=0;c->pending_source=0;
        c->phase=COURSE_BRAKE;c->phase_us=now;center_trim_clear(&c->center_trim);
    }else if(action=='s'){
        c->pending_action=0;c->pending_source=0;c->ignore_current_crossing=false;
    }
}
const char *course_on_barcode(course_control_t *c,char letter,encoder_sample_t enc,uint64_t now,bool on_crossing)
{
    (void)enc;
    if(c->phase==COURSE_FAILED)return "IGNORED_STOPPED";
    char action=course_config_action(&c->config,letter);
    if(!action)return "IGNORED_UNMAPPED";
    if(!course_action_valid(action)){stopped(c,"COURSE_BAD_CONFIG");return "INVALID_CONFIG";}
    c->route_updates++;
    if(action=='x'){
        c->remaining_junctions=0;c->pending_action=c->pending_source=0;c->deferred=false;
        stopped(c,"COURSE_BARCODE_STOP");return "STOP_NOW";
    }
    if(c->phase!=COURSE_FOLLOW || (c->flow_follow && c->bend_active)){
        if(c->deferred)c->deferred_replacements++;
        c->deferred=true;c->deferred_action=action;c->deferred_source=letter;
        return "DEFERRED_UNTIL_MANOEUVRE_COMPLETE";
    }
    bool replaced=c->remaining_junctions!=0;
    if(replaced)c->replacements++;
    apply_action(c,action,letter,now,on_crossing);
    return replaced?"REPLACED_WAITING_INSTRUCTION":
        c->phase==COURSE_BRAKE?"TURN_NOW":c->remaining_junctions?"WAIT_FOR_FUTURE_JUNCTION":"FOLLOW_STRAIGHT";
}
bool course_reads_barcodes(const course_control_t *c)
{
    /* Straight approach can capture another barcode; pivot/braking/search
     * fragments are discarded. A fresh white gap is required afterwards. */
    return !c->bend_active&&(c->phase==COURSE_FOLLOW||c->phase==COURSE_CENTER);
}
line_check_output_t course_step(course_control_t *c,ir_roles_t ir,encoder_sample_t enc,uint64_t now)
{
    if(c->phase==COURSE_FAILED)return stopped(c,c->fault);
    if(now<c->last_us || now-c->last_us>250000u)return stopped(c,"COURSE_CONTROL_LATE");
    uint64_t step_us=now-c->last_us;c->last_us=now;
#if !ROBOCAR_CENTER_LINE
    (void)step_us;
#endif
    if(enc.left_invalid!=c->reference.start.left_invalid || enc.right_invalid!=c->reference.start.right_invalid)
        return stopped(c,"COURSE_ENCODER_FAULT");
    int32_t l=motion_left_position(&c->reference,enc),r=motion_right_position(&c->reference,enc);
    /* Bound before magnitude arithmetic, including impossible/reset values. */
    if(l<=-COURSE_LIMIT_COUNTS || r<=-COURSE_LIMIT_COUNTS || l>=COURSE_LIMIT_COUNTS || r>=COURSE_LIMIT_COUNTS)
        return stopped(c,"COURSE_TRAVEL_LIMIT");
#if ROBOCAR_CENTER_LINE
    if(c->center_level_valid)ir.left=c->center_visible;
    if(c->phase!=COURSE_FOLLOW||c->center_level<c->center_reference*90/100||
       ir.right||c->cruise_limit_cps<=600){
        c->steady_line_us=0;c->drive.center_cruise_cps=c->cruise_limit_cps&&c->cruise_limit_cps<600?c->cruise_limit_cps:600;
    }
#endif
    bool both=ir.left&&ir.right;
    float center_steer=0;
#if ROBOCAR_CENTER_LINE
    if(c->flow_follow && c->center_level_valid)front_preview(c,now);
    if(c->center_level_valid && c->phase==COURSE_FOLLOW && !c->bend_active){
        /* A weak GP28 plus GP27 black is a right-edge hint, not a crossing. */
        both=ir.right && c->center_level>=c->center_reference*80/100;
        if(ir.left && !both && c->drive.launch_complete){
            bool active=c->center_trim.active,reversed=c->center_trim.reversed;
            /* A guessed trim direction is not evidence. Remember it only
             * after actual directed yaw AND an optical improvement. */
            bool good_hint=active&&!c->center_trim.exhausted&&
                ((int64_t)l-r-c->center_trim.entry_delta)*c->center_trim.direction>=20&&
                c->center_level>=c->center_trim.entry_level+20;
            int good_direction=c->center_trim.direction;
            center_steer=center_trim_step_profile(&c->center_trim,c->center_level,c->center_reference,
                                         ir.right||c->right_preview,l,r,now,c->flow_follow);
            if(good_hint&&!c->center_trim.exhausted&&
               (!c->center_trim.active||c->center_trim.direction==good_direction)){
                c->bend_direction=good_direction;c->bend_hint_us=now;
                c->bend_source="CONFIRMED_TRIM";
            }
            if(!active && c->center_trim.active)c->trim_attempts++;
            if(!reversed && c->center_trim.reversed)c->trim_reversals++;
            /* A small trim not producing20 differential counts is not a
             * wheel stall. Brake and enter the existing bounded recovery;
             * actual encoder stall/control-time guards remain unchanged. */
            if(c->center_trim.no_response)c->trim_no_responses++;
            if(c->center_trim.exhausted)ir.left=false;
        }else if(both)center_trim_clear(&c->center_trim);
    }
#endif
#if ROBOCAR_CENTER_LINE
    /* Brief rolling bend continuation. A centre-only loss has no left/right
     * sign. Require recent successful steering or the right sensor, and a
     * previously established moving line. No indefinite blind forward drive. */
    if(c->phase==COURSE_FOLLOW){
        bool strong=c->center_level_valid&&c->center_level>=c->center_reference*85/100;
        bool permitted=!c->bend_disabled&&(c->flow_follow||!near_barcode(c,now))&&
            (!both || (c->flow_follow && c->bend_active));
        if(!permitted){c->bend_ready=false;c->bend_ready_us=0;}
        if(!c->bend_active&&permitted&&strong&&!ir.right&&c->drive.launch_complete){
            if(!c->bend_ready_us){c->bend_ready_us=now;c->bend_ready_l=l;c->bend_ready_r=r;}
            if(now-c->bend_ready_us>=300000u&&l-c->bend_ready_l>=200&&r-c->bend_ready_r>=200)
                c->bend_ready=true;
        }else if(!c->bend_ready)c->bend_ready_us=0;
        if(!c->bend_active&&permitted&&ir.right&&!both){
            c->bend_direction=1;c->bend_hint_us=now;
            c->bend_source="RIGHT_BLACK";
        }
        bool recent=c->bend_hint_us&&now-c->bend_hint_us<=1500000u;
        /* A search direction is a drift-correction prior, not a measured
         * bend. In centre/right flow mode require CURRENT right-side ink for
         * a hard arc; keep memory for gentle probes and bounded search only. */
        bool direction_confirmed=c->flow_follow ?
            (c->bend_direction==1 && c->right_preview) : recent;
        bool arc_entry=!ir.left || (c->flow_follow && c->center_level<c->center_reference*65/100);
        bool measured_reentry=c->flow_follow && c->right_preview && c->recoveries && l>=200 && r>=200;
        if(!c->bend_active&&arc_entry&&permitted&&(c->bend_ready||measured_reentry)&&direction_confirmed&&
           (!c->center_trim.exhausted||measured_reentry)&&c->drive.launch_complete){
            c->bend_active=true;c->bend_ready=false;c->bend_ready_us=0;
            c->bend_start_us=now;c->bend_l=l;c->bend_r=r;c->bend_strong_us=0;
            c->bend_attempts++;center_trim_clear(&c->center_trim);
            c->loss_coast=false;c->bend_exit="ACTIVE";
        }
        if(c->bend_active){
            int32_t dl=l-c->bend_l,dr=r-c->bend_r;
            int32_t yaw=(dl-dr)*c->bend_direction;
            /* 540 counts ~=40mm per wheel; 400 differential counts ~=18deg.
             * The time/distance/yaw budget is never reset by flickering ink. */
            bool side_conflict=c->flow_follow && c->right_preview && c->bend_direction<0;
            bool exhausted=!permitted||side_conflict||now-c->bend_start_us>=650000u||
                dl>=540||dr>=540||yaw>=400||dl< -20||dr< -20||yaw< -64;
            if(strong){if(!c->bend_strong_us)c->bend_strong_us=now;}
            else c->bend_strong_us=0;
            if(!exhausted&&c->bend_strong_us&&now-c->bend_strong_us>=40000u){
                c->bend_active=false;c->drive.center_bend=false;c->bend_completed++;
                c->center_trim.last_good=c->bend_direction;c->bend_hint_us=now;
                c->drive.center_steer=0;center_steer=0;
                c->bend_exit="MOVING_REACQUIRED";
                if(c->flow_follow && c->deferred){
                    c->deferred=false;apply_action(c,c->deferred_action,c->deferred_source,now,both);
                    if(c->phase==COURSE_BRAKE)return hold("COURSE_BARCODE_BRAKE");
                }
                /* Keep speed feedback and motor effort: no stopped launch. */
            }else if(exhausted){
                c->bend_active=false;c->drive.center_bend=false;c->bend_fallbacks++;
                c->center_trim.last_good=c->bend_direction;
                ir.left=false; /* zero-power bounded search below */
                c->bend_exit=!permitted?"OVERRIDE":side_conflict?"SIDE_CONFLICT":now-c->bend_start_us>=650000u?"TIME_LIMIT":
                    dl>=540||dr>=540?"TRAVEL_LIMIT":yaw>=400?"YAW_LIMIT":"WRONG_MOTION";
            }else{
                c->drive.center_bend=true;c->drive.center_cruise_cps=600;
                float amount=strong?100.0f:now-c->bend_start_us<40000u?220.0f:c->flow_follow?480.0f:540.0f;
                c->drive.center_steer=amount*c->bend_direction;
                line_check_output_t arc=barcode_drive_step_profile((ir_roles_t){0},now-c->segment_us,
                    l-c->segment_l,r-c->segment_r,false,&c->drive,true,true);
                if(arc.stop)return stopped(c,arc.state);
                arc.state=c->bend_direction<0?"CENTER_BEND_LEFT":"CENTER_BEND_RIGHT";
                return arc;
            }
        }
    }else {c->bend_active=c->bend_ready=false;c->bend_ready_us=0;c->drive.center_bend=false;}
    /* If direction is momentarily unknown after a proven straight segment,
     * allow at most80ms /80 encoder counts forward for GP27 to provide a
     * side hint. This budget cannot be renewed by repeated white readings. */
    if(c->flow_follow && c->phase==COURSE_FOLLOW){
        if(ir.left || c->bend_active){c->loss_coast=false;}
        else if(!c->bend_disabled && c->bend_ready && c->drive.launch_complete && !c->center_trim.exhausted){
            if(!c->loss_coast){c->loss_coast=true;c->coast_us=now;c->coast_l=l;c->coast_r=r;c->loss_coasts++;}
            if(now-c->coast_us<80000u && l-c->coast_l<80 && r-c->coast_r<80 &&
               l>=c->coast_l-20 && r>=c->coast_r-20){
                c->drive.center_steer=0;c->drive.center_cruise_cps=600;
                line_check_output_t forward=barcode_drive_step_profile((ir_roles_t){0},now-c->segment_us,
                    l-c->segment_l,r-c->segment_r,false,&c->drive,true,true);
                if(forward.stop)return stopped(c,forward.state);
                forward.state="CENTER_EDGE_PREVIEW";return forward;
            }
            c->loss_coast=false;c->bend_ready=false;
        }
    }
    /* GP28 (ir.left legacy field) now sits directly on the tape. Both-white
     * is loss, not a centred state. Do not decode fragments during search. */
    if(c->phase==COURSE_FOLLOW && !ir.left){
        c->bend_active=c->bend_ready=false;c->bend_ready_us=0;c->drive.center_bend=false;
        c->phase=COURSE_LOST;c->center_search_started=true;
        /* A trial that just reversed is not a confirmed direction. Prefer
         * the most recent measured recovery/optical improvement over it. */
        bool fresh_hint=c->bend_hint_us&&now-c->bend_hint_us<=1500000u;
        int direction=ir.right?1:fresh_hint?c->bend_direction:
                      c->center_trim.active&&!c->center_trim.reversed&&!c->center_trim.exhausted?c->center_trim.direction:
                      c->center_trim.last_good?c->center_trim.last_good:-1;
        center_search_begin(&c->center_search,direction,l,r,now);
        if(c->recovery_limit_counts)c->center_search.limit=c->recovery_limit_counts>240?240:(int)c->recovery_limit_counts;
        if(!c->flow_follow && near_barcode(c,now))c->center_search.limit=60;
        center_trim_clear(&c->center_trim);
    }
    if(c->phase==COURSE_LOST || c->phase==COURSE_SEEK){
        bool after_turn=c->phase==COURSE_SEEK;
        /* Recent barcode activity narrows the existing search; it no longer
         * ends the mission immediately. Never reset its origins or budget. */
        if(!c->flow_follow && !after_turn && near_barcode(c,now))c->center_search.limit=60;
        if(!c->center_search_started){
            center_search_begin(&c->center_search,course_action_left(c->turn_action)?-1:1,l,r,now);
            c->center_search.limit=400; /* explicit turn retains its bounded seek */
            c->center_search_started=true;
        }
        /* Lower hold threshold prevents a modest settling change from breaking
         * an already started confirmation. Both exceed follow/loss thresholds. */
        int32_t reacquire=c->center_reference*(c->center_search.confirming?70:85)/100;
        if(reacquire<300)reacquire=300;
        bool strong=c->center_level_valid?c->center_level>=reacquire:ir.left;
        c->center_search.retry_confirmation=c->flow_follow && !after_turn;
        c->center_search.extended_recovery=c->flow_follow && !after_turn && !c->recovery_limit_counts;
        line_check_output_t search=center_search_step(&c->center_search,strong,l,r,now);
        if(!strcmp(search.state,"CENTER_SEARCH_WIDEN"))c->wide_searches++;
        if(!strcmp(search.state,"CENTER_CONFIRM_RETRY"))c->confirmation_retries++;
        if(search.stop)return stopped(c,!after_turn && c->center_search.limit==60 &&
            !strcmp(search.state,"CENTER_LINE_NOT_REACQUIRED")?"CENTER_LOCAL_RECOVERY_EXHAUSTED":search.state);
        if(c->center_search.found){
            c->center_search_started=false;c->phase=COURSE_FOLLOW;
            center_trim_clear(&c->center_trim);c->center_trim.last_good=c->center_search.direction;
            c->bend_direction=c->center_search.direction;c->bend_hint_us=now;
            c->bend_source=c->flow_follow?"RECOVERY_PRIOR":"CONFIRMED_RECOVERY";
            if(after_turn)c->turns++;else c->recoveries++;
            c->candidate=false;c->clear_tracking=false;
            c->latched=true;c->exit_l=l;c->exit_r=r;c->both_us=now;c->entry_l=l;c->entry_r=r;
            new_segment(c,l,r,now);
            if(c->deferred){c->deferred=false;apply_action(c,c->deferred_action,c->deferred_source,now,false);}
        }
        return search;
    }
#endif
    if(c->phase==COURSE_TURN){
        motion_poll(&c->turn,enc,now);
        if(c->turn.phase==MOTION_FAULT)return stopped(c,motion_fault_name(c->turn.fault));
        if(c->turn.phase==MOTION_DONE){c->phase=COURSE_SETTLE;c->phase_us=now;return hold("COURSE_TURN_SETTLE");}
        return (line_check_output_t){c->turn.left.throttle,c->turn.right.throttle,false,"COURSE_TURNING"};
    }
    if(c->phase==COURSE_BRAKE){
        if(now-c->phase_us>=250000u){
            motion_init(&c->turn);
            if(!motion_begin(&c->turn,c->turn_action=='U'?MOTION_RIGHT_180:course_action_left(c->turn_action)?MOTION_LEFT_90:MOTION_RIGHT_90,enc,now))
                return stopped(c,"COURSE_TURN_REJECTED");
            c->phase=COURSE_TURN;
        }
        return hold("COURSE_CENTER_BRAKE");
    }
    if(c->phase==COURSE_SETTLE){
        if(now-c->phase_us>=250000u){
            c->phase=COURSE_SEEK;c->phase_us=c->seek_us=now;c->seek_l=l;c->seek_r=r;c->seek_reverse=false;
        }
        return hold("COURSE_TURN_SETTLE");
    }
    if(c->phase==COURSE_SEEK){
        /* Both white cannot prove line presence. Seek a real single-sensor
         * edge with a bounded low-duty pivot before resuming forward drive. */
        if(now-c->phase_us>=2000000u)return stopped(c,"COURSE_LINE_NOT_REACQUIRED");
        if(ir.left!=ir.right){
            c->turns++;c->phase=COURSE_FOLLOW;c->candidate=false;c->latched=true;
            c->clear_tracking=false;c->exit_l=l;c->exit_r=r;c->both_us=now;
            new_segment(c,l,r,now);
            if(c->deferred){
                c->deferred=false;
                apply_action(c,c->deferred_action,c->deferred_source,now,false);
                return hold("COURSE_DEFERRED_APPLIED");
            }
            return hold("COURSE_LINE_REACQUIRED");
        }
        if(both)return stopped(c,"COURSE_REACQUIRE_AMBIGUOUS");
        int32_t dl=l-c->seek_l,dr=r-c->seek_r;
        if((!c->seek_reverse&&(dl>64||dr< -64)) || (c->seek_reverse&&(dl< -64||dr>64)))
            return stopped(c,"COURSE_SEEK_WRONG_DIRECTION");
        int32_t cap=c->seek_reverse?440:220;
        if(magnitude(dl)>=cap || magnitude(dr)>=cap || now-c->seek_us>=800000u){
            if(c->seek_reverse)return stopped(c,"COURSE_LINE_NOT_REACQUIRED");
            c->seek_reverse=true;c->seek_l=l;c->seek_r=r;c->seek_us=now;
            return hold("COURSE_SEEK_REVERSE");
        }
        float duty=c->seek_reverse?0.18f:-0.18f;
        return (line_check_output_t){duty,-duty,false,"COURSE_SEEK_LINE"};
    }
    if(c->phase==COURSE_CENTER){
        /* Nominal geometry: CPR2543, wheel60mm. 120mm is user-measured sensor
         * lead. Actual axle centering/coast needs track calibration. */
        int32_t target=(int32_t)c->config.advance_mm*2543*100/18850;
        if(now-c->phase_us>=4000000u)return stopped(c,"COURSE_CENTER_TIMEOUT");
        if(l-c->entry_l>=target && r-c->entry_r>=target){
            c->phase=COURSE_BRAKE;c->phase_us=now;return hold("COURSE_CENTER_BRAKE");
        }
        if(l-c->entry_l>target+500 || r-c->entry_r>target+500)return stopped(c,"COURSE_CENTER_SKEW");
        ir.left=ir.right=false;
    }else{
        if(!c->latched){
            if(both){
                if(!c->candidate){c->candidate=true;c->both_us=now;c->entry_l=l;c->entry_r=r;}
                if(now-c->both_us>=20000u){
                    c->junctions++;c->latched=true;c->clear_tracking=false;
                    if(c->ignore_current_crossing)c->ignore_current_crossing=false;
                    else if(c->remaining_junctions && --c->remaining_junctions==0){
                        c->turn_action=c->pending_action;c->turn_source=c->pending_source;
                        c->pending_action=c->pending_source=0;
                        c->phase=COURSE_CENTER;c->phase_us=now;
                    }
                }
            }else {c->candidate=false;c->ignore_current_crossing=false;}
        }
        if(c->latched){
            if(both){c->clear_tracking=false;}
            else{
                if(!c->clear_tracking){c->clear_tracking=true;c->clear_us=now;c->exit_l=l;c->exit_r=r;}
                /* Counts are encounters, not unique physical intersections. */
                if(now-c->clear_us>=100000u && l-c->exit_l>=600 && r-c->exit_r>=600){
                    c->latched=false;c->candidate=false;c->clear_tracking=false;
                }
            }
        }
        if(c->phase==COURSE_FOLLOW && both && c->latched &&
           (now-c->both_us>=1200000u || l-c->entry_l>=1200 || r-c->entry_r>=1200))
            return stopped(c,"COURSE_CROSSING_TOO_WIDE");
        if(both || c->phase==COURSE_CENTER)ir.left=ir.right=false;
    }
    /* Crossing/committed centering wins over the ordinary bend memory. */
    if(both || c->phase==COURSE_CENTER){
        barcode_clear_corner(&c->drive);
        c->drive.cruise_clear_tracking=false;
        if(c->drive.left_target==0)c->drive.left_progress=now-c->segment_us;
        if(c->drive.right_target==0)c->drive.right_progress=now-c->segment_us;
        c->drive.left_target=c->drive.right_target=ROBOCAR_CENTER_LINE?600.0f:TRACK_TARGET_CPS;
        c->drive.balance_offset=(l-c->segment_l)-(r-c->segment_r);
    }
    c->drive.center_steer=(c->phase==COURSE_FOLLOW && !both)?center_steer:0;
#if ROBOCAR_CENTER_LINE
    bool steady=c->phase==COURSE_FOLLOW&&!both&&!ir.right&&c->center_level_valid&&
        c->center_level>=c->center_reference*90/100&&!c->center_trim.active&&
        c->drive.launch_complete&&c->cruise_limit_cps>600;
    if(steady){
        if(!c->steady_line_us)c->steady_line_us=now;
        if(c->drive.center_cruise_cps<600)c->drive.center_cruise_cps=600;
        if(now-c->steady_line_us>=600000u)
            c->drive.center_cruise_cps=barcode_clamp(c->drive.center_cruise_cps+
                300.0f*(float)step_us/1000000.0f,600.0f,(float)c->cruise_limit_cps);
    }else {c->steady_line_us=0;c->drive.center_cruise_cps=c->cruise_limit_cps&&c->cruise_limit_cps<600?c->cruise_limit_cps:600;}
#endif
    line_check_output_t out=barcode_drive_step_profile(ROBOCAR_CENTER_LINE?(ir_roles_t){0}:ir,now-c->segment_us,
        l-c->segment_l,r-c->segment_r,false,&c->drive,true,true);
    if(out.stop)return stopped(c,out.state);
#if ROBOCAR_CENTER_LINE
    if(!c->drive.launch_complete)return out;
    out.state=c->phase==COURSE_CENTER?"COURSE_CENTERING":both?"CENTER_CROSS_STRAIGHT":center_steer<0?"CENTER_TRIM_LEFT":
              center_steer>0?"CENTER_TRIM_RIGHT":"CENTER_LINE_FOLLOW";
    return out;
#endif
    out.state=c->phase==COURSE_CENTER?"COURSE_CENTERING":both?"COURSE_CROSS_STRAIGHT":
              c->drive.corner_hold?"COURSE_CORNER_HOLD":c->drive.hard_corner?
              (c->drive.edge_direction<0?"COURSE_TIGHT_LEFT":"COURSE_TIGHT_RIGHT"):
              ir.left?"COURSE_CURVE_LEFT":ir.right?"COURSE_CURVE_RIGHT":"COURSE_FOLLOW_BETWEEN_OR_LOST";
    return out;
}
