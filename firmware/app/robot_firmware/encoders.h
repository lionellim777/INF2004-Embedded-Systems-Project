#ifndef INF2004_ENCODERS_H
#define INF2004_ENCODERS_H

#include <stdint.h>

/* Left motor encoder A/B: Grove 2, GP2/GP3.
 * Right motor encoder A/B: Grove 4, GP16/GP17.
 * Returns raw quadrature transitions; sign depends on connector orientation.
 */
void encoders_init(void);
int32_t encoder_left_count(void);
int32_t encoder_right_count(void);

#endif
