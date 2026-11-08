#include "scan_diagnostic.h"
#include <string.h>
typedef struct { uint8_t *bytes;unsigned bit;bool valid; } writer_t;
static void put(writer_t *w,uint32_t value,unsigned bits)
{
    if((bits<32 && value>=(UINT32_C(1)<<bits)) || w->bit+bits>SCAN_PACKET_BYTES*8u){w->valid=false;return;}
    for(unsigned i=0;i<bits;i++,w->bit++)if(value&(UINT32_C(1)<<i))w->bytes[w->bit/8u]|=(uint8_t)(1u<<(w->bit%8u));
}
static writer_t begin(uint8_t *out,unsigned kind,uint16_t run)
{
    memset(out,0,SCAN_PACKET_BYTES);writer_t w={out,0,true};put(&w,kind,8);put(&w,run,16);return w;
}
static size_t end(writer_t w){return w.valid?(w.bit+7u)/8u:0;}
static void letter(writer_t *w,char c)
{ if(c && (c<'A' || c>'Z'))w->valid=false;put(w,c?(uint32_t)(c-'A'+1):0,5); }
static void profile(writer_t *w,const barcode_wave_profile_t *p)
{
    put(w,p->extrema,7);put(w,p->windows,6);put(w,p->best_window,6);
    put(w,p->reason,4);put(w,p->bad_run,5);put(w,p->bad_width,32);
    for(unsigned i=0;i<3;i++)put(w,p->code39.patterns[i],9);
    for(unsigned i=0;i<3;i++)put(w,p->code39.width_reason[i],3);
    put(w,p->code39.guards,2);put(w,p->code39.separators,2);put(w,p->code39.reversed,1);put(w,p->code39.score,4);
    for(unsigned i=0;i<29;i++)put(w,p->runs[i],32);
}
size_t scan_state_encode(const scan_state_t *s,uint8_t out[SCAN_PACKET_BYTES])
{
    writer_t w=begin(out,SCAN_STATE,s->run);
    put(&w,s->ms,32);put(&w,s->samples,32);put(&w,s->frames,32);put(&w,s->rejected,32);
    put(&w,s->decode_us,32);put(&w,s->adc_us,32);put(&w,s->events,16);
    put(&w,s->queue_peak,9);put(&w,s->faults,3);put(&w,s->enabled,1);put(&w,s->recording,1);put(&w,s->sealed,1);put(&w,0,1);
    put(&w,s->attempts,6);put(&w,s->ready,1);put(&w,0,1);
    put(&w,s->attempt_overflow,32);put(&w,s->recoveries,32);put(&w,s->stop_us,32);put(&w,s->stop_discarded,9);
    return end(w);
}
size_t scan_attempt_encode(uint16_t run,unsigned index,const barcode_wave_attempt_t *a,uint8_t out[SCAN_PACKET_BYTES])
{
    writer_t w=begin(out,SCAN_ATTEMPT,run);put(&w,index,5);
    put(&w,a->start_us,32);put(&w,a->duration_us,32);put(&w,a->samples,14);
    put(&w,a->raw_min,12);put(&w,a->raw_max,12);put(&w,a->lateral_mm,16);put(&w,a->yaw_deg,16);
    put(&w,a->reason,4);letter(&w,a->letter);put(&w,a->coherent_mask,6);put(&w,a->geometry_mask,6);
    profile(&w,&a->best);return end(w);
}
size_t scan_snapshot_encode(uint16_t run,const scan_snapshot_t *s,uint8_t out[SCAN_PACKET_BYTES])
{
    writer_t w=begin(out,SCAN_SNAPSHOT,run);put(&w,s->count,15);put(&w,s->motion,1);
    put(&w,s->paper,12);put(&w,s->signal_range,12);put(&w,s->reason,4);letter(&w,s->letter);
    put(&w,s->coherent_mask,6);put(&w,s->geometry_mask,6);
    put(&w,s->start_us,32);put(&w,s->leading_us,32);put(&w,s->white_us,32);put(&w,s->duration_us,32);put(&w,s->checksum,32);
    return end(w);
}
size_t scan_profile_encode(uint16_t run,unsigned index,const barcode_wave_profile_t *p,uint8_t out[SCAN_PACKET_BYTES])
{ writer_t w=begin(out,SCAN_PROFILE,run);put(&w,index,3);profile(&w,p);return end(w); }
size_t scan_words_encode(uint16_t run,unsigned index,const uint16_t *words,unsigned count,uint8_t out[SCAN_PACKET_BYTES])
{
    if(!count || count>SCAN_CHUNK_WORDS)return 0;
    writer_t w=begin(out,SCAN_WORDS,run);put(&w,index,8);put(&w,count,7);put(&w,0,1);
    for(unsigned i=0;i<count;i++)put(&w,words[i],16);
    return end(w);
}
size_t scan_done_encode(uint16_t run,unsigned attempts,bool snapshot,uint8_t out[SCAN_PACKET_BYTES])
{ writer_t w=begin(out,SCAN_DONE,run);put(&w,attempts,6);put(&w,snapshot,1);return end(w); }
