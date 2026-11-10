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
    sources=[APP/n for n in ['board.c','safety_gate.c','motion_record.c','motion_runtime.c','track_sensor.c','track_runtime.c','scan_diagnostic.c']]
    sources += [BASE/n for n in ['motion_control.c','course_control.c','barcode_waveform.c','barcode_decoder.c']]
    command=[shutil.which('clang'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-DRTOS_TRACK=1','-DBARCODE_WAVE_RECOVERY_OPTICS=1','-DBARCODE_WAVE_LAST_PASS=1','-DROBOCAR_CENTER_LINE=1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
    if os.environ.get('RTOS_TEST_COMPACT_ADC')=='1':command.append('-DRTOS_COMPACT_ADC_STATE=1')
    if os.environ.get('RTOS_TEST_COMPACT_CORE')=='1':command+=['-DRTOS_COMPACT_CORE_STATE=1','-DCENTER_BASELINE_COMPACT=1']
    if os.environ.get('RTOS_TEST_COMPACT_CONTROL')=='1':command+=['-DROBOCAR_COMPACT_CONTROL_STATE=1']
    if os.environ.get('RTOS_TEST_FIRST_FAILURE_CACHE')=='0':command.append('-DRTOS_NO_FIRST_FAILURE_CACHE=1')
    command += ['-I'+str(p) for p in [APP,BASE,TEST,folder]]
    command += [str(p) for p in sources]+[str(TEST/'track_integration_test.c'),'-o',str(folder/'test')]
    subprocess.run(command,check=True)
    output=subprocess.check_output([str(folder/'test')],text=True)
    print('\n'.join(l for l in output.splitlines() if not l.startswith('SCAN_FIXTURE ')))
    from rtos_scan_capture import ScanCapture,decode_scan
    captures={};packets={}
    for line in output.splitlines():
        if line.startswith('SCAN_FIXTURE '):
            _,label,packet=line.split();packets.setdefault(label,[]).append(packet)
            capture=captures.setdefault(label,ScanCapture(ROOT/'build'/('rtos-scan-host-'+label)))
            capture.boot_id(777);capture.accept(decode_scan(bytes.fromhex(packet)))
    for label,capture in captures.items():
        run=capture.runs[capture.latest];assert run['complete'] and run['checksum_verified'],label
        reasons=[v['reason'] for v in run['attempts'].values()]
        if label=='AAZ':assert [v['letter'] for v in run['attempts'].values() if v['reason']=='ACCEPTED']==['A','A','Z']
        elif label=='events-wrap':assert [v['letter'] for v in run['attempts'].values() if v['reason']=='ACCEPTED']==['A','Z']*6
        elif label=='stopped-partial':assert 'STOPPED_PARTIAL' in reasons
        elif label=='first-failure':
            if os.environ.get('RTOS_TEST_FIRST_FAILURE_CACHE')=='0':
                assert run['snapshot']['reason']=='ACCEPTED' and run['snapshot']['letter']=='Z'
                assert run['attempts']['0']['reason']=='NO_VALID_FRAMED_WINDOW'
            else:
                assert run['snapshot']['reason']=='NO_VALID_FRAMED_WINDOW'
                assert run['snapshot']['start_us']==run['attempts']['0']['start_us']
            assert any(a['letter']=='Z' for a in run['attempts'].values())
        else:assert len(run['words'])==16384 and len(run['chunks'])==256
        capture.save()
    assert len(captures)==5
    (ROOT/'build/rtos-scan-fixtures.json').write_text(json.dumps(packets)+'\n')
    # Missing, duplicate, reordered and damaged chunks are not silently accepted.
    fixture=[decode_scan(bytes.fromhex(h)) for h in packets['AAZ']]
    missing=ScanCapture(folder/'missing');missing.boot_id(778)
    removed=False
    for packet in fixture:
        if packet['record']=='words' and not removed:removed=True;continue
        missing.accept(packet)
    assert not missing.runs[missing.latest]['complete']
    damaged=ScanCapture(folder/'damaged');damaged.boot_id(779)
    for packet in fixture:
        packet=dict(packet)
        if packet['record']=='words' and packet['index']==0:
            packet['words']=packet['words'][:];packet['words'][0]^=1
        damaged.accept(packet)
    assert not damaged.runs[damaged.latest]['complete']
    reordered=ScanCapture(folder/'reordered');reordered.boot_id(780)
    head=[p for p in fixture if p['record'] not in ('words','done')]
    chunks=[p for p in fixture if p['record']=='words'];done=next(p for p in fixture if p['record']=='done')
    for packet in head+list(reversed(chunks))+chunks[:1]+[done]:reordered.accept(packet)
    assert reordered.runs[reordered.latest]['complete']
    for h in packets['AAZ']:
        b=bytes.fromhex(h)
        for bad in [b[:-1],b+b'\0']:
            try:decode_scan(bad)
            except ValueError:pass
            else:raise AssertionError('Truncated or oversized diagnostic accepted')
    print('PASS waveform downloads: missing/corrupt chunks rejected; reordered/duplicate chunks verified; truncation and excess bytes rejected')
    print('PASS actual stopped diagnostic exports: independent decode, all profiles/chunks, checksum, AAZ, stopped-partial and run isolation')
    spec=importlib.util.spec_from_file_location('decode_motion',ROOT/'tools/decode_motion_record.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    fixture=next(bytes.fromhex(l.split()[1]) for l in output.splitlines() if l.startswith('TRACK_FIXTURE '))
    decoded=module.decode(fixture)
    assert decoded['kind']==7 and decoded['left_counts']>32767 and decoded['right_counts']>32767
    (ROOT/'build/rtos-track-tests.json').write_text(json.dumps(dict(passed=True,full_half_A_and_recovery_Z=True,adc_and_queue_safety=True,extended_motion_fixture=decoded,physical_tested=False),indent=2)+'\n')
    print('PASS independent Python track-count decode; these are host simulations, not physical course acceptance')
