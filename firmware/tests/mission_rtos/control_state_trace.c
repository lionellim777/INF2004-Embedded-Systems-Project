/* Differential stimulus. The runner compiles this against the accepted source
 * and candidate, comparing every logical field (including float bit patterns)
 * after each transition, without comparing struct padding or pointer values. */
#include "course_control.h"
#include <assert.h>
#include <limits.h>
#include "control_trace_dump.h"
static uint32_t rng=0x629a71d3u;
static uint32_t next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static encoder_sample_t at(encoder_sample_t start,int32_t l,int32_t r)
{start.left_raw-=(uint32_t)l;start.right_raw+=(uint32_t)r;return start;}
int main(void)
{
    for(unsigned scenario=0;scenario<320;scenario++){
        motion_control_t m;motion_init(&m);
        encoder_sample_t origin={next(),next(),next(),next()};
        uint64_t now=(UINT64_C(1)<<49)+next();
        motion_kind_t kind=(motion_kind_t)(1+scenario%4);
        assert(motion_begin(&m,kind,origin,now));
        /* Trusted internal profile flags exercise balancing/held-wheel paths. */
        m.slow_turn=(scenario&4)!=0;m.alignment_drive=(scenario&8)!=0;
        m.transit_drive=(scenario&16)!=0;m.slow_distance=(scenario&32)!=0;
        int32_t l=0,r=0;
        for(unsigned step=0;step<90;step++){
            now+=10000u+next()%40001u;
            if(scenario%8!=0){l+=(int32_t)(next()%100u)*(m.left.target<0?-1:1);r+=(int32_t)(next()%100u)*(m.right.target<0?-1:1);}
            encoder_sample_t sample=at(origin,l,r);
            if(step==8)switch(scenario%8){
                case 1:sample.left_invalid++;break;
                case 2:sample=at(origin,-100,100);break;
                case 3:now+=250001;break;
                case 4:now+=12000000;break;
                case 5:motion_abort(&m);break;
            }
            motion_poll(&m,sample,now);dump_motion_control_t(&m);
        }
    }
    /* Invalid external arguments remain wide and reject before any storage.
     * 17 would alias straight30cm if silently truncated to four bits first. */
    const int invalid[]={-1,0,5,8,9,16,17,99,INT_MAX};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++){
        motion_control_t m;motion_init(&m);
        assert(!motion_begin(&m,(motion_kind_t)invalid[i],(encoder_sample_t){0},0));
        dump_motion_control_t(&m);
    }
    const unsigned distances[]={0,1,30,699,700,701,UINT_MAX};
    for(unsigned i=0;i<sizeof distances/sizeof *distances;i++){
        motion_control_t m;motion_init(&m);
        bool began=motion_begin_distance(&m,distances[i],(encoder_sample_t){0},0);
        assert(began==(distances[i]>=1&&distances[i]<=700));dump_motion_control_t(&m);
    }
    for(unsigned scenario=0;scenario<240;scenario++){
        course_control_t c;
        encoder_sample_t origin={next(),next(),0,0};
        uint64_t now=(UINT64_C(1)<<49)+next();int32_t l=0,r=0;
        course_begin(&c,(course_config_t){scenario%3?'s':'r','s',120},origin,now);
        course_set_center_reference(&c,800);course_enable_flow_follow(&c,(scenario&1)!=0);
        if(scenario&2)c.drive.launch_complete=true;
        c.bend_disabled=(scenario&4)!=0;
        for(unsigned step=0;step<150;step++){
            now+=10000u+next()%40001u;
            l+=(int32_t)(next()%24u);r+=(int32_t)(next()%24u);
            if(c.phase==COURSE_LOST){l+=c.center_search.direction*10;r-=c.center_search.direction*10;}
            encoder_sample_t enc=at(origin,l,r);
            int32_t center=step%40<28?800:(int32_t)(next()%1001u);
            int32_t right=step%40>=23&&step%40<35?850:0;
            course_center_level(&c,center);course_right_level(&c,right);
            if(step==70)course_note_barcode_activity(&c,now);
            if(step==90)(void)course_on_barcode(&c,(char)('A'+scenario%26),enc,now,false);
            if(step==120&&scenario%6==0)enc.right_invalid++;
            line_check_output_t out=course_step(&c,(ir_roles_t){0,center>=250,right>=250},enc,now);
            dump_course_control_t(&c);dump_line_check_output_t(&out);
        }
    }
    return 0;
}
