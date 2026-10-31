#!/usr/bin/env python3
"""Host checks of barcode acquisition/decoding and existing motion protections.
No hardware timing, steering, power, optical contrast or course acceptance implied.
"""
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
app=root/'firmware/app/robot_firmware'
build=root/'build/wireless-host-tests'
build.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
checks={
    'independent_barcode':['barcode_acquisition.c','barcode_reader.c','barcode_decoder.c'],
    'barcode_motion':['barcode_acquisition.c','barcode_reader.c','barcode_decoder.c'],
    'center_launch':[],
    'barcode_signal':['barcode_acquisition.c','barcode_reader.c','barcode_decoder.c'],
    'barcode_optical':[],
    'barcode_drive':[],
    'center_course':['motion_control.c'],
    'center_recovery':['motion_control.c'],
    'course_control':['course_control.c','motion_control.c'],
    'continuous_scan':['barcode_acquisition.c','barcode_reader.c','barcode_decoder.c'],
    'barcode_decoder_bias':['barcode_decoder.c','barcode_reader.c','barcode_acquisition.c'],
    'barcode_acquisition':['barcode_acquisition.c','barcode_reader.c','barcode_decoder.c'],
    'line_check':[],
    'run_diagnostics':['run_diagnostics.c'],
    'motion_control_host':['motion_control.c'],
}
for test,sources in checks.items():
    exe=build/(test+'_test')
    command=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(app),
             str(root/'firmware/tests'/(test+'_test.c'))]+[str(app/s) for s in sources]+['-lm','-o',str(exe)]
    subprocess.run(command,check=True,env=env)
    subprocess.run([str(exe)],check=True,env=env)
