#include "board.h"
#include "encoders.h"
#include "motion_runtime.h"
#include "network_service.h"
#include "track_sensor.h"
#include "version.h"
#include "port/mtk_bridge.h"
#include <inttypes.h>
#include <stdio.h>
#include "hardware/clocks.h"
#include "hardware/exception.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "network_settings.h"
#include "runtime_report.h"
#include "port/task_observe.h"
#ifdef RTOS_OBSTACLE
#include "obstacle_io.h"
#endif
#ifdef RTOS_TERRAIN
#include "terrain.h"
#include "terrain_telemetry.h"
#endif
#ifdef RTOS_TERRAIN
#define PICKUP_IR_RAW 3500u  /* all three IR sensors above this: nothing under the car */
#define PICKUP_HOLD_MS 150u  /* for this long: ignores a single stray sample */
#define CALIBRATION_RETRY_MS 3000u /* between level-calibration retries while uncalibrated and stopped */
#endif
#ifndef RTOS_WAIT_FOR_USB
#define RTOS_WAIT_FOR_USB 0
#endif

/* Startup/exception adapter follows Justin's pinned single-core bridge.
 * This target never starts core1 and does not link a second network runtime. */
#define KERNEL_POOL_SIZE (32u * 1024u)
extern void knl_dispatch_entry(void);
extern void knl_systim_inthdr(void);
extern int mtk_kernel_main(void);
void *knl_lowmem_top;
void *knl_lowmem_limit;
static uint8_t kernel_pool[KERNEL_POOL_SIZE] __attribute__((aligned(8)));
static motion_runtime_t runtime;
#ifdef RTOS_TRACK
/* Latest course/track status text for the MQTT track_status record. The string
 * is always a static literal and the pointer read is a single aligned word. */
const char *rtos_track_status(void){const char *s=runtime.track.status;return s?s:"IDLE";}
#endif
static void motion_task(int start_code, void *context)
{
    (void)start_code; (void)context;
    motion_runtime_init(&runtime, encoders_snapshot(), time_us_32(), board_button_down());
#ifdef RTOS_HUMP_TEST_BOOT
    runtime.long_straight = true; /* matches the boot selection below: 50 cm slow move */
#endif
    for (;;) {
        bool armed;uint8_t reason;board_gate_snapshot(&armed,&reason);
        runtime_measure_run(runtime.token,armed);
        runtime_span_t span=runtime_measure_begin(RUNTIME_MOTION,time_us_32());
        motion_runtime_step(&runtime, encoders_snapshot(), time_us_64(), board_button_down());
        board_gate_snapshot(&armed,&reason);runtime_measure_run(runtime.token,armed);
        runtime_measure_end(RUNTIME_MOTION,span,time_us_32());
        mtk_bridge_delay_ms(1);
    }
}
static void decoder_task(int start_code,void *context)
{
    (void)start_code;(void)context;
    for(;;){
        runtime_span_t span=runtime_measure_begin(RUNTIME_DECODER,time_us_32());
        track_service();runtime_measure_end(RUNTIME_DECODER,span,time_us_32());mtk_bridge_delay_ms(1);
    }
}
static void observer_task(int start_code, void *context)
{
    (void)start_code; (void)context;
    puts("RTOS track integration v" RTOS_FIRMWARE_VERSION ": real motion/line/barcode producers");
#ifdef RTOS_OBSTACLE
    puts("Course run includes obstacle avoidance (sonar GP0/GP1, servo GP12). o is an alias for t.");
#else
    puts("Terrain and obstacle unavailable.");
#endif
    puts("Default t=track AAZ scan. Optional f=30cm,l=L90,r=R90,u=R180. GP20 press/release starts; next press stops. x=stop. No remote start.");
    uint32_t last_print = 0;
    uint32_t last_info = 0;
    uint32_t last_stop = 0;
    uint16_t last_reads=0;
#ifdef RTOS_TERRAIN
    /* IMU (GY-511, GP4/GP5): the same terrain.c as the polling firmware. Its state is not thread
     * safe, so only this task touches it. Level calibration blocks for about 2 s and needs the car
     * level and still: it runs once here at boot and again on 'g' while the car is not armed. */
    puts("[TERRAIN] Hump, tilt and motion state from the GY-511. g=level calibration while stopped.");
    terrain_calibrate();
    uint32_t humps_seen = 0u;
    uint32_t terrain_settle_until = 0u;
    uint32_t last_terrain_send = 0u;
    bool terrain_was_armed = false;
    uint32_t pickup_since_ms = 0u;
    uint32_t next_calibration_ms = 0u;
#endif
    for (;;) {
        runtime_span_t observer_span=runtime_measure_begin(RUNTIME_OBSERVER,time_us_32());
        int command = getchar_timeout_us(0);
        uint8_t selected = command == 't' ? MOTION_COURSE : command == 'f' ? MOTION_STRAIGHT_30CM :
            command == 'l' ? MOTION_LEFT_90 : command == 'r' ? MOTION_RIGHT_90 :
            command == 'u' ? MOTION_RIGHT_180 : MOTION_NONE;
#ifdef RTOS_OBSTACLE
        if(command=='o')selected=BOARD_OBSTACLE_SELECT;
#endif
#ifdef RTOS_TERRAIN
        /* j = 50 cm straight move at the 600 counts/s cruise speed, the hump height test (same as the
         * dashboard firmware's j). Any other selection clears it. */
        if (command == 'j') selected = MOTION_STRAIGHT_30CM;
#endif
        if (command == 'x') board_stop(STOP_BUTTON);
        if (selected != MOTION_NONE && board_select(selected)) {
#ifdef RTOS_TERRAIN
            runtime.long_straight = (command == 'j');
#endif
            printf("SELECT=%u; physical GP20 required\n", selected);
#ifdef RTOS_TERRAIN
            if (command == 'j') puts("SELECT j: 50 cm slow move (600 counts/s) for the hump test.");
#endif
        }
        uint32_t now = to_ms_since_boot(get_absolute_time());
        board_stop_trace_t trace; board_stop_trace(&trace);
        if (trace.sequence!=last_stop) {
            printf("[STOP_TRACE] seq=%" PRIu32 " reason=%u request_us=%" PRIu32
                   " off_us=%" PRIu32 " alarm_gap_us=%" PRIu32 "\n",
                   trace.sequence,trace.reason,trace.requested_us,trace.disabled_us,trace.alarm_gap_us);
            last_stop=trace.sequence;
        }
        if (command=='?' || (uint32_t)(now-last_info)>=1000u) {
            runtime_report_poll();
            printf("[RTOS_INFO] version=" RTOS_FIRMWARE_VERSION " network_state=%u network_error=%u wifi_link=%d wifi_pm_result=%d uptime_ms=%" PRIu32 "\n",network_state(),network_error(),network_wifi_link(),network_wifi_pm_result(),now);
            last_info=now;
            track_diagnostics_t d;track_diagnostics(&d);
            track_ir_t ir;track_sensor_read(&ir);
            uint32_t irq=save_and_disable_interrupts();
            const char *status=runtime.track.status;uint16_t reads=runtime.track.reads;
            char letter=runtime.track.letter;unsigned recoveries=runtime.track.course.recoveries;
            restore_interrupts(irq);
            printf("[TRACK_INFO] status=%s raw=%u/%u/%u age_us=%" PRIu32 " samples=%" PRIu32
                " frames=%" PRIu32 " rejected=%" PRIu32 " reads=%u letter=%c queue_peak=%u"
                " faults=%u decode_max_us=%" PRIu32 " adc_max_us=%" PRIu32 " recoveries=%u\n",
                status?status:"IDLE",ir.raw[0],ir.raw[1],ir.raw[2],ir.age_us,d.samples,d.frames,d.rejected,
                reads,letter?letter:'-',d.queue_peak,d.faults,d.decode_max_us,d.adc_max_us,recoveries);
            if(reads && reads!=last_reads)printf("[BARCODE_READ] letter=%c count=%u\n",letter,reads);
            last_reads=reads;
            for(unsigned slot=0;slot<=RUNTIME_TASKS;slot++){
                uint8_t bytes[RUNTIME_PACKET_BYTES];size_t length=runtime_report_packet(slot,now,bytes);
                if(length){
                    printf("[RUNTIME_V1] ");for(size_t i=0;i<length;i++)printf("%02x",bytes[i]);puts("");
                }
            }
        }
        if ((uint32_t)(now - last_print) >= 200u) {
            uint8_t bytes[MOTION_RECORD_BYTES];
            board_read(bytes);
            printf("[MOTION_V1] ");
            for (unsigned i = 0; i < sizeof bytes; ++i) printf("%02x", bytes[i]);
            printf(" gap_us=%" PRIu32 "\n", runtime.max_gap_us);
            last_print = now;
        }
#ifdef RTOS_TERRAIN
        {
            bool armed; uint8_t gate_reason;
            board_gate_snapshot(&armed,&gate_reason);
            if (command == 'g') {
                if (armed) puts("[TERRAIN] Level calibration only while the car is stopped.");
                else terrain_calibrate();
            }
            /* The boot calibration fails if the car is moved or tilted in its first seconds, and without
             * USB nothing could ask again: hump detection stayed off for the whole session. Retry every
             * few seconds while the car is stopped and not calibrated; each attempt keeps the Observer
             * busy for about 2 s and is rejected again if the car is still being moved. */
            if (!armed && terrain_settle_until == 0u && !terrain_get_status().calibrated &&
                (int32_t)(now - next_calibration_ms) >= 0) {
                (void)terrain_calibrate();
                next_calibration_ms = to_ms_since_boot(get_absolute_time()) + CALIBRATION_RETRY_MS;
            }
            /* A run starts when the gate arms (GP20) and ends when it disarms. The detector keeps
             * polling for 500 ms after the stop so that a hump still open at the stop is reported. */
            if (armed && !terrain_was_armed) {
                terrain_run_start();
                terrain_motion_go();
                humps_seen = 0u;
                terrain_settle_until = 0u;
            } else if (!armed && terrain_was_armed) {
                terrain_settle_until = now + 500u;
            }
            terrain_was_armed = armed;
            if (armed || (terrain_settle_until != 0u && (int32_t)(now - terrain_settle_until) < 0)) {
                terrain_poll();
            } else if (terrain_settle_until != 0u) {
                terrain_run_end();
                terrain_settle_until = 0u;
            }
            /* Speed, turn rate and motion state come from the same encoders Motion uses. The left
             * raw count falls as the car drives forward; the classifier wants both forward positive. */
            encoder_sample_t wheels = encoders_snapshot();
            terrain_motion_update((int32_t)(0u - wheels.left_raw), (int32_t)wheels.right_raw, now);
            const terrain_status_t st = terrain_get_status();
            /* Demo rule: the run ends when the car is picked up, and the motors must stop. Off the ground
             * is read from the three IR sensors: with nothing under the car all three saturate (about
             * 4000 of 4095 on the car), while floor reads about 170 and the centre sensor on tape about
             * 2600, so all three above PICKUP_IR_RAW cannot happen on the track. It must last
             * PICKUP_HOLD_MS. A hump can also tip the sensors off the floor, so during a hump phase the
             * tilt alert (above 15 degrees for 400 ms) must agree before the run is stopped. It uses the
             * existing STOP_MOTION reason so the motion record and runtime decoders are unchanged. */
            track_ir_t ir;
            track_sensor_read(&ir);
            const bool off_ground = ir.age_us < 100000u && ir.raw[0] > PICKUP_IR_RAW &&
                                    ir.raw[1] > PICKUP_IR_RAW && ir.raw[2] > PICKUP_IR_RAW;
            if (!armed || !off_ground) {
                pickup_since_ms = 0u;
            } else {
                if (pickup_since_ms == 0u) pickup_since_ms = now ? now : 1u;
                const bool hump_phase = st.motion_state == 5u || st.motion_state == 6u;
                if ((uint32_t)(now - pickup_since_ms) >= PICKUP_HOLD_MS && (!hump_phase || st.tilt_alert_active)) {
                    board_stop(STOP_MOTION);
                    printf("[PICKUP] off the ground (IR %u/%u/%u) tilt_alert=%d: motors stopped, run latched.\n",
                           ir.raw[0], ir.raw[1], ir.raw[2], (int)st.tilt_alert_active);
                    pickup_since_ms = 0u;
                }
            }
            const terrain_source_t source = {
                .available = st.available, .calibrated = st.calibrated, .sample_valid = st.pitch_valid,
                .roll_change_deg = st.current_roll_change_deg, .humps = st.roll_hump_count,
                .last_height_mm = st.last_roll_hump_mm, .highest_height_mm = st.highest_roll_hump_mm,
                .motion_state = st.motion_state, .tilt_active = st.tilt_alert_active,
                .tilt_alerts = st.tilt_alert_count };
            terrain_reading_t reading;
            if (terrain_reading_from(now, &source, &reading)) {
                if ((uint32_t)(now - last_terrain_send) >= 100u) {
                    (void)network_terrain(&reading);
                    last_terrain_send = now;
                }
                /* '>' not '!=': the count restarts at 0 at every run start and calibration. */
                if (st.roll_hump_count > humps_seen)
                    (void)network_hump_event(now, (uint8_t)reading.humps, reading.last_height_dmm);
            }
            humps_seen = st.roll_hump_count;
        }
#endif
        runtime_measure_end(RUNTIME_OBSERVER,observer_span,time_us_32());mtk_bridge_delay_ms(10);
    }
}
int usermain(void)
{
    if (!rtos_observed_start(RUNTIME_MOTION,motion_task, 2, 4096) ||
        !rtos_observed_start(RUNTIME_DECODER,decoder_task,4,8192) ||
        !rtos_observed_start(RUNTIME_NETWORK,network_task, 8, 4096) ||
        !rtos_observed_start(RUNTIME_OBSERVER,observer_task, 9, 4096)) {
        board_stop(STOP_INIT);
        puts("[RTOS_BOOT] error=task_create");
        stdio_flush();
        /* Do not leave an already created Motion task able to arm. */
        for (;;) { board_stop(STOP_INIT); tight_loop_contents(); }
    }
    mtk_bridge_sleep_forever();
    return 0;
}
int main(void)
{
    runtime_measure_init();
    bool initialized = board_init(); /* outputs zero before console/kernel */
    stdio_init_all();
    /* Default USB bench mode waits for its logger before starting the kernel,
     * preserving the boot breadcrumb if RTOS startup itself fails. A locally
     * configured radio build may boot without USB. Outputs remain zero. */
    if (RTOS_WAIT_FOR_USB)
        while (!stdio_usb_connected()) sleep_ms(10);
    if (!initialized || clock_get_hz(clk_sys) != 125000000u) {
        puts("[RTOS_BOOT] error=timer_or_clock");
        board_stop(STOP_INIT);
        for (;;) tight_loop_contents();
    }
    encoders_init();
#ifdef RTOS_OBSTACLE
    if(!obstacle_io_init()){
        board_stop(STOP_INIT);puts("[RTOS_BOOT] error=obstacle_io");
        for(;;)tight_loop_contents();
    }
#endif
    if(!track_sensor_init()){
        board_stop(STOP_INIT);puts("[RTOS_BOOT] error=adc_timer");
        for(;;)tight_loop_contents();
    }
    /* Autonomous power-bank boot selects the tested scan route, never starts
     * it. Motion still requires a fresh debounced physical GP20 gesture. */
#ifdef RTOS_HUMP_TEST_BOOT
    /* Test-only: boot with the 50 cm slow hump move selected so a USB-free run needs only GP20. */
    (void)board_select(MOTION_STRAIGHT_30CM);
#else
    (void)board_select(MOTION_COURSE);
#endif
    knl_lowmem_top = kernel_pool;
    knl_lowmem_limit = kernel_pool + sizeof kernel_pool;
    (void)exception_set_exclusive_handler(PENDSV_EXCEPTION, knl_dispatch_entry);
    (void)exception_set_exclusive_handler(SYSTICK_EXCEPTION, knl_systim_inthdr);
    (void)exception_set_priority(PENDSV_EXCEPTION, 0xc0u);
    (void)exception_set_priority(SYSTICK_EXCEPTION, 0x00u);
    puts("[RTOS_BOOT] version=" RTOS_FIRMWARE_VERSION " stage=kernel_start");
    stdio_flush();
    (void)mtk_kernel_main();
    board_stop(STOP_INIT);
    for (;;) tight_loop_contents();
}
