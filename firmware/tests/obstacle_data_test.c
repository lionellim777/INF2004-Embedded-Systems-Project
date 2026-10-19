#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../app/robot_firmware/obstacle_data.h"

int main(void)
{
    obstacle_sample_t near = obstacle_sample_make(299u, OBSTACLE_VALID);
    obstacle_sample_t edge = obstacle_sample_make(300u, OBSTACLE_VALID);
    obstacle_sample_t limit = obstacle_sample_make(1000u, OBSTACLE_VALID);
    obstacle_sample_t beyond = obstacle_sample_make(1001u, OBSTACLE_VALID);

    assert(sizeof(obstacle_sample_t) == 2u);
    assert(obstacle_sample_status(near) == OBSTACLE_VALID);
    assert(obstacle_sample_mm(near) < OBSTACLE_STOP_MM);
    assert(obstacle_sample_mm(edge) == OBSTACLE_STOP_MM);
    assert(obstacle_sample_status(limit) == OBSTACLE_VALID);
    assert(obstacle_sample_mm(limit) == 1000u);
    assert(obstacle_sample_status(beyond) == OBSTACLE_OVER_LIMIT);
    assert(obstacle_sample_mm(beyond) == 0u);

    assert(obstacle_sample_mm(obstacle_sample_from_echo_us(1749u)) == 300u);
    assert(obstacle_sample_status(obstacle_sample_from_echo_us(0u)) ==
           OBSTACLE_UNAVAILABLE);
    assert(obstacle_sample_status(obstacle_sample_from_echo_us(5834u)) ==
           OBSTACLE_OVER_LIMIT);

    uint16_t point = obstacle_fine_point_make(15u, limit);
    assert(obstacle_fine_point_index(point) == 15u);
    assert(obstacle_fine_point_angle_deg(point) == 125u);
    assert(obstacle_sample_status(point & 0x0fffu) == OBSTACLE_VALID);
    assert(obstacle_sample_mm(point & 0x0fffu) == 1000u);
    assert(obstacle_sample_status(obstacle_fine_point_make(16u, limit)) ==
           OBSTACLE_NOT_MEASURED);

    puts("obstacle data boundary tests passed");
    return 0;
}
