cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(ir_calibration_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(ir_calibration_test
    ir_calibration_test.c
)

target_link_libraries(ir_calibration_test
    pico_stdlib
    hardware_adc
)

pico_enable_stdio_usb(ir_calibration_test 1)
pico_enable_stdio_uart(ir_calibration_test 0)
pico_add_extra_outputs(ir_calibration_test)
