#include "adc12_filter.h"
#include "barcode_signal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static adc12_filter_t compact;
static barcode_signal_t reference;
static unsigned checked;
static void begin(void)
{memset(&compact,0,sizeof compact);reference=(barcode_signal_t){0};}
static void compare(uint16_t raw)
{
    uint16_t actual=0xffffu,expected=barcode_signal_update(&reference,raw);
    assert(adc12_filter_update(&compact,raw,&actual));
    assert(actual==expected);checked++;
}
int main(void)
{
    for(unsigned raw=0;raw<=4095u;raw++){
        begin();
        for(unsigned i=0;i<25;i++)compare((uint16_t)(i%2u?4095u-raw:raw));
    }
    begin();
    for(unsigned i=0;i<100;i++)compare(4095u);
    for(unsigned i=0;i<100;i++)compare(0);
    uint32_t rng=0x8a42fc31u;
    for(unsigned i=0;i<1000000;i++){
        rng=rng*1664525u+1013904223u;compare((uint16_t)(rng>>20u));
    }
    adc12_filter_t before=compact;uint16_t unchanged=1234;
    assert(!adc12_filter_update(&compact,4096,&unchanged));
    assert(!adc12_filter_update(&compact,UINT16_MAX,&unchanged));
    assert(unchanged==1234 && !memcmp(&before,&compact,sizeof before));
    assert(!adc12_filter_update(NULL,0,&unchanged));
    assert(!adc12_filter_update(&compact,0,NULL));
    compact.state=15;before=compact;
    assert(!adc12_filter_update(&compact,0,&unchanged));
    assert(!memcmp(&before,&compact,sizeof before));
    printf("PASS %u ADC12 updates equal the existing filter exactly: warm-up, all4096 levels, alternating rails and1000000 random readings; invalid values do not mutate state.\n",checked);
    printf("Host sizes: legacy%zu compact%zu; ARM layout is measured separately.\n",sizeof reference,sizeof compact);
    return 0;
}
