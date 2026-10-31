#!/usr/bin/env python3
"""Run the actual stationary firmware main loop against fake GPIO/clock/ADC.
All simulated writes to motor pins must be zero. No real robot connection.
"""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1]; app=root/'firmware/app/robot_firmware'
build=root/'build/stationary-host-tests';stubs=build/'stubs'
for d in ['hardware','pico']:(stubs/d).mkdir(parents=True,exist_ok=True)
(stubs/'mock.h').write_text('''#ifndef STATIONARY_MOCK_H
#define STATIONARY_MOCK_H
#include <stdint.h>
#include <stdbool.h>
typedef unsigned uint;
typedef struct {int unused;} repeating_timer_t;
#define GPIO_OUT 1
#define GPIO_IN 0
#define PICO_ERROR_TIMEOUT -1
void gpio_put(uint,bool);void gpio_init(uint);void gpio_set_dir(uint,bool);void gpio_pull_up(uint);bool gpio_get(uint);
void adc_init(void);void adc_gpio_init(uint);void adc_select_input(uint);uint16_t adc_read(void);
uint32_t save_and_disable_interrupts(void);void restore_interrupts(uint32_t);
uint32_t time_us_32(void);uint64_t time_us_64(void);void sleep_ms(uint32_t);
bool add_repeating_timer_us(int64_t,bool(*)(repeating_timer_t*),void*,repeating_timer_t*);
void stdio_init_all(void);bool stdio_usb_connected(void);int getchar_timeout_us(uint32_t);
#endif
''')
for name in ['hardware/adc.h','hardware/sync.h','pico/stdlib.h']:(stubs/name).write_text('#include "mock.h"\n')
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
exe=build/'stationary_main_test'
subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DROBOCAR_WIRELESS_CALIBRATION=1','-I'+str(stubs),'-I'+str(app),str(root/'firmware/tests/stationary_main_test.c'),str(app/'barcode_acquisition.c'),str(app/'barcode_reader.c'),str(app/'barcode_decoder.c'),'-o',str(exe)],check=True,env=env)
for scenario in range(18):subprocess.run([str(exe),str(scenario)],check=True,env=env)
