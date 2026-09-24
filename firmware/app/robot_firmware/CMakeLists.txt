cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(robot_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(robot_firmware
    robot_firmware.c
    terrain.c
    encoders.c
    barcode_decoder.c
    barcode_reader.c
    navigation_commands.c
)

target_link_libraries(robot_firmware
    pico_stdlib
    hardware_adc
    hardware_clocks
    hardware_pwm
    hardware_i2c
    m
)

pico_enable_stdio_usb(robot_firmware 1)
pico_enable_stdio_uart(robot_firmware 0)
pico_add_extra_outputs(robot_firmware)
