#include "mock.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint64_t clock_us,start_us,stop_us,echo_us;
static uint32_t ir_period_us;
static unsigned scenario,input_count,adc_channel;
static uint32_t injected_right_invalid;
static bool irq_disabled,in_alarm,command_sent,block_done,ever_center;
static unsigned pwm[8][2];static double left_count,right_count;
static char output[150000];static size_t output_n;
static jmp_buf done;
static bool (*ir_callback)(repeating_timer_t*),(*guard_callback)(repeating_timer_t*);
static int capture_printf(const char *fmt,...)
{
    va_list ap;va_start(ap,fmt);int n=vsnprintf(output+output_n,sizeof(output)-output_n,fmt,ap);va_end(ap);
    if(strncmp(fmt,"[OPTICAL",8)==0){
        assert(!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);
        assert(!in_alarm);
    }
    assert(n>=0 && (size_t)n<sizeof(output)-output_n);output_n+=(size_t)n;
    if(strncmp(fmt,"[MOTION GO]",11)==0)start_us=clock_us;
    if(strncmp(fmt,"[MOTION STOP]",13)==0||strncmp(fmt,"[STOP]",6)==0)stop_us=clock_us;
    return n;
}
#define printf capture_printf
#define main firmware_main
#include "../app/robot_firmware/robot_firmware.c"
#undef main
#undef printf
static void advance(uint64_t us)
{
    uint64_t end=clock_us+us;
    while(clock_us<end){
        if(start_us&&avoid_run&&avoidance.sonar_wait){assert(!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);}
        if(start_us&&course_run){
            if(course.phase==COURSE_CENTER)ever_center=true;
            if(course.phase==COURSE_BRAKE||course.phase==COURSE_FAILED)
                assert(!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);
        }
        uint64_t dt=end-clock_us;if(dt>100)dt=100;
        uint64_t previous=clock_us;clock_us+=dt;
        if(scenario!=6){
            left_count+=((double)pwm[4][0]-pwm[4][1])/6250.0*6500.0*dt/1000000.0;
            right_count+=((double)pwm[5][0]-pwm[5][1])/6250.0*6500.0*dt/1000000.0;
        }
        if(!in_alarm&&!irq_disabled){
            in_alarm=true;
            if(ir_period_us && clock_us/ir_period_us!=previous/ir_period_us && ir_callback && !(scenario==3&&start_us&&clock_us-start_us>500000))ir_callback(NULL);
            if(clock_us/10000!=previous/10000 && guard_callback)guard_callback(NULL);
            in_alarm=false;
        }
        if(stop_us&&clock_us-stop_us>650000)longjmp(done,1);
        if(clock_us>28000000)longjmp(done,1);
    }
}
void pwm_set_chan_level(uint s,uint c,uint16_t v){assert(s<8&&c<2);pwm[s][c]=v;if(s==4||s==5){if(v)assert(start_us);}}
uint pwm_gpio_to_slice_num(uint p){return p/2%8;}uint pwm_gpio_to_channel(uint p){return p%2;}
void pwm_set_clkdiv(uint s,float v){(void)s;(void)v;}void pwm_set_wrap(uint s,uint16_t v){(void)s;(void)v;}void pwm_set_enabled(uint s,bool v){(void)s;(void)v;}
uint32_t clock_get_hz(uint c){(void)c;return 125000000;}
void gpio_init(uint p){(void)p;}void gpio_set_dir(uint p,bool v){(void)p;(void)v;}void gpio_pull_up(uint p){(void)p;}void gpio_set_function(uint p,uint f){(void)p;(void)f;}
void gpio_put(uint p,bool v){if(p==0&&!v)echo_us=clock_us+100;}
bool gpio_get(uint p)
{
    if(p==20 && (scenario>=29&&scenario<=31) && scenario!=30)return true;
    if(p==20)return !((clock_us>=3000000&&clock_us<3100000)||((scenario==1&&start_us&&clock_us-start_us>=3000000&&clock_us-start_us<3100000)||((scenario==8&&course.phase==COURSE_TURN)||(scenario==35&&avoidance.phase==AVOID_TURN_OUT))));
    if(p==21)return true;
    if(p==1&&scenario==37&&start_us&&clock_us-start_us>500000)return true;
    if(p==1&&scenario==41&&start_us&&clock_us-start_us>500000){
        uint64_t width=clock_us-start_us<2000000?60000:1100;
        return clock_us>=echo_us&&clock_us<echo_us+width;
    }
    if(p==1&&scenario==40&&start_us&&clock_us-start_us>500000){
        uint64_t width=echo_us>start_us+500000 && echo_us<start_us+650000?60000:6000;
        return clock_us>=echo_us&&clock_us<echo_us+width;
    }
    if(p==1&&(scenario==38||scenario==39)&&start_us&&clock_us-start_us>500000){
        uint64_t width=scenario==39?60000:(echo_us/200000%2?38000:6000);
        return clock_us>=echo_us&&clock_us<echo_us+width;
    }
    if(p==1){uint64_t width=(scenario==2||scenario>=34)&&start_us&&clock_us-start_us>500000?(scenario==44?600:1000):6000;return clock_us>=echo_us&&clock_us<echo_us+width;}
    return false;
}
void adc_init(void){}void adc_gpio_init(uint p){(void)p;}void adc_select_input(uint p){adc_channel=p;}
uint16_t adc_read(void)
{
    if(adc_channel==0){
        if(scenario==32){
            /* Physical strip fixed to the floor: stopped wheels freeze colour.
             * One full Z, widths based on mean axle travel, not elapsed time. */
            if(!start_us || left_count+right_count<1200)return 190;
            double travel=left_count+right_count-1200;
            const unsigned pats[]={0x094,0x0d0,0x094};unsigned index=0;
            for(unsigned ch=0;ch<3;ch++){
                for(int bit=8;bit>=0;bit--){unsigned w=(pats[ch]&(1u<<bit))?180:60;
                    if(travel<w)return index%2?190:2100;travel-=w;index++;
                }
                if(ch<2){if(travel<60)return 190;travel-=60;index++;}
            }
            return 190;
        }
        if(scenario==10)return 2100;
        if(scenario==17)return 330; /* observed school background above old start threshold */
        if(scenario==16&&!start_us)return 2100;
        if(!start_us||clock_us-start_us<1200000)return 190;
        uint64_t t=clock_us-start_us-1200000;
        unsigned symbol=scenario>=7?0x190:0x109; /* Y or A */
        if((scenario==11||scenario==14||scenario==15||scenario==26)&&clock_us-start_us>=3000000){t=clock_us-start_us-3000000;symbol=scenario==26?0x190:0x0d0;}
        const unsigned pats[3]={0x094,symbol,0x094};unsigned index=0;
        for(unsigned j=0;j<3;j++){
            for(int b=8;b>=0;b--){unsigned w=(pats[j]&(1u<<b))?90000:30000;if(t<w)return (index%2)?190:2100;t-=w;index++;}
            if(j<2){if(t<30000)return 190;t-=30000;index++;}
        }
        return 190;
    }
    if((scenario==7||scenario==8||scenario==12||scenario==14) && course.phase==COURSE_SEEK)return adc_channel==2?2000:180;
#if ROBOCAR_CENTER_LINE
    if(scenario==20 && adc_channel==2)return 180;
    if(scenario==28 && !start_us && adc_channel==2)return clock_us/50000%2?180:2000;
    if(scenario==27 && adc_channel==2){
        if(!start_us)return clock_us/1000%100<4?2800:2000;
        return clock_us-start_us<4000000?2600:2000;
    }
    if((scenario==24||scenario==25)&&start_us&&clock_us-start_us>=1800000&&clock_us-start_us<1950000){
        if(adc_channel==2)return 1400;
        if(scenario==25&&adc_channel==1)return 2000;
    }
    if((scenario==26||scenario==32)&&adc_channel==2&&start_us&&clock_us-start_us>=1800000&&clock_us-start_us<1840000)return 180;
    if(scenario==21 && adc_channel==2 && start_us && clock_us-start_us>1800000)return 180;
    if(scenario==22 && adc_channel==2 && start_us && clock_us-start_us>1150000)return 180;
    if(scenario==23 && adc_channel==2 && start_us && clock_us-start_us>5000000){
        uint64_t t=clock_us-start_us;
        return t<5020000?180:t<5060000?1000:2000;
    }
    if((scenario==18||scenario==19)&&adc_channel==2&&start_us&&clock_us-start_us>5000000 &&
       (scenario==19||clock_us-start_us<5050000))return 180;
    if(scenario==9 && course.phase==COURSE_SEEK)return 180;
    if(adc_channel==2)return 2000;
#endif
    bool cross=start_us&&((clock_us-start_us>=400000&&clock_us-start_us<600000)||(clock_us-start_us>=3200000&&clock_us-start_us<3400000)||(scenario==11&&clock_us-start_us>=5400000&&clock_us-start_us<5600000));
    return cross?2000:180;
}
uint64_t time_us_64(void){return clock_us;}uint32_t time_us_32(void){return (uint32_t)clock_us;}
void sleep_us(uint64_t us){advance(us);}void sleep_ms(uint32_t ms){advance(ms*1000u);}void tight_loop_contents(void){advance(100);}
void busy_wait_us_32(uint32_t us){(void)us;}
uint32_t save_and_disable_interrupts(void){bool old=irq_disabled;irq_disabled=true;return old;}void restore_interrupts(uint32_t v){irq_disabled=v;}
bool add_repeating_timer_us(int64_t us,bool(*cb)(repeating_timer_t*),void*p,repeating_timer_t*t){assert(us<0);ir_period_us=(uint32_t)-us;(void)p;(void)t;ir_callback=cb;return true;}
bool add_repeating_timer_ms(int32_t us,bool(*cb)(repeating_timer_t*),void*p,repeating_timer_t*t){(void)us;(void)p;(void)t;guard_callback=cb;return true;}
void stdio_init_all(void){}bool stdio_usb_connected(void){return false;}int getchar_timeout_us(uint32_t us){(void)us;return -1;}
void calibration_wifi_capture_init(void){}bool calibration_wifi_init(void){return true;}
void calibration_wifi_poll(bool allowed){(void)allowed;if(scenario==4&&!block_done&&start_us&&clock_us-start_us>500000){block_done=true;advance(200000);assert(!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);}}
int calibration_wifi_getchar(void){
    if(!command_sent){command_sent=true;return scenario>=34?'o':(scenario>=29&&scenario<=31)?'v':'q';}
    if(scenario==29 && optical_pending && clock_us>4000000 && !input_count++){return 'x';}
    if((scenario>=29&&scenario<=31))return -1;
    if(scenario==36&&avoidance.phase==AVOID_SCAN_OUT&&!input_count++)return 'x';
    if(start_us&&clock_us-start_us>(scenario>=41?15000000u:scenario>=7?10000000u:4500000u)&&!input_count++){return 'x';}
    return -1;
}
course_config_t calibration_wifi_course_config(void){return (course_config_t){scenario>=16?'s':scenario==11?'b':scenario==12?'R':scenario==13?'x':(scenario==14||scenario==15)?'l':scenario>=7?'r':'s',scenario==14?'R':scenario==15?'x':'s',120};}
uint32_t calibration_wifi_max_poll_us(void){return 0;}void calibration_wifi_help(void){}void calibration_wifi_reset_timing(void){}
void encoders_init(void){}encoder_sample_t encoders_snapshot(void){
    if((scenario==42||scenario==43)&&avoidance.phase==AVOID_TURN_OUT&&motion_left_position(&avoidance.leg,(encoder_sample_t){(uint32_t)-(int32_t)left_count,(uint32_t)(int32_t)right_count,0,0})>400)injected_right_invalid=scenario==42?1:3;
    return (encoder_sample_t){(uint32_t)-(int32_t)left_count,(uint32_t)(int32_t)right_count,scenario==5&&start_us&&clock_us-start_us>500000,injected_right_invalid};}
int32_t encoder_left_count(void){return -(int32_t)left_count;}int32_t encoder_right_count(void){return (int32_t)right_count;}
bool terrain_init(void){return false;}bool terrain_calibrate(void){return false;}void terrain_poll(void){}terrain_status_t terrain_get_status(void){return (terrain_status_t){0};}
#if ROBOCAR_CENTER_LINE
static void independent_phase_level(bool black,unsigned duration)
{
    for(unsigned t=0;t<duration;t+=BARCODE_SAMPLE_US){
        clock_us+=BARCODE_SAMPLE_US;
        barcode_sampler_sample(&barcode_sampler,black?2100:190,(uint32_t)clock_us);
        barcode_sampler.stats.motion_breaks++; /* deliberately reversing metadata */
        if(clock_us%10000u==0)barcode_drain(false);
    }
}
static void independent_phase_test(void)
{
    /* Production drain and navigation dispatch, held in each manoeuvre phase.
     * This tests software separation; no claim about real sensor trajectories. */
    const course_phase_t phases[]={COURSE_LOST,COURSE_TURN,COURSE_SEEK,COURSE_BRAKE};
    for(unsigned p=0;p<sizeof(phases)/sizeof(phases[0]);p++){
        clock_us=0;course=(course_control_t){0};course.phase=phases[p];
        course.config=(course_config_t){'s','r',120};course_run=true;
        stream_logged=0;stream_paused_fragments=0;barcode_trace_count=0;
        barcode_trace_overflow=false;
        barcode_sampler_begin_levels(&barcode_sampler,226,220);
        barcode_capture_begin_stream(&barcode_capture,0);
        barcode_capture.independent_optical=true;barcode_capture.waiting_gap=true;
        independent_phase_level(false,400000);
        const unsigned patterns[]={0x094,0x0d0,0x094};
        for(unsigned ch=0;ch<3;ch++){
            for(int bit=8;bit>=0;bit--)
                independent_phase_level(!(bit%2),(patterns[ch]&(1u<<bit))?45000:15000);
            if(ch<2)independent_phase_level(false,15000);
        }
        independent_phase_level(false,400000);barcode_drain(false);
        assert(barcode_capture.event_count==1 && barcode_capture.events[0].letter=='Z');
        assert(!stream_paused_fragments && !barcode_capture.motion_aborts);
        assert(course.phase==phases[p] && course.deferred && course.deferred_source=='Z');
        assert(course.route_updates==1);
    }
    puts("PASS production barcode drain during LOST/TURN/SEEK/BRAKE; Z decoded and action deferred without resetting capture.");
}
#endif
int main(int argc,char **argv)
{
    assert(argc==2);scenario=(unsigned)atoi(argv[1]);
#if ROBOCAR_CENTER_LINE
    if(scenario==33){independent_phase_test();return 0;}
#endif
    if(!setjmp(done))firmware_main();
    if(scenario>=34){
        assert(start_us&&stop_us);
        assert(!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);
        assert(pwm[6][0]==1500); /* recentered on every abort */
        assert(avoid_run&&!course_run&&!barcode_sampler.enabled&&!barcode_capture.event_count);
        assert(avoidance.detected==((scenario>=37&&scenario<=40)||scenario==44?0u:1u)&&!avoidance.completed);
        assert(strstr(output,"[AVOID RESULT]")&&!strstr(output,"result=OBSTACLE_STOP"));
        if(scenario==44)assert(strstr(output,"AVOID_APPROACH_TOO_CLOSE")&&!avoidance.scans);
        else if(scenario==35)assert(strstr(output,"GP20 abort"));
        else if(scenario==36)assert(strstr(output,"USB/browser stop"));
        else if(scenario==37)assert(strstr(output,"AVOID_SONAR_RECOVERY_FAILED")&&avoidance.sonar_retries==1&&!avoidance.sonar_resumes);
        else if(scenario==38){
            if(strstr(output,"AVOID_SONAR_ECHO_STUCK_HIGH")){fputs("Normal 38 ms no-return pulse falsely latched a stop.\n",stderr);assert(0);}
            assert(strstr(output,"EXTERNAL_STOP")&&left_count>1000&&right_count>1000);
            assert(avoidance.no_echo>0);
        }else if(scenario==39||scenario==40){
            if(!strstr(output,"EXTERNAL_STOP"))fputs(output,stderr);
            assert(strstr(output,"EXTERNAL_STOP")&&!strstr(output,"[SONAR RETRY]"));
            assert(!avoidance.sonar_retries&&left_count>1000&&right_count>1000&&avoidance.no_echo>0);
        }else if(scenario==41){
            if(!strstr(output,"AVOID_RIGHT_PATH_BLOCKED"))fputs(output,stderr);
            assert(strstr(output,"AVOID_RIGHT_PATH_BLOCKED")&&!avoidance.sonar_retries&&avoidance.no_echo>0);
        }
        else if(scenario==42){
            assert(strstr(output,"AVOID_RIGHT_PATH_BLOCKED")&&strstr(output,"[ENCODER NOTE]")&&strstr(output,"[AVOID SCAN]"));
            assert(avoidance.encoder_skips[1]==1&&injected_right_invalid==1);
        }else if(scenario==43){assert(strstr(output,"AVOID_ENCODER_INVALID")&&avoidance.scans==1);}
        else assert(strstr(output,"AVOID_RIGHT_PATH_BLOCKED"));
        puts("PASS production obstacle main: mode selection, barcode disabled, sonar dispatch, stop PWM, servo recenter and result.");return 0;
    }
#if ROBOCAR_CENTER_LINE
    if((scenario>=29&&scenario<=31)){assert(!start_us&&!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);assert(strstr(output,"[OPTICAL RESULT]")&&strstr(output,"[OPTICAL END]"));assert(strstr(output,scenario==31?"aborted=0 sampling_ok=1":"aborted=1 sampling_ok=0"));puts("PASS optical capture: no motor start, GP20/browser cancel and timed completion.");return 0;}
    if(scenario==20||scenario==28){assert(!start_us&&!pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);assert(strstr(output,"CENTER_BASELINE_NOT_READY"));puts("PASS centre start alignment gate.");return 0;}
    if(scenario==27){
        assert(strstr(output,"source=DRIVE"));
        assert(strstr(output,"aborted=0 sampling_ok=1 source=DRIVE"));
        assert(strstr(output,"[OPTICAL DATA] bin=475"));
        assert(!optical.active && !optical.armed && !optical_drive_pending);
        assert(course.center_reference==667 && !course.trim_attempts && !course.recoveries);
        assert(strstr(output,"frozen_for_run=1") && strstr(output,"fixed_ref=667"));
    }
    if(scenario==18){assert(course.recoveries>=1 && strstr(output,"EXTERNAL_STOP"));}
    if(scenario==19){assert(strstr(output,"CENTER_LINE_NOT_REACQUIRED"));}
    if(scenario==21||scenario==22){assert(strstr(output,"CENTER_LOCAL_RECOVERY_EXHAUSTED"));assert(!course.recoveries);}
    if(scenario==23){assert(course.recoveries==1 && strstr(output,"EXTERNAL_STOP"));}
    if(scenario==24||scenario==25){
        assert(course.trim_attempts==1 && !course.recoveries);
        assert(strstr(output,"reads=1 sequence=Y") && !stream_paused_fragments);
        assert(course.junctions==2);
    }
    if(scenario==32){
        assert(course.recoveries==1 && !stream_paused_fragments);
        assert(!barcode_capture.motion_aborts && !barcode_capture.zero_distance_aborts && !barcode_capture.pause_timeouts);
        assert(strstr(output,"reads=1 sequence=Z") && strstr(output,"width_source=TIME"));
    }
    if(scenario==26){
        /* Optical timestamps remain usable while wheels stop: a moving
         * printed strip under a stationary sensor must still be readable. */
        assert(course.recoveries==1 && stream_paused_fragments==0);
        assert(!barcode_capture.zero_distance_aborts && !barcode_capture.motion_aborts);
        if(!strstr(output,"reads=2 sequence=YY")){fputs(output,stderr);assert(0);}
    }
#endif
    if(scenario==10||scenario==17){assert(start_us && left_count>100 && right_count>100);assert(!course.route_updates && !barcode_capture.event_count);assert(strstr(output,"Reading waits for 250ms white"));}
    if(!strstr(output,"[MOTION SETTLED]")){fputs(output,stderr);assert(0);}
    assert(start_us&&stop_us && !pwm[4][0]&&!pwm[4][1]&&!pwm[5][0]&&!pwm[5][1]);
    const char *reasons[]={"EXTERNAL_STOP","BUTTON_STOP","OBSTACLE_STOP","BARCODE_SAMPLE_FAULT","MOTOR_SERVICE_TIMEOUT","COURSE_ENCODER_FAULT","BARCODE_ENCODER_STALL","EXTERNAL_STOP","BUTTON_STOP","COURSE_LINE_NOT_REACQUIRED","EXTERNAL_STOP","EXTERNAL_STOP","EXTERNAL_STOP","COURSE_BARCODE_STOP","EXTERNAL_STOP","COURSE_BARCODE_STOP","EXTERNAL_STOP","EXTERNAL_STOP"};
    const char *expected=scenario<18?reasons[scenario]:scenario==18?"EXTERNAL_STOP":"CENTER_LINE_NOT_REACQUIRED";
#if ROBOCAR_CENTER_LINE
    if(scenario==9)expected="CENTER_LINE_NOT_REACQUIRED";
    if(scenario==21||scenario==22)expected="CENTER_LOCAL_RECOVERY_EXHAUSTED";
    if(scenario==23||scenario==24||scenario==25||scenario==26||scenario==27||scenario==32)expected="EXTERNAL_STOP";
#endif
    if(!strstr(output,expected)){fputs(output,stderr);assert(0);}
    if(scenario==6){
        assert(strstr(output,"stall_wheel=BOTH"));
        assert(strstr(output,"no_progress_ms="));
        assert(strstr(output,"last_target_cps="));
        assert(strstr(output,"last_measured_cps="));
        assert(strstr(output,"last_pwm="));
    }
    if(scenario==16){assert(course.route_updates==1 && strstr(output,"sequence=Y"));}
    if(scenario==0){
        assert(strstr(output,"aborted=1 sampling_ok=0 source=DRIVE"));
        assert(!optical.active && !optical.armed && !optical_drive_pending);
        assert(strstr(output,"encounters=2"));assert(strstr(output,"reads=1 sequence=A"));}
    assert(strstr(output,"[COURSE RESULT]"));
    assert(strstr(output,"last_ended_runs=") && strstr(output,"paused_fragments="));
    if(scenario==7){assert(ever_center);if(!strstr(output,"encounters=2 turns=1")){fputs(output,stderr);assert(0);}assert(strstr(output,"[ROUTE] turn complete"));}
    if(scenario==8)assert(course.phase==COURSE_TURN);
    if(scenario==11){assert(course.route_updates==2&&course.replacements==1&&!course.turns&&!course.remaining_junctions);assert(strstr(output,"sequence=YZ"));}
    if(scenario==12){assert(!ever_center);assert(course.turns==1 && strstr(output,"action=TURN_NOW"));}
    if(scenario==13){assert(course.route_updates==1&&strstr(output,"action=STOP_NOW"));}
    if(scenario==14){if(course.turns!=2){fputs(output,stderr);assert(0);}assert(strstr(output,"DEFERRED_UNTIL_MANOEUVRE_COMPLETE"));}
    if(scenario==15){assert(ever_center&&course.route_updates==2&&!course.turns);assert(strstr(output,"sequence=YZ"));}
    printf("Actual course main scenario %u passed: %s, stopped PWM and retained result.\n",scenario,expected);
}
