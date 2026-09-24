cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(line_junction_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(line_junction_test line_junction_test.c)
target_link_libraries(line_junction_test pico_stdlib hardware_adc)

pico_enable_stdio_usb(line_junction_test 1)
pico_enable_stdio_uart(line_junction_test 0)
pico_add_extra_outputs(line_junction_test)
