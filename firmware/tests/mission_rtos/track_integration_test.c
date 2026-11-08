#include "board.h"
#include "motion_runtime.h"
#include "track_sensor.h"
#include "network_service.h"
#include "track_test_sdk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t clock_us;
static bool button,adc_live=true,decode_live=true,travel=true;
static unsigned irq_depth,input;
static uint16_t raw[3]={191,141,2908},pwm[8][2];
static repeating_timer_t *guard,*adc_timer;
static encoder_sample_t enc;
static int32_t left_travel,right_travel;
static uint64_t restore_line_at;
static motion_runtime_t runtime;
static char reads[32];static unsigned read_count;static uint32_t last_barcode_ms;
uint32_t save_and_disable_interrupts(void){return irq_depth++;}
void restore_interrupts(uint32_t n){assert(irq_depth>0);irq_depth=n;}
uint32_t time_us_32(void){return (uint32_t)clock_us;}
uint64_t time_us_64(void){return clock_us;}
uint32_t clock_get_hz(int n){(void)n;return 125000000;}
void gpio_init(uint n){(void)n;}
void gpio_set_dir(uint n,bool b){(void)n;(void)b;}
void gpio_pull_up(uint n){(void)n;}
void gpio_set_function(uint n,int f){(void)n;(void)f;}
bool gpio_get(uint n){assert(n==20);return !button;}
uint pwm_gpio_to_slice_num(uint n){return (n>>1u)&7u;}
void pwm_set_both_levels(uint n,uint16_t a,uint16_t b){pwm[n][0]=a;pwm[n][1]=b;}
void pwm_set_enabled(uint n,bool b){(void)n;(void)b;}
void pwm_set_clkdiv(uint n,float f){(void)n;(void)f;}
void pwm_set_wrap(uint n,uint16_t w){(void)n;assert(w==6249);}
bool add_repeating_timer_us(int64_t us,bool (*fn)(repeating_timer_t *),void *context,repeating_timer_t *timer)
{
    (void)context;timer->callback=fn;
    if(us==-5000)guard=timer;else {assert(us==-500);adc_timer=timer;}
    return true;
}
void adc_init(void){}
void adc_gpio_init(uint pin){assert(pin>=26 && pin<=28);}
void adc_select_input(uint n){assert(n<3);input=n;}
uint16_t adc_read(void){return raw[input];}
void busy_wait_us_32(uint32_t us){assert(us==5);}
encoder_sample_t encoders_snapshot(void){return enc;}
bool network_line(uint32_t ms,const uint16_t values[3],uint8_t pattern,bool valid)
{(void)ms;(void)values;assert(pattern<=7);(void)valid;return true;}
bool network_barcode(uint32_t ms,char letter,bool valid,bool simulated)
{
    assert(!simulated);assert(ms==(uint32_t)(clock_us/1000u) || valid);
    if(valid){assert(letter>='A' && letter<='Z');assert(read_count<sizeof reads);reads[read_count++]=letter;last_barcode_ms=ms;}
    return true;
}
static bool armed(void){bool yes;uint8_t reason;board_gate_snapshot(&yes,&reason);return yes;}
static void stopped(void){assert(!armed());assert(!pwm[4][0] && !pwm[4][1] && !pwm[5][0] && !pwm[5][1]);}
static motion_record_t latest(void)
{uint8_t b[13];motion_record_t r;board_read(b);assert(motion_record_decode(b,sizeof b,&r));return r;}
static void advance(unsigned us)
{
    assert(us%500u==0);
    for(unsigned n=0;n<us;n+=500){
        clock_us+=500;
        if(restore_line_at && clock_us>=restore_line_at){raw[2]=2908;raw[1]=141;restore_line_at=0;}
        if(travel && armed() && clock_us%2000u==0){
            if(pwm[4][0]){enc.left_raw--;left_travel++;}
            else if(pwm[4][1]){enc.left_raw++;left_travel--;}
            if(pwm[5][0]){enc.right_raw++;right_travel++;}
            else if(pwm[5][1]){enc.right_raw--;right_travel--;}
        }
        if(adc_live)assert(adc_timer->callback(adc_timer));
        if(clock_us%5000u==0)assert(guard->callback(guard));
        if(clock_us%10000u==0)motion_runtime_step(&runtime,enc,clock_us,button);
        if(decode_live && clock_us%5000u==0)track_service();
        assert(irq_depth==0);
    }
}
static void begin_test(void)
{
    track_stop();clock_us=0;button=false;adc_live=decode_live=travel=true;
    enc=(encoder_sample_t){0};left_travel=right_travel=0;restore_line_at=0;raw[0]=191;raw[1]=141;raw[2]=2908;
    memset(pwm,0,sizeof pwm);read_count=0;
    assert(board_init());assert(track_sensor_init());motion_runtime_init(&runtime,enc,0,false);
    advance(700000);assert(board_select(MOTION_COURSE));stopped();
}
static void gesture(void)
{button=true;advance(50000);button=false;advance(50000);}
static void start(void)
{gesture();assert(armed() && runtime.motion.kind==MOTION_COURSE && runtime.motion.phase==MOTION_ACTIVE);}
static void samples(uint16_t v,unsigned us){raw[0]=v;advance(us);}
static void frame(unsigned pattern,unsigned narrow,bool recovery)
{
    unsigned patterns[]={0x094,pattern,0x094};unsigned element=0;
    for(unsigned symbol=0;symbol<3;symbol++){
        for(unsigned e=0;e<9;e++,element++){
            unsigned width=narrow*((patterns[symbol]&(1u<<(8-e)))?3u:1u);
            if(recovery && element==10){raw[2]=178;raw[1]=300;restore_line_at=clock_us+150000u;}
            raw[0]=element%2?191:2200;
            int32_t start=left_travel+right_travel;
            uint64_t deadline=clock_us+3000000u;
            while(left_travel+right_travel-start<(int32_t)(width/1000u)){
                advance(500);assert(armed() && clock_us<deadline);
            }
        }
        if(symbol<2){samples(191,narrow);element++;}
    }
}
static void diagnostic_export(const char *label)
{
    uint16_t run=track_scan_run();uint8_t b[SCAN_PACKET_BYTES];
    size_t n=track_scan_state(b);assert(n);
    printf("SCAN_FIXTURE %s ",label);for(size_t j=0;j<n;j++)printf("%02x",b[j]);puts("");
    unsigned count=0;
    while((n=track_scan_export(run,count,b))!=0){
        printf("SCAN_FIXTURE %s ",label);for(size_t j=0;j<n;j++)printf("%02x",b[j]);puts("");
        count++;assert(count<300);
    }
    assert(count>8);
    assert(!track_scan_export((uint16_t)(run+1u),0,b));
}
static void pipeline(void)
{
    begin_test();start();samples(191,600000);
    frame(0x109,160000,false);samples(191,900000);
    assert(read_count==1 && reads[0]=='A' && armed());
    frame(0x109,80000,false);samples(191,900000);
    assert(read_count==2 && reads[1]=='A' && armed());
    frame(0x0d0,80000,true);samples(191,900000);
    if(read_count!=3 || reads[2]!='Z')fprintf(stderr,"pipeline reads=%u status=%s phase=%d faults=%u\n",read_count,runtime.track.status,runtime.motion.phase,latest().fault);
    assert(read_count==3 && reads[2]=='Z' && armed());
    assert(runtime.track.reads==3);
    uint8_t pending[SCAN_PACKET_BYTES];assert(!track_scan_export(track_scan_run(),0,pending));
    samples(191,65000000);motion_record_t r=latest();
    assert(r.valid && r.kind==7 && r.left_counts>32767 && r.right_counts>32767);
    printf("TRACK_FIXTURE ");uint8_t b[13];board_read(b);for(unsigned i=0;i<13;i++)printf("%02x",b[i]);puts("");
    button=true;advance(5000);stopped();button=false;advance(200000);stopped();
    board_stop(STOP_BUTTON);advance(10000);stopped();
    diagnostic_export("AAZ");
    puts("PASS actual track pipeline: full A, half A and half Z through line recovery; extended travel; raw GP20 stop; no restart on release");
}
static void failures(void)
{
    begin_test();start();samples(191,600000);samples(2200,80000);samples(191,80000);
    button=true;advance(5000);stopped();button=false;advance(100000);stopped();
    assert(read_count==0);
    diagnostic_export("stopped-partial");
    uint16_t old_run=track_scan_run();
    start();samples(191,600000);frame(0x0d0,80000,false);samples(191,900000);
    assert(read_count==1 && reads[0]=='Z');
    uint8_t old[SCAN_PACKET_BYTES];assert(!track_scan_export(old_run,0,old));
    board_stop(STOP_BUTTON);advance(10000);stopped();
    begin_test();start();adc_live=false;advance(20000);stopped();
    assert(latest().fault==MOTION_TRACK_FAULT);
    begin_test();decode_live=false;gesture();advance(150000);stopped();
    assert(latest().fault==MOTION_TRACK_FAULT);
    begin_test();start();decode_live=false;advance(1100000);stopped();
    assert(latest().fault==MOTION_TRACK_FAULT);
    begin_test();raw[2]=178;advance(500000);gesture();stopped();
    assert(runtime.motion.phase==MOTION_ABORTED);
    puts("PASS track safety: partial scan aborted/reset, stale ADC, decoder startup timeout, queue overflow and off-line start reject");
}
static void extended_codec(void)
{
    const int32_t counts[]={-160000,-65537,-32769,-1,0,1,32768,65536,160000};
    for(unsigned a=0;a<9;a++)for(unsigned b=0;b<9;b++){
        motion_record_t r={.kind=7,.phase=1,.valid=true,.left_counts=counts[a],.right_counts=counts[b],.left_mm_s=-500,.right_mm_s=500},d;
        uint8_t bytes[13];assert(motion_record_encode(&r,bytes,sizeof bytes));assert(motion_record_decode(bytes,sizeof bytes,&d));
        assert(d.left_counts==r.left_counts && d.right_counts==r.right_counts);
    }
    motion_record_t r={.kind=7,.valid=true,.left_counts=160001};uint8_t bytes[13];
    assert(!motion_record_encode(&r,bytes,sizeof bytes));
    puts("PASS extended motion codec: all signed19 boundary pairs, original 13-byte size, out-of-range rejected");
}
static void clock_wrap(void)
{
    begin_test();clock_us=((uint64_t)UINT32_MAX-500000u)/5000u*5000u;
    advance(700000);start();samples(191,600000);
    frame(0x0d0,80000,false);samples(191,900000);
    assert(read_count==1 && reads[0]=='Z' && armed());
    assert(last_barcode_ms>UINT32_MAX/1000u);
    assert(latest().valid && latest().fault==0);
    board_stop(STOP_BUTTON);advance(10000);stopped();
    puts("PASS track microsecond-clock wrap: fresh ADC, framed Z and full-uptime millisecond publication");
}
static void held_boot(void)
{
    begin_test();button=true;motion_runtime_init(&runtime,enc,(uint32_t)clock_us,true);
    advance(200000);stopped();button=false;advance(200000);stopped();
    start();board_stop(STOP_BUTTON);advance(10000);stopped();
    puts("PASS autonomous track selection: held boot/release cannot start; fresh GP20 gesture required");
}
static void first_failure_evidence(void)
{
    begin_test();start();samples(191,600000);samples(2200,1200000);samples(191,1200000);
    assert(!read_count);track_diagnostics_t d;track_diagnostics(&d);assert(d.rejected);
    frame(0x0d0,80000,false);samples(191,900000);assert(read_count==1 && reads[0]=='Z');
    board_stop(STOP_BUTTON);advance(100000);stopped();diagnostic_export("first-failure");
    puts("PASS first failed optical evidence survives a later accepted Z; no barcode guessing or change to motor start");
}
static void emit_packet(const char *label,const uint8_t *bytes,size_t n)
{ assert(n && n<=SCAN_PACKET_BYTES);printf("SCAN_FIXTURE %s ",label);for(size_t i=0;i<n;i++)printf("%02x",bytes[i]);puts(""); }
static void diagnostic_boundaries(void)
{
    uint8_t b[SCAN_PACKET_BYTES];uint16_t words[SCAN_CHUNK_WORDS];uint32_t hash=2166136261u;
    for(unsigned i=0;i<16384;i++){
        uint16_t v=(uint16_t)(i*13u+17u);hash=(hash^(v&255u))*16777619u;hash=(hash^(v>>8u))*16777619u;
    }
    scan_snapshot_t s={.count=16384,.motion=true,.start_us=UINT32_MAX,.duration_us=UINT32_MAX,
        .leading_us=UINT32_MAX,.white_us=UINT32_MAX,.checksum=hash,.paper=4095,.signal_range=4095,
        .reason=WAVE_FRAME_LIMIT,.coherent_mask=63,.geometry_mask=63};
    emit_packet("max-wave",b,scan_snapshot_encode(UINT16_MAX,&s,b));
    barcode_wave_profile_t p={.extrema=95,.windows=33,.best_window=32,.bad_run=28,
        .reason=WAVE_MOTION,.bad_width=UINT32_MAX,.code39={.patterns={511,511,511},.width_reason={7,7,7},.guards=3,.separators=3,.reversed=1,.score=11}};
    for(unsigned i=0;i<29;i++)p.runs[i]=UINT32_MAX;
    for(unsigned i=0;i<6;i++)emit_packet("max-wave",b,scan_profile_encode(UINT16_MAX,i,&p,b));
    for(unsigned chunk=0;chunk<256;chunk++){
        for(unsigned i=0;i<SCAN_CHUNK_WORDS;i++)words[i]=(uint16_t)((chunk*SCAN_CHUNK_WORDS+i)*13u+17u);
        emit_packet("max-wave",b,scan_words_encode(UINT16_MAX,chunk,words,SCAN_CHUNK_WORDS,b));
    }
    emit_packet("max-wave",b,scan_done_encode(UINT16_MAX,0,true,b));
    scan_state_t state={.run=UINT16_MAX,.ms=UINT32_MAX,.samples=UINT32_MAX,.frames=UINT32_MAX,.rejected=UINT32_MAX,
        .decode_us=UINT32_MAX,.adc_us=UINT32_MAX,.attempt_overflow=UINT32_MAX,.recoveries=UINT32_MAX,
        .stop_us=UINT32_MAX,.events=UINT16_MAX,.queue_peak=511,.stop_discarded=511,.faults=7,.attempts=32,.sealed=true,.ready=true};
    assert(scan_state_encode(&state,b));state.queue_peak=512;assert(!scan_state_encode(&state,b));
    p.code39.patterns[0]=512;assert(!scan_profile_encode(1,0,&p,b));
    assert(!scan_words_encode(1,256,words,64,b));assert(!scan_words_encode(1,0,words,65,b));
    puts("PASS scan codec bounds: maximum 16384-word waveform, all 256 chunks, integer extremes and rejected overflow");
}
int main(void){pipeline();failures();extended_codec();clock_wrap();held_boot();first_failure_evidence();diagnostic_boundaries();return 0;}
