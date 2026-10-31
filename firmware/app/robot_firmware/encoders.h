#ifndef INF2004_ENCODERS_H
#define INF2004_ENCODERS_H

#include <stdint.h>

typedef struct {
    uint32_t left_raw;
    uint32_t right_raw;
    uint32_t left_invalid;
    uint32_t right_invalid;
} encoder_sample_t;

/* Left motor encoder A/B: Grove 2, GP2/GP3.
 * Right motor encoder A/B: Grove 4, GP16/GP17.
 * Returns raw quadrature transitions; sign depends on connector orientation.
 */
void encoders_init(void);
/* One short interrupt exclusion copies both wheels; counters are never reset.
 * Raw counts wrap modulo 2^32. Consumers must use bounded relative deltas. */
encoder_sample_t encoders_snapshot(void);
int32_t encoder_left_count(void);
int32_t encoder_right_count(void);

#endif
