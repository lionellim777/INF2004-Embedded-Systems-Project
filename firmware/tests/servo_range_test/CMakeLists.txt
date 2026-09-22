cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(servo_range_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(servo_range_test
    servo_range_test.c
)

target_link_libraries(servo_range_test
    pico_stdlib
    hardware_clocks
    hardware_pwm
)

pico_enable_stdio_usb(servo_range_test 1)
pico_enable_stdio_uart(servo_range_test 0)
pico_add_extra_outputs(servo_range_test)
