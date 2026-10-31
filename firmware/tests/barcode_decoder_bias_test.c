#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "barcode_decoder.h"
#include "barcode_drive.h"
#include "barcode_recorded_fixtures.h"
static const uint16_t letters[]={0x109,0x049,0x148,0x019,0x118,0x058,0x00d,0x10c,0x04c,0x01c,0x103,0x043,0x142,0x013,0x112,0x052,0x007,0x106,0x046,0x016,0x181,0x0c1,0x1c0,0x091,0x190,0x0d0};
static void make(uint16_t symbol,int bias,uint32_t *w){
    const uint16_t pat[]={0x094,symbol,0x094};
    for(unsigned i=0;i<29;i++){
        unsigned bit=i%10;bool wide=bit<9 && (pat[i/10]&(1u<<(8-bit)));
        int width=wide?300:100;
        w[i]=(uint32_t)(width+(i%2?bias:-bias));
    }
}
/* Packed storage keeps 1 ms steps. Small synthetic widths (<=750) are taken
 * as whole milliseconds, so storage is exact and must decode identically.
 * Larger (microsecond) widths must decode the same as their 1 ms rounding. */
static bool both(const uint32_t *w,char *c){
    barcode_runs_t p={{0}};
    uint32_t scale=1000u,q[29];
    for(unsigned i=0;i<29;i++){
        if(w[i]>BARCODE_RUN_MAX_US/BARCODE_RUN_QUANTUM){scale=1u;}
    }
    for(unsigned i=0;i<29;i++){
        uint32_t v=w[i]*scale;
        assert(barcode_runs_set(&p,i,v));
        q[i]=((v+BARCODE_RUN_QUANTUM/2u)/BARCODE_RUN_QUANTUM)*BARCODE_RUN_QUANTUM;
    }
    char other=0,rounded=0;bool a=barcode_decode_code39_letter(w,c);
    bool aq=barcode_decode_code39_letter(q,&rounded);
    bool b=barcode_decode_code39_packed(&p,&other);
    assert(aq==b && (!b || rounded==other));
    if(scale!=1u){
        assert(a==b && (!a || *c==other));
    }
    return a;
}
/* Real logged captures must give the same letter after 1 ms storage. */
static char packed_letter(const uint32_t *w){
    barcode_runs_t p={{0}};char letter=0;
    for(unsigned i=0;i<29;i++)assert(barcode_runs_set(&p,i,w[i]));
    return barcode_decode_code39_packed(&p,&letter)?letter:0;
}
int main(void){
    uint32_t w[29],r[29];char c=0;
    for(unsigned i=0;i<sizeof(recorded)/sizeof(recorded[0]);i++){
        bool ok=both(recorded[i].widths,&c);
        printf("Replay %s: %s %c\n",recorded[i].name,ok?"accepted":"rejected",ok?c:'-');
        assert(ok && c==recorded[i].letter);
        assert(packed_letter(recorded[i].widths)==recorded[i].letter);
        barcode_capture_t capture;barcode_capture_begin_drive(&capture);
        barcode_batch_t batch={.count=1,.edges={{1000,true}},.stats={.samples=1,.last_us=1000,.black=true}};
        barcode_capture_consume(&capture,&batch);
        for(unsigned j=0;j<29;j++){
            batch.stats.last_us+=recorded[i].widths[j];batch.stats.samples++;
            batch.stats.black=!batch.stats.black;
            batch.edges[0]=(barcode_edge_t){batch.stats.last_us,batch.stats.black};
            barcode_capture_consume(&capture,&batch);
        }
        assert(capture.decoded==recorded[i].letter && !capture.rejected && !capture.timeouts);
        line_check_output_t stop=barcode_capture_stop((line_check_output_t){.left=0.2f,.right=0.2f},&capture);
        assert(stop.stop && stop.left==0 && stop.right==0 && !strcmp(stop.state,"BARCODE_READ"));
        for(unsigned j=0;j<29;j++)r[j]=recorded[i].widths[28-j];
        assert(both(r,&c) && c==recorded[i].letter);
        assert(packed_letter(r)==recorded[i].letter);
    }
    /* Exact v5.30 half-A edges through the continuous acquisition path.
     * One event only, no expected-letter input, and no duplicate from trailing noise. */
    {
        barcode_capture_t capture;barcode_capture_begin_stream(&capture,0);
        capture.waiting_gap=true;
        barcode_batch_t batch={.stats={.samples=1,.last_us=1000,.black=false}};
        barcode_capture_consume(&capture,&batch);
        batch.stats.last_us+=300000;batch.stats.samples++;
        barcode_capture_consume(&capture,&batch);
        batch.count=1;batch.stats.last_us+=1000;batch.stats.black=true;
        batch.edges[0]=(barcode_edge_t){batch.stats.last_us,true};
        barcode_capture_consume(&capture,&batch);
        for(unsigned j=0;j<29;j++){
            batch.stats.last_us+=recorded[0].widths[j];batch.stats.samples++;
            batch.stats.black=!batch.stats.black;
            batch.edges[0]=(barcode_edge_t){batch.stats.last_us,batch.stats.black};
            barcode_capture_consume(&capture,&batch);
            assert(capture.event_count==(j==28?1u:0u));
        }
        assert(capture.events[0].letter=='A' && capture.frames==1 && !capture.rejected && !capture.timeouts);
        batch.stats.last_us+=39000;batch.stats.black=true;
        batch.edges[0]=(barcode_edge_t){batch.stats.last_us,true};barcode_capture_consume(&capture,&batch);
        batch.stats.last_us+=76000;batch.stats.black=false;
        batch.edges[0]=(barcode_edge_t){batch.stats.last_us,false};barcode_capture_consume(&capture,&batch);
        assert(capture.event_count==1 && !capture.reader.active);
    }
    /* All supported letters; equal widths, black erosion and black dilation. */
    for(unsigned letter=0;letter<26;letter++)for(int bias=-40;bias<=40;bias+=20){
        make(letters[letter],bias,w);
        assert(both(w,&c) && c==(char)('A'+letter));
        for(unsigned j=0;j<29;j++)r[j]=w[28-j];
        assert(both(r,&c) && c==(char)('A'+letter));
    }
    /* One expanded physical outer guard bar: all A-Z, both directions,
     * several time scales and colour biases. Payload never gets this tolerance. */
    for(unsigned letter=0;letter<26;letter++)for(unsigned endpoint=0;endpoint<=28;endpoint+=28)
    for(unsigned scale=1;scale<=100;scale*=10)for(int bias=-20;bias<=20;bias+=20){
        make(letters[letter],bias,w);
        w[endpoint]=230-bias;
        for(unsigned j=0;j<29;j++)w[j]*=scale;
        assert(both(w,&c) && c==(char)('A'+letter));
        for(unsigned j=0;j<29;j++)r[j]=w[28-j];
        assert(both(r,&c) && c==(char)('A'+letter));
    }
    /* The bounds are inclusive; beyond 80% of the shortest wide is ambiguous. */
    for(unsigned endpoint=0;endpoint<=28;endpoint+=28){
        make(letters[0],0,w);w[endpoint]=240;assert(both(w,&c)&&c=='A');
        w[endpoint]=241;assert(!both(w,&c));
        w[endpoint]=300;assert(!both(w,&c));
        /* With a large wide:narrow ratio, the independent 3x median cap binds. */
        make(letters[0],0,w);for(unsigned i=0;i<29;i++)if(w[i]==300)w[i]=500;
        w[endpoint]=300;assert(both(w,&c)&&c=='A');
        w[endpoint]=301;assert(!both(w,&c));
    }
    make(letters[0],0,w);w[0]=w[28]=230;assert(!both(w,&c));
    for(unsigned i=1;i<28;i++){
        make(letters[0],0,w);
        if(w[i]!=100 || i==9 || i==19)continue;
        w[i]=230;assert(!both(w,&c)); /* no interior narrow-bar repair */
    }
    /* Even with the fallback eligible, every guard element must spell '*'. */
    for(unsigned mask=0;mask<512;mask++){
        make(letters[0],0,w);w[28]=230;
        for(unsigned j=0;j<9;j++)w[j]=(mask&(1u<<(8-j)))?300:100;
        assert(both(w,&c)==(mask==0x094));
    }
    /* Exhaust every9-bit pattern: only A-Z are data, only '*' is a guard. */
    for(unsigned mask=0;mask<512;mask++){
        make(mask,25,w);char expected=0;
        for(unsigned j=0;j<26;j++)if(letters[j]==mask)expected=(char)('A'+j);
        assert(both(w,&c)==(expected!=0));if(expected)assert(c==expected);
        make(letters[0],0,w);
        for(unsigned j=0;j<9;j++)w[j]=(mask&(1u<<(8-j)))?300:100;
        assert(both(w,&c)==(mask==0x094));
    }
    /* Dropouts/zero gaps, extra-wide elements, weak separation and ties. */
    for(unsigned i=0;i<29;i++){
        make(letters[0],0,w);w[i]=0;assert(!both(w,&c));
        make(letters[0],0,w);w[i]=750000;assert(!both(w,&c));
    }
    for(unsigned gap=9;gap<=19;gap+=10){make(letters[0],0,w);w[gap]=300;assert(!both(w,&c));}
    make(letters[0],0,w);for(unsigned i=0;i<29;i++)if(w[i]==300)w[i]=170;
    assert(!both(w,&c));
    make(letters[0],0,w);for(unsigned i=0;i<29;i++)if(w[i]==300)w[i]=180;
    assert(both(w,&c) && c=='A');
    for(unsigned i=0;i<29;i++)if(w[i]==180)w[i]=179;
    assert(!both(w,&c));
    for(unsigned i=0;i<29;i++)w[i]=100;
    assert(!both(w,&c));
    puts("PASS recorded captures, both directions, A-Z bias matrix, bounded outer-guard expansion and continuous replay, exhaustive symbol/guard masks, malformed/weak frames and packed equivalence.");
}
