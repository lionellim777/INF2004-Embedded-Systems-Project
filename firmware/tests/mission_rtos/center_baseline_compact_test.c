#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define center_baseline_t reference_baseline_t
#define center_baseline_add reference_baseline_add
#define center_baseline_value reference_baseline_value
#include "center_baseline_reference.h"
#undef center_baseline_t
#undef center_baseline_add
#undef center_baseline_value
#undef CENTER_BASELINE_H
#undef CENTER_BASELINE_COMPACT
#define CENTER_BASELINE_COMPACT 1
#include "center_baseline.h"

static unsigned queries;
static void compare(const reference_baseline_t *r,const center_baseline_t *c,uint64_t now)
{
    int32_t rm=123,rs=456,cm=789,cs=123;
    bool a=reference_baseline_value(r,now,&rm,&rs),b=center_baseline_value(c,now,&cm,&cs);
    assert(a==b && rm==cm && rs==cs);queries++;
}
int main(void)
{
    const int32_t levels[]={INT32_MIN,-1,0,399,400,401,600,800,999,1000,1001,INT32_MAX};
    const uint32_t gaps[]={0,1,9999,10000,10001,99999,100000,100001};
    reference_baseline_t r={0};center_baseline_t c={0};uint64_t now=UINT32_MAX-200000u;
    uint32_t rng=0x934a712bu;
    for(unsigned i=0;i<40000;i++){
        rng=rng*1664525u+1013904223u;
        int32_t level=i%3u?levels[(rng>>16u)%12u]:(int32_t)((rng>>12u)%1200u)-100;
        now+=gaps[(rng>>24u)%8u];
        reference_baseline_add(&r,level,now);center_baseline_add(&c,level,now);
        compare(&r,&c,now);compare(&r,&c,now+100000u);compare(&r,&c,now+100001u);
    }
    for(unsigned series=0;series<3;series++){
        r=(reference_baseline_t){0};c=(center_baseline_t){0};
        now=(UINT64_C(1)<<48)+1000000u;
        for(unsigned i=0;i<100;i++){
            int32_t level=series==0?400:series==1?(i%4u?600:1000):(i%2u?0:1000);
            now+=10000;reference_baseline_add(&r,level,now);center_baseline_add(&c,level,now);
            compare(&r,&c,now);compare(&r,&c,now-1);
        }
    }
    reference_baseline_add(&r,500,1);center_baseline_add(&c,500,1);compare(&r,&c,1);
    printf("PASS %u baseline queries equal accepted source: fill/wrap, clamping, median/IQR gates,10ms spacing,100ms freshness, backwards time and times beyond32/48 bits.\n",queries);
    printf("Host baseline sizes: reference%zu compact%zu; target ARM layout measured separately.\n",sizeof r,sizeof c);
    return 0;
}
