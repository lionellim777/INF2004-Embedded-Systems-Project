#!/usr/bin/env python3
"""Grey waveform regression checks; synthetic signals, no physical acceptance."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
app=root/'firmware/app/robot_firmware'
build=root/'build/waveform-host-tests'
build.mkdir(parents=True,exist_ok=True)
exe=build/'barcode_waveform_test'
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-O1',
    '-fsanitize=address,undefined','-I'+str(app),
    str(root/'firmware/tests/barcode_waveform_test.c'),str(app/'barcode_waveform.c'),
    str(app/'barcode_decoder.c'),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe),str(root/'docs/testing/fixtures/v5_58-half-Z-ADC12_MS4.bin')],check=True,env=env)
