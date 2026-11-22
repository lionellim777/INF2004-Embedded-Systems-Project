/* Differential run of the real hump, tilt and motion-class state machines with pseudo-random sensor streams.
 * Build once with -DROBOCAR_COMPACT_CONTROL_STATE=0 and once with =1: the printed hash must be equal. */
#include "hump_roll.h"
#include "tilt_alert.h"
#include "motion_class.h"
#include <stdio.h>
#include <string.h>
static uint64_t rng, h;
static uint32_t rnd(void){rng=rng*6364136223846793005ULL+1442695040888963407ULL;return (uint32_t)(rng>>33);}
static void mix(uint64_t v){h^=v;h*=1099511628211ULL;}
static void mixf(float f){uint32_t u;memcpy(&u,&f,4);mix(u);}
int main(void){
    unsigned long long events[5]={0},tilts=0,changes=0;
    for(unsigned seed=1;seed<=300;seed++){
        rng=seed*15485863ULL;hump_roll_t hr;tilt_alert_t ta;motion_class_state_t mc;
        hump_roll_begin_run(&hr);tilt_alert_reset(&ta);motion_class_reset(&mc);
        uint32_t t=1000;int32_t left=0,right=0;float roll=0,pitch=0;hump_roll_motion_go(&hr,t);
        unsigned hump_left=0,tilt_left=0;float hump_sign=-9.0f;
        for(unsigned i=0;i<4000;i++){
            t+=10+rnd()%30;
            if(!hump_left&&rnd()%300==0){hump_left=20+rnd()%80;hump_sign=rnd()%4?-9.0f:9.0f;}
            float target=hump_left?hump_sign:0.0f;if(hump_left)hump_left--;
            if(!tilt_left&&rnd()%900==0)tilt_left=40+rnd()%60;
            roll+=(target-roll)*0.2f+((int)(rnd()%100)-50)*0.01f;
            pitch+=((tilt_left?25.0f:0.0f)-pitch)*0.3f+((int)(rnd()%100)-50)*0.005f;if(tilt_left)tilt_left--;
            if(rnd()%700==0)hump_roll_motion_go(&hr,t);
            int step=(rnd()%900<400)?(int)(rnd()%12):0;left+=step;right+=step+(int)(rnd()%3)-1;
            hump_roll_event_t e=hump_roll_update(&hr,roll,t);
            tilt_alert_event_t te=tilt_alert_update(&ta,pitch,roll,t);
            motion_class_event_t me=motion_class_update(&mc,left,right,roll,hr.phase==HUMP_ROLL_ACTIVE,t);
            mix(e.result);mix((uint64_t)(int64_t)e.sign);mixf(e.peak_deg);mixf(e.height_mm);mix(e.duration_ms);
            mix(te.result);mixf(te.tilt_deg);mix(te.duration_ms);mix(me.changed);mix(me.from);mix(me.to);
            mix(hr.phase);mix((uint64_t)(int64_t)hr.sign);mix(hr.below);mix(hr.need_clear);mix(hr.holdoff_active);
            mix(ta.active);mix(ta.above);mix(ta.below);
            mix(mc.state);mix(mc.candidate);mix(mc.moving);mix(mc.turning);mix(mc.have_last);
            mix(mc.hump_descending);mix(mc.hump_was_active);
            events[e.result%5]++;tilts+=te.result!=TILT_ALERT_NO_EVENT;changes+=me.changed;
        }
        hump_roll_event_t f=hump_roll_finish(&hr,t);mix(f.result);
    }
    printf("hash=%016llx completed=%llu wrong_side=%llu short=%llu tilt_events=%llu state_changes=%llu sizeof=%zu/%zu/%zu\n",
        (unsigned long long)h,events[HUMP_ROLL_COMPLETED],events[HUMP_ROLL_WRONG_SIDE],events[HUMP_ROLL_TOO_SHORT],
        tilts,changes,sizeof(hump_roll_t),sizeof(tilt_alert_t),sizeof(motion_class_state_t));
    return 0;}
