#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "telemetry.h"

int
main(void)
{
    command_request_t request = {0};
    telemetry_snapshot_t snapshot = {0};
    telemetry_sample_t sample = {0};
    telemetry_event_t event = {0};
    char output[TELEMETRY_JSON_SIZE] = {0};

    assert(true == command_parse("get_status", "", 0U, &request));
    assert(COMMAND_GET_STATUS == request.kind);
    assert(false == command_parse("get_status", "x", 1U, &request));
    assert(true == command_parse("set_period_ms", "200", 3U,
                                 &request));
    assert(200U == request.period_ms);
    assert(true == command_parse("set_period_ms", "5000", 4U,
                                 &request));
    assert(5000U == request.period_ms);
    assert(false == command_parse("set_period_ms", "199", 3U,
                                  &request));
    assert(false == command_parse("set_period_ms", "5001", 4U,
                                  &request));
    assert(false == command_parse("set_period_ms", "20x", 3U,
                                  &request));
    assert(false == command_parse("set_period_ms", "200000", 6U,
                                  &request));
    assert(false == command_parse("move", "left", 4U, &request));

    sample.source = TELEMETRY_SOURCE_MOTION;
    sample.data.motion.b_is_valid = true;
    sample.data.motion.captured_ms = 42U;
    sample.data.motion.left_ticks = INT32_MIN;
    sample.data.motion.right_ticks = INT32_MAX;
    sample.data.motion.distance_mm = UINT32_MAX;
    assert(true == telemetry_apply_sample(&snapshot, &sample));
    assert(true == telemetry_format_snapshot(
                        &snapshot, UINT32_MAX, 1U, 43U,
                        output, sizeof(output)));
    assert(NULL != strstr(output, "\"mission\":null"));
    assert(NULL != strstr(output, "\"obstacle\":null"));
    assert(NULL != strstr(output, "\"left_ticks\":-2147483648"));
    assert(NULL != strstr(output, "\"right_ticks\":2147483647"));
    assert(NULL != strstr(output, "\"distance_mm\":4294967295"));
    assert(false == telemetry_format_snapshot(
                         &snapshot, 0U, 0U, 0U, output, 8U));
    assert('\0' == output[0]);

    event.kind = TELEMETRY_EVENT_SCAN_POINT;
    event.captured_ms = 123U;
    event.scan_id = 4U;
    event.angle_mdeg = 90000;
    event.value = 250;
    assert(true == telemetry_format_event(&event, 10U, 12U,
                                           output, sizeof(output)));
    assert(NULL != strstr(output, "\"kind\":\"scan_point\""));
    assert(NULL != strstr(output, "\"angle_mdeg\":90000"));

    puts("protocol tests passed");
    return (0);
}
