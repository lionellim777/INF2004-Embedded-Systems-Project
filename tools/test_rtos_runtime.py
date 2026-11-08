#!/usr/bin/env python3
"""Test real measurement/report/stack code against SDK and kernel-call mocks."""
from pathlib import Path
import json,os,shutil,subprocess,tempfile
from rtos_runtime_capture import decode_runtime,RuntimeCapture
ROOT=Path(__file__).resolve().parents[1];APP=ROOT/'firmware/app/mission_rtos';TEST=ROOT/'firmware/tests/mission_rtos'
with tempfile.TemporaryDirectory(prefix='rtos-runtime-') as name:
    folder=Path(name)
    for h in ['hardware/sync.h','pico/stdlib.h']:
        p=folder/h;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "test_sdk.h"\n')
    for h in ['tk/tkernel.h','tk/dbgspt.h']:
        p=folder/h;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('#include "runtime_test_kernel.h"\n')
    cmd=[shutil.which('clang'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    cmd+=['-I'+str(p) for p in [APP,ROOT/'firmware/app/robot_firmware',TEST,folder]]
    cmd+=[str(APP/'runtime_measure.c'),str(APP/'runtime_report.c'),str(TEST/'runtime_probe_test.c'),'-o',str(folder/'test')]
    subprocess.run(cmd,check=True)
    output=subprocess.check_output([str(folder/'test')],text=True);print(output,end='')
    packets=[bytes.fromhex(x.split()[1]) for x in output.splitlines() if x.startswith('RUNTIME_FIXTURE ')]
    data=[decode_runtime(b) for b in packets]
    assert data[0]['allocated_bytes']==4224 and data[0]['untouched_bytes']==3896 and data[0]['observed_watermark_bytes']==328
    assert data[0]['armed_gap_max_us']==5000 and data[0]['gap_max_us']==2000000
    assert data[1]['request_to_pwm_writes_us']==10 and data[1]['stop_source_label']=='stop API entry'
    for packet in packets:
        for bad in [packet[:-1],packet+b'\0']:
            try:decode_runtime(bad)
            except ValueError:pass
            else:raise AssertionError('Malformed runtime length accepted')
    bad=bytearray(packets[0]);bad[13]|=128
    try:decode_runtime(bytes(bad))
    except ValueError:pass
    else:raise AssertionError('Runtime padding accepted')
    capture=RuntimeCapture();capture.boot_id(123);capture.accept(data[0]);older=dict(data[0],captured_ms=0xfffffffe,untouched_bytes=4000)
    capture.accept(older);assert capture.runs['123-65535']['tasks']['motion']['untouched_bytes']==3896
    newer=dict(data[0],captured_ms=0,untouched_bytes=3800);capture.accept(newer)
    assert capture.runs['123-65535']['tasks']['motion']['untouched_bytes']==3800
    capture.boot_id(124);capture.accept(data[0]);assert len(capture.runs)==2
    (ROOT/'build/rtos-v0_5-tests/runtime-fixtures.json').write_text(json.dumps([p.hex() for p in packets])+'\n')
    print('PASS independent runtime decoding, padding/length checks, delayed-snapshot ordering and boot isolation; these are host tests, not board measurements')
