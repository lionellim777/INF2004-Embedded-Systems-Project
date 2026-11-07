#include "motion_record.h"
#include <string.h>
static void put(uint8_t *b, unsigned offset, unsigned count, uint32_t value)
{
    for (unsigned i = 0; i < count; ++i)
        b[(offset+i)/8u] |= (uint8_t)(((value >> i) & 1u) << ((offset+i)%8u));
}
static uint32_t get(const uint8_t *b, unsigned offset, unsigned count)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < count; ++i)
        result |= (uint32_t)((b[(offset+i)/8u] >> ((offset+i)%8u)) & 1u) << i;
    return result;
}
static int32_t signed_value(uint32_t value, unsigned bits)
{
    return (value & (UINT32_C(1) << (bits-1u))) ?
        (int32_t)((int64_t)value - (INT64_C(1) << bits)) : (int32_t)value;
}
static bool valid(const motion_record_t *r)
{
    if (r->kind > 4 || r->phase > 4 || r->fault > 5 || r->stop_reason > 6) return false;
    if (!r->valid) return r->left_counts == 0 && r->right_counts == 0 &&
        r->left_mm_s == 0 && r->right_mm_s == 0;
    return r->left_counts >= -30000 && r->left_counts <= 30000 &&
        r->right_counts >= -30000 && r->right_counts <= 30000 &&
        r->left_mm_s >= -500 && r->left_mm_s <= 500 &&
        r->right_mm_s >= -500 && r->right_mm_s <= 500;
}
bool motion_record_encode(const motion_record_t *r, uint8_t *out, size_t n)
{
    if (r == NULL || out == NULL || n != MOTION_RECORD_BYTES || !valid(r)) return false;
    uint8_t b[MOTION_RECORD_BYTES] = {0};
    put(b, 0, 32, r->captured_ms);
    put(b, 32, 16, (uint32_t)r->left_counts);
    put(b, 48, 16, (uint32_t)r->right_counts);
    put(b, 64, 10, (uint32_t)r->left_mm_s);
    put(b, 74, 10, (uint32_t)r->right_mm_s);
    put(b, 84, 3, r->kind); put(b, 87, 3, r->phase);
    put(b, 90, 3, r->fault); put(b, 93, 3, r->stop_reason);
    put(b, 96, 1, r->valid); put(b, 97, 1, r->stop_latched);
    memcpy(out, b, sizeof b);
    return true;
}
bool motion_record_decode(const uint8_t *b, size_t n, motion_record_t *out)
{
    if (b == NULL || out == NULL || n != MOTION_RECORD_BYTES || (b[12] & 0xfcu)) return false;
    motion_record_t r = {
        .captured_ms = get(b, 0, 32),
        .left_counts = signed_value(get(b, 32, 16), 16),
        .right_counts = signed_value(get(b, 48, 16), 16),
        .left_mm_s = signed_value(get(b, 64, 10), 10),
        .right_mm_s = signed_value(get(b, 74, 10), 10),
        .kind = (uint8_t)get(b, 84, 3), .phase = (uint8_t)get(b, 87, 3),
        .fault = (uint8_t)get(b, 90, 3), .stop_reason = (uint8_t)get(b, 93, 3),
        .valid = get(b, 96, 1) != 0, .stop_latched = get(b, 97, 1) != 0
    };
    if (!valid(&r)) return false;
    *out = r;
    return true;
}
