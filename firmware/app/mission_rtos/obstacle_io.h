#ifndef RTOS_OBSTACLE_IO_H
#define RTOS_OBSTACLE_IO_H
#include <stdbool.h>

typedef struct {
    float cm;
    const char *error;
    bool fresh;
    bool valid;
    bool no_return;
} obstacle_range_t;

bool obstacle_io_init(void);
void obstacle_io_begin(void);
void obstacle_io_end(void);
void obstacle_io_angle(unsigned degrees);
obstacle_range_t obstacle_io_poll(void);
#endif
