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
    command_frame_t frame = {0};
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

    assert(true == command_frame_begin(&frame, 3U));
    assert(true == command_frame_append(&frame,
                                        (uint8_t const *)"5", 1U, false));
    assert(true == command_frame_append(&frame,
                                        (uint8_t const *)"00", 2U, true));
    assert(true == command_parse("set_period_ms", frame.payload,
                                 frame.length, &request));
    assert(500U == request.period_ms);
    assert(true == command_frame_begin(&frame, 4U));
    assert(false == command_frame_append(&frame,
                                         (uint8_t const *)"200", 3U,
                                         true));
    assert(false == command_frame_begin(&frame, COMMAND_PAYLOAD_SIZE));
    assert(true == command_frame_begin(&frame, 3U));
    assert(false == command_frame_append(&frame,
                                         (uint8_t const *)"5000", 4U,
                                         true));
    assert(true == command_frame_begin(&frame, 1U));
    assert(false == command_frame_append(&frame, NULL, 1U, true));
    assert(true == command_frame_begin(&frame, 0U));
    assert(true == command_frame_append(&frame, NULL, 0U, true));
    assert(true == command_parse("get_status", frame.payload,
                                 frame.length, &request));

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
    printf("SNAPSHOT:%s\n", output);
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
    printf("EVENT:%s\n", output);

    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_MISSION;
    sample.data.mission.b_is_valid = true;
    sample.data.mission.captured_ms = UINT32_MAX;
    sample.data.mission.state = TELEMETRY_MISSION_RUNNING;
    assert(true == telemetry_apply_sample(&snapshot, &sample));
    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_LINE;
    sample.data.line.b_is_valid = true;
    sample.data.line.captured_ms = UINT32_MAX;
    sample.data.line.ir_left = UINT16_MAX;
    sample.data.line.ir_middle = UINT16_MAX;
    sample.data.line.ir_right = UINT16_MAX;
    sample.data.line.barcode = '"';
    assert(true == telemetry_apply_sample(&snapshot, &sample));
    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_TERRAIN;
    sample.data.terrain.b_is_valid = true;
    sample.data.terrain.hump_mm = INT32_MIN;
    sample.data.terrain.highest_hump_mm = INT32_MAX;
    assert(true == telemetry_apply_sample(&snapshot, &sample));
    memset(&sample, 0, sizeof(sample));
    sample.source = TELEMETRY_SOURCE_OBSTACLE;
    sample.data.obstacle.b_is_valid = true;
    sample.data.obstacle.closest_mm = UINT32_MAX;
    sample.data.obstacle.width_mm = UINT32_MAX;
    sample.data.obstacle.left_clearance_mm = UINT32_MAX;
    sample.data.obstacle.right_clearance_mm = UINT32_MAX;
    assert(true == telemetry_apply_sample(&snapshot, &sample));
    assert(true == telemetry_format_snapshot(
                        &snapshot, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                        output, sizeof(output)));
    printf("FULL_SNAPSHOT:%s\n", output);

    puts("protocol tests passed");
    return (0);
}
