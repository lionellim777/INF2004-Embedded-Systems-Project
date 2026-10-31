#ifndef CALIBRATION_CORE_H
#define CALIBRATION_CORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "course_config.h"

/* Single foreground owner. Never call from an ISR or a second core. */
#define CAL_LOG_CAPACITY 16384u
#define CAL_LINE_CAPACITY 768u
typedef struct {
    char log[CAL_LOG_CAPACITY];
    uint32_t written;
    char line[CAL_LINE_CAPACITY];
    size_t line_length;
    bool line_overflow;
    char stop[CAL_LINE_CAPACITY];
    char settled[CAL_LINE_CAPACITY];
    char summary[CAL_LINE_CAPACITY];
    char timing[CAL_LINE_CAPACITY];
    char barcode[CAL_LINE_CAPACITY];
    char trace[2][CAL_LINE_CAPACITY];
    char optical_result[256];
    char pending;
    course_config_t course;
    char course_result[CAL_LINE_CAPACITY];
    bool selection_allowed;
} calibration_core_t;

void calibration_capture(calibration_core_t *c, const char *data, size_t length);
size_t calibration_read(const calibration_core_t *c, uint32_t since,
                        char *out, size_t capacity, uint32_t *next, bool *gap);
/* Only f/l/r/u/t/b/s select; x stops. There is deliberately no remote start. */
bool calibration_course_command(calibration_core_t *c,const char *path);
bool calibration_command(calibration_core_t *c, char command);
int calibration_take_command(calibration_core_t *c);
bool calibration_parse_cursor(const char *text, uint32_t *value);
#endif
