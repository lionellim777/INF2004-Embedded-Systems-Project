#!/usr/bin/env python3
"""Actual moving main loop against synthetic clocks/ADC/encoders/PWM, no hardware."""
from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parents[1];app=root/'firmware/app/robot_firmware'
build=root/'build/course-host-tests';stubs=build/'stubs'
for d in ['hardware','pico']:(stubs/d).mkdir(parents=True,exist_ok=True)
(stubs/'mock.h').write_text('''#ifndef COURSE_MAIN_MOCK_H
#define COURSE_MAIN_MOCK_H
#include <stdint.h>
#include <stdbool.h>
typedef unsigned uint;
typedef struct {int unused;} repeating_timer_t;
#define GPIO_OUT 1
#define GPIO_IN 0
#define GPIO_FUNC_PWM 4
#define clk_sys 0
#define PICO_ERROR_TIMEOUT -1
void gpio_put(uint,bool);void gpio_init(uint);void gpio_set_dir(uint,bool);void gpio_pull_up(uint);bool gpio_get(uint);void gpio_set_function(uint,uint);
void adc_init(void);void adc_gpio_init(uint);void adc_select_input(uint);uint16_t adc_read(void);
uint32_t save_and_disable_interrupts(void);void restore_interrupts(uint32_t);
uint32_t time_us_32(void);uint64_t time_us_64(void);void sleep_ms(uint32_t);void sleep_us(uint64_t);void busy_wait_us_32(uint32_t);void tight_loop_contents(void);
bool add_repeating_timer_us(int64_t,bool(*)(repeating_timer_t*),void*,repeating_timer_t*);
bool add_repeating_timer_ms(int32_t,bool(*)(repeating_timer_t*),void*,repeating_timer_t*);
void stdio_init_all(void);bool stdio_usb_connected(void);int getchar_timeout_us(uint32_t);
void pwm_set_chan_level(uint,uint,uint16_t);uint pwm_gpio_to_slice_num(uint);uint pwm_gpio_to_channel(uint);
void pwm_set_clkdiv(uint,float);void pwm_set_wrap(uint,uint16_t);void pwm_set_enabled(uint,bool);uint32_t clock_get_hz(uint);
#endif
''')
for name in ['hardware/adc.h','hardware/sync.h','hardware/clocks.h','hardware/pwm.h','pico/stdlib.h']:(stubs/name).write_text('#include "mock.h"\n')
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
center='--center' in sys.argv
exe=build/('course_center_main_test' if center else 'course_main_test')
sources=['course_control.c','motion_control.c','barcode_acquisition.c','barcode_reader.c','barcode_decoder.c','run_diagnostics.c','navigation_commands.c']
subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DROBOCAR_WIRELESS_CALIBRATION=1','-DROBOCAR_CENTER_LINE='+str(int(center)),'-I'+str(stubs),'-I'+str(app),str(root/'firmware/tests/course_main_test.c')]+[str(app/s) for s in sources]+['-lm','-o',str(exe)],check=True,env=env)
for scenario in range(34 if center else 18):subprocess.run([str(exe),str(scenario)],check=True,env=env)
