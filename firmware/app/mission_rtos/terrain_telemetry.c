#include "terrain_telemetry.h"

/* Rounds value to tenths. False for NaN or when the rounded result would fall
 * outside lo..hi tenths; checked in float so the cast is always defined. */
static bool tenths(float value, int32_t lo, int32_t hi, int32_t *out)
{
    float scaled = value * 10.0f;
    if (!(scaled > (float)lo - 0.5f && scaled < (float)hi + 0.5f))
    {
        return false;
    }
    *out = (int32_t)(scaled < 0.0f ? scaled - 0.5f : scaled + 0.5f);
    return true;
}
bool terrain_reading_from(uint32_t ms, const terrain_source_t *s,
                          terrain_reading_t *out)
{
    int32_t roll;
    int32_t last;
    int32_t highest;
    /* A tilt past 25.5 degrees saturates the roll field instead of dropping the
     * whole reading: the tilt alert flag in the same record carries the real
     * condition. NaN is still rejected. */
    float roll_deg = s->roll_change_deg;
    if (roll_deg > 25.5f)
    {
        roll_deg = 25.5f;
    }
    else if (roll_deg < -25.5f)
    {
        roll_deg = -25.5f;
    }
    if (s->humps > 255 || !tenths(roll_deg, -255, 255, &roll) ||
        !tenths(s->last_height_mm, 0, 511, &last) ||
        !tenths(s->highest_height_mm, 0, 511, &highest) || s->motion_state > 6u)
    {
        return false;
    }
    *out = (terrain_reading_t){
        .ms = ms,
        .valid = s->available && s->calibrated && s->sample_valid,
        .calibrated = s->calibrated,
        .roll_decideg = (int16_t)roll,
        .humps = (uint16_t)s->humps,
        .last_height_dmm = (uint16_t)last,
        .highest_height_dmm = (uint16_t)highest,
        .motion_state = s->motion_state,
        .tilt_active = s->tilt_active,
        .tilt_alerts = (uint8_t)(s->tilt_alerts > 15u ? 15u : s->tilt_alerts)};
    return true;
}
bool hump_queue_push(hump_queue_t *q, uint32_t ms, uint8_t index,
                     uint16_t height_dmm)
{
    if (height_dmm > 511)
    {
        return false;
    }
    if (q->count == HUMP_QUEUE_CAPACITY)
    {
        hump_queue_pop(q);
        if (q->dropped < 255u)
        {
            q->dropped++;
        }
    }
    unsigned slot = (q->head + q->count) % HUMP_QUEUE_CAPACITY;
    q->ms[slot] = ms;
    q->index[slot] = index;
    q->height_dmm[slot] = height_dmm;
    q->count++;
    return true;
}
bool hump_queue_peek(const hump_queue_t *q, uint8_t out[HUMP_EVENT_BYTES])
{
    if (q->count == 0)
    {
        return false;
    }
    return hump_event_record_encode(q->ms[q->head], q->index[q->head],
                                    q->height_dmm[q->head], q->dropped, out);
}
void hump_queue_pop(hump_queue_t *q)
{
    if (q->count == 0)
    {
        return;
    }
    q->head = (uint8_t)((q->head + 1u) % HUMP_QUEUE_CAPACITY);
    q->count--;
}
