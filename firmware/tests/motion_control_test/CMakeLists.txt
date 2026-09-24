cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(motion_control_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(motion_control_test motion_control_test.c)
target_link_libraries(motion_control_test
    pico_stdlib
    hardware_clocks
    hardware_flash
    hardware_pwm
    hardware_sync
)

pico_enable_stdio_usb(motion_control_test 1)
pico_enable_stdio_uart(motion_control_test 0)
pico_add_extra_outputs(motion_control_test)
