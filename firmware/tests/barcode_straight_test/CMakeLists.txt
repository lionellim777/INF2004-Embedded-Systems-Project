cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(barcode_straight_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(APP_DIR ${CMAKE_CURRENT_LIST_DIR}/../../app/robot_firmware)

# Barcode, motion and encoder modules only: no line following, sonar,
# servo, IMU or Wi-Fi.
add_executable(barcode_straight_test
    barcode_straight_test.c
    ${APP_DIR}/barcode_acquisition.c
    ${APP_DIR}/barcode_reader.c
    ${APP_DIR}/barcode_decoder.c
    ${APP_DIR}/navigation_commands.c
    ${APP_DIR}/motion_control.c
    ${APP_DIR}/encoders.c
)

target_include_directories(barcode_straight_test PRIVATE ${APP_DIR})
target_link_libraries(barcode_straight_test
    pico_stdlib
    hardware_adc
    hardware_clocks
    hardware_flash
    hardware_pwm
    hardware_sync
)

pico_enable_stdio_usb(barcode_straight_test 1)
pico_enable_stdio_uart(barcode_straight_test 0)
pico_add_extra_outputs(barcode_straight_test)
