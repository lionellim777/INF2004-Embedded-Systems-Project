#include "board.h"
#include "motion_runtime.h"
#include "test_sdk.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t clock_us;
static bool button, timer_ok = true;
static uint16_t outputs[8][2];
static repeating_timer_t *alarm;
static uint32_t irq_depth;
uint32_t save_and_disable_interrupts(void) { return irq_depth++; }
void restore_interrupts(uint32_t previous) { assert(irq_depth > 0); irq_depth = previous; }
uint32_t time_us_32(void) { return (uint32_t)clock_us; }
uint32_t clock_get_hz(int clock) { (void)clock; return 125000000; }
void gpio_init(uint pin) { (void)pin; }
void gpio_set_dir(uint pin, bool output) { (void)pin; (void)output; }
void gpio_pull_up(uint pin) { (void)pin; }
void gpio_set_function(uint pin, int function) { (void)pin; (void)function; }
bool gpio_get(uint pin) { assert(pin == 20); return !button; }
uint pwm_gpio_to_slice_num(uint pin) { return (pin >> 1u) & 7u; }
void pwm_set_both_levels(uint slice, uint16_t left, uint16_t right)
{ outputs[slice][0] = left; outputs[slice][1] = right; }
void pwm_set_enabled(uint slice, bool enabled) { (void)slice; (void)enabled; }
void pwm_set_clkdiv(uint slice, float divider) { (void)slice; (void)divider; }
void pwm_set_wrap(uint slice, uint16_t wrap) { (void)slice; assert(wrap == 6249); }
bool add_repeating_timer_us(int64_t period, bool (*callback)(repeating_timer_t *),
    void *context, repeating_timer_t *timer)
{
    (void)context; assert(period == -5000);
    alarm = timer; timer->callback = callback; return timer_ok;
}
static bool armed(void) { bool value; uint8_t why; board_gate_snapshot(&value, &why); return value; }
static uint8_t reason(void) { bool value; uint8_t why; board_gate_snapshot(&value, &why); return why; }
static void stopped(void)
{
    assert(!armed());
    for (unsigned i=4; i<=5; ++i) assert(outputs[i][0] == 0 && outputs[i][1] == 0);
}
static void advance(uint32_t us)
{
    assert(us % 5000 == 0);
    for (uint32_t i=0; i<us; i+=5000) {
        clock_us += 5000; assert(alarm->callback(alarm));
    }
}
static encoder_sample_t enc;
static motion_runtime_t runtime;
static void tick(bool down)
{
    button = down; advance(10000);
    motion_runtime_step(&runtime, enc, clock_us, down);
    assert(irq_depth == 0);
}
static void hold(bool down, unsigned ticks) { for (unsigned i=0; i<ticks; ++i) tick(down); }
static void reset(void)
{
    clock_us=0; button=false; timer_ok=true; enc=(encoder_sample_t){0};
    assert(board_init()); motion_runtime_init(&runtime, enc, 0, false);
    stopped();
}
static void start_kind(uint8_t kind)
{
    assert(board_select(kind));
    hold(false,5); hold(true,5); stopped(); hold(false,5);
    assert(armed() && runtime.motion.phase == MOTION_ACTIVE);
}
static void start(void) { start_kind(MOTION_STRAIGHT_30CM); }
static motion_record_t latest(void)
{
    uint8_t b[MOTION_RECORD_BYTES]; motion_record_t r;
    board_read(b); assert(motion_record_decode(b,sizeof b,&r)); return r;
}
static void safety_tests(void)
{
    safety_gate_t g; uint16_t token=0, next=0;
    safety_gate_init(&g); assert(!safety_gate_arm(&g,0,&token));
    safety_gate_release(&g); assert(safety_gate_arm(&g,UINT32_MAX-10000,&token));
    assert(safety_gate_service(&g,token,5000)); /* clock wrap */
    assert(!safety_gate_tick(&g,44999,false));
    assert(safety_gate_tick(&g,45000,false)); assert(!safety_gate_armed(&g));
    assert(!safety_gate_service(&g,token,45001));
    safety_gate_release(&g); assert(safety_gate_arm(&g,50000,&next));
    assert(next != token); assert(!safety_gate_service(&g,token,50001));
    assert(safety_gate_armed(&g));
    assert(safety_gate_tick(&g,50001,true)); assert(safety_gate_reason(&g)==STOP_BUTTON);
    assert(!safety_gate_arm(&g,50002,&token));
    safety_gate_release(&g); g.epoch=UINT16_MAX;
    assert(!safety_gate_arm(&g,50003,&token)); assert(safety_gate_reason(&g)==STOP_INIT);
    safety_gate_init(&g); safety_gate_release(&g); assert(safety_gate_arm(&g,0,&token));
    assert(!safety_gate_service(&g,token,40000)); /* cannot refresh after expiry */
    assert(safety_gate_reason(&g)==STOP_SERVICE);
    puts("PASS gate: release, exact deadline, clock wrap, stale tokens, no epoch recycling");
}
static void runtime_tests(void)
{
    reset(); button=true; motion_runtime_init(&runtime,enc,0,true);
    assert(board_select(1)); hold(true,10); stopped(); hold(false,5); stopped();
    hold(true,5); hold(false,5); assert(armed()); /* fresh gesture after boot hold */
    hold(false,12); assert(outputs[4][0]>0 && outputs[5][0]>0);
    tick(true); stopped(); assert(runtime.motion.phase==MOTION_ABORTED);
    board_stop_trace_t trace;board_stop_trace(&trace);
    assert(trace.sequence==1 && trace.reason==STOP_BUTTON && trace.disabled_us==trace.requested_us);
    assert(trace.alarm_gap_us==5000);
    hold(true,5); hold(false,5); stopped(); /* stop-release cannot restart */
    hold(true,5); hold(false,5); assert(armed());
    assert(!board_apply(0,0.5f,0.5f)); stopped(); /* stale hardware apply */

    reset(); assert(board_select(1)); hold(false,5); hold(true,5);
    board_stop(STOP_BUTTON); hold(false,5); stopped(); /* stop cancels pending start */
    hold(true,5); hold(false,5); assert(armed());
    assert(!board_apply(runtime.token,NAN,0.5f)); stopped();

    reset(); start(); hold(false,12); advance(40000); stopped();
    assert(reason()==STOP_SERVICE); tick(false); assert(runtime.motion.phase==MOTION_ABORTED);
    stopped(); hold(false,5); stopped();

    reset(); start(); enc.left_invalid=1; tick(false); stopped();
    motion_record_t r=latest(); assert(r.phase==MOTION_FAULT && r.fault==MOTION_ENCODER_INVALID);
    assert(r.stop_latched);

    reset(); start();
    for (unsigned i=0; i<360 && armed(); ++i) {
        enc.left_raw-=10; enc.right_raw+=10; tick(false);
    }
    stopped(); assert(runtime.motion.phase==MOTION_DONE && reason()==STOP_COMPLETE);
    r=latest(); assert(r.valid && r.left_counts>=3537 && r.right_counts>=3537);

    for (uint8_t kind=MOTION_LEFT_90; kind<=MOTION_RIGHT_180; ++kind) {
        reset(); start_kind(kind);
        for (unsigned i=0; i<400 && armed(); ++i) {
            enc.left_raw += runtime.motion.left.target<0 ? 10u : (uint32_t)-10;
            enc.right_raw += runtime.motion.right.target<0 ? (uint32_t)-10 : 10u;
            tick(false);
        }
        stopped(); assert(runtime.motion.phase==MOTION_DONE && reason()==STOP_COMPLETE);
    }
    reset(); enc.left_raw=3; enc.right_raw=UINT32_MAX-5u;
    motion_runtime_init(&runtime,enc,0,false); start();
    enc.left_raw-=10; enc.right_raw+=10; tick(false);
    r=latest(); assert(r.valid && r.left_counts==10 && r.right_counts==10);
    assert(r.left_mm_s==74 && r.right_mm_s==74); /* both raw counters wrapped */

    reset(); start(); enc.left_raw-=1000; tick(false); stopped();
    r=latest(); assert(!r.valid && r.left_counts==0 && r.left_mm_s==0 && r.stop_reason==STOP_RANGE);

    reset(); timer_ok=false; assert(!board_init()); assert(board_select(1));
    board_note_release(); uint16_t t; assert(!board_start(&t)); stopped();
    reset(); board_stop(STOP_INIT); assert(board_select(1)); board_note_release();
    assert(!board_start(&t)); stopped();
    puts("PASS actual board/runtime: held boot, local-only start, raw abort, no restart on release, pending cancellation, stale apply, NaN, starvation, encoder fault, all four endpoints, encoder wrap, range fault, init failure");
}
static void codec_tests(void)
{
    const int32_t counts[]={-30000,-1,0,1,30000};
    unsigned cases=0;
    for (unsigned i=0;i<5;++i) for (int32_t s=-500;s<=500;++s) {
        motion_record_t r={.captured_ms=UINT32_MAX,.left_counts=counts[i],.right_counts=-counts[i],
            .left_mm_s=s,.right_mm_s=-s,.kind=4,.phase=4,.fault=5,.stop_reason=6,
            .valid=true,.stop_latched=true}, decoded;
        uint8_t b[MOTION_RECORD_BYTES];
        assert(motion_record_encode(&r,b,sizeof b)); assert(motion_record_decode(b,sizeof b,&decoded));
        assert(decoded.captured_ms==r.captured_ms && decoded.left_counts==r.left_counts &&
            decoded.right_counts==r.right_counts && decoded.left_mm_s==r.left_mm_s &&
            decoded.right_mm_s==r.right_mm_s && decoded.kind==4 && decoded.phase==4 &&
            decoded.fault==5 && decoded.stop_reason==6 && decoded.valid && decoded.stop_latched);
        ++cases;
    }
    motion_record_t r={.valid=true}, decoded={.captured_ms=123};
    uint8_t b[MOTION_RECORD_BYTES], before[MOTION_RECORD_BYTES]; memset(b,0xaa,sizeof b); memcpy(before,b,sizeof b);
    r.left_mm_s=501; assert(!motion_record_encode(&r,b,sizeof b)); assert(!memcmp(b,before,sizeof b));
    r.left_mm_s=0; r.left_counts=30001; assert(!motion_record_encode(&r,b,sizeof b));
    r.left_counts=0; r.kind=5; assert(!motion_record_encode(&r,b,sizeof b));
    r.kind=0; assert(motion_record_encode(&r,b,sizeof b)); b[12]|=0x80;
    assert(!motion_record_decode(b,sizeof b,&decoded)); assert(decoded.captured_ms==123);
    assert(!motion_record_decode(b,sizeof b-1,&decoded));
    assert(!motion_record_decode(NULL,sizeof b,&decoded));
    r.valid=false; r.left_counts=1; assert(!motion_record_encode(&r,b,sizeof b));
    printf("PASS codec: %u signed boundary round trips; rejection and no partial outputs\n",cases);
    motion_record_t fixtures[]={
        {.valid=true},
        {.captured_ms=UINT32_MAX,.left_counts=-30000,.right_counts=30000,
         .left_mm_s=-500,.right_mm_s=500,.kind=4,.phase=4,.fault=5,
         .stop_reason=6,.valid=true,.stop_latched=true}
    };
    for (unsigned i=0;i<2;++i) {
        assert(motion_record_encode(&fixtures[i],b,sizeof b)); printf("FIXTURE ");
        for (unsigned j=0;j<sizeof b;++j) printf("%02x",b[j]);
        puts("");
    }
}
int main(void) { safety_tests(); runtime_tests(); codec_tests(); return 0; }
