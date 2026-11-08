#include "barcode_waveform.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "barcode_motion.h"
#ifndef BARCODE_WAVE_RECOVERY_OPTICS
#define BARCODE_WAVE_RECOVERY_OPTICS 0
#endif

/* Adapted from the record-then-decide method in the user-supplied
 * autonomous-robotic-car-main/app_program/subsystem3/barcode_decoder.c:
 * alternating prominent extrema, time-weighted local midpoint crossings.
 * Our ADC units, bounded contrast sweep, framing and existing Code39 decoder
 * differ. Complete framed windows can start after unrelated prefix activity.
 * No stripes are inserted or removed to force a match. See testing notes. */
enum { EXT_MAX=95, START_RISE=24, WHITE_BAND=24,
       QUIET_US=250000, ABANDON_WHITE_US=750000, MAX_FRAME_US=16000000,
       MIN_RUN_US=8000 };
/* Geometry measured by user 2026-11-06: powered axle to GP26 +80 mm
 * forward, +85 mm left. Wheel geometry remains nominal (60 mm/2543 CPR,
 * 115 mm track). Spatial widths use 1000 units per mean-wheel count. */
#define MM_PER_COUNT (188.4955592f/2543.0f)
#define LEVEL(w,i) ((unsigned)((w)->sample[i]&4095u))
#define STEP(w,i) ((uint32_t)((w)->sample[i]>>12)*1000u)

void barcode_waveform_begin(barcode_waveform_t *w)
{ memset(w,0,sizeof(*w)); }

static uint32_t fraction(unsigned d,unsigned lo,unsigned hi)
{
    int32_t den=(int32_t)lo-(int32_t)hi;
    /* ADC differences <=4095, so 32-bit arithmetic suffices on Cortex-M0+. */
    int32_t f=den ? (((int32_t)d-(int32_t)hi)*1024)/den : 0;
    return f<0 ? 0u : f>1024 ? 1024u : (uint32_t)f;
}

/* Iterate a monotonic spatial envelope when odometry is present. Samples at
 * a stop or behind the furthest observed position do not add stripe width or
 * duplicate stripes. With flat long bars, another lateral row can contain
 * the same stripe sequence; optical framing and geometry must validate it.
 * Raw samples are retained. */
static bool series_next(const barcode_waveform_t *w,unsigned *index,int32_t *coordinate)
{
    for(unsigned i=*index+1u;i<w->count;i++){
        int32_t next=w->motion?w->position[i]:*coordinate+(int32_t)STEP(w,i);
        if(w->motion && next<=*coordinate)continue;
        *index=i;*coordinate=next;return true;
    }
    return false;
}
static uint32_t series_step(const barcode_waveform_t *w,int32_t a,int32_t b)
{return (uint32_t)(b-a)*(w->motion?1000u:1u);}
static unsigned extract_edges(barcode_waveform_t *w,unsigned prominence,
                              uint32_t edges[EXT_MAX],barcode_wave_profile_t *d)
{
    uint16_t ext[EXT_MAX];unsigned n=1,best=0,i=0;int32_t coordinate=0;
    bool rising=true;ext[0]=0;
    while(series_next(w,&i,&coordinate)){
        unsigned level=LEVEL(w,i),b=LEVEL(w,best);
        if(rising ? level>b : level<b)best=i;
        else if((rising ? b-level : level-b)>=prominence){
            if(n>=EXT_MAX-1u){d->extrema=EXT_MAX;d->reason=WAVE_TOO_MANY;return 0;}
            ext[n++]=(uint16_t)best;best=i;rising=!rising;
        }
    }
    if(!rising)ext[n++]=(uint16_t)best;
    d->extrema=(uint16_t)n;
    if(n<31u){d->reason=WAVE_TOO_FEW;return 0;}
    uint32_t t=0,t_lo=0,at_lo=0;unsigned k=0,previous=0;i=0;coordinate=0;
    while(k+1u<n){
        int32_t before=coordinate;if(!series_next(w,&i,&coordinate))break;
        uint32_t step=series_step(w,before,coordinate);t+=step;
        unsigned lo=LEVEL(w,ext[k]),hi=LEVEL(w,ext[k+1u]);
        at_lo+=step*(fraction(LEVEL(w,previous),lo,hi)+fraction(LEVEL(w,i),lo,hi))/2048u;
        if(i==ext[k+1u]){
            uint32_t guess=t_lo+at_lo,edge=guess,off_best=UINT32_MAX,tj=t_lo;
            unsigned mid=(lo+hi)/2u;bool up=hi>lo;
            unsigned j=ext[k];int32_t cj=w->motion?w->position[j]:0;
            while(j<i){
                unsigned before_j=j;int32_t old=cj;if(!series_next(w,&j,&cj))break;
                unsigned a=LEVEL(w,before_j),b=LEVEL(w,j);uint32_t sj=series_step(w,old,cj);
                if((up && a<mid && b>=mid)||(!up && a>mid && b<=mid)){
                    uint32_t x=tj+sj*(up?mid-a:a-mid)/(up?b-a:a-b);
                    uint32_t off=x>guess?x-guess:guess-x;
                    if(off<off_best){off_best=off;edge=x;}
                }
                tj+=sj;
            }
            edges[k++]=edge;t_lo=t;at_lo=0;
        }
        previous=i;
    }
    return k;
}

bool barcode_waveform_decode(barcode_waveform_t *w,char *letter)
{
    static const unsigned prominence[]={12,24,48,96,192,384};
    if(!w || !letter || w->count<3u || w->count>BARCODE_WAVE_SAMPLES)return false;
    if(w->motion && w->motion_fault)return false;
    char geometry=0;unsigned geometry_profiles=0;uint32_t *geometry_runs=w->geometry_work;
    char accepted=0,fit=0;uint32_t *fit_runs=w->fit_work;unsigned fit_profiles=0;unsigned best_score=0;bool conflict=false;
    w->complete_profiles=0;
    for(unsigned p=0;p<6;p++){
        uint32_t *edges=w->edge_work;barcode_wave_profile_t *d=&w->profile[p];
        *d=(barcode_wave_profile_t){0};
        unsigned count=extract_edges(w,prominence[p],edges,d);
        w->profile_extrema[p]=w->last_extrema=d->extrema;
        /* Skip whole black/white pairs. A complete frame's own 29 intervals
         * remain intact; prefix/tail activity is never used to fill gaps. */
        for(unsigned start=0;start+29u<count;start+=2u){
            uint32_t runs[29];bool bounded=true,subfloor_ok=true;unsigned subfloor=0;d->windows++;
            for(unsigned i=0;i<29;i++){
                runs[i]=edges[start+i+1u]-edges[start+i];
                if(runs[i]<(w->motion?4000u:MIN_RUN_US) || runs[i]>BARCODE_RUN_MAX_US){
                    if(!d->code39.score){d->reason=runs[i]<(w->motion?4000u:MIN_RUN_US)?WAVE_RUN_SHORT:WAVE_RUN_LONG;
                        d->bad_run=(uint8_t)i;d->bad_width=runs[i];}
                    bounded=false;
                    /* Crossing bias can shorten a positive black interval.
                     * Never admit a missing edge,
                     * a tiny white gap, multiple tiny bars or an overlong run. */
                    if(!w->motion || runs[i]<1000u || runs[i]>=4000u || i%2u)subfloor_ok=false;
                    subfloor++;
                }
            }
            if(!bounded){
                /* Retain the floor for ordinary decoding. On a COMPLETE
                 * 31-extrema motion frame, one positive black interval may
                 * be shortened by the sensor's crossing bias. The existing
                 * strict width/bias/guard/separator validator must accept
                 * all29 runs, with agreement from >=2 contrast profiles.
                 * No edge is inserted, deleted or labelled from expectation. */
                char candidate=0;
                if(BARCODE_WAVE_RECOVERY_OPTICS && w->motion && d->extrema==31u &&
                   subfloor_ok && subfloor==1u && barcode_decode_code39_letter(runs,&candidate)){
                    if(fit && fit!=candidate)conflict=true;
                    fit=candidate;fit_profiles|=1u<<p;memcpy(fit_runs,runs,sizeof(runs));
                }
                /* Retain all29 apparent widths even when the absolute floor
                 * fails, so later diagnostics do not replace them with zeros. */
                if(!d->code39.score)memcpy(d->runs,runs,sizeof(runs));
                continue;
            }
            char geometric=0;
            if(w->motion && barcode_decode_code39_geometry(runs,&geometric)){
                if(geometry && geometry!=geometric)conflict=true;
                geometry=geometric;geometry_profiles|=1u<<p;
                memcpy(geometry_runs,runs,sizeof(runs));
            }
            w->complete_profiles|=(uint8_t)(1u<<p);
            barcode_decode_diagnostic_t hint;barcode_inspect_code39(runs,&hint);
            if(d->reason!=WAVE_ACCEPTED && (!d->code39.score || hint.score>d->code39.score)){
                d->code39=hint;d->best_window=(uint16_t)(start/2u);
                d->reason=WAVE_CODE39;memcpy(d->runs,runs,sizeof(runs));
            }
            char candidate=0;
            if(!barcode_decode_code39_letter(runs,&candidate)){
                if(d->extrema==31u && barcode_decode_code39_coherent(runs,&candidate)){
                    if(fit && fit!=candidate)conflict=true;
                    fit=candidate;fit_profiles|=1u<<p;
                    memcpy(fit_runs,runs,sizeof(runs));
                }
                continue;
            }
            d->reason=WAVE_ACCEPTED;d->best_window=(uint16_t)(start/2u);
            d->code39=hint;memcpy(d->runs,runs,sizeof(runs));
            if(accepted && candidate!=accepted)conflict=true;
            accepted=candidate;memcpy(w->last_runs,runs,sizeof(runs));
            w->last_prominence=(uint16_t)prominence[p];
        }
        if(!accepted && d->code39.score>=best_score){
            best_score=d->code39.score;memcpy(w->last_runs,d->runs,sizeof(d->runs));
        }
    }
    unsigned geometry_votes=0;
    for(unsigned bits=geometry_profiles;bits;bits>>=1)geometry_votes+=bits&1u;
    if(geometry_votes>=2){
        if(accepted && accepted!=geometry)conflict=true;
        else if(!accepted){accepted=geometry;memcpy(w->last_runs,geometry_runs,sizeof(w->last_runs));}
    }
    w->geometry_mask=(uint8_t)geometry_profiles;
    unsigned votes=0;for(unsigned bits=fit_profiles;bits;bits>>=1)votes+=bits&1u;
    if(votes>=2){if(accepted && accepted!=fit)conflict=true;else if(!accepted){accepted=fit;memcpy(w->last_runs,fit_runs,sizeof(w->last_runs));}}
    if(votes>=2 && accepted==fit){
        for(unsigned p=0;p<6;p++)if(fit_profiles&(1u<<p)){
            w->profile[p].reason=WAVE_ACCEPTED;w->last_prominence=(uint16_t)(12u<<p);
        }
    }
    w->coherent_mask=(uint8_t)fit_profiles;
    w->last_conflict=conflict;
    if(conflict){w->ambiguous++;return false;}
    if(!accepted)return false;
    w->last_extrema=31;*letter=accepted;return true;
}

const char *barcode_waveform_reason(unsigned reason)
{
    static const char *names[]={"NONE","ACCEPTED","TOO_FEW_EXTREMA","TOO_MANY_EXTREMA",
        "RUN_TOO_SHORT","RUN_TOO_LONG","CODE39_PATTERN_OR_WIDTH","AMBIGUOUS",
        "FRAME_LIMIT","BUFFER_FULL","SAMPLE_TIMING","STOPPED_PARTIAL","NO_VALID_FRAMED_WINDOW","MOTION_PATH_INVALID"};
    return reason<sizeof(names)/sizeof(names[0])?names[reason]:"UNKNOWN";
}

static void remember_attempt(barcode_waveform_t *w,unsigned reason,char letter);

static void snapshot(barcode_waveform_t *w,unsigned reason,char letter)
{
    if(w->motion_fault)reason=WAVE_MOTION;
    w->frame_reason=(uint8_t)reason;
    barcode_wave_snapshot_t *s=&w->saved;
    /* A later failed bend scan must not be hidden by an earlier successful
     * A on the same course. Ignore short later noise (<1s) when a useful
     * snapshot already exists; otherwise preserve the latest substantial
     * capture or any accepted frame. This does not change decode decisions. */
    if(!w->count || (!letter && s->count && w->frame_us<1000000u))return;
    unsigned lo=4095,hi=0;
    for(unsigned i=0;i<w->count;i++){unsigned v=LEVEL(w,i);if(v<lo)lo=v;if(v>hi)hi=v;}
    /* Do not hide useful ink with a later paper-noise capture. */
    if(!letter && s->count && hi-lo<100u && s->signal_range>=100u)return;
    s->geometry_mask=w->geometry_mask;s->signal_range=(uint16_t)(hi-lo);s->coherent_mask=w->coherent_mask;
    s->motion=w->motion;s->count=w->count*(w->motion?2u:1u);s->paper=w->paper;s->start_us=w->capture_start_us;s->leading_us=w->leading_us;
    s->white_us=w->white_us;s->duration_us=w->frame_us;s->reason=(uint8_t)reason;s->letter=letter;
    for(unsigned i=0;i<w->count;i++){
        if(w->motion){s->sample[2u*i]=w->sample[i];s->sample[2u*i+1u]=(uint16_t)w->position[i];}
        else s->sample[i]=w->sample[i];
    }
    memcpy(s->profile,w->profile,sizeof(s->profile));
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<s->count;i++){
        hash=(hash^(s->sample[i]&255u))*16777619u;
        hash=(hash^(s->sample[i]>>8))*16777619u;
    }
    s->checksum=hash;
}

void barcode_waveform_save_partial(barcode_waveform_t *w)
{
    if(w->recording){char ignored=0;(void)barcode_waveform_decode(w,&ignored);
        snapshot(w,WAVE_STOPPED_PARTIAL,0);
        if(!w->partial_saved){remember_attempt(w,w->frame_reason,0);w->partial_saved=true;}}
}

unsigned barcode_waveform_details(const barcode_wave_snapshot_t *s,char *out,unsigned capacity)
{
    static const char *width_reasons[]={"OK","RANGE","RANK_TIE","NONPOSITIVE_OFFSET",
        "WIDE_NARROW_SEPARATION","NARROW_SPREAD","WIDE_SPREAD","OUTER_BAR"};
    if(!capacity)return 0;
    int n=snprintf(out,capacity,"[WAVEFORM SNAPSHOT] id=%lu samples=%u readings=%u paper=%u start_us=%lu duration_us=%lu leading_us=%lu trailing_us=%lu result=%s letter=%c fnv=%08lx encoding=%s width_domain=%s coherent_mask=0x%x geometry_mask=0x%x selection=latest_accepted_or_capture_ge1s\n",
        (unsigned long)s->start_us,s->count,s->count/(s->motion?2u:1u),s->paper,(unsigned long)s->start_us,(unsigned long)s->duration_us,
        (unsigned long)s->leading_us,(unsigned long)s->white_us,barcode_waveform_reason(s->reason),
        s->letter?s->letter:'-',(unsigned long)s->checksum,s->motion?"ADC12_MS4_POS16":"ADC12_MS4",s->motion?"MEAN_COUNT1000_PROJECTED_GP26":"TIME_US",s->coherent_mask,s->geometry_mask);
    if(n<0 || (unsigned)n>=capacity){out[0]=0;return 0;}
    unsigned used=(unsigned)n;
    for(unsigned i=0;i<6;i++){
        const barcode_wave_profile_t *d=&s->profile[i];
        n=snprintf(out+used,capacity-used,"[WAVEFORM PROFILE] prominence=%u extrema=%u windows=%u best_window=%u reason=%s bad_run=%u bad_width_%s=%lu reversed=%u patterns=%03x/%03x/%03x strict_width_reason=%s/%s/%s guards=0x%x separators=0x%x widths_%s=",
            12u<<i,d->extrema,d->windows,d->best_window,barcode_waveform_reason(d->reason),d->bad_run,
            s->motion?"count1000":"us",(unsigned long)d->bad_width,d->code39.reversed,d->code39.patterns[0],d->code39.patterns[1],
            d->code39.patterns[2],width_reasons[d->code39.width_reason[0]],width_reasons[d->code39.width_reason[1]],width_reasons[d->code39.width_reason[2]],d->code39.guards,d->code39.separators,s->motion?"count1000":"us");
        if(n<0 || (unsigned)n>=capacity-used)return 0;
        used+=(unsigned)n;
        for(unsigned j=0;j<29;j++){
            n=snprintf(out+used,capacity-used,"%s%lu",j?",":"",(unsigned long)d->runs[j]);
            if(n<0 || (unsigned)n>=capacity-used)return 0;
            used+=(unsigned)n;
        }
        if(used+1u>=capacity)return 0;
        out[used++]='\n';out[used]=0;
    }
    return used;
}

static void remember_attempt(barcode_waveform_t *w,unsigned reason,char letter)
{
    if(!w->count)return;
    if(w->motion_fault)reason=WAVE_MOTION;
    if(w->attempt_count==BARCODE_WAVE_ATTEMPTS){w->attempt_overflow++;return;}
    barcode_wave_attempt_t *a=&w->attempts[w->attempt_count++];
    *a=(barcode_wave_attempt_t){.start_us=w->capture_start_us,.duration_us=w->frame_us,
        .geometry_mask=w->geometry_mask,.coherent_mask=w->coherent_mask,.samples=w->count,.reason=(uint8_t)reason,.letter=letter,.raw_min=4095,
        .lateral_mm=(uint16_t)(w->max_lateral*MM_PER_COUNT),.yaw_deg=(uint16_t)(w->max_yaw*57.29578f)};
    for(unsigned i=0;i<w->count;i++){unsigned level=LEVEL(w,i);
        if(level<a->raw_min)a->raw_min=(uint16_t)level;
        if(level>a->raw_max)a->raw_max=(uint16_t)level;}
    for(unsigned p=0;p<6;p++)if(w->profile[p].code39.score>=a->best.code39.score)a->best=w->profile[p];
}
static void finish(barcode_waveform_t *w,bool accepted)
{
    remember_attempt(w,accepted?WAVE_ACCEPTED:w->frame_reason,accepted?w->saved.letter:0);
    w->recording=false;w->count=0;w->frame_us=0;w->attempted=false;
    /* A successful read already includes the trailing quiet zone; a failed
     * capture must establish a fresh quiet zone before another attempt. */
    w->armed=accepted;w->white_us=accepted?QUIET_US:0;
}

bool barcode_waveform_update(barcode_waveform_t *w,uint16_t raw,uint32_t now,char *letter)
{
    if(!w || !letter || raw>4095u)return false;
    if(!w->seen){w->seen=true;w->last_us=now;w->previous=w->paper=raw;return false;}
    uint32_t dt=now-w->last_us;w->last_us=now;
    if(!dt || dt>BARCODE_WAVE_GAP_US){
        w->timing_faults++;snapshot(w,WAVE_TIMING,0);finish(w,false);
        w->previous=w->paper=raw;return false;
    }
    uint32_t total=dt+w->residual_us,ms=total/1000u;
    w->residual_us=total%1000u;
    uint16_t previous=w->previous;w->previous=raw;
    if(raw<w->paper)w->paper=raw;
    bool white=(unsigned)raw<=(unsigned)w->paper+WHITE_BAND;
    if(!w->recording){
        if(white){w->white_us+=dt;if(w->white_us>=QUIET_US)w->armed=true;}
        else if(w->armed && (unsigned)raw>=(unsigned)w->paper+START_RISE){
            w->recording=true;w->count=1;w->sample[0]=previous;
            w->capture_start_us=now-dt;w->motion_fault=w->odometry_fault;
            w->partial_saved=false;w->start_yaw=w->yaw;w->white_counts=0;w->white_frontier=0;
            w->progress=w->frontier=0;w->recrossing=false;
            w->origin_x=w->sensor_x;w->origin_y=w->sensor_y;
            w->axis_cos=cosf(w->yaw);w->axis_sin=sinf(w->yaw);
            w->position[0]=0;w->frontier_y=0;w->max_lateral=w->max_yaw=0;
            memset(w->profile,0,sizeof(w->profile));
            w->leading_us=w->white_us;w->white_us=0;w->frame_us=0;w->attempted=false;
            w->frame_reason=WAVE_NONE;w->coherent_mask=0;w->geometry_mask=0;
        }else w->white_us=0;
        if(!w->recording)return false;
    }
    if(w->count==BARCODE_WAVE_SAMPLES){
        w->overflows++;snapshot(w,WAVE_BUFFER_FULL,0);finish(w,false);return false;
    }
    if(w->motion){
        float dx=w->sensor_x-w->origin_x,dy=w->sensor_y-w->origin_y;
        float x=dx*w->axis_cos+dy*w->axis_sin,y=-dx*w->axis_sin+dy*w->axis_cos;
        if(x>32767 || x< -32767){w->motion_fault=true;w->motion_faults++;x=0;}
        int32_t position=(int32_t)lroundf(x);
        if(position<w->frontier-4)w->recrossing=true;
        if(position>w->frontier){
            /* Flat260mm-tall bars can be crossed at another lateral row.
             * A3mm shift along a bar is not a lost stripe. Keep the shift as
             * telemetry; require complete optical framing/model validation.
             * No centred placement within the260mm height is assumed. */
            if(w->recrossing && fabsf(y-w->frontier_y)*MM_PER_COUNT>3.0f)w->lateral_recrosses++;
            if(w->recrossing)w->recrosses++;
            w->recrossing=false;w->frontier=position;w->frontier_y=y;
        }else w->stationary_samples++;
        w->progress=position;w->position[w->count]=(int16_t)position;
        if(fabsf(y)>w->max_lateral)w->max_lateral=fabsf(y);
        if(fabsf(w->yaw-w->start_yaw)>w->max_yaw)w->max_yaw=fabsf(w->yaw-w->start_yaw);
    }
    w->sample[w->count++]=(uint16_t)(raw|(ms<<12));w->frame_us+=dt;
    if(w->frame_us>MAX_FRAME_US){w->rejected++;snapshot(w,WAVE_FRAME_LIMIT,0);finish(w,false);return false;}
    if(white){
        w->white_us+=dt;
        if(w->motion)w->white_counts=w->frontier>w->white_frontier?(uint32_t)(w->frontier-w->white_frontier):0;
    }else{
        w->white_us=0;w->white_counts=0;w->white_frontier=w->frontier;w->attempted=false;
    }
    /* A hold in a white interior gap cannot imitate a moving quiet zone. */
    if(w->white_us>=QUIET_US && (!w->motion || w->white_counts>=100u) && !w->attempted){
        w->attempted=true;w->frames++;
        if(barcode_waveform_decode(w,letter)){snapshot(w,WAVE_ACCEPTED,*letter);finish(w,true);return true;}
    }
    if(w->white_us>=ABANDON_WHITE_US && (!w->motion || w->white_counts>=300u)){
        w->rejected++;snapshot(w,w->last_conflict?WAVE_AMBIGUOUS:WAVE_NO_FRAME,0);finish(w,false);
    }
    return false;
}

bool barcode_waveform_update_motion(barcode_waveform_t *w,uint16_t raw,uint32_t now,
    uint32_t left,uint32_t right,char *letter)
{
    if(!w)return false;
    w->motion=true;
    if(!w->motion_seen){w->motion_seen=true;w->left_raw=left;w->right_raw=right;}
    int64_t dl=-(int64_t)barcode_count_delta(left,w->left_raw),dr=barcode_count_delta(right,w->right_raw);
    w->left_raw=left;w->right_raw=right;
    if(dl>128 || dl< -128 || dr>128 || dr< -128){
        w->motion_fault=w->odometry_fault=true;w->motion_faults++;return false;
    }
    float turn=(dr-dl)*MM_PER_COUNT/115.0f;
    float mid=w->yaw+turn*0.5f,advance=(dl+dr)*0.5f;
    w->pose_x+=advance*cosf(mid);w->pose_y+=advance*sinf(mid);w->yaw+=turn;
    w->sensor_x=w->pose_x+(80.0f*cosf(w->yaw)-85.0f*sinf(w->yaw))/MM_PER_COUNT;
    w->sensor_y=w->pose_y+(80.0f*sinf(w->yaw)+85.0f*cosf(w->yaw))/MM_PER_COUNT;
    return barcode_waveform_update(w,raw,now,letter);
}
