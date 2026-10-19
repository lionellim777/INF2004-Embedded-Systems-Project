#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "command.h"
#include "telemetry.h"

static void test_wheel_record(void);
static void test_wheel_history(void);
static void test_motion_ingestion(void);

int
main(void)
{
    command_request_t request = {0};
    command_frame_t frame = {0};
    telemetry_snapshot_t snapshot = {0};
    telemetry_sample_t sample = {0};
    telemetry_event_t event = {0};
    char output[TELEMETRY_JSON_SIZE] = {0};

    test_wheel_record();
    test_wheel_history();
    test_motion_ingestion();

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

static void
test_wheel_record(void)
{
    uint32_t record = 0U;
    uint32_t captured_ms = 0U;
    uint32_t times[] = {0U, 123U, TELEMETRY_WHEEL_CAPTURE_MAX_MS};
    int32_t speed_mm_s = 0;
    int32_t speed = 0;
    size_t index = 0U;

    assert(4U == sizeof(record));
    for (speed = -500; speed <= 500; speed++)
    {
        for (index = 0U; index < (sizeof(times) / sizeof(times[0])); index++)
        {
            assert(true == telemetry_wheel_record_pack(speed, times[index],
                                                        &record));
            assert(true == telemetry_wheel_record_unpack(record, &speed_mm_s,
                                                          &captured_ms));
            assert(speed == speed_mm_s);
            assert(times[index] == captured_ms);
        }
    }
    assert(true == telemetry_wheel_record_pack(-500, 123U, &record));
    assert(126476U == record);
    assert(true == telemetry_wheel_record_pack(-1, 0U, &record));
    assert(1023U == record);

    record = 99U;
    assert(false == telemetry_wheel_record_pack(-501, 0U, &record));
    assert(false == telemetry_wheel_record_pack(501, 0U, &record));
    assert(false == telemetry_wheel_record_pack(INT32_MIN, 0U, &record));
    assert(false == telemetry_wheel_record_pack(INT32_MAX, 0U, &record));
    assert(false == telemetry_wheel_record_pack(0, UINT32_MAX, &record));
    assert(false == telemetry_wheel_record_pack(
                         0, TELEMETRY_WHEEL_CAPTURE_MAX_MS + 1U, &record));
    assert(99U == record);
    assert(false == telemetry_wheel_record_pack(0, 0U, NULL));

    speed_mm_s = 99;
    captured_ms = 99U;
    assert(false == telemetry_wheel_record_unpack(511U, &speed_mm_s,
                                                  &captured_ms));
    assert(false == telemetry_wheel_record_unpack(512U, &speed_mm_s,
                                                  &captured_ms));
    assert(false == telemetry_wheel_record_unpack(0x80000000U, &speed_mm_s,
                                                  &captured_ms));
    assert(false == telemetry_wheel_record_unpack(1800001U * 1024U,
                                                  &speed_mm_s, &captured_ms));
    assert(99 == speed_mm_s);
    assert(99U == captured_ms);
    assert(false == telemetry_wheel_record_unpack(0U, NULL, &captured_ms));
    assert(false == telemetry_wheel_record_unpack(0U, &speed_mm_s, NULL));
}

static void
test_wheel_history(void)
{
    telemetry_wheel_history_t left = {0};
    telemetry_wheel_history_t right = {0};
    telemetry_wheel_history_t before = {0};
    int32_t speed_mm_s = 0;
    uint32_t captured_ms = 0U;
    size_t index = 0U;
    size_t total = (TELEMETRY_WHEEL_HISTORY_CAPACITY * 3U) + 2U;

    assert(0U == telemetry_wheel_history_count(NULL));
    assert(0U == telemetry_wheel_history_count(&left));
    assert(false == telemetry_wheel_history_append(NULL, 0, 0U));
    assert(false == telemetry_wheel_history_get(NULL, 0U, &speed_mm_s,
                                               &captured_ms));
    assert(false == telemetry_wheel_history_get(&left, 0U, &speed_mm_s,
                                               &captured_ms));
    assert(true == telemetry_wheel_history_append(&left, 0, 0U));
    assert(1U == telemetry_wheel_history_count(&left));
    assert(true == telemetry_wheel_history_get(&left, 0U, &speed_mm_s,
                                              &captured_ms));
    assert(0 == speed_mm_s);
    assert(0U == captured_ms);

    memset(&left, 0, sizeof(left));
    assert(true == telemetry_wheel_history_append(&left, 100, 1000U));
    assert(true == telemetry_wheel_history_append(&right, 90, 1000U));
    assert(true == telemetry_wheel_history_append(&left, 100, 1200U));
    assert(true == telemetry_wheel_history_append(&right, 80, 1200U));
    assert(1U == telemetry_wheel_history_count(&left));
    assert(2U == telemetry_wheel_history_count(&right));
    assert(true == telemetry_wheel_history_get(&left, 0U, &speed_mm_s,
                                              &captured_ms));
    assert(100 == speed_mm_s);
    assert(1000U == captured_ms);
    assert(true == telemetry_wheel_history_append(&left, 60, 1400U));
    assert(true == telemetry_wheel_history_append(&left, 0, 1600U));
    assert(true == telemetry_wheel_history_append(&left, 0, 20000U));
    assert(3U == telemetry_wheel_history_count(&left));
    assert(true == telemetry_wheel_history_get(&left, 2U, &speed_mm_s,
                                              &captured_ms));
    assert(0 == speed_mm_s);
    assert(1600U == captured_ms);
    memcpy(&before, &left, sizeof(left));
    assert(false == telemetry_wheel_history_append(&left, 501, 22000U));
    assert(false == telemetry_wheel_history_append(
                         &left, 0, TELEMETRY_WHEEL_CAPTURE_MAX_MS + 1U));
    assert(0 == memcmp(&before, &left, sizeof(left)));

    memset(&left, 0, sizeof(left));
    for (index = 0U; index < total; index++)
    {
        assert(true == telemetry_wheel_history_append(
                           &left, (int32_t)index - 40,
                           (uint32_t)index * 200U));
    }
    assert(TELEMETRY_WHEEL_HISTORY_CAPACITY ==
           telemetry_wheel_history_count(&left));
    for (index = 0U; index < TELEMETRY_WHEEL_HISTORY_CAPACITY; index++)
    {
        assert(true == telemetry_wheel_history_get(&left, index, &speed_mm_s,
                                                   &captured_ms));
        assert((int32_t)(total - TELEMETRY_WHEEL_HISTORY_CAPACITY + index) -
               40 == speed_mm_s);
        assert((total - TELEMETRY_WHEEL_HISTORY_CAPACITY + index) * 200U ==
               captured_ms);
    }
    memcpy(&before, &left, sizeof(left));
    assert(true == telemetry_wheel_history_append(
                       &left, (int32_t)total - 41, (uint32_t)total * 200U));
    assert(0 == memcmp(&before, &left, sizeof(left)));
    assert(false == telemetry_wheel_history_get(
                         &left, TELEMETRY_WHEEL_HISTORY_CAPACITY,
                         &speed_mm_s, &captured_ms));
    assert(false == telemetry_wheel_history_get(&left, SIZE_MAX,
                                               &speed_mm_s, &captured_ms));
    assert(false == telemetry_wheel_history_get(&left, 0U, NULL,
                                               &captured_ms));
    assert(false == telemetry_wheel_history_get(&left, 0U, &speed_mm_s, NULL));
    assert(2U == telemetry_wheel_history_count(&right));
    assert(true == telemetry_wheel_history_get(&right, 1U, &speed_mm_s,
                                              &captured_ms));
    assert(80 == speed_mm_s);
    assert(1200U == captured_ms);
    printf("HISTORY:record=%zu, samples_per_wheel=%zu, "
           "history_per_wheel=%zu, both_histories=%zu\n", sizeof(uint32_t),
           sizeof(left.records), sizeof(left), sizeof(left) + sizeof(right));
}

static void
test_motion_ingestion(void)
{
    telemetry_motion_t current = {0};
    telemetry_motion_t sample = {0};
    telemetry_motion_t before_current = {0};
    telemetry_wheel_history_t left = {0};
    telemetry_wheel_history_t right = {0};
    telemetry_wheel_history_t before_left = {0};
    telemetry_wheel_history_t before_right = {0};
    int32_t speed_mm_s = 0;
    uint32_t captured_ms = 0U;
    size_t index = 0U;

    sample.b_is_valid = true;
    sample.b_is_simulated = true;
    sample.left_mm_s = 100;
    sample.right_mm_s = 90;
    sample.left_ticks = 12;
    sample.distance_mm = 20U;
    sample.captured_ms = 1000U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(true == current.b_is_valid);
    assert(true == current.b_is_simulated);
    assert(1U == telemetry_wheel_history_count(&left));
    assert(1U == telemetry_wheel_history_count(&right));

    sample.b_is_valid = false;
    sample.left_mm_s = 0;
    sample.right_mm_s = 0;
    sample.captured_ms = 1200U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(false == current.b_is_valid);
    assert(1200U == current.captured_ms);
    assert(1U == telemetry_wheel_history_count(&left));
    assert(1U == telemetry_wheel_history_count(&right));
    assert(true == telemetry_wheel_history_get(&left, 0U, &speed_mm_s,
                                              &captured_ms));
    assert(100 == speed_mm_s);
    assert(1000U == captured_ms);

    sample.b_is_valid = true;
    sample.left_mm_s = 100;
    sample.right_mm_s = 90;
    sample.captured_ms = 1400U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(true == current.b_is_valid);
    assert(2U == telemetry_wheel_history_count(&left));
    assert(2U == telemetry_wheel_history_count(&right));
    assert(true == telemetry_wheel_history_get(&left, 1U, &speed_mm_s,
                                              &captured_ms));
    assert(100 == speed_mm_s);
    assert(1400U == captured_ms);
    assert(true == telemetry_wheel_history_get(&right, 1U, &speed_mm_s,
                                              &captured_ms));
    assert(90 == speed_mm_s);
    assert(1400U == captured_ms);

    sample.captured_ms = 1600U;
    sample.left_ticks = 30;
    sample.distance_mm = 40U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(1600U == current.captured_ms);
    assert(30 == current.left_ticks);
    assert(40U == current.distance_mm);
    assert(2U == telemetry_wheel_history_count(&left));
    assert(2U == telemetry_wheel_history_count(&right));
    sample.captured_ms = 1800U;
    sample.right_mm_s = 80;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(2U == telemetry_wheel_history_count(&left));
    assert(3U == telemetry_wheel_history_count(&right));

    memcpy(&before_current, &current, sizeof(current));
    memcpy(&before_left, &left, sizeof(left));
    memcpy(&before_right, &right, sizeof(right));
    sample.left_mm_s = 60;
    sample.right_mm_s = 501;
    sample.captured_ms = 2000U;
    assert(false == telemetry_apply_motion_sample(&current, &left, &right,
                                                   &sample));
    sample.left_mm_s = -501;
    sample.right_mm_s = 60;
    assert(false == telemetry_apply_motion_sample(&current, &left, &right,
                                                   &sample));
    sample.left_mm_s = 60;
    sample.captured_ms = TELEMETRY_WHEEL_CAPTURE_MAX_MS + 1U;
    assert(false == telemetry_apply_motion_sample(&current, &left, &right,
                                                   &sample));
    assert(0 == memcmp(&before_current, &current, sizeof(current)));
    assert(0 == memcmp(&before_left, &left, sizeof(left)));
    assert(0 == memcmp(&before_right, &right, sizeof(right)));

    sample.b_is_valid = false;
    sample.left_mm_s = INT32_MIN;
    sample.right_mm_s = INT32_MAX;
    sample.captured_ms = 2200U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    sample.captured_ms = 2400U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(false == current.b_is_valid);
    assert(2400U == current.captured_ms);
    assert(0 == memcmp(before_left.records, left.records,
                       sizeof(left.records)));
    assert(0 == memcmp(before_right.records, right.records,
                       sizeof(right.records)));
    sample.b_is_valid = true;
    sample.left_mm_s = 100;
    sample.right_mm_s = 80;
    sample.captured_ms = 2600U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(3U == telemetry_wheel_history_count(&left));
    assert(4U == telemetry_wheel_history_count(&right));

    assert(false == telemetry_apply_motion_sample(NULL, &left, &right,
                                                   &sample));
    assert(false == telemetry_apply_motion_sample(&current, NULL, &right,
                                                   &sample));
    assert(false == telemetry_apply_motion_sample(&current, &left, NULL,
                                                   &sample));
    assert(false == telemetry_apply_motion_sample(&current, &left, &right,
                                                   NULL));
    assert(false == telemetry_apply_motion_sample(&current, &left, &left,
                                                   &sample));

    for (index = 0U; index < TELEMETRY_WHEEL_HISTORY_CAPACITY; index++)
    {
        sample.left_mm_s = (int32_t)index;
        sample.right_mm_s = -(int32_t)index;
        sample.captured_ms = 3000U + ((uint32_t)index * 200U);
        assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                      &sample));
    }
    sample.b_is_valid = false;
    sample.captured_ms += 200U;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    sample.b_is_valid = true;
    sample.captured_ms = TELEMETRY_WHEEL_CAPTURE_MAX_MS;
    assert(true == telemetry_apply_motion_sample(&current, &left, &right,
                                                  &sample));
    assert(TELEMETRY_WHEEL_HISTORY_CAPACITY ==
           telemetry_wheel_history_count(&left));
    assert(TELEMETRY_WHEEL_HISTORY_CAPACITY ==
           telemetry_wheel_history_count(&right));
    assert(true == telemetry_wheel_history_get(&left,
                       TELEMETRY_WHEEL_HISTORY_CAPACITY - 1U,
                       &speed_mm_s, &captured_ms));
    assert((int32_t)TELEMETRY_WHEEL_HISTORY_CAPACITY - 1 == speed_mm_s);
    assert(TELEMETRY_WHEEL_CAPTURE_MAX_MS == captured_ms);
}
