#include "barcode_waveform.h"
#include "barcode_signal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "barcode_recorded_fixtures.h"

static barcode_waveform_t w,restored;
static barcode_signal_t filter;
static const unsigned patterns[26]={0x109,0x049,0x148,0x019,0x118,0x058,0x00d,
    0x10c,0x04c,0x01c,0x103,0x043,0x142,0x013,0x112,0x052,0x007,0x106,
    0x046,0x016,0x181,0x0c1,0x1c0,0x091,0x190,0x0d0};
static uint32_t clock_us;
static unsigned divider;
static unsigned reads;
static char result;
static uint32_t rng=12345;
static unsigned random_value(void){rng=rng*1664525u+1013904223u;return rng>>16;}
static void samples(uint16_t raw,unsigned duration)
{
    for(unsigned t=0;t<duration;t+=500u){
        clock_us+=500u;uint16_t v=barcode_signal_update(&filter,raw);
        if(++divider==4u){divider=0;char c=0;if(barcode_waveform_update(&w,v,clock_us,&c)){reads++;result=c;}}
    }
}
static void begin(void)
{
    barcode_waveform_begin(&w);filter=(barcode_signal_t){0};clock_us=0xffff0000u;
    reads=divider=0;result=0;
}
static void frame(unsigned letter,unsigned narrow,unsigned reverse,unsigned style,unsigned count)
{
    uint32_t runs[29];const unsigned p[]={0x094,patterns[letter],0x094};
    for(unsigned c=0;c<3;c++){
        for(unsigned e=0;e<9;e++)runs[c*10+e]=narrow*((p[c]&(1u<<(8-e)))?3u:1u);
        if(c<2)runs[c*10+9]=narrow;
    }
    for(unsigned i=0;i<count;i++){
        unsigned j=reverse?28-i:i;bool black=i%2==0;
        /* High contrast; grey gaps above 220; uniformly faint; variable
         * contrast: narrow peaks 260, wide peaks 1900, all gaps 200. */
        uint16_t raw=style==0?(black?2100:190):style==1?(black?2300:450):
            style==2?(black?265:215):(black?(runs[j]==narrow?260:1900):200);
        samples(raw,runs[j]);
    }
}
static void matrix(void)
{
    const unsigned widths[]={16000,30000,68000,136000};
    for(unsigned s=0;s<4;s++)for(unsigned scale=0;scale<4;scale++)
    for(unsigned c=0;c<26;c++)for(unsigned reverse=0;reverse<2;reverse++){
        begin();samples(190,600000);frame(c,widths[scale],reverse,s,29);samples(190,900000);
        if(reads!=1 || result!='A'+(int)c){
            fprintf(stderr,"style=%u narrow=%u letter=%c reverse=%u reads=%u got=%c reject=%u extrema=%u\n",
                s,widths[scale],'A'+c,reverse,reads,result?result:'-',w.rejected,w.last_extrema);assert(0);
        }
        assert(!w.timing_faults && !w.overflows);
    }
}
static void rejection_and_recovery(void)
{
    begin();samples(190,600000);frame(0,68000,0,1,27);samples(190,900000);
    assert(!reads && w.rejected);
    samples(190,600000);frame(25,68000,0,1,29);samples(190,900000);
    assert(reads==1 && result=='Z');
    begin();samples(190,600000);samples(2000,1500000);samples(190,900000);assert(!reads);
    begin();samples(190,600000);
    for(unsigned i=0;i<29;i++)samples(i%2?450:2300,68000);
    samples(190,900000);assert(!reads);
    begin();samples(190,600000);frame(0,68000,0,0,29);
    /* Incomplete trailing quiet must never trigger a route action. */
    samples(190,200000);assert(!reads);
    samples(190,100000);assert(reads==1);
    samples(190,600000);assert(reads==1);
    frame(0,68000,0,0,29);samples(190,600000);assert(reads==2);
    begin();samples(190,100000);frame(0,68000,0,0,29);samples(190,900000);assert(!reads);
    begin();samples(190,600000);samples(2100,100000);clock_us+=4000;
    char c=0;assert(!barcode_waveform_update(&w,2100,clock_us,&c));assert(w.timing_faults==1 && !w.recording);
    begin();samples(190,600000);
    for(unsigned i=0;i<1000;i++)samples(i%2?210:270,20000);
    assert(w.rejected && !reads); /* 16 s frame deadline precedes buffer end. */
    begin();samples(190,600000);samples(2100,100000);
    w.count=BARCODE_WAVE_SAMPLES;
    samples(2100,4000);assert(w.overflows==1 && !reads);
    begin();
    for(unsigned i=0;i<20000;i++)samples((uint16_t)(190+i%11),2000);
    assert(!reads && !w.recording);
}
static void noisy_corpus(void)
{
    unsigned accepted=0;
    for(unsigned trial=0;trial<3000;trial++){
        begin();samples(330,600000);
        unsigned c=random_value()%26u,narrow=30000+(random_value()%40u)*1000u;
        unsigned p[]={0x094,patterns[c],0x094},index=0;
        for(unsigned ch=0;ch<3;ch++){
            for(unsigned e=0;e<10;e++){
                if(ch==2 && e==9)break;
                unsigned width=narrow*((e<9 && (p[ch]&(1u<<(8-e))))?3u:1u);
                width=width*(85u+random_value()%31u)/100u;
                uint16_t level=(uint16_t)(index%2?450:1100);
                for(unsigned t=0;t<width;t+=2000u)samples((uint16_t)(level+random_value()%21u),2000);
                index++;
            }
        }
        samples(330,900000);
        assert(reads<=1);
        if(reads){assert(result=='A'+(int)c);accepted++;}
    }
    assert(accepted>1500); /* Prevent a reject-everything false-positive check. */
    printf("PASS noisy corpus: %u/3000 decoded, zero wrong letters (synthetic +/-15%% width jitter, ADC noise).\n",accepted);
}
static void prefix_and_export(void)
{
    /* The v5.57 exact-whole-recording rule rejects this construction:
     * low-prominence profiles see prefix bumps; higher profiles lose the
     * faint narrow bars. A complete framed window is still present. */
    for(unsigned c=0;c<26;c++)for(unsigned reverse=0;reverse<2;reverse++){
        begin();samples(190,600000);
        for(unsigned n=0;n<2;n++){samples(420,40000);samples(190,60000);}
        frame(c,68000,reverse,3,29);samples(190,900000);
        assert(reads==1 && result=='A'+(int)c);
        assert(w.saved.letter==result && w.saved.count);
        assert(w.saved.profile[0].windows>1);
        for(unsigned p=0;p<6;p++)assert(w.saved.profile[p].extrema!=31);
        uint32_t hash=2166136261u;
        for(unsigned i=0;i<w.saved.count;i++){
            hash=(hash^(w.saved.sample[i]&255u))*16777619u;
            hash=(hash^(w.saved.sample[i]>>8))*16777619u;
        }
        assert(hash==w.saved.checksum);
        char detail[4096];assert(barcode_waveform_details(&w.saved,detail,sizeof(detail)));
        assert(strstr(detail,"[WAVEFORM SNAPSHOT]") && strstr(detail,"strict_width_reason="));
    }
    begin();samples(190,600000);frame(0,30000,0,0,29);samples(190,100000);
    frame(25,30000,0,0,29);samples(190,900000);
    assert(!reads && w.ambiguous && w.saved.count);
    begin();samples(190,600000);frame(25,68000,0,3,15);
    barcode_waveform_save_partial(&w);assert(w.saved.reason==WAVE_STOPPED_PARTIAL && !w.saved.letter);
    begin();samples(190,600000);frame(0,68000,0,0,29);samples(190,900000);
    assert(w.saved.letter=='A');
    samples(2200,1200000);samples(190,900000);
    assert(reads==1 && !w.saved.letter && w.saved.reason==WAVE_NO_FRAME);
    puts("PASS 52 faint framed scans with prefix bumps, conflicting-window rejection, frozen samples/checksum/profile export and stopped-partial capture.");
}
/* A printed waveform indexed by physical forward encoder position. Changing
 * speed cannot change the printed widths; pauses and same-path recrosses add
 * no new ink. Exercise all letters/orientations without a target-letter hint. */
static void spatial_matrix(void)
{
    for(unsigned c=0;c<26;c++)for(unsigned reverse=0;reverse<2;reverse++){
        begin();float x=-260;unsigned state=0;bool recrossed=false,paused=false;
        unsigned widths[29],total=0;unsigned p[]={0x094,patterns[c],0x094};
        for(unsigned i=0;i<29;i++){
            unsigned ch=i/10,e=i%10;
            widths[i]=(e<9 && (p[ch]&(1u<<(8-e))))?120u:40u;
        }
        for(unsigned i=0;i<29;i++)total+=widths[i];
        for(unsigned tick=0;tick<7800;tick++){
            /* ~0.5 s leading paper; speed changes by up to4x, including
             * a600ms hold inside an interior stripe and a100-count recross. */
            if(x>520 && !paused){state=300;paused=true;}
            if(x>980 && !recrossed){state=1050;recrossed=true;}
            if(state>=1000){x-=1;state--;if(state==1000)state=0;}
            else if(state)state--;
            else x+=0.6f+(float)(((unsigned)(x+500)/120u)%4u)*0.6f;
            uint16_t raw=190;
            if(x>=0 && x<(float)total){unsigned at=(unsigned)x,run=0;
                while(run<29){unsigned width=widths[reverse?28-run:run];if(at<width)break;at-=width;run++;}
                raw=(run%2)?200:((widths[reverse?28-run:run]==40)?265:2100);
            }
            clock_us+=2000;char letter=0;int32_t pos=(int32_t)x;
            if(barcode_waveform_update_motion(&w,raw,clock_us,(uint32_t)-pos,(uint32_t)pos,&letter)){
                assert(letter=='A'+(int)c);reads++;
            }
            if(x>(float)total+500)break;
        }
        if(reads!=1){fprintf(stderr,"spatial %c reverse=%u reads=%u attempts=%u rejects=%u fault=%u\n",'A'+c,reverse,reads,w.attempt_count,w.rejected,w.motion_fault);assert(0);}
        assert(w.recrosses && !w.motion_faults && w.saved.motion && w.saved.count%2==0);
        char detail[4096];assert(barcode_waveform_details(&w.saved,detail,sizeof(detail)));
        assert(strstr(detail,"encoding=ADC12_MS4_POS16"));
        uint32_t hash=2166136261u;
        for(unsigned i=0;i<w.saved.count;i++){hash=(hash^(w.saved.sample[i]&255u))*16777619u;hash=(hash^(w.saved.sample[i]>>8))*16777619u;}
        assert(hash==w.saved.checksum && w.attempt_count==1 && w.attempts[0].letter=='A'+(int)c);
        barcode_waveform_begin(&restored);restored.motion=true;restored.count=w.saved.count/2;
        for(unsigned i=0;i<restored.count;i++){restored.sample[i]=w.saved.sample[2*i];restored.position[i]=(int16_t)w.saved.sample[2*i+1];}
        char replayed=0;assert(barcode_waveform_decode(&restored,&replayed) && replayed=='A'+(int)c);
    }
    begin();char letter=0;
    assert(!barcode_waveform_update_motion(&w,190,2000,0,0,&letter));
    assert(!barcode_waveform_update_motion(&w,190,4000,0,10000,&letter));
    assert(w.motion_faults && !reads);
    puts("PASS 52 spatial A-Z/orientation scans:4x speed variation,600ms stop, same-path recross, strict widths, attempt ledger and frozen odometry checksum; encoder jump rejected.");
}
/* Analytic constant-radius right bend. Ink is indexed by the world X
 * coordinate of a sensor80mm forward/85mm left, independent of the decoder's
 * incremental integration. A body-centre clock cannot represent this path. */
static void curved_matrix(void)
{
    for(unsigned c=0;c<26;c++)for(unsigned reverse=0;reverse<2;reverse++){
        begin();unsigned widths[29],total=0;unsigned p[]={0x094,patterns[c],0x094};
        for(unsigned i=0;i<29;i++){
            widths[i]=(i%10<9 && (p[i/10]&(1u<<(8-i%10))))?120u:40u;total+=widths[i];
        }
        for(unsigned tick=1;tick<5000;tick++){
            double u=tick*1.2,theta=0,x=u,left=u,right=u,mm=188.4955592/2543.0;
            if(u>700){
                double arc=u-700;theta=-arc*mm*(2.0/3.0)/115.0;
                x=700-115.0/mm*1.5*sin(theta)+(80*(cos(theta)-1)-85*sin(theta))/mm;
                left=700+arc*4.0/3.0;right=700+arc*2.0/3.0;
            }
            x-=300;uint16_t raw=190;
            if(x>=0 && x<total){unsigned at=(unsigned)x,run=0;
                while(run<29){unsigned width=widths[reverse?28-run:run];if(at<width)break;at-=width;run++;}
                raw=run%2?200:(widths[reverse?28-run:run]==40?265:2100);
            }
            clock_us+=2000;char letter=0;
            if(barcode_waveform_update_motion(&w,raw,clock_us,(uint32_t)-(int32_t)lround(left),(uint32_t)lround(right),&letter)){
                assert(letter=='A'+(int)c);reads++;
            }
            if(x>total+500)break;
        }
        if(reads!=1){fprintf(stderr,"curve %c reverse%u got%u faults%u\n",'A'+c,reverse,reads,w.motion_faults);assert(0);}
        assert(!w.motion_faults && w.saved.motion);
    }
    /* A reverse followed by a pivot that returns on a different lateral path
     * must not silently join two different traversals into a route command. */
    begin();char letter=0;int left=0,right=0;
    for(unsigned t=0;t<300;t++){left++;right++;clock_us+=2000;assert(!barcode_waveform_update_motion(&w,190,clock_us,(uint32_t)-left,(uint32_t)right,&letter));}
    for(unsigned t=0;t<50;t++){left++;right++;clock_us+=2000;assert(!barcode_waveform_update_motion(&w,2100,clock_us,(uint32_t)-left,(uint32_t)right,&letter));}
    left-=20;right-=20;clock_us+=2000;assert(!barcode_waveform_update_motion(&w,2100,clock_us,(uint32_t)-left,(uint32_t)right,&letter));
    left+=40;right-=40;clock_us+=2000;assert(!barcode_waveform_update_motion(&w,2100,clock_us,(uint32_t)-left,(uint32_t)right,&letter));
    assert(w.motion_fault && w.motion_faults && !reads);
    barcode_waveform_save_partial(&w);assert(w.saved.reason==WAVE_MOTION && w.attempts[0].reason==WAVE_MOTION);
    puts("PASS52 analytic right-bend A-Z/orientation scans with offset sensor; lateral path mismatch cannot produce a letter.");
}
static void spatial_sequence(void)
{
    begin();unsigned widths[2][29],total=0;const unsigned letters[]={0,25};
    for(unsigned f=0;f<2;f++)for(unsigned i=0;i<29;i++){
        unsigned ch=i/10,e=i%10,p=ch==1?patterns[letters[f]]:0x094;
        widths[f][i]=(e<9 && (p&(1u<<(8-e))))?120:40;
        if(!f)total+=widths[f][i];
    }
    for(unsigned pos=0;pos<2*total+1200;pos++){
        int x=(int)pos-300;unsigned frame=0;uint16_t raw=190;
        if(x>=(int)total+500){x-=(int)total+500;frame=1;}
        if(x>=0 && x<(int)total){unsigned run=0,at=(unsigned)x;
            while(run<29 && at>=widths[frame][run]){at-=widths[frame][run];run++;}
            raw=run%2?200:(widths[frame][run]==40?265:2100);
        }
        clock_us+=2000;char letter=0;
        if(barcode_waveform_update_motion(&w,raw,clock_us,(uint32_t)-(int)pos,pos,&letter)){
            assert(reads<2 && letter=='A'+(int)letters[reads]);reads++;
        }
    }
    assert(reads==2 && w.attempt_count==2 && w.attempts[0].letter=='A' && w.attempts[1].letter=='Z');
    assert(w.saved.letter=='Z' && !w.motion_faults);
    puts("PASS repeated spatial frames retain independent A/Z attempt records and rebase sensor coordinates.");
}
static void recorded_half_z(const char *path)
{
    begin();FILE *f=fopen(path,"rb");assert(f);
    w.count=(uint16_t)fread(w.sample,sizeof(w.sample[0]),BARCODE_WAVE_SAMPLES,f);
    assert(!ferror(f) && feof(f));fclose(f);assert(w.count==2202);
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<w.count;i++){hash=(hash^(w.sample[i]&255u))*16777619u;hash=(hash^(w.sample[i]>>8))*16777619u;}
    assert(hash==0xbec1cc17u);char letter=0;
    assert(barcode_waveform_decode(&w,&letter) && letter=='Z');
    puts("PASS archived physical half-Z decoder-input fixture:2202 samples, FNVbec1cc17, pattern Z; batch replay only.");
}
int main(int argc,char **argv)
{
    assert(argc==2);recorded_half_z(argv[1]);
    spatial_sequence();curved_matrix();spatial_matrix();matrix();rejection_and_recovery();noisy_corpus();prefix_and_export();
    for(unsigned i=0;i<sizeof(recorded)/sizeof(recorded[0]);i++)for(unsigned reverse=0;reverse<2;reverse++){
        begin();samples(190,600000);
        for(unsigned j=0;j<29;j++)samples(j%2?190:2100,recorded[i].widths[reverse?28-j:j]);
        samples(190,900000);assert(reads==1 && result==recorded[i].letter);
    }
    puts("PASS waveform: A-Z x both directions x 4 contrasts x 4 sizes (832 frames), actual 2kHz/6ms filter and 2ms reader, timestamp wrap, partial/noise/line rejection, quiet zones, duplicate prevention, repeated A, timing and storage bounds.");
}
