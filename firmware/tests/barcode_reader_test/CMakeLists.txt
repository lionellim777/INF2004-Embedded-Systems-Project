cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(barcode_reader_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(APP_DIR ${CMAKE_CURRENT_LIST_DIR}/../../app/robot_firmware)

add_executable(barcode_reader_test
    barcode_reader_test.c
    ${APP_DIR}/barcode_decoder.c
    ${APP_DIR}/barcode_reader.c
    ${APP_DIR}/navigation_commands.c
)

target_include_directories(barcode_reader_test PRIVATE ${APP_DIR})
target_link_libraries(barcode_reader_test pico_stdlib hardware_adc)

pico_enable_stdio_usb(barcode_reader_test 1)
pico_enable_stdio_uart(barcode_reader_test 0)
pico_add_extra_outputs(barcode_reader_test)
