cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ultrasonic_scan_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(ultrasonic_scan_test
    ultrasonic_scan_test.c
)

target_link_libraries(ultrasonic_scan_test
    pico_stdlib
    hardware_clocks
    hardware_pwm
    m
)

pico_enable_stdio_usb(ultrasonic_scan_test 1)
pico_enable_stdio_uart(ultrasonic_scan_test 0)
pico_add_extra_outputs(ultrasonic_scan_test)
