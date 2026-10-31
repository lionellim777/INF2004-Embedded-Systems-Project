#include "barcode_acquisition.h"
#include <string.h>
void barcode_sampler_begin(barcode_sampler_t *s)
{
    barcode_sampler_begin_levels(s,BARCODE_BLACK_ON,BARCODE_BLACK_OFF);
}
void barcode_sampler_begin_levels(barcode_sampler_t *s,uint16_t on,uint16_t off)
{
    memset(s,0,sizeof(*s));s->stats.raw_min=UINT16_MAX;
    if(on<=off || on>4095u)return;
    s->black_on=on;s->black_off=off;s->enabled=true;
}
void barcode_sampler_enable_motion(barcode_sampler_t *s,uint32_t left_raw,uint32_t right_raw)
{barcode_motion_begin(&s->motion,left_raw,right_raw);}
void barcode_sampler_sample_motion(barcode_sampler_t *s,uint16_t raw,uint32_t now,uint32_t left_raw,uint32_t right_raw)
{
    if(!s->enabled)return;
    barcode_motion_step(&s->motion,left_raw,right_raw);
    s->stats.position=s->motion.position;s->stats.motion_breaks=s->motion.breaks;
    barcode_sampler_sample(s,raw,now);
}
void barcode_sampler_sample(barcode_sampler_t *s,uint16_t raw,uint32_t now)
{
    if(!s->enabled)return;
    if(s->stats.samples){
        uint32_t gap=now-s->stats.last_us;
        if(gap>s->stats.max_gap_us)s->stats.max_gap_us=gap;
        if(gap>BARCODE_SAMPLE_GAP_US)s->stats.gap_fault=true;
    }
    s->stats.samples++;s->stats.last_us=now;
    if(raw<s->stats.raw_min)s->stats.raw_min=raw;
    if(raw>s->stats.raw_max)s->stats.raw_max=raw;
    if(raw<=s->black_off)s->stats.raw_white++;
    else if(raw<s->black_on)s->stats.raw_band++;
    else s->stats.raw_black++;
    if(s->stats.black){
        uint32_t width=now-s->black_since_us;
        if(width>s->stats.longest_black_us)s->stats.longest_black_us=width;
    }
    bool next=s->stats.black ? raw>s->black_off : raw>=s->black_on;
    if(next==s->stats.black){
        if(s->stats.black && s->candidate_samples && !s->candidate)s->stats.white_dips++;
        s->candidate_samples=0;return;
    }
    if(s->candidate_samples==0 || next!=s->candidate){
        s->candidate=next;s->candidate_us=now;s->candidate_position=s->stats.position;s->candidate_samples=1;return;
    }
    /* Two consecutive samples confirm an edge; preserve first crossing time. */
    s->stats.black=next;s->candidate_samples=0;s->stats.edges++;
    if(next)s->black_since_us=s->candidate_us;
    if(s->count==BARCODE_EDGE_CAPACITY){s->stats.overflow=true;return;}
    s->positions[s->count]=s->candidate_position;
    s->edges[s->count++]=(barcode_edge_t){s->candidate_us,next};
    if(s->count>s->stats.high_water)s->stats.high_water=s->count;
}
void barcode_sampler_take(barcode_sampler_t *s,barcode_batch_t *out)
{
    out->count=s->count;out->stats=s->stats;
    out->pending_edge=s->candidate_samples != 0;
    memcpy(out->edges,s->edges,s->count*sizeof(s->edges[0]));
    memcpy(out->positions,s->positions,s->count*sizeof(s->positions[0]));s->count=0;
}
void barcode_capture_begin(barcode_capture_t *c)
{
    memset(c,0,sizeof(*c));barcode_reader_init(&c->reader);c->clock_divisor=1;
}
void barcode_capture_begin_drive(barcode_capture_t *c)
{
    barcode_capture_begin(c);c->halt_on_failure=true;
}
void barcode_capture_begin_stream(barcode_capture_t *c,uint32_t origin)
{
    barcode_capture_begin(c);c->continuous=true;c->clock_origin=origin;
}
static bool complete(const barcode_capture_t *c)
{
    return c->decoded && !c->continuous;
}
static bool failed(const barcode_capture_t *c)
{
    return c->halt_on_failure && (c->rejected || c->timeouts);
}
void barcode_capture_begin_slow(barcode_capture_t *c,uint32_t origin)
{
    barcode_capture_begin(c);c->clock_origin=origin;c->clock_divisor=8;
}
static void abandon_motion(barcode_capture_t *c,bool black,uint32_t now)
{
    c->last_runs=c->reader.runs;c->saved_runs=c->reader.run_count;
    barcode_reader_init(&c->reader);c->waiting_gap=true;
    c->white_tracking=!black;c->white_since_us=now;
}
static void feed(barcode_capture_t *c,bool black,uint32_t wall_us,uint32_t position)
{
    uint32_t now=(uint32_t)(wall_us-c->clock_origin)/c->clock_divisor;
    if(c->continuous && c->waiting_gap){
        if(c->white_tracking && (uint32_t)(now-c->white_since_us)>=BARCODE_STREAM_QUIET_US){
            c->waiting_gap=false;c->white_tracking=false;barcode_reader_init(&c->reader);
            c->reader.search_frames=c->independent_optical;
        }else{
            if(black)c->white_tracking=false;
            else if(!c->white_tracking){c->white_tracking=true;c->white_since_us=now;}
            return;
        }
    }
    bool active=c->reader.active;
    uint8_t count=c->reader.run_count;
    bool edge=active && black!=c->reader.level_black;
    uint32_t measurement=c->distance_mode?position:now;
    uint32_t elapsed=measurement-c->reader.edge_us;
    if(c->distance_mode && active && black!=c->reader.level_black && elapsed==0){
        c->zero_distance_aborts++;abandon_motion(c,black,now);return;
    }
    c->reader.search_frames=c->independent_optical;
    char letter=0;
    bool emitted=barcode_reader_update(&c->reader,black,measurement,&letter);
    if(c->independent_optical && active && c->reader.active &&
       edge && c->reader.run_count==BARCODE_CODE39_RUN_COUNT){
        c->frames++;c->rejected++;c->saved_runs=BARCODE_CODE39_RUN_COUNT;c->last_runs=c->reader.runs;
    }
    if(active && !c->reader.active){
        c->last_runs=c->reader.runs;
        if(elapsed>BARCODE_RUN_MAX_US){c->timeouts++;c->saved_runs=count;}
        else if(count==BARCODE_CODE39_RUN_COUNT-1){
            c->frames++;c->saved_runs=BARCODE_CODE39_RUN_COUNT;
            if(!emitted)c->rejected++;
        } else c->saved_runs=count;
        if(c->continuous){
            c->waiting_gap=!c->independent_optical || emitted;
            c->white_tracking=!black;c->white_since_us=now;
        }
    }
    if(emitted){
        c->decoded=letter;
        if(c->continuous){
            if(c->event_count==BARCODE_STREAM_EVENTS){c->event_overflow=true;c->integrity_fault=true;return;}
            c->events[c->event_count].us=now;
            c->events[c->event_count++].letter=letter;
        }
    }
}
void barcode_capture_consume(barcode_capture_t *c,const barcode_batch_t *batch)
{
    c->stats=batch->stats;
    if(batch->stats.gap_fault || batch->stats.overflow){
        c->integrity_fault=true;c->decoded=0;return;
    }
    if(c->integrity_fault || complete(c) || failed(c))return;
    if(c->distance_mode){
        uint32_t now=(uint32_t)(batch->stats.last_us-c->clock_origin)/c->clock_divisor;
        if(c->seen_motion_breaks!=batch->stats.motion_breaks){
            c->seen_motion_breaks=batch->stats.motion_breaks;
            if(c->reader.active)c->motion_aborts++;
            abandon_motion(c,batch->stats.black,now);
            c->progress_position=batch->stats.position;c->progress_us=now;
            /* All queued edges straddling a reversal are unusable. */
            return;
        }
        if(c->progress_position!=batch->stats.position || !c->reader.active){
            c->progress_position=batch->stats.position;c->progress_us=now;
        }else if((uint32_t)(now-c->progress_us)>=2000000u){
            c->pause_timeouts++;abandon_motion(c,batch->stats.black,now);return;
        }
    }
    for(unsigned i=0;i<batch->count && !complete(c) && !c->integrity_fault && !failed(c);i++)
        feed(c,batch->edges[i].black,batch->edges[i].us,batch->positions[i]);
    /* Wait for a candidate edge to resolve before checking the old level's
     * timeout. All confirmed timestamps precede last_us, including jitter. */
    if(!complete(c) && !c->integrity_fault && !failed(c) && batch->stats.samples && !batch->pending_edge)
        feed(c,batch->stats.black,batch->stats.last_us,batch->stats.position);
}
