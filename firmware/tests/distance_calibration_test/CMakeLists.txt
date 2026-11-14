cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(distance_calibration_test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Deliberately reuse the application's encoder module so the calibration is
# measured with exactly the counting code the robot runs.
set(APP_DIR ${CMAKE_CURRENT_LIST_DIR}/../../app/robot_firmware)

add_executable(distance_calibration_test
    distance_calibration_test.c
    ${APP_DIR}/encoders.c
)

target_include_directories(distance_calibration_test PRIVATE ${APP_DIR})

target_link_libraries(distance_calibration_test
    pico_stdlib
    hardware_gpio
    hardware_sync
)

pico_enable_stdio_usb(distance_calibration_test 1)
pico_enable_stdio_uart(distance_calibration_test 0)
pico_add_extra_outputs(distance_calibration_test)
