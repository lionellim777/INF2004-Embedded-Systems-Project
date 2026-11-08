#!/usr/bin/env python3
"""Run actual ADC/queue/decoder/course/motor integration against SDK mocks."""
from pathlib import Path
import importlib.util,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
APP=ROOT/'firmware/app/mission_rtos';BASE=ROOT/'firmware/app/robot_firmware';TEST=ROOT/'firmware/tests/mission_rtos'
with tempfile.TemporaryDirectory(prefix='rtos-track-') as name:
    folder=Path(name)
    for header in ['hardware/adc.h','hardware/clocks.h','hardware/pwm.h','hardware/sync.h','pico/stdlib.h']:
        p=folder/header;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "track_test_sdk.h"\n')
    sources=[APP/n for n in ['board.c','safety_gate.c','motion_record.c','motion_runtime.c','track_sensor.c','track_runtime.c']]
    sources += [BASE/n for n in ['motion_control.c','course_control.c','barcode_waveform.c','barcode_decoder.c']]
    command=[shutil.which('clang'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DRTOS_TRACK=1','-DROBOCAR_CENTER_LINE=1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    command += ['-I'+str(p) for p in [APP,BASE,TEST,folder]]
    command += [str(p) for p in sources]+[str(TEST/'track_integration_test.c'),'-o',str(folder/'test')]
    subprocess.run(command,check=True)
    output=subprocess.check_output([str(folder/'test')],text=True);print(output,end='')
    spec=importlib.util.spec_from_file_location('decode_motion',ROOT/'tools/decode_motion_record.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    fixture=next(bytes.fromhex(l.split()[1]) for l in output.splitlines() if l.startswith('TRACK_FIXTURE '))
    decoded=module.decode(fixture)
    assert decoded['kind']==7 and decoded['left_counts']>32767 and decoded['right_counts']>32767
    (ROOT/'build/rtos-track-tests.json').write_text(json.dumps(dict(passed=True,full_half_A_and_recovery_Z=True,adc_and_queue_safety=True,extended_motion_fixture=decoded,physical_tested=False),indent=2)+'\n')
    print('PASS independent Python track-count decode; these are host simulations, not physical course acceptance')
