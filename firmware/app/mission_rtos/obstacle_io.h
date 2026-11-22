#ifndef RTOS_OBSTACLE_IO_H
#define RTOS_OBSTACLE_IO_H
#include <stdbool.h>
#include "control_representation.h"

typedef struct
{
    float cm;
    const char *error;
    bool fresh ROBOCAR_BIT1;
    bool valid ROBOCAR_BIT1;
    bool no_return ROBOCAR_BIT1;
} obstacle_range_t;

bool obstacle_io_init(void);
void obstacle_io_begin(void);
void obstacle_io_end(void);
void obstacle_io_angle(unsigned degrees);
obstacle_range_t obstacle_io_poll(void);
#endif
