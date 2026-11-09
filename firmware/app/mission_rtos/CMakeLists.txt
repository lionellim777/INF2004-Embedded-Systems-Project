cmake_minimum_required(VERSION 3.20)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(robocar_rtos_motion C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(MTK_BSP_PATH "$ENV{MTK_BSP_PATH}" CACHE PATH "Pinned single-core micro T-Kernel BSP")
if(NOT EXISTS "${MTK_BSP_PATH}/kernel/sysinit/sysinit.c")
    message(FATAL_ERROR "Set MTK_BSP_PATH to tron-forum/mtk3_bsp at 15ed232c08f2d89e54e514db79de150e224307e3")
endif()
execute_process(COMMAND git -C "${MTK_BSP_PATH}" rev-parse HEAD
    OUTPUT_VARIABLE MTK_REV OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT MTK_REV STREQUAL "15ed232c08f2d89e54e514db79de150e224307e3")
    message(FATAL_ERROR "Kernel revision differs from pinned Buddy1 BSP: ${MTK_REV}")
endif()
set(MTK "${MTK_BSP_PATH}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
set(RTOS_NETWORK_SETTINGS_FILE "" CACHE FILEPATH "Private network settings header")
option(RTOS_COMPACT_ADC_STATE "Opt-in minimum-bit ADC/filter candidate; not the accepted default" OFF)
if(RTOS_NETWORK_SETTINGS_FILE)
    configure_file("${RTOS_NETWORK_SETTINGS_FILE}" "${CMAKE_BINARY_DIR}/generated/network_settings.h" COPYONLY)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/network_settings.local.h")
    configure_file(network_settings.local.h "${CMAKE_BINARY_DIR}/generated/network_settings.h" COPYONLY)
else()
    configure_file(network_settings.example.h "${CMAKE_BINARY_DIR}/generated/network_settings.h" COPYONLY)
endif()
file(READ "${MTK}/config/config.h" MTK_CONFIG)
foreach(OPTION USE_TMONITOR USE_SDEV_DRV USE_SHUTDOWN USE_SYSTEM_MESSAGE USE_EXCEPTION_DBG_MSG)
    string(REGEX REPLACE "#define[ \t]+${OPTION}[ \t]+\\(1\\)"
        "#define ${OPTION} (0)" MTK_CONFIG "${MTK_CONFIG}")
endforeach()
string(REGEX REPLACE "#define[ \t]+CNF_TIMER_PERIOD[ \t]+10"
    "#define CNF_TIMER_PERIOD 5" MTK_CONFIG "${MTK_CONFIG}")
string(REGEX REPLACE "#define[ \t]+USE_DBGSPT[ \t]+\\(0\\)"
    "#define USE_DBGSPT (1)" MTK_CONFIG "${MTK_CONFIG}")
file(WRITE "${CMAKE_BINARY_DIR}/generated/config.h" "${MTK_CONFIG}")
file(GLOB MTK_KERNEL "${MTK}/kernel/tkernel/*.c")
file(GLOB MTK_STDLIB "${MTK}/kernel/tstdlib/*.c")
file(GLOB MTK_INIT "${MTK}/kernel/inittask/*.c")
file(GLOB MTK_SYSINIT "${MTK}/kernel/sysinit/*.c")
file(GLOB MTK_LIBTK "${MTK}/lib/libtk/*.c")
file(GLOB MTK_LIBTK_CORE "${MTK}/lib/libtk/sysdepend/cpu/core/armv6m/*.c")
add_library(mtkernel STATIC ${MTK_KERNEL} ${MTK_STDLIB} ${MTK_INIT}
    ${MTK_SYSINIT} ${MTK_LIBTK} ${MTK_LIBTK_CORE}
    "${MTK}/lib/libtk/sysdepend/cpu/rp2040/int_rp2040.c"
    "${MTK}/kernel/sysdepend/pico_rp2040/devinit.c"
    "${MTK}/kernel/sysdepend/pico_rp2040/power_save.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/cpu_cntl.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/interrupt.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/exc_hdr.c"
    "${MTK}/kernel/sysdepend/cpu/core/armv6m/dispatch.S")
set_source_files_properties("${MTK}/kernel/sysinit/sysinit.c"
    PROPERTIES COMPILE_DEFINITIONS "main=mtk_kernel_main")
target_compile_definitions(mtkernel PRIVATE _PICO_RP2040_)
target_include_directories(mtkernel PRIVATE "${CMAKE_BINARY_DIR}/generated"
    "${MTK}/config" "${MTK}/include" "${MTK}/kernel/knlinc")

set(BASELINE "${CMAKE_CURRENT_LIST_DIR}/../robot_firmware")
add_executable(robocar_rtos_motion main.c board.c safety_gate.c motion_record.c motion_runtime.c
    binary_protocol.c network_service.c
    track_sensor.c track_runtime.c scan_diagnostic.c runtime_measure.c runtime_report.c port/task_observe.c
    "${BASELINE}/course_control.c" "${BASELINE}/barcode_waveform.c" "${BASELINE}/barcode_decoder.c"
    port/mtk_bridge.c "${BASELINE}/encoders.c" "${BASELINE}/motion_control.c")
target_include_directories(robocar_rtos_motion PRIVATE "${CMAKE_CURRENT_LIST_DIR}"
    "${BASELINE}" "${CMAKE_BINARY_DIR}/generated" "${MTK}/config" "${MTK}/include")
target_compile_definitions(robocar_rtos_motion PRIVATE _PICO_RP2040_
    RTOS_TRACK=1 RTOS_RUNTIME_EVIDENCE=1 BARCODE_WAVE_RECOVERY_OPTICS=1 BARCODE_WAVE_LAST_PASS=1 ROBOCAR_CENTER_LINE=1 PROHIBIT_DEF_SIZE_T PICO_STACK_SIZE=0x1000 PICO_STDIO_USB_STDOUT_TIMEOUT_US=1000)
target_compile_options(robocar_rtos_motion PRIVATE -Wall -Wextra -Werror -fstack-usage)
if(RTOS_COMPACT_ADC_STATE)
    target_compile_definitions(robocar_rtos_motion PRIVATE RTOS_COMPACT_ADC_STATE=1 RTOS_FIRMWARE_VERSION="0.6-dev1")
endif()
target_link_libraries(robocar_rtos_motion PRIVATE mtkernel pico_stdlib
    hardware_adc hardware_clocks hardware_exception hardware_pwm pico_rand pico_cyw43_arch_lwip_poll pico_lwip_mqtt m)
pico_enable_stdio_usb(robocar_rtos_motion 1)
pico_enable_stdio_uart(robocar_rtos_motion 0)
pico_add_extra_outputs(robocar_rtos_motion)
