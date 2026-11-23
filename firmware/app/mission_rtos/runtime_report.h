#ifndef RTOS_RUNTIME_REPORT_H
#define RTOS_RUNTIME_REPORT_H
#include "runtime_measure.h"
void runtime_report_poll(void);
size_t runtime_report_packet(unsigned slot,uint32_t ms,uint8_t out[RUNTIME_PACKET_BYTES]);
#endif
