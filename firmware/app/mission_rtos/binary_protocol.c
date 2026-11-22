#include "binary_protocol.h"
#include <string.h>
#include "track_status_codes.h"
#include "fault_flags.h"
#include <string.h>
#define STATUS_NAME(name) #name,
static const char *const track_status_names[TRACK_STATUS_COUNT] = {TRACK_STATUS_LIST(STATUS_NAME)};
uint8_t track_status_code(const char *status)
{
    if (status == NULL) {
        return 0u;
    }
    for (uint32_t i = 0u; i < TRACK_STATUS_COUNT; i++) {
        if (strcmp(status, track_status_names[i]) == 0) {
            return (uint8_t)(i + 1u);
        }
    }
    return 0u;
}
bool track_status_record_encode(const char *status,uint8_t active,uint8_t latched,
                                uint8_t out[TRACK_STATUS_RECORD_BYTES])
{
    if (((active & ~FAULT_ALL) != 0u) || ((latched & ~FAULT_ALL) != 0u)) {
        return false;
    }
    out[0] = track_status_code(status);
    out[1] = active;
    out[2] = latched;
    return true;
}
static void put32(uint8_t *b, uint32_t value)
{
    for (unsigned i=0;i<4;++i) b[i]=(uint8_t)(value>>(8u*i));
}
bool line_record_encode(uint32_t ms,const uint16_t raw[3],uint8_t pattern,bool valid,uint8_t out[LINE_RECORD_BYTES])
{
    if(out==NULL || raw==NULL || pattern>7)return false;
    uint8_t b[9]={0};put32(b,ms);
    uint64_t fields=0;
    if(valid){
        for(unsigned i=0;i<3;i++){
            if(raw[i]>4095)return false;
            fields|=(uint64_t)raw[i]<<(12u*i);
        }
        fields|=(uint64_t)pattern<<36u;fields|=UINT64_C(1)<<39u;
    }
    for(unsigned i=0;i<5;i++)b[4+i]=(uint8_t)(fields>>(8u*i));
    memcpy(out,b,sizeof b);return true;
}
bool terrain_record_encode(const terrain_reading_t *r,uint8_t out[TERRAIN_RECORD_BYTES])
{
    if(out==NULL || r==NULL || r->roll_decideg<-255 || r->roll_decideg>255 || r->humps>255 ||
       r->last_height_dmm>511 || r->highest_height_dmm>511 || r->motion_state>6u || r->tilt_alerts>15u)return false;
    uint8_t b[TERRAIN_RECORD_BYTES]={0};put32(b,r->ms);
    /* Run results are kept while the sensor reading is invalid; only live values are zeroed. */
    uint64_t fields=r->calibrated?2u:0u;
    fields|=(uint64_t)r->humps<<11u;
    fields|=(uint64_t)r->last_height_dmm<<19u;
    fields|=(uint64_t)r->highest_height_dmm<<28u;
    uint8_t motion=(uint8_t)r->motion_state;
    if(r->valid){
        fields|=1u;
        fields|=(uint64_t)((uint16_t)r->roll_decideg&511u)<<2u;
        motion=(uint8_t)(motion|(r->tilt_active?8u:0u)|(uint8_t)(r->tilt_alerts<<4u));
    }
    for(unsigned i=0;i<5;i++)b[4+i]=(uint8_t)(fields>>(8u*i));
    b[9]=motion;
    memcpy(out,b,sizeof b);return true;
}
bool hump_event_record_encode(uint32_t ms,uint8_t index,uint16_t height_dmm,uint8_t dropped,uint8_t out[HUMP_EVENT_BYTES])
{
    if(out==NULL || height_dmm>511)return false;
    uint32_t fields=(uint32_t)index | ((uint32_t)height_dmm<<8u) | ((uint32_t)dropped<<17u);
    put32(out,ms);put32(out+4,fields);return true;
}
bool obstacle_record_encode(const obstacle_reading_t *r,uint8_t out[OBSTACLE_RECORD_BYTES])
{
    if(out==NULL || r==NULL || r->phase>31 || r->servo_deg>127 || r->range_mm>4095)return false;
    uint8_t b[10]={0};put32(b,r->ms);
    uint64_t fields=0;
    if(r->active){
        fields=1u | (uint64_t)r->phase<<1u | (uint64_t)r->servo_deg<<6u |
            (uint64_t)r->detected<<26u | (uint64_t)r->completed<<34u;
        if(r->range_valid)fields|=UINT64_C(1)<<13u | (uint64_t)r->range_mm<<14u;
    }
    for(unsigned i=0;i<6;i++)b[4+i]=(uint8_t)(fields>>(8u*i));
    memcpy(out,b,sizeof b);return true;
}
bool barcode_record_encode(uint32_t ms, char letter, bool valid, bool simulated, uint8_t out[BARCODE_RECORD_BYTES])
{
    if (out==NULL || (!valid && (letter!=0 || simulated))) return false;
    uint8_t code=0;
    if (letter>='A' && letter<='Z') code=(uint8_t)(letter-'A'+1);
    else if (letter=='?') code=27;
    else if (letter!=0) return false;
    uint8_t b[5]; put32(b,ms);
    b[4]=(uint8_t)(code | (valid?32u:0u) | (simulated?64u:0u));
    memcpy(out,b,sizeof b); return true;
}
bool barcode_record_valid(const uint8_t *b, size_t n)
{
    return b!=NULL && n==5 && !(b[4]&128u) && (b[4]&31u)<=27u &&
        ((b[4]&32u) || b[4]==0);
}
bool status_record_encode(uint32_t ms, uint32_t boot, uint8_t available,
    uint8_t simulated, uint8_t state, uint8_t out[STATUS_RECORD_BYTES])
{
    if (out==NULL || available>31 || simulated>31 || (simulated & (uint8_t)~available) || state>5) return false;
    uint8_t b[10]; put32(b,ms); put32(b+4,boot);
    uint16_t flags=(uint16_t)(available | ((uint16_t)simulated<<5u) | ((uint16_t)state<<10u));
    b[8]=(uint8_t)flags; b[9]=(uint8_t)(flags>>8u);
    memcpy(out,b,sizeof b); return true;
}
bool binary_command_begin(binary_command_frame_t *f, uint8_t kind, size_t length)
{
    if (f==NULL) return false;
    memset(f,0,sizeof *f);
    if (kind<1 || kind>3 || length>2 ||
        (kind==BINARY_COMMAND_PERIOD ? length!=2 : length!=0)) return false;
    f->state=(uint8_t)(kind | 4u | (length<<3u)); return true;
}
bool binary_command_append(binary_command_frame_t *f,const uint8_t *b,size_t n,bool last,binary_command_t *out)
{
    if (f==NULL || out==NULL || !(f->state&4u)) return false;
    uint8_t expected=(f->state>>3u)&3u, received=(f->state>>5u)&3u;
    if (expected>2 || received>expected || !(f->state&3u)) { f->state=0; return false; }
    if ((b==NULL && n!=0) || n>(size_t)(expected-received)) { f->state=0; return false; }
    if (n) memcpy(f->payload+received,b,n);
    received=(uint8_t)(received+n);
    f->state=(uint8_t)((f->state&31u) | (received<<5u));
    if (!last) return false;
    uint8_t kind=f->state&3u; f->state=0;
    if (received!=expected) return false;
    uint16_t period=0;
    if (kind==BINARY_COMMAND_PERIOD) {
        period=(uint16_t)(f->payload[0] | ((uint16_t)f->payload[1]<<8u));
        if (period<200 || period>5000) return false;
    }
    *out=(binary_command_t){.kind=kind,.period_ms=period}; return true;
}
