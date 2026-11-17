cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(imu_orientation_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(imu_orientation_test
    imu_orientation_test.c
)

target_link_libraries(imu_orientation_test
    pico_stdlib
    hardware_i2c
    m
)

pico_enable_stdio_usb(imu_orientation_test 1)
pico_enable_stdio_uart(imu_orientation_test 0)
pico_add_extra_outputs(imu_orientation_test)
