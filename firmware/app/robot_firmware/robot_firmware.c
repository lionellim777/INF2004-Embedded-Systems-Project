/*
 * INF2004 autonomous robot - line following with terrain and obstacle safety
 *
 * Wiring:
 *   Motor 1 (left):  M1A GP8,  M1B GP9
 *   Motor 2 (right): M2A GP10, M2B GP11
 *   Servo signal: GP12 (held at centre)
 *   Offset left barcode sensor AO: GP26 via Grove 5
 *   Central pair left sensor AO:   GP28 via Grove 7
 *   Central pair right sensor AO:  GP27 via Grove 6
 *   GP20 button: start/abort selected bounded test
 *   HC-SR04+ TRIG: GP0, ECHO: GP1 (Grove 1)
 *
 * The robot starts stopped. Line-check mode is bounded to two seconds or a
 * wheel-count cap; two white sensors cannot establish whether a line is present.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "barcode_reader.h"
#include "encoders.h"
#include "navigation_commands.h"
#include "motion_control.h"
#include "motor_guard.h"
#include "run_diagnostics.h"
#include "line_check.h"
#include "barcode_acquisition.h"
#include "barcode_optical.h"
#include "barcode_signal.h"
#include "barcode_drive.h"
#include "course_control.h"
#include "terrain.h"
#include "calibration_wifi.h"

enum {
    LEFT_MOTOR_A_PIN = 8,
    LEFT_MOTOR_B_PIN = 9,
    RIGHT_MOTOR_A_PIN = 10,
    RIGHT_MOTOR_B_PIN = 11,
    SERVO_PIN = 12,
    START_STOP_BUTTON_PIN = 20,
    TERRAIN_CALIBRATE_BUTTON_PIN = 21,
    LEFT_IR_PIN = 26,
    CENTRE_IR_PIN = 28,
    RIGHT_IR_PIN = 27,
    ULTRASONIC_TRIG_PIN = 0,
    ULTRASONIC_ECHO_PIN = 1,
};

/* Values measured on the assembled robot on 23 September 2026. */
enum {
    LEFT_IR_WHITE = 191,
    LEFT_IR_BLACK = 3164,
    CENTRE_IR_WHITE = 178,
    CENTRE_IR_BLACK = 2908,
    RIGHT_IR_WHITE = 141,
    RIGHT_IR_BLACK = 842,
};

/* These signs reflect the motor wiring verified on the assembled robot. */
static const float LEFT_FORWARD_SIGN = 1.0f;
static const float RIGHT_FORWARD_SIGN = 1.0f;

static const int32_t LINE_PRESENT_LEVEL = IR_BLACK_LEVEL;
static const float OBSTACLE_STOP_CM = 30.0f;
static const uint64_t RANGE_INTERVAL_US = 100000u;

typedef struct {
    uint slice;
    uint channel_a;
    uint channel_b;
    float forward_sign;
} motor_t;

static motor_t left_motor;
static motor_t right_motor;
static uint16_t motor_wrap;
static uint servo_slice;
static uint servo_channel;
static volatile motor_guard_t drive_guard;
static repeating_timer_t drive_guard_timer;
static bool drive_guard_ready;

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static void motor_stop(motor_t *motor)
{
    pwm_set_chan_level(motor->slice, motor->channel_a, 0);
    pwm_set_chan_level(motor->slice, motor->channel_b, 0);
}

static void motors_stop(void)
{
    drive_guard.armed = false;
    motor_stop(&left_motor);
    motor_stop(&right_motor);
}

static void motor_set(motor_t *motor, float forward_throttle)
{
    float throttle = clamp_float(forward_throttle * motor->forward_sign,
                                 -1.0f, 1.0f);
    float magnitude = throttle < 0.0f ? -throttle : throttle;
    uint16_t level = (uint16_t)((float)(motor_wrap + 1u) * magnitude);

    if (throttle > 0.0f) {
        pwm_set_chan_level(motor->slice, motor->channel_a, level);
        pwm_set_chan_level(motor->slice, motor->channel_b, 0);
    } else if (throttle < 0.0f) {
        pwm_set_chan_level(motor->slice, motor->channel_a, 0);
        pwm_set_chan_level(motor->slice, motor->channel_b, level);
    } else {
        motor_stop(motor);
    }
}

/* Alarm IRQ remains independent of the foreground Wi-Fi poll. It only
 * removes drive; the main loop reports/latches the fault after it resumes. */
static bool drive_guard_alarm(repeating_timer_t *timer)
{
    (void)timer;
    if (motor_guard_tick(&drive_guard, time_us_32())) { motors_stop(); }
    return true;
}

static void motors_drive(float left, float right)
{
    uint32_t irq = save_and_disable_interrupts();
    if (!drive_guard_ready || !motor_guard_refresh(&drive_guard, time_us_32(),
                                                  left != 0.0f || right != 0.0f)) {
        motors_stop();
    } else {
        motor_set(&left_motor, left);
        motor_set(&right_motor, right);
    }
    restore_interrupts(irq);
}

static void motor_init_one(motor_t *motor, uint pin_a, uint pin_b,
                           float forward_sign)
{
    gpio_set_function(pin_a, GPIO_FUNC_PWM);
    gpio_set_function(pin_b, GPIO_FUNC_PWM);

    motor->slice = pwm_gpio_to_slice_num(pin_a);
    motor->channel_a = pwm_gpio_to_channel(pin_a);
    motor->channel_b = pwm_gpio_to_channel(pin_b);
    motor->forward_sign = forward_sign;

    pwm_set_clkdiv(motor->slice, 1.0f);
    pwm_set_wrap(motor->slice, motor_wrap);
    motor_stop(motor);
    pwm_set_enabled(motor->slice, true);
}

static void motors_init(void)
{
    motor_wrap = (uint16_t)(clock_get_hz(clk_sys) / 20000u - 1u);
    motor_init_one(&left_motor, LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN,
                   LEFT_FORWARD_SIGN);
    motor_init_one(&right_motor, RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN,
                   RIGHT_FORWARD_SIGN);
}

static void servo_init_centred(void)
{
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    servo_slice = pwm_gpio_to_slice_num(SERVO_PIN);
    servo_channel = pwm_gpio_to_channel(SERVO_PIN);

    float divider = (float)clock_get_hz(clk_sys) / 1000000.0f;
    pwm_set_clkdiv(servo_slice, divider);
    pwm_set_wrap(servo_slice, 19999u);
    pwm_set_chan_level(servo_slice, servo_channel, 1500u);
    pwm_set_enabled(servo_slice, true);
}

static void line_sensors_init(void)
{
    adc_init();
    adc_gpio_init(LEFT_IR_PIN);
    adc_gpio_init(CENTRE_IR_PIN);
    adc_gpio_init(RIGHT_IR_PIN);
}

static void ultrasonic_init(void)
{
    gpio_init(ULTRASONIC_TRIG_PIN);
    gpio_set_dir(ULTRASONIC_TRIG_PIN, GPIO_OUT);
    gpio_put(ULTRASONIC_TRIG_PIN, 0);
    gpio_init(ULTRASONIC_ECHO_PIN);
    gpio_set_dir(ULTRASONIC_ECHO_PIN, GPIO_IN);
}

static bool ultrasonic_read_cm(float *distance_cm)
{
    gpio_put(ULTRASONIC_TRIG_PIN, 0);
    sleep_us(2);
    gpio_put(ULTRASONIC_TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(ULTRASONIC_TRIG_PIN, 0);

    uint64_t deadline = time_us_64() + 30000u;
    while (!gpio_get(ULTRASONIC_ECHO_PIN)) {
        if (time_us_64() >= deadline) {
            return false;
        }
        tight_loop_contents();
    }
    uint64_t start = time_us_64();
    deadline = start + 30000u;
    while (gpio_get(ULTRASONIC_ECHO_PIN)) {
        if (time_us_64() >= deadline) {
            return false;
        }
        tight_loop_contents();
    }
    *distance_cm = (float)(time_us_64() - start) * 0.01715f;
    return *distance_cm >= 2.0f && *distance_cm <= 400.0f;
}

static uint16_t line_sensor_read(uint gpio_pin)
{
    adc_select_input(gpio_pin - 26u);
    busy_wait_us_32(5);

    uint32_t total = 0;
    for (uint sample = 0; sample < 8u; ++sample) {
        total += adc_read();
    }
    return (uint16_t)(total / 8u);
}

/* ADC has exactly one owner after this timer starts. The IRQ never logs,
 * decodes, uses Wi-Fi or calls RTOS services. Foreground copies under IRQ mask. */
static repeating_timer_t ir_timer;
static volatile uint16_t ir_latest[3];
static volatile uint32_t ir_last_us;
static volatile uint32_t adc_max_us;
static barcode_sampler_t barcode_sampler;
static barcode_signal_t barcode_signal;
static volatile uint16_t barcode_filtered;
static barcode_optical_t optical;
static bool optical_pending;
static bool optical_drive_pending, optical_drive_frozen, optical_drive_aborted;
static barcode_batch_t barcode_batch;
static barcode_capture_t barcode_capture;
static unsigned stream_logged, stream_rejected_logged, stream_timeouts_logged, stream_paused_fragments;
static uint32_t stream_motion_logged,stream_pause_logged,stream_zero_logged;
static bool scan_kind(motion_kind_t kind)
{
    return kind==MOTION_BARCODE || kind==MOTION_CONTINUOUS||kind==MOTION_COURSE;
}
static barcode_edge_t barcode_trace[128];
static unsigned barcode_trace_count;
static bool barcode_trace_overflow;
static uint32_t barcode_origin_us, barcode_stale_age_us;
static bool ir_timer_ready;

static bool ir_sample_alarm(repeating_timer_t *timer)
{
    (void)timer;
    uint32_t start = time_us_32();
    ir_latest[0] = line_sensor_read(LEFT_IR_PIN);
    ir_latest[1] = line_sensor_read(CENTRE_IR_PIN);
    ir_latest[2] = line_sensor_read(RIGHT_IR_PIN);
    ir_last_us = start;
    barcode_filtered=barcode_signal_update(&barcode_signal,ir_latest[0]);
    if(barcode_sampler.enabled && barcode_sampler.motion.enabled){
        encoder_sample_t e=encoders_snapshot();
        barcode_sampler_sample_motion(&barcode_sampler,barcode_filtered,start,e.left_raw,e.right_raw);
    }else barcode_sampler_sample(&barcode_sampler, barcode_filtered, start);
    if(optical.armed && barcode_sampler.enabled && barcode_sampler.stats.edges)
        barcode_optical_trigger(&optical,start);
    barcode_optical_sample(&optical, ir_latest[0], start);
    uint32_t duration = time_us_32() - start;
    if (barcode_sampler.enabled && duration > adc_max_us) { adc_max_us = duration; }
    return true;
}

static void optical_finish(bool aborted, bool driving)
{
    uint32_t irq=save_and_disable_interrupts();
    if(!driving)barcode_optical_end(&optical,time_us_32());
    restore_interrupts(irq);
    unsigned missing=0;uint16_t low=UINT16_MAX,high=0;
    for(unsigned i=0;i<OPTICAL_BINS;i++){
        if(optical.low[i]==UINT16_MAX){missing++;continue;}
        if(optical.low[i]<low)low=optical.low[i];
        if(optical.high[i]>high)high=optical.high[i];
    }
    printf("[OPTICAL RESULT] motors=OFF samples=%lu raw=%u..%u max_gap_us=%lu empty_bins=%u aborted=%u sampling_ok=%u source=%s; diagnostic only, no threshold changes.\n",
           (unsigned long)optical.samples,optical.samples?low:0,high,
           (unsigned long)optical.max_gap_us,missing,aborted,
           !aborted && optical.samples && !missing && optical.max_gap_us<=BARCODE_SAMPLE_GAP_US,driving?"DRIVE":"STATIONARY");
    /* Each line is bounded below the retained log line limit. Missing windows
     * use '-' instead of inventing zero-valued samples. Browser plots extrema. */
    for(unsigned i=0;i<OPTICAL_BINS;i+=25){
        printf("[OPTICAL DATA] bin=%u",i);
        for(unsigned j=i;j<i+25;j++){
            if(optical.low[j]==UINT16_MAX)printf(" -");
            else printf(" %u:%u",optical.low[j],optical.high[j]);
        }
        printf("\n");
    }
    printf("[OPTICAL END] window_ms=10 duration_ms=5000; download log before next test.\n");
    optical_pending=false;
}

static course_control_t course;
static center_baseline_t center_baseline;
static bool course_run;
static int32_t blackness(uint16_t raw,int32_t white,int32_t black);
static void barcode_drain(bool end)
{
    uint32_t irq = save_and_disable_interrupts();
    if (end) { barcode_sampler.enabled = false; }
    barcode_sampler_take(&barcode_sampler, &barcode_batch);
    bool on_crossing=blackness(ir_latest[1],CENTRE_IR_WHITE,CENTRE_IR_BLACK)>=IR_BLACK_LEVEL &&
                     blackness(ir_latest[2],RIGHT_IR_WHITE,RIGHT_IR_BLACK)>=IR_BLACK_LEVEL;
    restore_interrupts(irq);
    if(course_run && !end && (barcode_batch.count || barcode_batch.stats.black))
        course_note_barcode_activity(&course,time_us_64());
    for(unsigned i=0;i<barcode_batch.count;i++){
        if(barcode_trace_count==128u){barcode_trace_overflow=true;break;}
        barcode_trace[barcode_trace_count++]=(barcode_edge_t){
            barcode_batch.edges[i].us-barcode_origin_us,barcode_batch.edges[i].black};
    }
    if(course_run && !barcode_capture.independent_optical && !barcode_capture.distance_mode && !course_reads_barcodes(&course)) {
        /* Keep sampler faults visible, but discard optical fragments while
         * braking/pivoting/seeking. Resume only after a fresh white quiet zone. */
        barcode_capture.stats=barcode_batch.stats;
        if(barcode_batch.stats.gap_fault||barcode_batch.stats.overflow)barcode_capture.integrity_fault=true;
        if(barcode_capture.reader.active)stream_paused_fragments++;
        barcode_reader_init(&barcode_capture.reader);
        barcode_capture.waiting_gap=true;barcode_capture.white_tracking=false;
    }else barcode_capture_consume(&barcode_capture, &barcode_batch);
    if(barcode_trace_overflow && !barcode_capture.continuous){barcode_capture.integrity_fault=true;barcode_capture.decoded=0;}
    if(barcode_capture.continuous){
        if(stream_motion_logged!=barcode_capture.motion_aborts ||
           stream_pause_logged!=barcode_capture.pause_timeouts ||
           stream_zero_logged!=barcode_capture.zero_distance_aborts){
            const char *reason=stream_motion_logged!=barcode_capture.motion_aborts?"REVERSE_OR_ENCODER_JUMP":
                stream_pause_logged!=barcode_capture.pause_timeouts?"NO_PROGRESS_2S":"EDGE_WITHOUT_TRAVEL";
            printf("[BARCODE DISCARD] reason=%s completed_runs=%u motion_aborts=%lu pause_timeouts=%lu zero_distance_aborts=%lu; new white gap required.\n",
                   reason,barcode_capture.saved_runs,(unsigned long)barcode_capture.motion_aborts,
                   (unsigned long)barcode_capture.pause_timeouts,(unsigned long)barcode_capture.zero_distance_aborts);
            printf("[BARCODE DISCARD WIDTHS] sumcounts_x1000=");
            for(unsigned i=0;i<barcode_capture.saved_runs;i++)
                printf("%s%lu",i?",":"",(unsigned long)barcode_runs_get(&barcode_capture.last_runs,i));
            printf("\n");
            stream_motion_logged=barcode_capture.motion_aborts;stream_pause_logged=barcode_capture.pause_timeouts;
            stream_zero_logged=barcode_capture.zero_distance_aborts;
        }
        while(stream_logged<barcode_capture.event_count){
            char letter=barcode_capture.events[stream_logged].letter;
            const char *disposition="RECORD_ONLY";
            char previous=course.pending_source;
            if(course_run && !end && !barcode_capture.integrity_fault){
                disposition=course_on_barcode(&course,letter,encoders_snapshot(),time_us_64(),on_crossing);
                /* Immediate turns/STOP remove old forward PWM before sonar or
                 * logging. The course controller applies subsequent outputs. */
                if(course.phase==COURSE_BRAKE||course.phase==COURSE_FAILED)motors_stop();
            }
            printf("[BARCODE EVENT] seq=%u letter=%c at_ms=%lu action=%s\n",
                   stream_logged+1,letter,(unsigned long)(barcode_capture.events[stream_logged].us/1000u),disposition);
            if(course_run && !end && (letter=='Y'||letter=='Z')){
                char action=letter=='Y'?course.config.y_action:course.config.z_action;
                printf("[ROUTE BARCODE] source=%c selected=%s prior_waiting=%c remaining=%u deferred=%u.\n",
                       letter,course_action_name(action),previous?previous:'-',course.remaining_junctions,course.deferred);
            }
            stream_logged++;
        }
        if(stream_rejected_logged!=barcode_capture.rejected || stream_timeouts_logged!=barcode_capture.timeouts){
            printf("[BARCODE RETRY] rejected=%lu timeouts=%lu; %s.\n",
                   (unsigned long)barcode_capture.rejected,(unsigned long)barcode_capture.timeouts,
                   barcode_capture.independent_optical?"searching for next complete framed letter":"wait250ms white before rearming");
            printf("[BARCODE PARTIAL] completed_runs=%u expected=29 widths_%s=",barcode_capture.saved_runs,barcode_capture.distance_mode?"sumcounts_x1000":"us");
            for(unsigned i=0;i<barcode_capture.saved_runs;i++)
                printf("%s%lu",i?",":"",(unsigned long)barcode_runs_get(&barcode_capture.last_runs,i));
            printf(" active=%u raw_range=%u..%u max_gap_us=%lu. No accepted letter from this capture.\n",
                barcode_capture.reader.active,barcode_batch.stats.raw_min,barcode_batch.stats.raw_max,
                (unsigned long)barcode_batch.stats.max_gap_us);
            stream_rejected_logged=barcode_capture.rejected;stream_timeouts_logged=barcode_capture.timeouts;
        }
    }
}

static void barcode_result(void)
{
    const barcode_sample_stats_t *s = &barcode_capture.stats;
    if(barcode_capture.continuous){
        printf("[BARCODE RESULT] mode=CONTINUOUS reads=%u sequence=",barcode_capture.event_count);
        for(unsigned i=0;i<barcode_capture.event_count;i++)printf("%c",barcode_capture.events[i].letter);
        printf(" rejected=%lu timeouts=%lu edges=%lu max_gap_us=%lu integrity_fault=%u event_overflow=%u trace_truncated=%u route_mode=%s",
               (unsigned long)barcode_capture.rejected,(unsigned long)barcode_capture.timeouts,
               (unsigned long)s->edges,(unsigned long)s->max_gap_us,barcode_capture.integrity_fault,
               barcode_capture.event_overflow,barcode_trace_overflow,course_run?"Y_Z_CONFIGURED":"RECORD_ONLY");
        printf(" width_source=%s motion_breaks=%lu motion_aborts=%lu pause_timeouts=%lu zero_distance_aborts=%lu",barcode_capture.distance_mode?"ENCODER_AXLE_DISTANCE":"TIME",(unsigned long)s->motion_breaks,
               (unsigned long)barcode_capture.motion_aborts,(unsigned long)barcode_capture.pause_timeouts,(unsigned long)barcode_capture.zero_distance_aborts);
        printf(" adc_max_us=%lu",(unsigned long)adc_max_us);
        printf(" sample_period_us=500 filter_samples=12 black_on=226 white_off=220 paused_fragments=%u last_ended_runs=%u last_ended_widths_%s=",stream_paused_fragments,barcode_capture.saved_runs,barcode_capture.distance_mode?"sumcounts_x1000":"us");
        for(unsigned i=0;i<barcode_capture.saved_runs;i++)
            printf("%s%lu",i?",":"",(unsigned long)barcode_runs_get(&barcode_capture.last_runs,i));
        printf(" active_runs=%u.\n",barcode_capture.reader.active?barcode_capture.reader.run_count:0);
        return;
    }
    const barcode_runs_t *runs = barcode_capture.reader.active ?
        &barcode_capture.reader.runs : &barcode_capture.last_runs;
    unsigned count = barcode_capture.reader.active ?
        barcode_capture.reader.run_count : barcode_capture.saved_runs;
    printf("[BARCODE RESULT] decoded=%c valid=%u samples=%lu edges=%lu raw=%u..%u "
           "frames=%lu rejected=%lu timeouts=%lu max_gap_us=%lu adc_max_us=%lu "
           "queue_peak=%u overflow=%u gap_fault=%u integrity_fault=%u trace_overflow=%u stale_age_us=%lu runs=%u widths_us=",
           barcode_capture.decoded ? barcode_capture.decoded : '-',
           barcode_capture.decoded && !barcode_capture.integrity_fault ? 1u : 0u,
           (unsigned long)s->samples, (unsigned long)s->edges,
           s->samples ? s->raw_min : 0, s->raw_max,
           (unsigned long)barcode_capture.frames, (unsigned long)barcode_capture.rejected,
           (unsigned long)barcode_capture.timeouts, (unsigned long)s->max_gap_us,
           (unsigned long)adc_max_us, s->high_water, s->overflow, s->gap_fault, barcode_capture.integrity_fault,
           barcode_trace_overflow,(unsigned long)barcode_stale_age_us,count);
    for (unsigned i = 0; i < count; i++) {
        printf("%s%lu", i ? "," : "", (unsigned long)barcode_runs_get(runs, i));
    }
    printf(". Record only; no navigation action.\n");
    for(unsigned part=0;part<2;part++){
        printf("[SCAN EDGES %u] total=%u time_ms:level=",part+1,barcode_trace_count);
        for(unsigned i=part*64;i<barcode_trace_count && i<(part+1)*64;i++)
            printf("%s%lu:%c",i==part*64?"":",",(unsigned long)(barcode_trace[i].us/1000u),barcode_trace[i].black?'B':'W');
        printf("\n");
    }
}

static int32_t blackness(uint16_t raw, int32_t white, int32_t black)
{
    int32_t result = ((int32_t)raw - white) * 1000 / (black - white);
    if (result < 0) {
        return 0;
    }
    if (result > 1000) {
        return 1000;
    }
    return result;
}

typedef struct {
    bool stable;
    bool candidate;
    uint64_t changed_us;
} button_t;

static bool button_fell(button_t *button, uint pin, uint64_t now_us)
{
    bool level = gpio_get(pin);
    if (level != button->candidate) {
        button->candidate = level;
        button->changed_us = now_us;
    }
    if (button->stable != level && now_us - button->changed_us >= 25000u) {
        button->stable = level;
        return !level;
    }
    return false;
}

static void print_controls(void)
{
#ifdef ROBOCAR_WIRELESS_CALIBRATION
    printf("\nAAI10 MOTION INTEGRATION v5.34 WIRELESS | independent optical barcode | %s %s\n", __DATE__, __TIME__);
#else
    printf("\nAAI10 MOTION INTEGRATION v5.34 USB | %s %s\n", __DATE__, __TIME__);
#endif
    printf("Starts STOPPED. Existing IR, sonar, servo and terrain settings retained.\n");
    printf("Batch 5: CPR2543 wheel60mm track115mm; turn early290 L90+65 R180-60.\n");
    printf("Straight 30cm cutoff=3537 counts, adjusted from measured v2 travel 320mm.\n");
    printf("Provisional 30cm-only endpoint. Target stop checked every loop; PI remains 100ms.\n");
    printf("R90-only target=987 counts/wheel (+58 provisional); L90 and R180 unchanged.\n");
    printf("Motor service guard: 150ms timeout, checked by 10ms alarm; requires GP20 restart.\n");
    printf("Straight PWM position balance: 0.0003/count, max +/-0.04. Turn PI unchanged.\n");
    printf("Compact barcode reader: %u bytes (previously 132). Full 1352B layout NOT implemented.\n",
           (unsigned)sizeof(barcode_reader_t));
    printf("VS Code serial 115200. Send ONE character while stopped to SELECT:\n");
    if(!ROBOCAR_CENTER_LINE)printf(" f=straight30cm  l=left90  r=right90  u=right180  t=line-check2s (n alias)  b=barcode-pass  c=continuous-scan  ?=help\n");
    printf("Selection alone NEVER moves motors. GP20=start/abort. x=serial stop.\n");
    printf("v=5-second GP26 optical sweep, motors OFF; GP20/x cancel it.\n");
    if(!ROBOCAR_CENTER_LINE)printf("All tests have a 3-second countdown. Line check stops at 2s or 3537 wheel counts or both black.\n");
    if(!ROBOCAR_CENTER_LINE)printf("Barcode pass: 0.5ms ADC target, stop on read / rejection / capture timeout / 8s / 8000 counts; GP26 starts on WHITE.\n");
    printf("Decoder: colour-bias correction; one bounded expanded outer guard bar allowed; payload and other marker elements stay strict.\n");
    if(!ROBOCAR_CENTER_LINE)printf("Continuous scan:20s/24000count bound; read/reject/timeout continue;250ms white rearm.\n");
    if(!ROBOCAR_CENTER_LINE)printf("Both line black: straight900cps bridge, max300ms/360counts;50ms clear required. No cliff detection or navigation turns.\n");
    if(!ROBOCAR_CENTER_LINE)printf("Barcode speed: target1200counts/s per wheel, 100ms PI including steering; inner target900/outer1200cps; ramp1500cps/s.\n");
    if(ROBOCAR_CENTER_LINE)printf("GP28-centred mission: select on website, GP20 starts after 3s countdown. GP28 must see black. f/l/r/u retain calibrated motor diagnostics.\n");
    if(ROBOCAR_CENTER_LINE)printf("CENTER SENSOR BUILD: GP28 on tape, GP27 right, GP26 barcode; 600cps; encoder-balanced start, response-checked optical steering, strong-black recovery, short local barcode-area search; legacy tracking diagnostics disabled.\n");
    printf("Y/Z mission: web actions trigger on decoded barcode; delayed actions count future crossings. New read replaces waiting action; committed turn defers newest; STOP wins.\n");
    printf("Course advance default120mm from measured sensor lead; nominal encoder scale, centering/curves/junctions pending physical validation.\n");
    printf("Course barcode widths use encoder axle distance: forward steering/brief holds preserved; reverse excursions discard partial frames. Tight-curve sensor path remains unvalidated.\n");
    printf("GP26 barcode BLACK filtered>=%u / WHITE filtered<=%u; 12-sample mean at2kHz, provisional low-contrast calibration. Raw optical test unchanged.\n",BARCODE_DRIVE_BLACK_ON,BARCODE_DRIVE_BLACK_OFF);
    printf("GP21=level calibration while stopped. Initial level calibration is automatic.\n");
    printf("IR order: barcode GP26 / line-left GP28 / line-right GP27. Barcode record-only in line check.\n");
    printf("No obstacle bypass. No-echo is UNKNOWN, not clear.\n\n");
    calibration_wifi_help();
}

int main(void)
{
    motors_init();
    drive_guard_ready = add_repeating_timer_ms(-10, drive_guard_alarm, NULL, &drive_guard_timer);
    stdio_init_all();
    calibration_wifi_capture_init();
    servo_init_centred();
    line_sensors_init();
    ultrasonic_init();
    encoders_init();
    gpio_init(START_STOP_BUTTON_PIN);
    gpio_set_dir(START_STOP_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(START_STOP_BUTTON_PIN);
    gpio_init(TERRAIN_CALIBRATE_BUTTON_PIN);
    gpio_set_dir(TERRAIN_CALIBRATE_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(TERRAIN_CALIBRATE_BUTTON_PIN);
    sleep_ms(2500);
    (void)calibration_wifi_init();
    print_controls();
    if (terrain_init()) {
        terrain_calibrate();
    }

    ir_timer_ready = add_repeating_timer_us(-(int64_t)BARCODE_SAMPLE_US, ir_sample_alarm, NULL, &ir_timer);
    sleep_ms(2);

    button_t start_button = {.stable = gpio_get(START_STOP_BUTTON_PIN),
                            .candidate = gpio_get(START_STOP_BUTTON_PIN)};
    button_t calibrate_button = {.stable = gpio_get(TERRAIN_CALIBRATE_BUTTON_PIN),
                                .candidate = gpio_get(TERRAIN_CALIBRATE_BUTTON_PIN)};
    bool running = false;
    bool usb_was_connected = false;
    uint64_t next_report_us = 0, next_range_us = 0;
    uint64_t arming_until_us = 0, settling_until_us = 0;
    unsigned last_countdown = 0;
    float distance_cm = 0.0f;
    bool range_valid = false;
    unsigned close_readings = 0;
    navigation_command_t last_navigation_command = NAV_COMMAND_NONE;
    motion_kind_t selected = MOTION_NONE, pending = MOTION_NONE;
#ifdef ROBOCAR_WIRELESS_CALIBRATION
    /* Power-bank reboot must not fall back to line-follow when GP20 is pressed. */
    selected = MOTION_STRAIGHT_30CM;
#endif
    course_config_t selected_course={'s','s',120};
    motion_control_t motion;
    motion_init(&motion);
    run_diagnostics_t diagnostics = {0};
    barcode_reader_t barcode_reader;
    barcode_reader_init(&barcode_reader);
    const char *motion_state = "STOPPED";
    barcode_progress_t barcode_progress = {0};

    uint64_t previous_loop_us = 0u;
    uint32_t max_loop_us = 0u;
    while (true) {
        uint64_t now_us = time_us_64();
        if (previous_loop_us != 0u && now_us - previous_loop_us > max_loop_us) {
            max_loop_us = (uint32_t)(now_us - previous_loop_us);
        }
        previous_loop_us = now_us;
        calibration_wifi_poll(!running && settling_until_us == 0u && !optical_pending);
        now_us = time_us_64();
        bool external_stop = false;
        if (drive_guard.tripped && running) {
            motors_stop();
            if (motion.phase == MOTION_ACTIVE) { settling_until_us = now_us + 500000u; }
            motion_abort(&motion);
            running = false;
            arming_until_us = 0u;
            external_stop = true;
            motion_state = "MOTOR_SERVICE_TIMEOUT";
            printf("[STOP] MOTOR_SERVICE_TIMEOUT: alarm removed PWM; GP20 required to restart.\n");
        }
        bool usb_connected = stdio_usb_connected();
        if (usb_connected && !usb_was_connected) {
            print_controls();
            printf("Selected=%s. Run=%u\n", motion_name(selected), running ? 1u : 0u);
        }
        usb_was_connected = usb_connected;

        /* Bounded input processing. Serial can stop/select, but cannot start. */
        for (unsigned i = 0; i < 16u; ++i) {
            int ch = calibration_wifi_getchar();
            if (ch < 0) { ch = getchar_timeout_us(0u); }
            if (ch == PICO_ERROR_TIMEOUT) { break; }
            if (ch >= 'A' && ch <= 'Z') { ch += 'a' - 'A'; }
            if (ch == 'x') {
                motors_stop();
                if(optical_pending)optical_finish(true,false);
                if (motion.phase == MOTION_ACTIVE) { settling_until_us = now_us + 500000u; }
                motion_abort(&motion);
                running = false;
                arming_until_us = 0;
                external_stop = true;
                motion_state = "EXTERNAL_STOP";
                printf("[STOP] USB/browser stop. GP20 required for another start.\n");
             } else if(ch=='v') {
                if(running || settling_until_us || optical_pending || !ir_timer_ready)
                    printf("[OPTICAL] Rejected: stop and wait for the current test to finish.\n");
                else {
                    motors_stop();
                    uint32_t irq=save_and_disable_interrupts();
                    barcode_optical_begin(&optical,time_us_32());
                    optical_pending=true;
                    restore_interrupts(irq);
                    printf("[OPTICAL START] motors=OFF duration_ms=5000; slide paper now; GP20 cancels, never starts during capture.\n");
                }
             } else if(optical_pending && ch!='?') {
                printf("[SELECT] Wait for optical capture to finish.\n");
             } else if(ch=='q') {
                if(running || settling_until_us!=0u)printf("[SELECT] Rejected while running/settling.\n");
                else {
                    course_config_t config=calibration_wifi_course_config();
                    if(course_config_valid(config)){
                        selected_course=config;selected=MOTION_COURSE;
                        printf("[SELECT Y/Z] Y=%s Z=%s junction_advance_mm=%u timer=none travel_limit=160000counts; GP20 starts; actions trigger on barcode.\n",course_action_name(config.y_action),course_action_name(config.z_action),config.advance_mm);
                    }else printf("[SELECT] Use course settings on the web page.\n");
                }
            } else if (ch == '?') {
                print_controls();
            } else if (ROBOCAR_CENTER_LINE && (ch=='n'||ch=='t'||ch=='b'||ch=='c')) {
                printf("[SELECT] Use Y/Z mission for GP28-centred tracking; legacy straddle diagnostics disabled.\n");
            } else if (ch == 'f' || ch == 'l' || ch == 'r' || ch == 'u' || ch == 'n' || ch == 't' || ch == 'b' || ch == 'c') {
                if (running || settling_until_us != 0u) {
                    printf("[SELECT] Rejected while running/settling. Stop first.\n");
                } else {
                    selected = ch == 'f' ? MOTION_STRAIGHT_30CM :
                        (ch == 'l' ? MOTION_LEFT_90 : (ch == 'r' ? MOTION_RIGHT_90 :
                        (ch == 'u' ? MOTION_RIGHT_180 : (ch == 'b' ? MOTION_BARCODE : (ch == 'c' ? MOTION_CONTINUOUS : MOTION_NONE)))));
                    printf("[SELECT] %s. No movement until GP20.\n", motion_name(selected));
                }
            }
        }

        bool start_pressed = button_fell(&start_button, START_STOP_BUTTON_PIN, now_us);
        if (start_pressed && !external_stop) {
            if(optical_pending) {
                motors_stop();optical_finish(true,false);
                external_stop=true;
            } else if (running) {
                motors_stop();
                if (motion.phase == MOTION_ACTIVE) { settling_until_us = now_us + 500000u; }
                motion_abort(&motion);
                running = false;
                arming_until_us = 0;
                motion_state = "BUTTON_STOP";
                printf("[STOP] GP20 abort. Both motors stopped.\n");
            } else if (settling_until_us != 0u) {
                printf("[START] Wait for settled result (500ms after stop).\n");
            } else if (!drive_guard_ready || !ir_timer_ready) {
                printf("[STOP] Motor guard / IR sampling alarm unavailable; restart firmware before driving.\n");
            } else {
                uint32_t irq = save_and_disable_interrupts();
                motor_guard_reset(&drive_guard);
                restore_interrupts(irq);
                running = true;
                barcode_reader_init(&barcode_reader);
                motion_init(&motion);
                pending = selected;
                course_run = selected==MOTION_COURSE;
                max_loop_us = 0u;
                calibration_wifi_reset_timing();
                arming_until_us = now_us + 3000000u;
                center_baseline=(center_baseline_t){0};
                last_countdown = 0u;
                printf("[START] %s. GP20 aborts.\n", motion_name(selected));
            }
        }
        if (button_fell(&calibrate_button, TERRAIN_CALIBRATE_BUTTON_PIN, now_us)) {
            if (running || settling_until_us != 0u || optical_pending) {
                printf("[TERRAIN] Stop before level calibration.\n");
            } else {
                motors_stop();
                terrain_calibrate();
                now_us = time_us_64();
            }
        }

        if(optical_pending && (uint32_t)(time_us_32()-optical.start_us)>=OPTICAL_DURATION_US)optical_finish(false,false);

        uint32_t ir_irq = save_and_disable_interrupts();
        uint16_t left_raw = ir_latest[0];
        uint16_t centre_raw = ir_latest[1];
        uint16_t right_raw = ir_latest[2];
        restore_interrupts(ir_irq);
        int32_t left_blackness = blackness(left_raw, LEFT_IR_WHITE, LEFT_IR_BLACK);
        int32_t centre_blackness = blackness(centre_raw, CENTRE_IR_WHITE, CENTRE_IR_BLACK);
        int32_t right_blackness = blackness(right_raw, RIGHT_IR_WHITE, RIGHT_IR_BLACK);
        now_us = time_us_64();
        const int32_t levels[3] = {left_blackness, centre_blackness, right_blackness};
        ir_roles_t roles = ir_roles(levels);
        char letter = '\0';
        bool following = running && arming_until_us == 0u &&
                         motion.phase == MOTION_ACTIVE && motion.kind == MOTION_NONE;
        bool barcode_ready = following && barcode_reader_update(
            &barcode_reader, roles.barcode, (uint32_t)now_us, &letter);
        bool barcode_scanning = following && barcode_reader_active(&barcode_reader);
        if (barcode_ready) {
            last_navigation_command = navigation_decode_symbol(letter);
            printf("[BARCODE] %c -> %s (record only during bounded line check; no turn).\n",
                   letter, navigation_command_name(last_navigation_command));
        }

        bool barcode_mode = running && motion.phase == MOTION_ACTIVE && scan_kind(motion.kind);
        bool barcode_finished = false;
        if (barcode_mode) {
            barcode_drain(false);
            barcode_finished = barcode_capture.integrity_fault ||
                (!barcode_capture.continuous && (barcode_capture.decoded || barcode_capture.rejected || barcode_capture.timeouts));
            if (barcode_finished) { motors_stop(); } /* Stop before blocking sonar/logging. */
            barcode_scanning = barcode_capture.reader.active;
        }

        bool range_fresh = false;
        if (now_us >= next_range_us) {
            range_fresh = true;
            range_valid = ultrasonic_read_cm(&distance_cm);
            next_range_us = time_us_64() + RANGE_INTERVAL_US;
            if (range_valid && distance_cm < OBSTACLE_STOP_CM) {
                if (close_readings < 2u) { ++close_readings; }
            } else {
                close_readings = 0u;
            }
        }
        now_us = time_us_64();
        encoder_sample_t enc = encoders_snapshot();
        bool line_visible = roles.left || roles.right;
        float left_throttle = 0.0f, right_throttle = 0.0f;

        if (running && close_readings >= 2u) {
            motors_stop();
            if (motion.phase == MOTION_ACTIVE) { settling_until_us = now_us + 500000u; }
            motion_abort(&motion);
            running = false;
            arming_until_us = 0u;
            motion_state = "OBSTACLE_STOP";
            printf("[STOP] OBSTACLE %.1fcm. Clear and press GP20 to restart.\n", distance_cm);
        } else if (running && arming_until_us != 0u) {
            motors_stop();
            motion_state = "MOTION_COUNTDOWN";
            if(ROBOCAR_CENTER_LINE && pending==MOTION_COURSE)
                center_baseline_add(&center_baseline,centre_blackness,now_us);
            if (now_us >= arming_until_us) {
                arming_until_us = 0u;
                bool began;
                if (pending == MOTION_NONE || scan_kind(pending)) {
                    motion_init(&motion);
                    motion.kind = pending;
                    motion.start = enc;
                    motion.start_us = now_us;
                    motion.phase = MOTION_ACTIVE;
                    motion.left.target = motion.right.target = scan_kind(pending) ?
                        (pending==MOTION_COURSE?COURSE_LIMIT_COUNTS:(pending==MOTION_CONTINUOUS?CONTINUOUS_LIMIT_COUNTS:BARCODE_DRIVE_LIMIT_COUNTS)) : LINE_CHECK_LIMIT_COUNTS;
                    began = true;
                    if (scan_kind(pending)) {
                        int32_t baseline=500,baseline_spread=0;
                        bool baseline_ok=!ROBOCAR_CENTER_LINE || pending!=MOTION_COURSE ||
                            center_baseline_value(&center_baseline,now_us,&baseline,&baseline_spread);
                        /* Sonar may have blocked since the earlier snapshot. */
                        uint32_t irq = save_and_disable_interrupts();
                        bool white = ir_latest[0] <= BARCODE_BLACK_OFF;
                        bool fresh = (uint32_t)(time_us_32() - ir_last_us) <= BARCODE_SAMPLE_GAP_US;
                        bool stream = pending==MOTION_CONTINUOUS || pending==MOTION_COURSE;
                        bool aligned=!ROBOCAR_CENTER_LINE || pending!=MOTION_COURSE ||
                            (baseline_ok && blackness(ir_latest[1],CENTRE_IR_WHITE,CENTRE_IR_BLACK)>=IR_BLACK_LEVEL);
                        if (fresh && aligned && (white || stream)) {
                            barcode_origin_us=time_us_32();
                            barcode_signal=(barcode_signal_t){0};
                            barcode_sampler_begin_levels(&barcode_sampler,BARCODE_DRIVE_BLACK_ON,BARCODE_DRIVE_BLACK_OFF);
                            if(ROBOCAR_CENTER_LINE && pending==MOTION_COURSE){
                                encoder_sample_t scan_start=encoders_snapshot();
                                barcode_sampler_enable_motion(&barcode_sampler,scan_start.left_raw,scan_start.right_raw);
                            }
                            barcode_optical_arm(&optical);
                            optical_drive_pending=true;
                            optical_drive_frozen=optical_drive_aborted=false;
                            adc_max_us = 0;
                        }
                        restore_interrupts(irq);
                        began = fresh && aligned && (white || stream);
                        if(stream){
                            barcode_capture_begin_stream(&barcode_capture,barcode_origin_us);
                            barcode_capture.distance_mode=false;
                            barcode_capture.independent_optical=ROBOCAR_CENTER_LINE && pending==MOTION_COURSE;
                            /* Line following does not require barcode white.
                             * Ignore any partial symbol under the sensor at
                             * start; arm decoding after a full quiet gap. */
                            barcode_capture.waiting_gap=true;
                            if(began)printf("[BARCODE] Reading waits for 250ms white; line following may start now.\n");
                        }
                        else barcode_capture_begin_drive(&barcode_capture);
                        stream_logged=stream_rejected_logged=stream_timeouts_logged=stream_paused_fragments=0;
                        stream_motion_logged=stream_pause_logged=stream_zero_logged=0;
                        barcode_trace_count=0;barcode_trace_overflow=false;barcode_stale_age_us=0;
                        barcode_progress = (barcode_progress_t){0};
                        if(course_run){
                            course_begin(&course,selected_course,enc,now_us);
                            course_set_center_reference(&course,baseline);
                            if(began && ROBOCAR_CENTER_LINE)
                                printf("[CENTER BASELINE] median=%ld iqr=%ld samples=%u frozen_for_run=1.\n",
                                    (long)baseline,(long)baseline_spread,center_baseline.count);
                        }
                        if (!began) {
                            motion_abort(&motion);
                            motion_state=!fresh?"IR_SAMPLING_UNAVAILABLE":!baseline_ok?"CENTER_BASELINE_NOT_READY":!aligned?"CENTER_START_NOT_ON_LINE":"BARCODE_START_NOT_WHITE";
                            if(!baseline_ok)printf("[CENTER BASELINE] rejected median=%ld iqr=%ld samples=%u. Need stable black while stopped.\n",
                                (long)baseline,(long)baseline_spread,center_baseline.count);
                            printf("[BARCODE] Start rejected: %s.\n",!fresh?"IR sampling is not live":!aligned?"place GP28 directly over black tape and hold still throughout countdown":"single-pass test requires GP26 on white");
                        }
                    }
                } else { began = motion_begin(&motion, pending, enc, now_us); }
                if (began) {
                    terrain_status_t initial = terrain_get_status();
                    run_diagnostics_begin(&diagnostics, initial.hump_count, initial.read_errors,
                                          initial.pitch_sequence);
                    printf("[MOTION GO] %s targets L=%ld R=%ld.\n", motion_name(pending),
                           (long)motion.left.target, (long)motion.right.target);
                } else {
                    running = false;
                    printf("[STOP] Motion request rejected: %s.\n",motion_state);
                }
            } else {
                unsigned countdown = (unsigned)((arming_until_us - now_us + 999999u) / 1000000u);
                if (countdown != last_countdown) {
                    printf("[ARM] %s in %u seconds. GP20 aborts.\n", motion_name(pending), countdown);
                    last_countdown = countdown;
                }
            }
        } else if (running && motion.phase == MOTION_ACTIVE &&
                   (motion.kind == MOTION_NONE || scan_kind(motion.kind))) {
            /* Consume any new barcode BEFORE choosing this loop's output.
             * Never apply a stale forward command after an immediate turn/STOP. */
            if(course_run){
                barcode_drain(false);
                uint32_t irq=save_and_disable_interrupts();
                left_raw=ir_latest[0];centre_raw=ir_latest[1];right_raw=ir_latest[2];
                restore_interrupts(irq);
                left_blackness=blackness(left_raw,LEFT_IR_WHITE,LEFT_IR_BLACK);
                centre_blackness=blackness(centre_raw,CENTRE_IR_WHITE,CENTRE_IR_BLACK);
                right_blackness=blackness(right_raw,RIGHT_IR_WHITE,RIGHT_IR_BLACK);
                const int32_t fresh_levels[3]={left_blackness,centre_blackness,right_blackness};
                roles=ir_roles(fresh_levels);
                if(ROBOCAR_CENTER_LINE)course_center_level(&course,centre_blackness);
                now_us=time_us_64();enc=encoders_snapshot();
            }
            int32_t left = motion_left_position(&motion, enc);
            int32_t right = motion_right_position(&motion, enc);
            motion.left.position = left;
            motion.right.position = right;
            bool invalid = enc.left_invalid != motion.start.left_invalid ||
                           enc.right_invalid != motion.start.right_invalid;
            unsigned previous_junctions=course.junctions,previous_turns=course.turns;
            bool had_deferred=course.deferred;
            line_check_output_t out = motion.kind==MOTION_COURSE ? course_step(&course,roles,enc,now_us) : scan_kind(motion.kind) ?
                barcode_drive_step_mode(roles, now_us - motion.start_us, left, right, invalid, &barcode_progress,motion.kind==MOTION_CONTINUOUS) :
                line_check_step(roles, now_us - motion.start_us, left, right, invalid);
            if(course_run){
                barcode_progress=course.drive;
                if(ROBOCAR_CENTER_LINE && course.center_level_valid)roles.left=course.center_visible;
            }
            if(course_run && course.junctions!=previous_junctions)
                printf("[JUNCTION] encounter=%u at_ms=%lu counts=%ld/%ld action=%s remaining=%u. Candidate from both sensors, verify physically.\n",course.junctions,(unsigned long)((now_us-motion.start_us)/1000u),(long)left,(long)right,course.phase==COURSE_CENTER?"CENTER_FOR_TURN":"STRAIGHT",course.remaining_junctions);
            if(course_run && course.turns!=previous_turns)
                printf("[ROUTE] turn complete; line edge reacquired; turns=%u pending=%c remaining=%u phase=%u.\n",course.turns,course.pending_source?course.pending_source:'-',course.remaining_junctions,(unsigned)course.phase);
            if(course_run && had_deferred && !course.deferred && course.turns!=previous_turns)
                printf("[ROUTE] Deferred barcode %c activated as %s after line recovery.\n",course.deferred_source,course_action_name(course.deferred_action));
            /* Re-drain after sonar: never drive past a detected capture fault/read. */
            if (scan_kind(motion.kind)) {
                if(!course_run)barcode_drain(false);
                uint32_t irq = save_and_disable_interrupts();
                uint32_t sampled=ir_last_us;
                uint32_t sample_age=(uint32_t)(time_us_32()-sampled);
                bool stale = sample_age > BARCODE_SAMPLE_GAP_US;
                restore_interrupts(irq);
                if (stale) { barcode_stale_age_us=sample_age;barcode_capture.integrity_fault = true; barcode_capture.decoded = 0; }
                out = barcode_capture_stop(out, &barcode_capture);
            }
            motion_state = out.state;
            if (out.stop) {
                motors_stop();
                if (scan_kind(motion.kind)) {
                    barcode_drain(true);
                    if (barcode_capture.integrity_fault) { motion_state = "BARCODE_SAMPLE_FAULT"; }
                }
                motion_abort(&motion);
                running = false;
                settling_until_us = now_us + 500000u;
                motion.left.cutoff_position = left;
                motion.right.cutoff_position = right;
                motion.left.cutoff_elapsed_us = motion.right.cutoff_elapsed_us =
                    (uint32_t)(now_us - motion.start_us);
                if(scan_kind(motion.kind) && barcode_progress.stall_mask){
                    /* Keep this in the retained stop line: live Wi-Fi logs may
                     * have gaps. Values describe the last commanded drive. */
                    printf("[MOTION STOP] %s result=%s counts=%ld/%ld stall_wheel=%s no_progress_ms=%lu/%lu last_target_cps=%.0f/%.0f last_measured_cps=%.0f/%.0f last_pwm=%.3f/%.3f. Motors stopped.\n",
                        motion_name(motion.kind),motion_state,(long)left,(long)right,
                        barcode_progress.stall_mask==3?"BOTH":barcode_progress.stall_mask==1?"LEFT":"RIGHT",
                        (unsigned long)barcode_progress.stall_left_ms,(unsigned long)barcode_progress.stall_right_ms,
                        barcode_progress.left_target,barcode_progress.right_target,
                        barcode_progress.left_cps,barcode_progress.right_cps,
                        barcode_progress.left_pwm,barcode_progress.right_pwm);
                }else{
                    printf("[MOTION STOP] %s result=%s counts=%ld/%ld. Diagnostic ended.\n",
                           motion_name(motion.kind), motion_state, (long)left, (long)right);
                }
            } else {
                left_throttle = out.left;
                right_throttle = out.right;
                motors_drive(left_throttle, right_throttle);
            }
        } else if (running && motion.phase == MOTION_ACTIVE) {
            motion_poll(&motion, enc, now_us);
            if (motion.phase == MOTION_ACTIVE) {
                left_throttle = motion.left.throttle;
                right_throttle = motion.right.throttle;
                motors_drive(left_throttle, right_throttle);
                motion_state = "ENCODER_MOTION";
            } else {
                motors_stop();
                running = false;
                motion_state = motion.phase == MOTION_DONE ? "MOTION_COMPLETE" : "MOTION_FAULT";
                settling_until_us = now_us + 500000u;
                printf("[MOTION STOP] %s result=%s reason=%s cutoff=%ld/%ld cutoff_us=%lu/%lu. Motors stopped.\n",
                       motion_name(motion.kind), motion_state, motion_fault_name(motion.fault),
                       (long)motion.left.cutoff_position, (long)motion.right.cutoff_position,
                       (unsigned long)motion.left.cutoff_elapsed_us,
                       (unsigned long)motion.right.cutoff_elapsed_us);
            }
        } else {
            motors_stop();
        }

        if (!running && barcode_sampler.enabled) { barcode_drain(true); }
        /* Freeze before settling/lifting can contaminate a driving trace.
         * Nothing is printed until PWM is off and settling has finished. */
        if(!running && optical_drive_pending && !optical_drive_frozen){
            uint32_t irq=save_and_disable_interrupts();
            optical_drive_aborted=optical.active || optical.armed;
            barcode_optical_end(&optical,time_us_32());
            optical_drive_frozen=true;
            restore_interrupts(irq);
        }
        if (running || settling_until_us != 0u) { terrain_poll(); }
        if (diagnostics.active) {
            const uint16_t raw[3] = {left_raw, centre_raw, right_raw};
            const bool black[3] = {left_blackness >= LINE_PRESENT_LEVEL,
                                  centre_blackness >= LINE_PRESENT_LEVEL,
                                  right_blackness >= LINE_PRESENT_LEVEL};
            terrain_status_t sample = terrain_get_status();
            run_diagnostics_sample(&diagnostics, raw, black, range_fresh, range_valid, distance_cm,
                                   sample.pitch_valid && sample.calibrated, sample.pitch_sequence,
                                   sample.current_pitch_deg);
        }
        if (settling_until_us != 0u && now_us >= settling_until_us) {
            enc = encoders_snapshot();
            int32_t left = motion_left_position(&motion, enc);
            int32_t right = motion_right_position(&motion, enc);
            if (motion.kind == MOTION_NONE || scan_kind(motion.kind)) {
                printf("[MOTION SETTLED] %s L=%ld R=%ld invalid=%lu/%lu outcome=%s.\n",
                       motion_name(motion.kind), (long)left, (long)right,
                       (unsigned long)(enc.left_invalid-motion.start.left_invalid),
                       (unsigned long)(enc.right_invalid-motion.start.right_invalid), motion_state);
            } else printf("[MOTION SETTLED] %s L=%ld/%ld error=%lld R=%ld/%ld error=%lld "
                   "invalid=%lu/%lu peakSkew=%ld cutoff=%ld/%ld cutoff_us=%lu/%lu.\n", motion_name(motion.kind),
                   (long)left, (long)motion.left.target, (long long)left - motion.left.target,
                   (long)right, (long)motion.right.target, (long long)right - motion.right.target,
                   (unsigned long)(enc.left_invalid - motion.start.left_invalid),
                   (unsigned long)(enc.right_invalid - motion.start.right_invalid),
                   (long)motion.peak_skew,
                   (long)motion.left.cutoff_position, (long)motion.right.cutoff_position,
                   (unsigned long)motion.left.cutoff_elapsed_us,
                   (unsigned long)motion.right.cutoff_elapsed_us);
            terrain_status_t final = terrain_get_status();
            printf("[CHECK] samples=%lu IR_L=%u..%u edges=%lu IR_C=%u..%u edges=%lu IR_R=%u..%u edges=%lu "
                   "Sonar_valid=%lu no_echo=%lu range_cm=%.1f..%.1f "
                   "Pitch_samples=%lu pitch_deg=%.1f..%.1f new_humps=%lu imu_errors=%lu outcome=%s.\n",
                   (unsigned long)diagnostics.samples,
                   diagnostics.ir_min[0], diagnostics.ir_max[0], (unsigned long)diagnostics.ir_edges[0],
                   diagnostics.ir_min[1], diagnostics.ir_max[1], (unsigned long)diagnostics.ir_edges[1],
                   diagnostics.ir_min[2], diagnostics.ir_max[2], (unsigned long)diagnostics.ir_edges[2],
                   (unsigned long)diagnostics.sonar_valid, (unsigned long)diagnostics.sonar_no_echo,
                   diagnostics.sonar_min, diagnostics.sonar_max, (unsigned long)diagnostics.pitch_samples,
                   diagnostics.pitch_min, diagnostics.pitch_max,
                   (unsigned long)(final.hump_count - diagnostics.start_humps),
                   (unsigned long)(final.read_errors - diagnostics.start_errors), motion_state);
            if (scan_kind(motion.kind)) { barcode_result(); }
            if(course_run)printf("[COURSE RESULT] encounters=%u turns=%u Y=%s Z=%s advance_mm=%u updates=%u replacements=%u deferred_replacements=%u pending=%c remaining=%u deferred=%c recoveries=%u trims=%u trim_reversals=%u trim_no_response=%u fixed_ref=%ld settle_coast=%ld/%ld sensor_layout=%s outcome=%s.\n",
                course.junctions,course.turns,course_action_name(course.config.y_action),course_action_name(course.config.z_action),course.config.advance_mm,
                course.route_updates,course.replacements,course.deferred_replacements,course.pending_source?course.pending_source:'-',course.remaining_junctions,
                course.deferred?course.deferred_source:'-',course.recoveries,course.trim_attempts,course.trim_reversals,course.trim_no_responses,(long)course.center_reference,(long)course.center_search.coast_l,(long)course.center_search.coast_r,ROBOCAR_CENTER_LINE?"GP28_CENTER":"STRADDLE",motion_state);
            diagnostics.active = false;
            printf("[TIMING] max_loop_us=%lu wifi_max_poll_us=%lu",
                   (unsigned long)max_loop_us, (unsigned long)calibration_wifi_max_poll_us());
            if (scan_kind(motion.kind)) {
                unsigned n = barcode_progress.speed_samples;
                printf(" clear_speed_samples=%u steering_intervals=%u", n, barcode_progress.steering_intervals);
                if (n) {
                    printf(" mean_cps=%.0f/%.0f range_left=%.0f..%.0f range_right=%.0f..%.0f",
                           barcode_progress.left_speed_sum / n, barcode_progress.right_speed_sum / n,
                           barcode_progress.left_speed_min, barcode_progress.left_speed_max,
                           barcode_progress.right_speed_min, barcode_progress.right_speed_max);
                }
                printf(" speed_stats=clear_intervals_after_0.5s raw_white=%lu raw_band=%lu raw_black=%lu white_dips=%lu longest_black_us=%lu",
                       (unsigned long)barcode_capture.stats.raw_white,
                       (unsigned long)barcode_capture.stats.raw_band,
                       (unsigned long)barcode_capture.stats.raw_black,
                       (unsigned long)barcode_capture.stats.white_dips,
                       (unsigned long)barcode_capture.stats.longest_black_us);
            }
            printf(". Run-scoped, not a timing guarantee.\n");
            if(optical_drive_pending){
                if(optical.samples){
                    printf("[OPTICAL START] source=DRIVE recorded_at_ms=%lu motors=OFF; retained first-activity trace, emitted after stop.\n",
                           (unsigned long)((optical.start_us-barcode_origin_us)/1000u));
                    optical_finish(optical_drive_aborted,true);
                }else printf("[OPTICAL DRIVE] no barcode edge triggered a recording.\n");
                optical_drive_pending=false;
            }
            settling_until_us = 0u;
        }
        if (now_us >= next_report_us) {
            printf("Run=%u State=%s Selected=%s Line=%s IR=%u/%u/%u Drive=%+.2f/%+.2f",
                   running ? 1u : 0u, motion_state, motion_name(selected),
                   ROBOCAR_CENTER_LINE?(roles.left?"CENTER_ON_TAPE":"CENTER_OFF_TAPE"):(line_visible ? "EDGE" : "BETWEEN_OR_LOST"), left_raw, centre_raw, right_raw,
                   left_throttle, right_throttle);
            printf(ROBOCAR_CENTER_LINE?" Roles=Barcode:%u/LineCenter:%u/LineRight:%u":" Roles=Barcode:%u/LineLeft:%u/LineRight:%u", roles.barcode, roles.left, roles.right);
            if(ROBOCAR_CENTER_LINE && course_run)printf(" Recoveries=%u Trims=%u TrimReversals=%u TrimNoResponse=%u Ref=%ld SteerCPS=%.0f YawError=%.1f YawPWM=%.3f",course.recoveries,course.trim_attempts,course.trim_reversals,course.trim_no_responses,(long)course.center_reference,course.drive.center_steer,course.drive.yaw_error,course.drive.yaw_pwm);
            if(course_run)printf(" Junctions=%u Turns=%u CoursePhase=%u Pending=%c Remaining=%u Deferred=%c",course.junctions,course.turns,(unsigned)course.phase,course.pending_source?course.pending_source:'-',course.remaining_junctions,course.deferred?course.deferred_source:'-');
            if(scan_kind(motion.kind)){
                printf(" BarcodeRawBlack=%u BarcodeCPS=%.0f/%.0f BaseTargetCPS=%u TargetCPS=%.0f/%.0f Edges=%lu Frames=%lu Rejected=%lu",
                       barcode_capture.stats.black,barcode_progress.left_cps,barcode_progress.right_cps,ROBOCAR_CENTER_LINE?600u:1200u,
                       barcode_progress.left_target,barcode_progress.right_target,
                       (unsigned long)barcode_capture.stats.edges,(unsigned long)barcode_capture.frames,
                       (unsigned long)barcode_capture.rejected);
            }
            if(scan_kind(motion.kind))printf(" GP26Filtered=%u",barcode_filtered);
            terrain_status_t terrain = terrain_get_status();
            printf(" Humps=%lu PeakPitch=%.1f Pitch=%.1f PitchValid=%u Terrain=%s",
                   (unsigned long)terrain.hump_count, terrain.highest_peak_angle_deg,
                   terrain.current_pitch_deg, terrain.pitch_valid && terrain.calibrated ? 1u : 0u,
                   terrain.calibrated ? "READY" : "UNAVAILABLE");
            if (range_valid) { printf(" Range=%.1fcm", distance_cm); }
            else { printf(" Range=NO_ECHO"); }
            printf(" Enc=%ld/%ld Invalid=%lu/%lu Barcode=%s Nav=%s\n",
                   (long)encoder_left_count(), (long)encoder_right_count(),
                   (unsigned long)enc.left_invalid, (unsigned long)enc.right_invalid,
                   barcode_scanning ? "CAPTURE" : "IDLE", navigation_command_name(last_navigation_command));
            if (motion.phase == MOTION_ACTIVE && motion.kind != MOTION_NONE && !scan_kind(motion.kind)) {
                printf("  Motion=%s L=%ld/%ld %.0fcps R=%ld/%ld %.0fcps peakSkew=%ld\n",
                       motion_name(motion.kind), (long)motion.left.position, (long)motion.left.target,
                       motion.left.cps, (long)motion.right.position, (long)motion.right.target,
                       motion.right.cps, (long)motion.peak_skew);
            }
            next_report_us = time_us_64() + 250000u;
        }
        sleep_ms(5);
    }
}
