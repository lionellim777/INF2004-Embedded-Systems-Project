/* v5.7 dedicated hand-scan diagnostic. No PWM/encoder/navigation code is linked.
 * All four motor inputs are held LOW for the whole lifetime of this program. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "barcode_acquisition.h"
#include "calibration_wifi.h"
#define TRACE_CAPACITY 128u
#define SCAN_LIMIT_US 45000000u
static barcode_sampler_t sampler;
static barcode_batch_t batch;
static barcode_capture_t capture;
static barcode_edge_t trace[TRACE_CAPACITY];
static unsigned trace_count;
static bool trace_overflow;
static uint32_t scan_origin;
static volatile uint16_t raw;
static volatile uint32_t last_sample_us, max_adc_us;
static uint32_t checked_sample_age_us, stale_fault_age_us;
static repeating_timer_t timer;

static void motor_outputs_off(void)
{
    for (uint pin=8;pin<=11;pin++) { gpio_put(pin,0); }
}
static bool sample_alarm(repeating_timer_t *t)
{
    (void)t;
    uint32_t start=time_us_32(),total=0;
    for(unsigned i=0;i<8;i++)total+=adc_read();
    raw=(uint16_t)(total/8u);last_sample_us=start;
    barcode_sampler_sample(&sampler,raw,start);
    uint32_t duration=time_us_32()-start;
    if(sampler.enabled && duration>max_adc_us)max_adc_us=duration;
    return true;
}
static bool sample_fresh(void)
{
    /* Keep the sample timestamp and current clock coherent. Otherwise an ADC
     * interrupt between reads can make last_sample_us newer than now, wrapping
     * unsigned age to ~UINT32_MAX and falsely aborting a healthy scan.
     * Save/restore supports callers that already hold the interrupt mask. */
    uint32_t irq=save_and_disable_interrupts();
    uint32_t sampled=last_sample_us;
    uint32_t now=time_us_32();
    restore_interrupts(irq);
    checked_sample_age_us=now-sampled;
    return checked_sample_age_us<=BARCODE_SAMPLE_GAP_US;
}
static void drain(bool end)
{
    uint32_t irq=save_and_disable_interrupts();
    if(end)sampler.enabled=false;
    barcode_sampler_take(&sampler,&batch);
    restore_interrupts(irq);
    for(unsigned i=0;i<batch.count;i++){
        if(trace_count==TRACE_CAPACITY){trace_overflow=true;break;}
        trace[trace_count++]=(barcode_edge_t){batch.edges[i].us-scan_origin,batch.edges[i].black};
    }
    barcode_capture_consume(&capture,&batch);
    if(trace_overflow){capture.integrity_fault=true;capture.decoded=0;}
}
static void finish(const char *reason)
{
    motor_outputs_off();drain(true);
    if(capture.integrity_fault)reason="SCAN_DATA_FAULT";
    printf("[SCAN STOP] reason=%s motors=OFF elapsed_ms=%lu\n",reason,
           (unsigned long)((uint32_t)(time_us_32()-scan_origin)/1000u));
    const barcode_sample_stats_t *s=&capture.stats;
    const barcode_runs_t *runs=capture.reader.active?&capture.reader.runs:&capture.last_runs;
    unsigned count=capture.reader.active?capture.reader.run_count:capture.saved_runs;
    printf("[BARCODE RESULT] decoded=%c valid=%u samples=%lu edges=%lu raw=%u..%u "
           "frames=%lu rejected=%lu timeouts=%lu max_gap_us=%lu adc_max_us=%lu "
           "overflow=%u integrity_fault=%u trace_overflow=%u gap_fault=%u stale_age_us=%lu runs=%u widths_us=",
           capture.decoded?capture.decoded:'-',capture.decoded&&!capture.integrity_fault?1u:0u,
           (unsigned long)s->samples,(unsigned long)s->edges,s->samples?s->raw_min:0,s->raw_max,
           (unsigned long)capture.frames,(unsigned long)capture.rejected,(unsigned long)capture.timeouts,
           (unsigned long)s->max_gap_us,(unsigned long)max_adc_us,s->overflow,
           capture.integrity_fault,trace_overflow,s->gap_fault,(unsigned long)stale_fault_age_us,count);
    for(unsigned i=0;i<count;i++)printf("%s%lu",i?",":"",(unsigned long)barcode_runs_get(runs,i)*8u);
    printf(". Hand-scan timing; 8us resolution; no navigation action.\n");
    for(unsigned part=0;part<2;part++){
        printf("[SCAN EDGES %u] total=%u time_ms:level=",part+1,trace_count);
        for(unsigned i=part*64;i<trace_count && i<(part+1)*64;i++)
            printf("%s%lu:%c",i==part*64?"":",",(unsigned long)(trace[i].us/1000u),trace[i].black?'B':'W');
        printf("\n");
    }
}
static void help(void)
{
    printf("\nAAI10 STATIONARY BARCODE v5.7 | motors OFF | %s %s\n",__DATE__,__TIME__);
    printf("GP26 barcode thresholds: BLACK raw>=%u; WHITE raw<=%u; two samples confirm.\n",
           BARCODE_BLACK_ON,BARCODE_BLACK_OFF);
    printf("Only hand scanning. Motor pins8/9/10/11 heldLOW; no driving commands.\n");
    printf("GP20: start3s countdown / finish scan. Start on white; wait for [SCAN READY].\n");
    printf("Slide whole sheet smoothly in one direction over5-15s, then leave white under sensor.\n");
    printf("Ends on decode, GP20/browser finish, data fault or45s. Per-element timeout6s.\n");
    printf("s=select stationary scan; x=finish; ?=help. Keep sensing height constant.\n");
    calibration_wifi_help();
}
int main(void)
{
    for(uint pin=8;pin<=11;pin++){gpio_init(pin);gpio_put(pin,0);gpio_set_dir(pin,GPIO_OUT);}
    /* Disable servo pulses too; this diagnostic owns only the barcode ADC. */
    gpio_init(12);gpio_put(12,0);gpio_set_dir(12,GPIO_OUT);
    stdio_init_all();calibration_wifi_capture_init();
    gpio_init(20);gpio_set_dir(20,GPIO_IN);gpio_pull_up(20);
    adc_init();adc_gpio_init(26);adc_select_input(0);
    bool ready=add_repeating_timer_us(-(int64_t)BARCODE_SAMPLE_US,sample_alarm,NULL,&timer);
    sleep_ms(2000);(void)calibration_wifi_init();help();
    bool active=false,stable=gpio_get(20),candidate=stable,usb_before=false;
    uint64_t changed=0,arming=0,next_report=0;
    while(true){
        motor_outputs_off();
        calibration_wifi_poll(!active && !arming);
        uint64_t now=time_us_64();bool stop_requested=false;
        bool usb=stdio_usb_connected();if(usb&&!usb_before)help();usb_before=usb;
        for(unsigned i=0;i<16;i++){
            int ch=calibration_wifi_getchar();if(ch<0)ch=getchar_timeout_us(0);
            if(ch==PICO_ERROR_TIMEOUT)break;
            if(ch=='x')stop_requested=true;
            else if(ch=='s'&&!active&&!arming)printf("[SELECT] STATIONARY_SCAN motors=OFF. GP20 starts capture.\n");
            else if(ch=='?')help();
            else if(ch!='\r'&&ch!='\n')printf("[SELECT] Ignored: this firmware cannot drive.\n");
        }
        bool level=gpio_get(20),pressed=false;
        if(level!=candidate){candidate=level;changed=now;}
        if(stable!=level && now-changed>=25000u){stable=level;pressed=!level;}
        if(stop_requested || (pressed&&(active||arming))){
            if(active){finish("USER_FINISHED");active=false;}
            else if(arming)printf("[SCAN STOP] Countdown cancelled; motors OFF.\n");
            arming=0;pressed=false;
        }
        if(pressed&&!stop_requested){
            if(!ready || !sample_fresh())printf("[SCAN STOP] Sampling unavailable; restart firmware.\n");
            else{arming=now+3000000u;printf("[SCAN START] Countdown3s; motors OFF. Put sensor on white.\n");}
        }
        if(arming && now>=arming){
            arming=0;
            uint32_t irq=save_and_disable_interrupts();
            bool can_begin=sample_fresh() && raw<=BARCODE_BLACK_OFF;
            if(can_begin){scan_origin=time_us_32();barcode_sampler_begin(&sampler);max_adc_us=0;}
            restore_interrupts(irq);
            if(can_begin){
                trace_count=0;trace_overflow=false;stale_fault_age_us=0;barcode_capture_begin_slow(&capture,scan_origin);active=true;
                printf("[SCAN READY] motors OFF. Slide one whole barcode steadily now; GP20 finishes.\n");
            }else printf("[SCAN STOP] Start rejected: GP26 must see WHITE (raw<=%u) with fresh samples.\n",BARCODE_BLACK_OFF);
        }
        if(active){
            drain(false);
            if(!sample_fresh()){stale_fault_age_us=checked_sample_age_us;capture.integrity_fault=true;capture.decoded=0;}
            const char *end=capture.integrity_fault?"SCAN_DATA_FAULT":
                (capture.decoded?"BARCODE_READ":((uint32_t)(time_us_32()-scan_origin)>=SCAN_LIMIT_US?"SCAN_TIME_LIMIT":NULL));
            if(end){finish(end);active=false;}
        }
        if(now>=next_report){
            printf("[SCAN LIVE] active=%u motors=OFF IR_GP26=%u last_result=%c edges=%lu frames=%lu timeouts=%lu\n",
                   active?1u:0u,raw,capture.decoded?capture.decoded:'-',
                   (unsigned long)capture.stats.edges,(unsigned long)capture.frames,(unsigned long)capture.timeouts);
            next_report=now+250000u;
        }
        sleep_ms(2);
    }
}
