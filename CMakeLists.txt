cmake_minimum_required(VERSION 3.20)

option(BUDDY1_HOST_TESTS "Build protocol tests without Pico SDK" OFF)

if(BUDDY1_HOST_TESTS)
    project(buddy1_host C)
    set(CMAKE_C_STANDARD 99)
    enable_testing()
    add_library(buddy1_protocol STATIC src/command.c src/telemetry.c)
    target_include_directories(buddy1_protocol PUBLIC include)
    add_executable(buddy1_protocol_tests tests/protocol_tests.c)
    target_link_libraries(buddy1_protocol_tests PRIVATE buddy1_protocol)
    add_test(NAME protocol COMMAND buddy1_protocol_tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME json_contract COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/json_contract_test.py
        $<TARGET_FILE:buddy1_protocol_tests>)
    return()
endif()

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Set PICO_SDK_PATH to Pico SDK 2.2.0")
endif()

set(PICO_BOARD pico_w CACHE STRING "Pico W board")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(buddy1_pico_w C CXX ASM)
# Pico SDK 2.2.0 headers use static_assert; application sources remain C99.
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

if(NOT MTK_BSP_PATH AND DEFINED ENV{MTK_BSP_PATH})
    set(MTK_BSP_PATH "$ENV{MTK_BSP_PATH}")
endif()
if(NOT EXISTS "${MTK_BSP_PATH}/kernel/sysinit/sysinit.c")
    message(FATAL_ERROR
        "Set MTK_BSP_PATH to the pico_rp2040 BSP at commit 15ed232c")
endif()
set(MTK "${MTK_BSP_PATH}")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
file(READ "${MTK}/config/config.h" MTK_CONFIG)
string(REGEX REPLACE
    "#define[ \t]+USE_TMONITOR[ \t]+\\(1\\)"
    "#define USE_TMONITOR (0)" MTK_CONFIG "${MTK_CONFIG}")
string(REGEX REPLACE
    "#define[ \t]+USE_SDEV_DRV[ \t]+\\(1\\)"
    "#define USE_SDEV_DRV (0)" MTK_CONFIG "${MTK_CONFIG}")
string(REGEX REPLACE
    "#define[ \t]+USE_SHUTDOWN[ \t]+\\(1\\)"
    "#define USE_SHUTDOWN (0)" MTK_CONFIG "${MTK_CONFIG}")
string(REGEX REPLACE
    "#define[ \t]+CNF_TIMER_PERIOD[ \t]+10"
    "#define CNF_TIMER_PERIOD 5" MTK_CONFIG "${MTK_CONFIG}")
file(WRITE "${CMAKE_BINARY_DIR}/generated/config.h" "${MTK_CONFIG}")

if(EXISTS "${CMAKE_SOURCE_DIR}/config/comm_secrets.h")
    set(COMM_SECRETS "${CMAKE_SOURCE_DIR}/config/comm_secrets.h")
else()
    set(COMM_SECRETS
        "${CMAKE_SOURCE_DIR}/config/comm_secrets.example.h")
endif()
configure_file("${COMM_SECRETS}"
               "${CMAKE_BINARY_DIR}/generated/comm_secrets.h" COPYONLY)

file(GLOB MTK_KERNEL "${MTK}/kernel/tkernel/*.c")
file(GLOB MTK_STDLIB "${MTK}/kernel/tstdlib/*.c")
file(GLOB MTK_INIT "${MTK}/kernel/inittask/*.c")
file(GLOB MTK_SYSINIT "${MTK}/kernel/sysinit/*.c")
file(GLOB MTK_LIBTK "${MTK}/lib/libtk/*.c")
file(GLOB MTK_LIBTK_CORE
     "${MTK}/lib/libtk/sysdepend/cpu/core/armv6m/*.c")

set(MTK_SOURCES
    ${MTK_KERNEL}
    ${MTK_STDLIB}
    ${MTK_INIT}
    ${MTK_SYSINIT}
    ${MTK_LIBTK}
    ${MTK_LIBTK_CORE}
    "${MTK}/lib/libtk/sysdepend/cpu/rp2040/int_rp2040.c"
    "${MTK}/kernel/sysdepend/pico_rp2040/devinit.c"
    "${MTK}/kernel/sysdepend/pico_rp2040/power_save.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/cpu_cntl.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/interrupt.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/exc_hdr.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/dispatch.S")

set_source_files_properties("${MTK}/kernel/sysinit/sysinit.c"
    PROPERTIES COMPILE_DEFINITIONS "main=mtk_kernel_main")
add_library(mtkernel STATIC ${MTK_SOURCES})
target_compile_definitions(mtkernel PRIVATE _PICO_RP2040_)
target_include_directories(mtkernel PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
    "${MTK}/config"
    "${MTK}/include"
    "${MTK}/kernel/knlinc")

set(BUDDY1_APP_SOURCES
    port/mtk_bridge.c
    port/mtk_pico_main.c
    src/app_main.c
    src/comm.c
    src/command.c
    src/telemetry.c)
add_executable(buddy1_pico_w ${BUDDY1_APP_SOURCES})
target_include_directories(buddy1_pico_w PRIVATE
    include config port
    "${CMAKE_BINARY_DIR}/generated"
    "${MTK}/config"
    "${MTK}/include")
target_compile_definitions(buddy1_pico_w PRIVATE
    _PICO_RP2040_ PROHIBIT_DEF_SIZE_T)
set_source_files_properties(${BUDDY1_APP_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wconversion")
target_link_libraries(buddy1_pico_w PRIVATE
    mtkernel
    pico_stdlib
    pico_rand
    pico_cyw43_arch_lwip_poll
    pico_lwip_mqtt
    hardware_exception
    hardware_clocks)
pico_enable_stdio_usb(buddy1_pico_w 1)
pico_enable_stdio_uart(buddy1_pico_w 0)
pico_add_extra_outputs(buddy1_pico_w)
