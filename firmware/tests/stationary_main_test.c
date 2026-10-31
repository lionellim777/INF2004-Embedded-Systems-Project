#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock.h"
static unsigned scenario, motor_writes, command_index;
static uint64_t clock_us,ready_us,ended_us;
static char output[100000];static size_t output_n;
static jmp_buf done;
static bool (*alarm_cb)(repeating_timer_t*);
static bool irq_disabled, in_alarm, freshness_probe, probe_seen;
static int mock_printf(const char *fmt,...)
{
    va_list ap;va_start(ap,fmt);
    int n=vsnprintf(output+output_n,sizeof(output)-output_n,fmt,ap);va_end(ap);
    assert(n>=0 && (size_t)n<sizeof(output)-output_n);output_n+=(size_t)n;
    if(strncmp(fmt,"[SCAN READY]",12)==0)ready_us=clock_us;
    if(strstr(fmt,"[SCAN STOP]"))ended_us=clock_us;
    return n;
}
#define printf mock_printf
#define main firmware_main
#include "../app/robot_firmware/stationary_barcode.c"
#undef main
#undef printf
void gpio_put(uint p,bool v){if(p>=8&&p<=11){assert(!v);motor_writes++;}}
void gpio_init(uint p){(void)p;}
void gpio_set_dir(uint p,bool out){if(p>=8&&p<=11)assert(out);}
void gpio_pull_up(uint p){assert(p==20);}
bool gpio_get(uint p){assert(p==20);return !((clock_us>=3000000 && clock_us<3100000) || (scenario==9 && ready_us && clock_us>=ready_us+1500000 && clock_us<ready_us+1600000));}
void adc_init(void){}void adc_gpio_init(uint p){assert(p==26);}void adc_select_input(uint p){assert(p==0);}
uint16_t adc_read(void)
{
    if(scenario==5)return 2500; /* start rejects black */
    if(scenario==15)return 254; /* measured narrow black also rejects start */
    if(!ready_us || clock_us<ready_us+300000 || scenario==3)return 200;
    if(scenario==7)return 2500;
    if(scenario==8)return ((clock_us-ready_us-300000)/20000)%2?200:2500;
    uint64_t elapsed=clock_us-ready_us-300000;
    unsigned patterns[3]={0x094,(scenario==1||scenario==12||scenario==14)?0x0d0:0x109,0x094};
    unsigned widths[29],n=0;
    for(unsigned j=0;j<3;j++){for(int bit=8;bit>=0;bit--)widths[n++]=(patterns[j]&(1u<<bit))?1200000:400000;if(j<2)widths[n++]=400000;}
    for(unsigned i=0;i<29;i++){
        unsigned index=(scenario==6||scenario==13||scenario==14)?28-i:i;
        if(elapsed<widths[index]){
            if(scenario>=11 && scenario<=14){
                unsigned tick=(unsigned)(clock_us/1000);
                return i%2 ? (widths[index]==400000?196+tick%7:182+tick%6) :
                             (widths[index]==400000?254+tick%7:1992+tick%11);
            }
            return i%2?200:2500;
        }
        elapsed-=widths[index];
    }
    return 200;
}
uint32_t save_and_disable_interrupts(void){uint32_t old=irq_disabled;irq_disabled=true;return old;}
void restore_interrupts(uint32_t x){irq_disabled=x;}
uint32_t time_us_32(void){
    uint32_t result=(uint32_t)clock_us+(scenario==10?UINT32_MAX-7000000u:0);
    /* Interrupt after the clock value was read, before sample_fresh reads
     * last_sample_us. A newer sample must not underflow the age calculation. */
    if(freshness_probe&&!in_alarm){
        probe_seen=true;
        if(!irq_disabled){clock_us++;in_alarm=true;sample_alarm(NULL);in_alarm=false;}
    }
    return result;
}
uint64_t time_us_64(void){return clock_us;}
void sleep_ms(uint32_t ms)
{
    for(unsigned i=0;i<ms;i++){
        clock_us+=1000;
        bool missed=(scenario==4||scenario==17) && ready_us && clock_us>ready_us+400000 && (scenario==17||clock_us<ready_us+405000);
        if(alarm_cb&&!missed&&!irq_disabled){in_alarm=true;alarm_cb(NULL);in_alarm=false;}
        if((ended_us&&clock_us>ended_us+10000) || clock_us>55000000)longjmp(done,1);
    }
}
bool add_repeating_timer_us(int64_t us,bool(*cb)(repeating_timer_t*),void*p,repeating_timer_t*t)
{assert(us==-1000);(void)p;(void)t;alarm_cb=cb;return true;}
void stdio_init_all(void){}bool stdio_usb_connected(void){return false;}int getchar_timeout_us(uint32_t u){(void)u;return -1;}
void calibration_wifi_capture_init(void){}bool calibration_wifi_init(void){return true;}
void calibration_wifi_poll(bool allowed){(void)allowed;}
int calibration_wifi_getchar(void)
{
    /* Explicitly try every old drive command; all must remain harmless. */
    const char *commands="sflrutb";
    if(command_index<strlen(commands))return commands[command_index++];
    if(scenario==2 && ready_us && clock_us>ready_us+1500000 && command_index==strlen(commands)){command_index++;return 'x';}
    return -1;
}
uint32_t calibration_wifi_max_poll_us(void){return 0;}void calibration_wifi_help(void){}void calibration_wifi_reset_timing(void){}
int main(int argc,char **argv)
{
    assert(argc==2);scenario=(unsigned)atoi(argv[1]);
    if(scenario==16){
        clock_us=100000;last_sample_us=99999;freshness_probe=true;
        assert(sample_fresh());assert(probe_seen && !irq_disabled);
        freshness_probe=false;
        /* Nested use must retain the caller's interrupt mask. */
        uint32_t mask=save_and_disable_interrupts();
        last_sample_us=(uint32_t)clock_us;assert(sample_fresh() && irq_disabled);
        restore_interrupts(mask);assert(!irq_disabled);
        clock_us=100;last_sample_us=UINT32_MAX-899;assert(sample_fresh());
        last_sample_us=(uint32_t)clock_us-3001;assert(!sample_fresh());
        clock_us=0;last_sample_us=0;
    }
    if(setjmp(done)==0)(void)firmware_main();
    assert(motor_writes>1000);
    if(scenario==0||scenario==1||scenario==6||scenario==10||scenario==16||(scenario>=11&&scenario<=14)){
        assert(strstr(output,(scenario==1||scenario==12||scenario==14)?"decoded=Z valid=1":"decoded=A valid=1"));
        assert(strstr(output,"reason=BARCODE_READ motors=OFF"));
        assert(trace_count==30 && !trace_overflow);
        assert(capture.timeouts==0 && capture.saved_runs==29);
        for(unsigned i=0;i<29;i++)assert(barcode_runs_get(&capture.last_runs,i)*8u==(trace[i+1].us-trace[i].us));
    }else if(scenario==2||scenario==9){assert(strstr(output,"reason=USER_FINISHED"));assert(trace_count>0);}
    else if(scenario==3){assert(strstr(output,"reason=SCAN_TIME_LIMIT"));assert(trace_count==0);}
    else if(scenario==4||scenario==17){
        assert(strstr(output,"reason=SCAN_DATA_FAULT"));assert(!capture.decoded);
        assert(stale_fault_age_us>BARCODE_SAMPLE_GAP_US);
        assert(strstr(output,"stale_age_us=4000"));
    }
    else if(scenario==5||scenario==15){assert(strstr(output,"Start rejected"));assert(!ready_us);}
    else if(scenario==7){assert(strstr(output,"reason=SCAN_TIME_LIMIT"));assert(capture.timeouts==1 && trace_count==1);}
    else if(scenario==8){assert(strstr(output,"reason=SCAN_DATA_FAULT"));assert(trace_overflow && trace_count==128 && !capture.decoded);}
    printf("PASS stationary actual main scenario%u: all %u motor writes LOW; result/edge evidence verified.\n",scenario,motor_writes);
}
