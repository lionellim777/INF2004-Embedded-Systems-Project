cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(encoder_wiring_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(encoder_wiring_test encoder_wiring_test.c)
target_link_libraries(encoder_wiring_test pico_stdlib hardware_sync)

pico_enable_stdio_usb(encoder_wiring_test 1)
pico_enable_stdio_uart(encoder_wiring_test 0)
pico_add_extra_outputs(encoder_wiring_test)
