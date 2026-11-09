#!/usr/bin/env python3
"""Compare opt-in compact ADC state against the accepted filter and pipeline."""
import json,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
APP=ROOT/'firmware/app/mission_rtos';BASE=ROOT/'firmware/app/robot_firmware'
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
with tempfile.TemporaryDirectory(prefix='rtos-minbits-') as name:
    exe=Path(name)/'filter'
    subprocess.run([shutil.which('clang'),'-std=c11','-O1','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-I'+str(APP),'-I'+str(BASE),
        str(ROOT/'firmware/tests/mission_rtos/adc12_filter_test.c'),'-o',str(exe)],check=True,env=env)
    subprocess.run([str(exe)],check=True,env=env)
    reference=None
    for mode in ('0','1'):
        output=subprocess.check_output([os.sys.executable,str(ROOT/'tools/test_rtos_track.py')],
            env=dict(env,RTOS_TEST_COMPACT_ADC=mode),text=True)
        print('Compact ADC mode='+mode+'\n'+output,end='')
        packets=json.loads((ROOT/'build/rtos-scan-fixtures.json').read_text())
        if reference is None:reference=packets
        else:assert packets==reference,'ADC packing changed an actual pipeline diagnostic export'
    print('PASS default/compact ADC-to-decoder/course/PWM integrations: identical bytes for AAZ, stopped partial, first-failure and maximum-size diagnostic exports')
