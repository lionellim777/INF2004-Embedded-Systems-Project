#include "app_main.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "comm.h"
#include "comm_secrets.h"
#include "mtk_bridge.h"
#include "pico/stdlib.h"

#define APP_TASK_STACK_SIZE 4096U
#define APP_DEMO_PERIOD_MS 500U

static void app_demo_task(int start_code, void *p_context);
static void app_publish_simulated(uint32_t now_ms);

/* The RTOS requires this exported name; see docs/barr-c-review.md. */
int
usermain(void)
{
    int result = -1;
    comm_config_t const config = COMM_CONFIG_INITIALIZER;

    if (COMM_OK == comm_init(&config))
    {
        if ((true == mtk_bridge_start_task(comm_task, 9,
                                           APP_TASK_STACK_SIZE)) &&
            (true == mtk_bridge_start_task(app_demo_task, 12,
                                           APP_TASK_STACK_SIZE)))
        {
            printf("Communication and demo tasks started\n");
            mtk_bridge_sleep_forever();
            result = 0;
        }
    }
    else
    {
        printf("Communication configuration failed\n");
    }

    return (result);
}

static void
app_demo_task(int start_code, void *p_context)
{
    uint32_t now_ms = 0U;
    uint32_t demo_count = 0U;
    comm_status_t status = {0};

    (void)start_code;
    (void)p_context;

    for (;;)
    {
        now_ms = to_ms_since_boot(get_absolute_time());
        app_publish_simulated(now_ms);
        demo_count++;
        if ((0U == (demo_count % 20U)) &&
            (COMM_OK == comm_get_status(&status)))
        {
            printf("demo=%lu comm=%lu state=%u retries=%lu error=%lu\n",
                   (unsigned long)demo_count,
                   (unsigned long)status.task_ticks,
                   (unsigned int)status.state,
                   (unsigned long)status.reconnect_count,
                   (unsigned long)status.last_error);
        }
        mtk_bridge_delay_ms(APP_DEMO_PERIOD_MS);
    }
}

static void
app_publish_simulated(uint32_t now_ms)
{
    telemetry_sample_t sample = {0};

    sample.source = TELEMETRY_SOURCE_MISSION;
    sample.data.mission.b_is_valid = true;
    sample.data.mission.b_is_simulated = true;
    sample.data.mission.captured_ms = now_ms;
    sample.data.mission.state = TELEMETRY_MISSION_IDLE;
    (void)comm_update_telemetry(&sample);

    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_MOTION;
    sample.data.motion.b_is_valid = true;
    sample.data.motion.b_is_simulated = true;
    sample.data.motion.captured_ms = now_ms;
    (void)comm_update_telemetry(&sample);

    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_LINE;
    sample.data.line.b_is_valid = true;
    sample.data.line.b_is_simulated = true;
    sample.data.line.captured_ms = now_ms;
    sample.data.line.b_is_on_line = false;
    sample.data.line.barcode = '?';
    (void)comm_update_telemetry(&sample);

    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_TERRAIN;
    sample.data.terrain.b_is_valid = true;
    sample.data.terrain.b_is_simulated = true;
    sample.data.terrain.captured_ms = now_ms;
    sample.data.terrain.motion = TELEMETRY_MOTION_STATIONARY;
    (void)comm_update_telemetry(&sample);

    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_OBSTACLE;
    sample.data.obstacle.b_is_valid = false;
    (void)comm_update_telemetry(&sample);
}
