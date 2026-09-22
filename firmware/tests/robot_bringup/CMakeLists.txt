cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(robot_bringup C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(robot_bringup
    robot_bringup.c
)

target_link_libraries(robot_bringup
    pico_stdlib
    hardware_adc
    hardware_i2c
    hardware_pwm
    hardware_clocks
)

pico_enable_stdio_usb(robot_bringup 1)
pico_enable_stdio_uart(robot_bringup 0)
pico_add_extra_outputs(robot_bringup)
