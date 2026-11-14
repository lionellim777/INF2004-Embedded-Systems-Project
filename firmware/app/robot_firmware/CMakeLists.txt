cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(robot_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()
option(ROBOCAR_CENTER_LINE "GP28 directly over line with GP27 to its right" OFF)

option(ROBOCAR_STATIONARY_BARCODE "Build motor-disabled hand-scan diagnostic" OFF)
option(ROBOCAR_MEASUREMENT_DIAG "Build motor-disabled encoder-distance and IMU measurement dashboard" OFF)
if(ROBOCAR_STATIONARY_BARCODE AND ROBOCAR_MEASUREMENT_DIAG)
    message(FATAL_ERROR "Choose only one diagnostic build: ROBOCAR_STATIONARY_BARCODE or ROBOCAR_MEASUREMENT_DIAG")
endif()
if(ROBOCAR_STATIONARY_BARCODE)
    add_executable(robot_firmware stationary_barcode.c barcode_decoder.c barcode_reader.c barcode_acquisition.c)
    target_compile_definitions(robot_firmware PRIVATE ROBOCAR_STATIONARY_BARCODE=1)
elseif(ROBOCAR_MEASUREMENT_DIAG)
    add_executable(robot_firmware measurement_diag.c encoders.c)
    target_compile_definitions(robot_firmware PRIVATE ROBOCAR_MEASUREMENT_DIAG=1)
else()
    add_executable(robot_firmware
        robot_firmware.c terrain.c encoders.c motion_control.c run_diagnostics.c course_control.c obstacle_bypass.c
        barcode_decoder.c barcode_reader.c barcode_acquisition.c barcode_waveform.c navigation_commands.c)
endif()

target_link_libraries(robot_firmware
    pico_stdlib
    hardware_adc
    hardware_clocks
    hardware_pwm
    hardware_i2c
    m
)
if(ROBOCAR_CENTER_LINE)
    target_compile_definitions(robot_firmware PRIVATE ROBOCAR_CENTER_LINE=1)
endif()

option(ROBOCAR_WIRELESS_CALIBRATION "Build the local Wi-Fi calibration page" OFF)
option(ROBOCAR_WIFI_STATION "Join an existing Wi-Fi network (credentials in station_settings.local.h) instead of hosting AAI10-Calibrate; falls back to the access point if the join fails" OFF)
set(ROBOCAR_STATION_SETTINGS_DIR "${CMAKE_CURRENT_LIST_DIR}/wireless" CACHE PATH "Directory holding station_settings.local.h")
if(ROBOCAR_WIFI_STATION AND NOT ROBOCAR_WIRELESS_CALIBRATION)
    message(FATAL_ERROR "ROBOCAR_WIFI_STATION needs -DROBOCAR_WIRELESS_CALIBRATION=ON")
endif()
if(ROBOCAR_WIRELESS_CALIBRATION)
    target_sources(robot_firmware PRIVATE
        wireless/calibration_core.c
        wireless/calibration_wifi.c
        wireless/dhcpserver.c
    )
    target_include_directories(robot_firmware PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/wireless
    )
    target_compile_definitions(robot_firmware PRIVATE ROBOCAR_WIRELESS_CALIBRATION=1)
    if(ROBOCAR_WIFI_STATION)
        if(NOT EXISTS "${ROBOCAR_STATION_SETTINGS_DIR}/station_settings.local.h")
            message(FATAL_ERROR "Station mode needs ${ROBOCAR_STATION_SETTINGS_DIR}/station_settings.local.h. "
                "Copy station_settings.example.h to station_settings.local.h in that folder and enter your Wi-Fi name and password. "
                "The local file is git-ignored; never commit it or a UF2 built from it.")
        endif()
        target_include_directories(robot_firmware PRIVATE ${ROBOCAR_STATION_SETTINGS_DIR})
        target_compile_definitions(robot_firmware PRIVATE ROBOCAR_WIFI_STATION=1 "CYW43_HOST_NAME=\"aai10-car\"")
    endif()
    target_link_libraries(robot_firmware pico_cyw43_arch_lwip_poll pico_rand)
    if(NOT ROBOCAR_STATIONARY_BARCODE AND NOT ROBOCAR_MEASUREMENT_DIAG)
        target_sources(robot_firmware PRIVATE measurement.c)
        target_compile_definitions(robot_firmware PRIVATE ROBOCAR_WIFI_CORE1=1 ROBOCAR_MEASUREMENTS=1)
        target_link_libraries(robot_firmware pico_multicore)
    endif()
    pico_configure_ip4_address(robot_firmware PRIVATE CYW43_DEFAULT_IP_AP_ADDRESS 192.168.4.1)
endif()

pico_enable_stdio_usb(robot_firmware 1)
pico_enable_stdio_uart(robot_firmware 0)
target_compile_definitions(robot_firmware PRIVATE PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000 PICO_STACK_SIZE=0x1000)
pico_add_extra_outputs(robot_firmware)
