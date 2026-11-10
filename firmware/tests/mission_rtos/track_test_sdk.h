#ifndef TRACK_TEST_SDK_H
#define TRACK_TEST_SDK_H
#include "test_sdk.h"
uint64_t time_us_64(void);
void adc_init(void);
void adc_gpio_init(uint pin);
void adc_select_input(uint input);
uint16_t adc_read(void);
void busy_wait_us_32(uint32_t us);
#endif
