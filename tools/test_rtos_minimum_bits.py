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
    accepted=subprocess.check_output(['git','show','a4e15b5:firmware/app/robot_firmware/center_baseline.h'],text=True,cwd=ROOT)
    (Path(name)/'center_baseline_reference.h').write_text(accepted)
    baseline=Path(name)/'baseline'
    subprocess.run([shutil.which('clang'),'-std=c11','-O1','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-I'+name,'-I'+str(BASE),
        str(ROOT/'firmware/tests/mission_rtos/center_baseline_compact_test.c'),'-o',str(baseline)],check=True,env=env)
    subprocess.run([str(baseline)],check=True,env=env)
    reference=None
    for adc,core in (('0','0'),('1','0'),('0','1'),('1','1')):
        output=subprocess.check_output([os.sys.executable,str(ROOT/'tools/test_rtos_track.py')],
            env=dict(env,RTOS_TEST_COMPACT_ADC=adc,RTOS_TEST_COMPACT_CORE=core),text=True)
        print('Compact ADC/core='+adc+'/'+core+'\n'+output,end='')
        packets=json.loads((ROOT/'build/rtos-scan-fixtures.json').read_text())
        if reference is None:reference=packets
        else:assert packets==reference,'Core packing changed an actual pipeline diagnostic export'
    print('PASS all four ADC/core combinations: identical bytes for AAZ,12-event ring wrap, stopped partial, first-failure and maximum-size diagnostic exports')
    output=subprocess.check_output([os.sys.executable,str(ROOT/'tools/test_rtos_track.py')],
        env=dict(env,RTOS_TEST_COMPACT_ADC='1',RTOS_TEST_COMPACT_CORE='1',RTOS_TEST_FIRST_FAILURE_CACHE='0'),text=True)
    print('Lean diagnostics\n'+output,end='')
    lean=json.loads((ROOT/'build/rtos-scan-fixtures.json').read_text())
    assert {k:v for k,v in lean.items() if k!='first-failure'}=={k:v for k,v in reference.items() if k!='first-failure'}
    print('PASS lean profile: identical AAZ/ring-wrap/partial/max exports; documented first-failure raw retention differs, with attempt summary and latest accepted waveform verified')
