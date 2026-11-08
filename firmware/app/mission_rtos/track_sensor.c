#include "track_sensor.h"
#include "barcode_signal.h"
#include "barcode_waveform.h"
#include "encoders.h"
#include "network_service.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <string.h>

#define QUEUE_SIZE 512u
#define EVENT_SIZE 8u
/* us32 + raw encoder32x2 + ADC12: 108 bits in 14 bytes. Four zero pad bits.
 * Preserve raw encoder wrap and the validated 2ms waveform/filter precision. */
static uint8_t queue[QUEUE_SIZE][14],events[EVENT_SIZE][5];
static uint16_t head,tail,event_head,event_tail,requested,active;
static bool enabled;
static track_ir_t latest;
static track_diagnostics_t diagnostics;
static repeating_timer_t timer;
static barcode_signal_t filter;
static barcode_waveform_t waveform; /* exact v5.63 decoder and geometry */
static unsigned divider;

static void put32(uint8_t *p,uint32_t n)
{ for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8u*i)); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8u)|((uint32_t)p[2]<<16u)|((uint32_t)p[3]<<24u); }
static uint16_t adc_mean(unsigned input)
{
    adc_select_input(input);busy_wait_us_32(5);
    uint32_t total=0;
    for(unsigned i=0;i<8;i++)total+=adc_read();
    return (uint16_t)(total/8u);
}
static bool sample(repeating_timer_t *unused)
{
    (void)unused;
    uint32_t now=time_us_32();
    /* GP26 barcode, GP28 centre, GP27 right: same order as v5.63. */
    latest.raw[0]=adc_mean(0);latest.raw[1]=adc_mean(2);latest.raw[2]=adc_mean(1);
    latest.us=now;
    uint16_t grey=barcode_signal_update(&filter,latest.raw[0]);
    if(enabled && ++divider==4u){
        divider=0;
        uint16_t next=(uint16_t)((head+1u)%QUEUE_SIZE);
        if(next==tail)diagnostics.faults|=TRACK_QUEUE_FAULT;
        else {
            encoder_sample_t e=encoders_snapshot();uint8_t *p=queue[head];
            put32(p,now);put32(p+4,e.left_raw);put32(p+8,e.right_raw);
            p[12]=(uint8_t)grey;p[13]=(uint8_t)(grey>>8u);
            head=next;
            unsigned depth=(head+QUEUE_SIZE-tail)%QUEUE_SIZE;
            if(depth>diagnostics.queue_peak)diagnostics.queue_peak=(uint16_t)depth;
            diagnostics.samples++;
        }
    }
    uint32_t duration=time_us_32()-now;
    if(duration>diagnostics.adc_max_us)diagnostics.adc_max_us=duration;
    return true;
}
bool track_sensor_init(void)
{
    enabled=false;requested=active=0;head=tail=event_head=event_tail=0;divider=0;
    latest=(track_ir_t){0};diagnostics=(track_diagnostics_t){0};filter=(barcode_signal_t){0};
    adc_init();adc_gpio_init(26);adc_gpio_init(28);adc_gpio_init(27);
    latest.ready=add_repeating_timer_us(-(int64_t)TRACK_SAMPLE_US,sample,NULL,&timer);
    return latest.ready;
}
void track_sensor_read(track_ir_t *out)
{
    uint32_t irq=save_and_disable_interrupts();*out=latest;out->age_us=time_us_32()-latest.us;restore_interrupts(irq);
}
void track_request(uint16_t token)
{
    uint32_t irq=save_and_disable_interrupts();
    requested=token;enabled=true;head=tail=event_head=event_tail=0;divider=0;
    diagnostics=(track_diagnostics_t){0};
    restore_interrupts(irq);
    (void)network_barcode((uint32_t)(time_us_64()/1000u),0,false,false);
}
void track_stop(void)
{
    uint32_t irq=save_and_disable_interrupts();enabled=false;head=tail=event_head=event_tail=0;
    restore_interrupts(irq);
}
bool track_ready(uint16_t token)
{
    uint32_t irq=save_and_disable_interrupts();
    bool result=enabled && requested==token && diagnostics.ack==token;
    restore_interrupts(irq);return result;
}
bool track_faulted(void)
{
    uint32_t irq=save_and_disable_interrupts();bool result=diagnostics.faults!=0;
    restore_interrupts(irq);return result;
}
bool track_recording(void)
{
    uint32_t irq=save_and_disable_interrupts();bool result=enabled && diagnostics.recording;
    restore_interrupts(irq);return result;
}
void track_diagnostics(track_diagnostics_t *out)
{
    uint32_t irq=save_and_disable_interrupts();*out=diagnostics;restore_interrupts(irq);
}
bool track_event_take(uint16_t token,track_event_t *out)
{
    uint32_t irq=save_and_disable_interrupts();
    bool found=enabled && requested==token && event_tail!=event_head;
    if(found){
        uint8_t *p=events[event_tail];
        *out=(track_event_t){get32(p),(char)('A'-1+(p[4]&31u)),(p[4]&32u)!=0};
        event_tail=(uint16_t)((event_tail+1u)%EVENT_SIZE);
    }
    restore_interrupts(irq);return found;
}
static bool current(uint16_t token)
{ return enabled && requested==token; }
void track_service(void)
{
    uint32_t irq=save_and_disable_interrupts();
    uint16_t token=requested;bool running=enabled;
    restore_interrupts(irq);
    if(!running)return;
    if(active!=token){
        barcode_waveform_begin(&waveform);active=token;
        irq=save_and_disable_interrupts();
        if(current(token))diagnostics.ack=token;
        restore_interrupts(irq);
    }
    for(unsigned work=0;work<24;work++){
        uint8_t p[14];
        irq=save_and_disable_interrupts();
        bool found=current(token) && tail!=head;
        if(found){memcpy(p,queue[tail],sizeof p);tail=(uint16_t)((tail+1u)%QUEUE_SIZE);}
        restore_interrupts(irq);
        if(!found)break;
        char letter=0;uint32_t start=time_us_32();
        bool read=barcode_waveform_update_motion(&waveform,(uint16_t)(p[12]|((uint16_t)p[13]<<8u)),get32(p),get32(p+4),get32(p+8),&letter);
        uint32_t duration=time_us_32()-start;
        irq=save_and_disable_interrupts();
        if(current(token)){
            if(duration>diagnostics.decode_max_us)diagnostics.decode_max_us=duration;
            diagnostics.frames=waveform.frames;diagnostics.rejected=waveform.rejected;
            diagnostics.recording=waveform.recording;
            if(waveform.timing_faults)diagnostics.faults|=TRACK_TIMING_FAULT;
            if(read){
                uint16_t next=(uint16_t)((event_head+1u)%EVENT_SIZE);
                if(next==event_tail)diagnostics.faults|=TRACK_EVENT_FAULT;
                else if(letter>='A' && letter<='Z'){
                    bool crossing=latest.raw[1]>=861u && latest.raw[2]>=317u;
                    put32(events[event_head],get32(p));
                    events[event_head][4]=(uint8_t)(letter-'A'+1u)|(crossing?32u:0u);
                    event_head=next;diagnostics.events++;
                    uint64_t now=time_us_64();uint32_t age=(uint32_t)now-get32(p);
                    (void)network_barcode((uint32_t)((now-age)/1000u),letter,true,false);
                }
            }
        }
        restore_interrupts(irq);
    }
}
