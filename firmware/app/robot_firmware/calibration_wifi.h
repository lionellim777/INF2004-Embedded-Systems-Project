#ifndef CALIBRATION_WIFI_H
#define CALIBRATION_WIFI_H
#include <stdbool.h>
#include <stdint.h>
#include "course_config.h"
#ifdef ROBOCAR_WIRELESS_CALIBRATION
void calibration_wifi_capture_init(void);
bool calibration_wifi_init(void);
void calibration_wifi_poll(bool selection_allowed);
int calibration_wifi_getchar(void);
course_config_t calibration_wifi_course_config(void);
uint32_t calibration_wifi_max_poll_us(void);
void calibration_wifi_help(void);
void calibration_wifi_reset_timing(void);
/* Samples/details stay immutable until the owner unpublishes before reuse.
 * Call outside IRQ exclusion. Export is read-only and never starts motion. */
void calibration_wifi_set_waveform(const uint16_t *samples,unsigned count,
    const char *details,uint32_t id,uint32_t checksum);
#else
static inline void calibration_wifi_capture_init(void) {}
static inline bool calibration_wifi_init(void) { return false; }
static inline void calibration_wifi_poll(bool allowed) { (void)allowed; }
static inline course_config_t calibration_wifi_course_config(void) { return (course_config_t){'s','s',120,{0}}; }
static inline int calibration_wifi_getchar(void) { return -1; }
static inline uint32_t calibration_wifi_max_poll_us(void) { return 0u; }
static inline void calibration_wifi_help(void) {}
static inline void calibration_wifi_reset_timing(void) {}
static inline void calibration_wifi_set_waveform(const uint16_t *samples,unsigned count,
    const char *details,uint32_t id,uint32_t checksum)
{(void)samples;(void)count;(void)details;(void)id;(void)checksum;}
#endif
#endif
