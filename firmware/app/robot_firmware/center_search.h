#ifndef CENTER_SEARCH_H
#define CENTER_SEARCH_H
#include "line_check.h"
/* Provisional bounded search for GP28 mounted over the line. The centre
 * argument must mean strong black, not merely the follow threshold. 120 counts
 * limits an uncommanded search; it is not a guarantee of barcode clearance.
 * Encoder caps
 * limit sweep, time/stall checks limit a blocked or disconnected drive. */
typedef struct {
    uint64_t start_us,stage_us,last_us,left_us,right_us,confirm_us;
    int32_t origin_l,origin_r,peak_l,peak_r;
    int32_t confirm_l,confirm_r,settle_l,settle_r,coast_l,coast_r;
    uint64_t still_us,black_us;
    bool black_tracking;
    int direction;
    int32_t limit;
    bool reversed,confirming,found;
} center_search_t;
static inline void center_search_begin(center_search_t *s,int direction,
                                      int32_t l,int32_t r,uint64_t now)
{
    *s=(center_search_t){.start_us=now,.stage_us=now,.last_us=now,
        .left_us=now,.right_us=now,.origin_l=l,.origin_r=r,.direction=direction,.limit=120};
}
static inline line_check_output_t center_search_step(center_search_t *s,bool center,
                                                   int32_t l,int32_t r,uint64_t now)
{
    if(now<s->last_us || now-s->last_us>250000u)
        return (line_check_output_t){0,0,true,"CENTER_SEARCH_CONTROL_LATE"};
    s->last_us=now;
    if(now-s->start_us>=3000000u)
        return (line_check_output_t){0,0,true,"CENTER_LINE_NOT_REACQUIRED"};
    /* Once a candidate is seen WITHIN the sweep budget, hold zero power.
     * A separate, bounded settling window may cross the soft sweep boundary;
     * it can never add powered travel or reverse while confirming. */
    if(s->confirming){
        int64_t cl=(int64_t)l-s->confirm_l,cr=(int64_t)r-s->confirm_r;
        if(cl<0)cl=-cl;
        if(cr<0)cr=-cr;
        if(cl>s->coast_l)s->coast_l=(int32_t)cl;
        if(cr>s->coast_r)s->coast_r=(int32_t)cr;
        if(cl>80 || cr>80)return (line_check_output_t){0,0,true,"CENTER_SETTLE_TRAVEL_LIMIT"};
        int64_t ml=(int64_t)l-s->settle_l,mr=(int64_t)r-s->settle_r;
        if(ml>2 || ml< -2 || mr>2 || mr< -2){
            s->settle_l=l;s->settle_r=r;s->still_us=now;
        }
        if(!center)s->black_tracking=false;
        else if(!s->black_tracking){s->black_tracking=true;s->black_us=now;}
        if(center && now-s->still_us>=50000u && now-s->black_us>=30000u){
            s->found=true;return (line_check_output_t){0,0,false,"CENTER_LINE_REACQUIRED"};
        }
        if(now-s->confirm_us>=300000u)
            return (line_check_output_t){0,0,true,"CENTER_CONFIRMATION_TIMEOUT"};
        return (line_check_output_t){0,0,false,"CENTER_STOP_CONFIRM"};
    }
    /* Right pivot: left forward, right reverse. Left pivot is the inverse. */
    int32_t dl=(l-s->origin_l)*s->direction;
    int32_t dr=-(r-s->origin_r)*s->direction;
    if(dl< -64 || dr< -64)
        return (line_check_output_t){0,0,true,"CENTER_SEARCH_WRONG_DIRECTION"};
    if(dl>s->peak_l){s->peak_l=dl;s->left_us=now;}
    if(dr>s->peak_r){s->peak_r=dr;s->right_us=now;}
    if(now-s->left_us>=1000000u || now-s->right_us>=1000000u)
        return (line_check_output_t){0,0,true,"CENTER_SEARCH_ENCODER_STALL"};
    int32_t cap=s->reversed?2*s->limit:s->limit;
    if(center && dl<cap && dr<cap){
        s->confirming=true;s->confirm_us=s->still_us=s->black_us=now;
        s->confirm_l=s->settle_l=l;s->confirm_r=s->settle_r=r;s->black_tracking=true;
        return (line_check_output_t){0,0,false,"CENTER_STOP_CONFIRM"};
    }
    if(dl>=cap || dr>=cap || now-s->stage_us>=1200000u){
        if(s->reversed)return (line_check_output_t){0,0,true,"CENTER_LINE_NOT_REACQUIRED"};
        s->reversed=true;s->direction=-s->direction;
        s->origin_l=l;s->origin_r=r;s->peak_l=s->peak_r=0;
        s->stage_us=s->left_us=s->right_us=now;
        return (line_check_output_t){0,0,false,"CENTER_SEARCH_REVERSE"};
    }
    if(now-s->stage_us<50000u)return (line_check_output_t){0,0,false,"CENTER_SEARCH_BRAKE"};
    /* v5.33 physical pivot stalled with BOTH wheels free at fixed18% duty.
     * Raise only a non-progressing wheel after150ms, capped at28%. Fresh
     * progress restores18%; stall deadline and sweep caps never extend. */
    uint64_t ll=now-s->left_us,rr=now-s->right_us;
    float ld=0.18f,rd=0.18f;
    if(ll>150000u)ld+=0.20f*(float)(ll-150000u)/1000000.0f;
    if(rr>150000u)rd+=0.20f*(float)(rr-150000u)/1000000.0f;
    if(ld>0.28f)ld=0.28f;
    if(rd>0.28f)rd=0.28f;
    return (line_check_output_t){ld*s->direction,-rd*s->direction,false,
        ld>0.18f||rd>0.18f?"CENTER_SEARCH_ASSIST":s->direction<0?"CENTER_SEARCH_LEFT":"CENTER_SEARCH_RIGHT"};
}
#endif
