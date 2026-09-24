cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(encoder_motor_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(encoder_motor_test encoder_motor_test.c)
target_link_libraries(encoder_motor_test
    pico_stdlib
    hardware_clocks
    hardware_pwm
    hardware_sync
)

pico_enable_stdio_usb(encoder_motor_test 1)
pico_enable_stdio_uart(encoder_motor_test 0)
pico_add_extra_outputs(encoder_motor_test)
