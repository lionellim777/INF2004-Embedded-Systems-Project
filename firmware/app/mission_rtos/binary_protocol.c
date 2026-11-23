#include "binary_protocol.h"
#include <string.h>
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
