#ifndef BARCODE_DRIVE_H
#define BARCODE_DRIVE_H
#include "line_check.h"
#include "barcode_acquisition.h"
#define BARCODE_DRIVE_LIMIT_US 8000000u
#define BARCODE_DRIVE_LIMIT_COUNTS 8000
#define CONTINUOUS_LIMIT_US 20000000u
#define CONTINUOUS_LIMIT_COUNTS 24000
#define CONTINUOUS_BOTH_US 300000u
#define BARCODE_DRIVE_TARGET_CPS 1200.0f
#define TRACK_TARGET_CPS 900.0f
#define TRACK_CRUISE_CPS 1200.0f
#define TRACK_CLEAR_CRUISE_US 250000u
#define TRACK_CORNER_CONFIRM_US 80000u
#define TRACK_CORNER_HOLD_US 100000u
typedef struct {
    int32_t left, right, previous_left, previous_right, balance_offset;
    uint64_t left_progress, right_progress, control_us, step_us;
    float left_target, right_target;
    float left_cps, right_cps, left_integral, right_integral;
    float left_pwm, right_pwm;
    float left_speed_sum, right_speed_sum, left_speed_min, right_speed_min;
    float left_speed_max, right_speed_max;
    unsigned speed_samples, steering_intervals;
    bool started, steering_since_update, speed_valid;
    bool both_active, clear_tracking;
    uint64_t both_since, clear_since;
    int32_t both_left, both_right;
    int8_t edge_direction; /* -1 left, +1 right; barcode channel never steers */
    uint64_t edge_since, edge_last;
    bool hard_corner, corner_hold;
    bool cruise_clear_tracking;
    bool center_profile;
    bool launch_active,launch_complete;
    uint64_t launch_us;
    int32_t launch_l,launch_r;
    float launch_left_pwm,launch_right_pwm;
    float center_steer; /* signed target difference: negative left, positive right */
    float yaw_error, yaw_pwm; /* requested minus measured wheel difference, counts/duty */
    int yaw_direction;
    uint64_t cruise_clear_since;
    unsigned stall_mask; /* bit0 left, bit1 right; latched only on a stall */
    uint32_t stall_left_ms, stall_right_ms;
} barcode_progress_t;
static inline void barcode_clear_corner(barcode_progress_t *p)
{p->edge_direction=0;p->hard_corner=false;p->corner_hold=false;}
static inline float barcode_clamp(float v,float low,float high)
{return v<low?low:(v>high?high:v);}
static inline float barcode_speed_pwm(float target,float cps,float dt,float *integral,bool right)
{
    float error=target-cps;
    /* Provisional duty seed from17 live rows/wheel: median PWM*1200/cps
     * left0.1569/right0.1417. Not a measured full motor/deadband model.
     * Integral is now in PWM units. Allow <20% duty so overspeed can actually
     * be corrected; existing1s encoder-stall protection remains mandatory. */
    float feedforward=(right?0.142f:0.157f)*target/BARCODE_DRIVE_TARGET_CPS;
    float proportional=0.000040f*error;
    float trial=barcode_clamp(*integral+0.000200f*error*dt,-0.20f,0.35f);
    float candidate=feedforward+proportional+trial;
    /* Do not accumulate further into output saturation; allow recovery. */
    if(!((candidate>0.50f&&error>0) || (candidate<0&&error<0)))*integral=trial;
    return barcode_clamp(feedforward+proportional+*integral,0.0f,0.50f);
}
static inline line_check_output_t barcode_drive_step_profile(ir_roles_t ir,uint64_t elapsed,
 int32_t left,int32_t right,bool invalid,barcode_progress_t *p,bool continuous,bool course)
{
    if(invalid)return (line_check_output_t){0,0,true,"BARCODE_ENCODER_FAULT"};
    if(left < -64 || right < -64)return (line_check_output_t){0,0,true,"BARCODE_WRONG_DIRECTION"};
    if(left>p->left){p->left=left;p->left_progress=elapsed;}
    if(right>p->right){p->right=right;p->right_progress=elapsed;}
    if(!course && elapsed>=(continuous?CONTINUOUS_LIMIT_US:BARCODE_DRIVE_LIMIT_US))return (line_check_output_t){0,0,true,"BARCODE_TIME_LIMIT"};
    if(!course && (left>=(continuous?CONTINUOUS_LIMIT_COUNTS:BARCODE_DRIVE_LIMIT_COUNTS) || right>=(continuous?CONTINUOUS_LIMIT_COUNTS:BARCODE_DRIVE_LIMIT_COUNTS)))
        return (line_check_output_t){0,0,true,"BARCODE_TRAVEL_LIMIT"};
    /* A deliberately unpowered inside wheel is not an encoder stall. Refresh
     * only its grace period; the driven outside wheel remains monitored. */
    if(p->started && p->left_target==0.0f)p->left_progress=elapsed;
    if(p->started && p->right_target==0.0f)p->right_progress=elapsed;
    if(elapsed-p->left_progress>=1000000u || elapsed-p->right_progress>=1000000u){
        p->stall_mask=(elapsed-p->left_progress>=1000000u?1u:0u) |
                      (elapsed-p->right_progress>=1000000u?2u:0u);
        p->stall_left_ms=(uint32_t)((elapsed-p->left_progress)/1000u);
        p->stall_right_ms=(uint32_t)((elapsed-p->right_progress)/1000u);
        return (line_check_output_t){0,0,true,"BARCODE_ENCODER_STALL"};
    }
    if(ir.left&&ir.right){
        if(!continuous)return (line_check_output_t){0,0,true,"BARCODE_BOTH_LINE_BLACK"};
        if(!p->both_active){p->both_active=true;p->both_since=elapsed;p->both_left=left;p->both_right=right;}
        p->clear_tracking=false;
    }else if(p->both_active){
        if(!p->clear_tracking){p->clear_tracking=true;p->clear_since=elapsed;}
        if(elapsed-p->clear_since>=50000u){p->both_active=false;p->clear_tracking=false;}
    }
    if(p->both_active && (elapsed-p->both_since>=CONTINUOUS_BOTH_US ||
       (int64_t)left-p->both_left>=360 || (int64_t)right-p->both_right>=360))
        return (line_check_output_t){0,0,true,"CONTINUOUS_LINE_AMBIGUOUS"};
    /* Barcode-only velocity loop; calibrated manoeuvre controller is untouched.
     * 1200 counts/s is nominally89mm/s at CPR2543 and60mm wheel diameter.
     * Physical speed remains measured evidence, not an assumed guarantee. */
    /* Centre profile starts both wheels over a tiny encoder-checked distance.
     * Hold the leading wheel instead of yawing while the other breaks friction.
     * Existing1s stall checks above remain active; no unbounded startup boost. */
    if(p->center_profile && !p->launch_complete){
        if(p->launch_active && (elapsed<p->step_us || elapsed-p->step_us>250000u))
            return (line_check_output_t){0,0,true,"BARCODE_CONTROL_LATE"};
        if(!p->launch_active){
            p->launch_active=true;p->launch_us=elapsed;p->launch_l=left;p->launch_r=right;
            p->launch_left_pwm=barcode_clamp(p->launch_left_pwm,0.12f,0.22f);
            p->launch_right_pwm=barcode_clamp(p->launch_right_pwm,0.14f,0.22f);
            p->step_us=elapsed;
        }
        int64_t dl=(int64_t)left-p->launch_l,dr=(int64_t)right-p->launch_r;
        if(dl>=8 && dr>=8){
            p->launch_complete=true;
            /* Seed the PI output from the effort that actually moved each wheel.
             * Subtract its initial zero-speed P term for a bumpless handoff. */
            p->left_integral=p->launch_left_pwm-0.157f*0.5f-0.000040f*600;
            p->right_integral=p->launch_right_pwm-0.142f*0.5f-0.000040f*600;
        }else{
            float increment=0.12f*(float)(elapsed-p->step_us)/1000000.0f;
            if(dl<8)p->launch_left_pwm=barcode_clamp(p->launch_left_pwm+increment,0.0f,0.26f);
            if(dr<8)p->launch_right_pwm=barcode_clamp(p->launch_right_pwm+increment,0.0f,0.26f);
            p->step_us=elapsed;p->left_target=p->right_target=600;
            p->left_pwm=dl-dr>=12?0:p->launch_left_pwm;
            p->right_pwm=dr-dl>=12?0:p->launch_right_pwm;
            return (line_check_output_t){p->left_pwm,p->right_pwm,false,"CENTER_BALANCED_START"};
        }
    }
    if(!p->started){
        p->started=true;p->control_us=elapsed;
        p->previous_left=left;p->previous_right=right;
        if(p->center_profile)p->yaw_error=barcode_clamp((float)((int64_t)right-left),-40.0f,40.0f);
        p->left_pwm=0.20f;p->right_pwm=0.19f;
        p->left_target=p->right_target=p->center_profile?600.0f:continuous?TRACK_TARGET_CPS:BARCODE_DRIVE_TARGET_CPS;p->step_us=elapsed;
    }
    if(elapsed<p->step_us || elapsed<p->control_us || elapsed-p->control_us>250000u)
        return (line_check_output_t){0,0,true,"BARCODE_CONTROL_LATE"};
    /* Track profile: stronger corrections than the single-frame scan test.
     * Sustained one-sided black cuts inside drive after80ms. Brief white after
     * that holds the direction for at most100ms, not an unbounded blind turn.
     * Geometry/traction still need a physical corner test. */
    int direction=ir.left!=ir.right?(ir.left?-1:1):0;
    p->corner_hold=false;
    if(continuous && !p->both_active){
        if(direction){
            if(direction!=p->edge_direction){p->edge_direction=(int8_t)direction;p->edge_since=elapsed;p->hard_corner=false;}
            p->edge_last=elapsed;
            if(elapsed-p->edge_since>=TRACK_CORNER_CONFIRM_US)p->hard_corner=true;
        }else if(p->hard_corner && elapsed-p->edge_last<TRACK_CORNER_HOLD_US){
            direction=p->edge_direction;p->corner_hold=true;
        }else barcode_clear_corner(p);
    }else barcode_clear_corner(p);
    /* Accelerate only after250ms without a line edge. Crossings/centering
     * explicitly reset this timer in the course controller. */
    if(continuous){
        if(direction || p->both_active)p->cruise_clear_tracking=false;
        else if(!p->cruise_clear_tracking){p->cruise_clear_tracking=true;p->cruise_clear_since=elapsed;}
    }
    bool cruise=continuous && p->cruise_clear_tracking && elapsed-p->cruise_clear_since>=TRACK_CLEAR_CRUISE_US;
    float desired_left=continuous?(cruise?TRACK_CRUISE_CPS:TRACK_TARGET_CPS):BARCODE_DRIVE_TARGET_CPS,desired_right=desired_left;
    if(p->center_profile)desired_left=desired_right=600.0f;
    if(p->both_active){
        desired_left=desired_right=900.0f;
        p->steering_since_update=true;p->balance_offset=left-right;
    }else if(direction){
        p->steering_since_update=true;
        p->balance_offset=left-right;
        float inner=continuous?(p->hard_corner?0.0f:250.0f):900.0f;
        if(direction<0)desired_left=inner;
        else desired_right=inner;
    } else if(p->center_profile && p->center_steer!=0.0f){
        /* Keep the outside wheel at the proven600cps profile. The450cps
         * trial left the physical motors in deadband and reversed actual yaw. */
        desired_left=desired_right=600.0f;
        float amount=barcode_clamp(p->center_steer,-220.0f,220.0f);
        if(amount<0)desired_left+=amount;else desired_right-=amount;
        p->steering_since_update=true;p->balance_offset=left-right;
    } else if(!p->center_profile) {
        float balance=barcode_clamp((float)((int64_t)left-right-p->balance_offset)*1.5f,-120.0f,120.0f);
        if(balance>0)desired_left-=balance;
        else desired_right+=balance;
    }
    float limit=(continuous?4000.0f:1500.0f)*(float)(elapsed-p->step_us)/1000000.0f;
    p->left_target+=barcode_clamp(desired_left-p->left_target,-limit,limit);
    p->right_target+=barcode_clamp(desired_right-p->right_target,-limit,limit);
    if(continuous && direction){
        if(p->left_target>TRACK_TARGET_CPS)p->left_target=TRACK_TARGET_CPS;
        if(p->right_target>TRACK_TARGET_CPS)p->right_target=TRACK_TARGET_CPS;
    }
    /* Lower the inside target promptly, but retain its speed-loop correction
     * at nonzero targets so friction can be overcome during gentle steering.
     * The zero-target branch below still clears power AND memory on a pivot. */
    if(continuous && direction){
        if(direction<0)p->left_target=desired_left;
        else p->right_target=desired_right;
    }
    p->step_us=elapsed;
    /* Centre mode cruises at600, so the legacy1080 cutoff incorrectly labelled
     * every straight interval as steering and hid wheel-speed evidence. */
    float clear_target=p->center_profile?540.0f:1080.0f;
    if(p->left_target<clear_target || p->right_target<clear_target)p->steering_since_update=true;
    /* Drop unfinished yaw demand when the optical controller changes its mind
     * or finds strong tape. Never finish an old left turn after a new right
     * request. The speed loops keep their learned motor/friction effort. */
    int yaw_direction=desired_left>desired_right?1:desired_left<desired_right?-1:0;
    if(p->center_profile && yaw_direction!=p->yaw_direction){
        p->yaw_error=p->yaw_pwm=0;p->yaw_direction=yaw_direction;
    }
    float integration_dt=0.0f;
    if(elapsed-p->control_us>=(p->center_profile?20000u:100000u)){
        float dt=(float)(elapsed-p->control_us)/1000000.0f;
        p->left_cps=(float)((int64_t)left-p->previous_left)/dt;
        p->right_cps=(float)((int64_t)right-p->previous_right)/dt;
        integration_dt=dt;p->speed_valid=true;
        if(p->center_profile){
            /* Independent PI loops were too slow to establish the small yaw
             * requested by the optical trim (v5.32 physical runs1/2). Close a
             * bounded differential-travel loop as well, including straight
             * travel. Use only fresh encoder intervals; no reverse PWM. */
            float yaw_rate_error=(p->left_target-p->right_target)-(p->left_cps-p->right_cps);
            p->yaw_error=barcode_clamp(p->yaw_error+yaw_rate_error*dt,-40.0f,40.0f);
            p->yaw_pwm=barcode_clamp(0.0012f*p->yaw_error+0.00004f*yaw_rate_error,-0.06f,0.06f);
        }
        if(!p->steering_since_update){
            if(elapsed>=500000u){
                if(!p->speed_samples){
                    p->left_speed_min=p->left_speed_max=p->left_cps;
                    p->right_speed_min=p->right_speed_max=p->right_cps;
                }
                if(p->left_cps<p->left_speed_min)p->left_speed_min=p->left_cps;
                if(p->left_cps>p->left_speed_max)p->left_speed_max=p->left_cps;
                if(p->right_cps<p->right_speed_min)p->right_speed_min=p->right_cps;
                if(p->right_cps>p->right_speed_max)p->right_speed_max=p->right_cps;
                p->left_speed_sum+=p->left_cps;p->right_speed_sum+=p->right_cps;p->speed_samples++;
            }
        }else{
            p->steering_intervals++;
        }
        p->steering_since_update=false;
        p->previous_left=left;p->previous_right=right;p->control_us=elapsed;
    }
    /* Feedback stays active during steering. Target changes affect output
     * each pass; integration only uses fresh100ms encoder measurements. */
    if(p->speed_valid || continuous){
        p->left_pwm=barcode_speed_pwm(p->left_target,p->left_cps,integration_dt,&p->left_integral,false);
        p->right_pwm=barcode_speed_pwm(p->right_target,p->right_cps,integration_dt,&p->right_integral,true);
    }
    if(p->center_profile){
        p->left_pwm=barcode_clamp(p->left_pwm+p->yaw_pwm,0.0f,0.50f);
        p->right_pwm=barcode_clamp(p->right_pwm-p->yaw_pwm,0.0f,0.50f);
    }
    if(p->left_target==0){p->left_pwm=0;p->left_integral=0;}
    if(p->right_target==0){p->right_pwm=0;p->right_integral=0;}
    return (line_check_output_t){p->left_pwm,p->right_pwm,false,
        p->both_active?"CONTINUOUS_LINE_BRIDGE":p->corner_hold?"TRACK_CORNER_HOLD":
        p->hard_corner?(direction<0?"TRACK_TIGHT_LEFT":"TRACK_TIGHT_RIGHT"):
        direction<0?"BARCODE_CORRECT_LEFT":direction>0?"BARCODE_CORRECT_RIGHT":"BARCODE_SPEED_HOLD"};
}
static inline line_check_output_t barcode_drive_step_mode(ir_roles_t ir,uint64_t elapsed,
 int32_t left,int32_t right,bool invalid,barcode_progress_t *p,bool continuous)
{return barcode_drive_step_profile(ir,elapsed,left,right,invalid,p,continuous,false);}
static inline line_check_output_t barcode_drive_step(ir_roles_t ir,uint64_t elapsed,
 int32_t left,int32_t right,bool invalid,barcode_progress_t *p)
{
    return barcode_drive_step_mode(ir,elapsed,left,right,invalid,p,false);
}
/* Only moving diagnostics halt on capture failures; stationary scans can retry. */
static inline line_check_output_t barcode_capture_stop(line_check_output_t out,const barcode_capture_t *c)
{
    if(c->integrity_fault)return (line_check_output_t){0,0,true,"BARCODE_SAMPLE_FAULT"};
    if(out.stop)return out;
    if(c->continuous)return out;
    if(c->rejected)return (line_check_output_t){0,0,true,"BARCODE_REJECTED"};
    if(c->timeouts)return (line_check_output_t){0,0,true,"BARCODE_CAPTURE_TIMEOUT"};
    if(c->decoded)return (line_check_output_t){0,0,true,"BARCODE_READ"};
    return out;
}
#endif
