#include "board.h"
#include "encoders.h"
#include "motion_runtime.h"
#include "network_service.h"
#include "version.h"
#include "port/mtk_bridge.h"
#include <inttypes.h>
#include <stdio.h>
#include "hardware/clocks.h"
#include "hardware/exception.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "network_settings.h"
#ifndef RTOS_WAIT_FOR_USB
#define RTOS_WAIT_FOR_USB (!RTOS_NETWORK_CONFIGURED)
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
static void motion_task(int start_code, void *context)
{
    (void)start_code; (void)context;
    motion_runtime_init(&runtime, encoders_snapshot(), time_us_32(), board_button_down());
    for (;;) {
        motion_runtime_step(&runtime, encoders_snapshot(), time_us_64(), board_button_down());
        mtk_bridge_delay_ms(10);
    }
}
static void observer_task(int start_code, void *context)
{
    (void)start_code; (void)context;
    puts("RTOS motion foundation v" RTOS_FIRMWARE_VERSION ": encoders/motion REAL; barcode/terrain/obstacle UNAVAILABLE");
    puts("Select f=30cm,l=L90,r=R90,u=R180. GP20 press/release starts; next press stops. x=stop. No remote start.");
    uint32_t last_print = 0;
    uint32_t last_info = 0;
    uint32_t last_stop = 0;
    for (;;) {
        int command = getchar_timeout_us(0);
        uint8_t selected = command == 'f' ? MOTION_STRAIGHT_30CM :
            command == 'l' ? MOTION_LEFT_90 : command == 'r' ? MOTION_RIGHT_90 :
            command == 'u' ? MOTION_RIGHT_180 : MOTION_NONE;
        if (command == 'x') board_stop(STOP_BUTTON);
        if (selected != MOTION_NONE && board_select(selected))
            printf("SELECT=%u; physical GP20 required\n", selected);
        uint32_t now = to_ms_since_boot(get_absolute_time());
        board_stop_trace_t trace; board_stop_trace(&trace);
        if (trace.sequence!=last_stop) {
            printf("[STOP_TRACE] seq=%" PRIu32 " reason=%u request_us=%" PRIu32
                   " off_us=%" PRIu32 " alarm_gap_us=%" PRIu32 "\n",
                   trace.sequence,trace.reason,trace.requested_us,trace.disabled_us,trace.alarm_gap_us);
            last_stop=trace.sequence;
        }
        if (command=='?' || (uint32_t)(now-last_info)>=1000u) {
            printf("[RTOS_INFO] version=" RTOS_FIRMWARE_VERSION " network_state=%u network_error=%u uptime_ms=%" PRIu32 "\n",network_state(),network_error(),now);
            last_info=now;
        }
        if ((uint32_t)(now - last_print) >= 200u) {
            uint8_t bytes[MOTION_RECORD_BYTES];
            board_read(bytes);
            printf("[MOTION_V1] ");
            for (unsigned i = 0; i < sizeof bytes; ++i) printf("%02x", bytes[i]);
            printf(" gap_us=%" PRIu32 "\n", runtime.max_gap_us);
            last_print = now;
        }
        mtk_bridge_delay_ms(10);
    }
}
int usermain(void)
{
    if (!mtk_bridge_start_task(motion_task, 2, 4096) ||
        !mtk_bridge_start_task(network_task, 8, 4096) ||
        !mtk_bridge_start_task(observer_task, 9, 4096)) {
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
