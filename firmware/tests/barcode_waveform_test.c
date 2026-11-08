#include "barcode_waveform.h"
#include "barcode_signal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "barcode_recorded_fixtures.h"
#ifndef BARCODE_WAVE_RECOVERY_OPTICS
#define BARCODE_WAVE_RECOVERY_OPTICS 0
#endif

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
    assert(!w.motion_fault && !w.motion_faults && w.lateral_recrosses && !reads);
    barcode_waveform_save_partial(&w);assert(w.saved.reason==WAVE_STOPPED_PARTIAL && !w.saved.letter);
    puts("PASS52 analytic right-bend A-Z/orientation scans with offset sensor; lateral shift alone does not invent a letter or invalidate long bars.");
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
static void new_bend_fixture(const char *path)
{
    begin();FILE*f=fopen(path,"rb");assert(f);uint16_t words[16384];
    unsigned n=(unsigned)fread(words,2,16384,f);fclose(f);assert(n==4924);
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<n;i++){hash=(hash^(words[i]&255))*16777619u;hash=(hash^(words[i]>>8))*16777619u;}
    assert(hash==0x9a48ce7eu);w.motion=true;w.count=n/2;
    for(unsigned i=0;i<w.count;i++){w.sample[i]=words[2*i];w.position[i]=(int16_t)words[2*i+1];}
    char letter=0;assert(barcode_waveform_decode(&w,&letter) && letter=='Z');
    assert((w.coherent_mask&0xcu)==0xcu);
    /* The earlier half-A has a complete bounded-width A candidate. */
    uint32_t a[29]={19762,61431,72229,72455,70852,68604,20829,119475,26725,60867,71206,67327,21000,123666,25564,47770,26600,62891,66804,64133,26238,58806,71261,73630,67544,70343,9750,131311,15219};
    assert(!barcode_decode_code39_letter(a,&letter));
    assert(barcode_decode_code39_coherent(a,&letter) && letter=='A');
    /* Exhaust invalid guard ranks; no amount of bounded jitter may invent *. */
    unsigned rejected=0;
    for(unsigned guard=0;guard<512;guard++){
        if(guard==0x094 || guard==0x052)continue;
        for(unsigned i=0;i<29;i++){unsigned ch=i/10,e=i%10,p=ch==1?0x109:guard;
            a[i]=(e<9 && (p&(1u<<(8-e))))?80000:40000;}
        assert(!barcode_decode_code39_coherent(a,&letter));rejected++;
    }
    for(unsigned i=0;i<29;i++){unsigned ch=i/10,e=i%10,p=ch==1?0x109:0x094;
        a[i]=(e<9 && (p&(1u<<(8-e))))?56000:40000;}
    assert(!barcode_decode_code39_coherent(a,&letter)); /*1.4 ratio */
    printf("PASS captured bend Z consensus and half-A width candidate; %u invalid guard pairs and1.4 ratio rejected. Batch evidence only.\n",rejected);
}
static void geometry_model_matrix(void)
{
    unsigned decoded=0;
    for(unsigned c=0;c<26;c++)for(unsigned reverse=0;reverse<2;reverse++)for(unsigned size=0;size<2;size++){
        uint32_t runs[29];unsigned pat[]={0x094,patterns[c],0x094};
        unsigned n=size?81000:40500;
        for(unsigned i=0;i<29;i++){
            bool wide=i%10<9 && (pat[i/10]&(1u<<(8-i%10)));
            /* Edge bias cancels at adjacent black-bar centres, unlike raw
             * narrow-black duration. Narrow appearance can be <old8 counts. */
            int bias=(int)(n*85/100);runs[i]=n*(wide?3u:1u)+(i%2?bias:-bias);
        }
        uint32_t ordered[29];for(unsigned i=0;i<29;i++)ordered[i]=runs[reverse?28-i:i];
        char letter=0;assert(barcode_decode_code39_geometry(ordered,&letter) && letter=='A'+(int)c);decoded++;
        ordered[14]+=n*2;assert(!barcode_decode_code39_geometry(ordered,&letter)); /* missing/expanded interior element */
    }
    unsigned rejected=0;
    for(unsigned g=0;g<512;g++){
        if(g==0x094||g==0x052)continue;
        uint32_t runs[29];for(unsigned i=0;i<29;i++)runs[i]=(i%10<9 && ((i/10==1?0x109:g)&(1u<<(8-i%10))))?121500:40500;
        char letter=0;assert(!barcode_decode_code39_geometry(runs,&letter));rejected++;
    }
    printf("PASS geometry model:%u A-Z/orientation/print-size biased frames; expanded interior elements and %u invalid guard pairs rejected.\n",decoded,rejected);
}
static void long_bar_recross(void)
{
    for(unsigned outside=0;outside<2;outside++){
        begin();w.motion=true;float x=-300;bool backed=false;unsigned reverse=0;
        for(unsigned tick=0;tick<4000;tick++){
            if(x>=980&&!backed){backed=true;reverse=50;}
            x+=reverse?-1:1;if(reverse)reverse--;
            float y=backed?(outside?2100.0f:135.0f):0; /* ~156mm or10mm */
            uint16_t raw=190;int at=(int)x;
            for(unsigned i=0;i<29&&at>=0;i++){
                unsigned p=i/10==1?0x0d0:0x094;
                int width=(i%10<9 && (p&(1u<<(8-i%10))))?120:40;
                if(at<width){if(!outside || !backed)raw=i%2?200:2100;break;}
                at-=width;
            }
            clock_us+=2000;w.sensor_x=x;w.sensor_y=y;char letter=0;
            if(barcode_waveform_update(&w,raw,clock_us,&letter)){assert(letter=='Z');reads++;}
        }
        if(outside)assert(!reads); /* after leaving the print, guard is missing */
        else assert(reads==1 && !w.motion_faults && w.lateral_recrosses);
    }
    puts("PASS10mm transverse recross within simulated260mm bars; leaving the printed band cannot complete a partial frame.");
}
static void short_optical_bar(void)
{
    unsigned recovered=0;
    for(unsigned c=0;c<26;c++)for(unsigned reverse=0;reverse<2;reverse++)for(unsigned size=0;size<2;size++){
        uint32_t runs[29];unsigned n=size?81000:40500;
        for(unsigned i=0;i<29;i++){
            unsigned p=i/10==1?patterns[c]:0x094;
            bool wide=i%10<9 && (p&(1u<<(8-i%10)));
            runs[i]=n*(wide?3u:1u)+(i%2?(int)(n*8/10):-(int)(n*8/10));
        }
        /* Shorten ONE measured narrow black, moving its two optical edges
         * inwards equally. All actual black-bar centres remain unchanged. */
        unsigned delta=runs[2]-2100; runs[2]=2100;runs[1]+=delta/2;runs[3]+=delta-delta/2;
        for(unsigned bad=0;bad<5;bad++){
            uint32_t r[29];memcpy(r,runs,sizeof r);
            if(bad==1){unsigned d=r[22]-2100;r[22]=2100;r[21]+=d/2;r[23]+=d-d/2;}
            if(bad==2)r[2]=0; /* missing edge/bar */
            if(bad==3)r[1]=2100; /* white gap below floor */
            if(bad==4){ /* a narrow payload bar becomes a THIRD wide black */
                unsigned i=10;while(patterns[c]&(1u<<(8-i%10)))i+=2;
                r[i]+=n*2;
            }
            begin();w.motion=true;unsigned position=0;w.sample[0]=190;w.position[0]=0;w.count=1;
            for(unsigned i=0;i<29;i++){
                unsigned j=reverse?28-i:i,counts=(r[j]+500)/1000;
                for(unsigned k=0;k<counts;k++){
                    assert(w.count<BARCODE_WAVE_SAMPLES);position++;
                    w.position[w.count]=(int16_t)position;w.sample[w.count++]=(uint16_t)(0x2000u|(i%2?190:2300));
                }
            }
            w.position[w.count]=(int16_t)(++position);w.sample[w.count++]=0x2000u|190;
            char letter=0;bool ok=barcode_waveform_decode(&w,&letter);
            if(!bad && BARCODE_WAVE_RECOVERY_OPTICS){
                if(!ok || letter!='A'+(int)c){
                    fprintf(stderr,"short-bar fixture c=%c reverse=%u size=%u got=%c geometry=%x extrema=%u reason=%u bad_run=%u width=%u\n",'A'+c,reverse,size,letter?letter:'-',w.geometry_mask,w.profile[5].extrema,w.profile[5].reason,w.profile[5].bad_run,w.profile[5].bad_width);
                    for(unsigned i=0;i<29;i++)fprintf(stderr,"%u%s",w.profile[5].runs[i],i==28?"\n":",");
                }
                assert(ok && letter=='A'+(int)c);recovered++;
            }
            else assert(!ok);
        }
    }
    printf("PASS short optical bar: %u recovered A-Z/orientation/size frames; multiple tiny bars, missing bar, tiny white gap and expanded interior rejected.\n",recovered);
}
static void rtos_bend_fixture(const char *path)
{
    begin();FILE *f=fopen(path,"rb");assert(f);uint16_t words[16384];
    unsigned n=(unsigned)fread(words,2,16384,f);fclose(f);assert(n==8982);
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<n;i++){hash=(hash^(words[i]&255u))*16777619u;hash=(hash^(words[i]>>8u))*16777619u;}
    assert(hash==0x9b9bfd4au);w.motion=true;w.count=n/2;
    for(unsigned i=0;i<w.count;i++){w.sample[i]=words[2*i];w.position[i]=(int16_t)words[2*i+1];}
    char letter=0;bool ok=barcode_waveform_decode(&w,&letter);
    if(BARCODE_WAVE_RECOVERY_OPTICS){assert(ok && letter=='Z' && (w.coherent_mask&0x38u)==0x38u);}
    else assert(!ok);
    /* Feed the same retained ADC/time/projected positions through streaming
     * framing, with its documented leading white duration. No expected letter
     * enters the decoder. This is a captured-input replay, not a new run. */
    begin();w.motion=true;
    for(unsigned i=0;i<271;i++){clock_us+=2000;w.sensor_x=(float)((int)i-271);char c=0;assert(!barcode_waveform_update(&w,166,clock_us,&c));}
    for(unsigned i=0;i<n/2;i++){
        uint16_t value=words[2*i];clock_us+=(uint32_t)(value>>12)*1000u;
        w.sensor_x=(float)(int16_t)words[2*i+1];char c=0;
        if(barcode_waveform_update(&w,value&4095u,clock_us,&c)){assert(c=='Z');reads++;}
    }
    if(BARCODE_WAVE_RECOVERY_OPTICS)assert(reads==1);else assert(!reads);
    printf("PASS physical RTOS bend input: FNV %08x, batch and captured-cadence replay, recovery mode=%u, reads=%u.\n",hash,BARCODE_WAVE_RECOVERY_OPTICS,reads);
}
int main(int argc,char **argv)
{
    geometry_model_matrix();long_bar_recross();
    assert(argc==4);recorded_half_z(argv[1]);new_bend_fixture(argv[2]);short_optical_bar();rtos_bend_fixture(argv[3]);
    spatial_sequence();curved_matrix();spatial_matrix();matrix();rejection_and_recovery();noisy_corpus();prefix_and_export();
    for(unsigned i=0;i<sizeof(recorded)/sizeof(recorded[0]);i++)for(unsigned reverse=0;reverse<2;reverse++){
        begin();samples(190,600000);
        for(unsigned j=0;j<29;j++)samples(j%2?190:2100,recorded[i].widths[reverse?28-j:j]);
        samples(190,900000);assert(reads==1 && result==recorded[i].letter);
    }
    puts("PASS waveform: A-Z x both directions x 4 contrasts x 4 sizes (832 frames), actual 2kHz/6ms filter and 2ms reader, timestamp wrap, partial/noise/line rejection, quiet zones, duplicate prevention, repeated A, timing and storage bounds.");
}
