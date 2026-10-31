#!/usr/bin/env python3
"""Run the barcode, course and motion host suites with gcc (e.g. Windows/MinGW).

Same sources and scenario counts as the clang/macOS scripts, without
sanitizers. Usage: python tools/test_barcode_host_gcc.py .
"""
from pathlib import Path
import re, subprocess, sys, tempfile

root = Path(sys.argv[1]).resolve()
app = root / 'firmware/app/robot_firmware'
tests = root / 'firmware/tests'
out = Path(tempfile.mkdtemp(prefix='b3-'))
CC = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
      '-Wno-error=misleading-indentation', '-I' + str(app)]
BC = ['barcode_acquisition.c', 'barcode_reader.c', 'barcode_decoder.c']


def stubs(script):
    text = (root / 'tools' / script).read_text()
    mock = re.search(r"'''(.*?)'''", text, re.S).group(1)
    d = out / script.replace('.py', '')
    for sub in ['hardware', 'pico']:
        (d / sub).mkdir(parents=True, exist_ok=True)
    (d / 'mock.h').write_text(mock)
    for name in ['hardware/adc.h', 'hardware/sync.h', 'hardware/clocks.h',
                 'hardware/pwm.h', 'pico/stdlib.h']:
        (d / name).write_text('#include "mock.h"\n')
    return d


def run(name, files, extra=(), scenarios=0):
    exe = out / (name + '.exe')
    cmd = CC + list(extra) + [str(f) for f in files] + ['-lm', '-o', str(exe)]
    b = subprocess.run(cmd, capture_output=True, text=True)
    if b.returncode:
        print(f'{name}: BUILD FAIL\n{b.stderr[:1500]}')
        return False
    args = [[str(i)] for i in range(scenarios)] if scenarios else [[]]
    for a in args:
        r = subprocess.run([str(exe)] + a, capture_output=True, text=True)
        if r.returncode:
            print(f'FAIL(exit {r.returncode}) {name} {a}')
            print((r.stdout + r.stderr)[-1500:])
            return False
    last = (r.stdout.strip().splitlines() or [''])[-1]
    print(f'PASS {name} ({len(args)} run(s)): {last[:150]}')
    return True


ok = True
for t in ['independent_barcode', 'barcode_motion', 'barcode_signal',
          'continuous_scan', 'barcode_decoder_bias', 'barcode_acquisition']:
    ok &= run(t, [tests / f'{t}_test.c'] + [app / s for s in BC])
for t in ['barcode_optical', 'barcode_drive', 'line_check', 'center_launch']:
    ok &= run(t, [tests / f'{t}_test.c'])
for t in ['center_course', 'center_recovery', 'motion_control_host']:
    ok &= run(t, [tests / f'{t}_test.c', app / 'motion_control.c'])
ok &= run('course_control', [tests / 'course_control_test.c',
                             app / 'course_control.c', app / 'motion_control.c'])
ok &= run('run_diagnostics', [tests / 'run_diagnostics_test.c',
                              app / 'run_diagnostics.c'])
d = tests / 'barcode_decoder_test'
ok &= run('regression', [d / 'barcode_decoder_test.c', app / 'barcode_decoder.c',
                         app / 'barcode_reader.c', app / 'navigation_commands.c'])
ok &= run('packed_storage', [d / 'packed_storage_test.c',
                             app / 'barcode_decoder.c', app / 'barcode_reader.c'])
extra_tests = sys.argv[2:]
for t in extra_tests:
    ok &= run(Path(t).stem, [Path(t)] + [app / s for s in BC] +
              [app / 'navigation_commands.c'])
s = stubs('test_stationary_barcode.py')
ok &= run('stationary_main', [tests / 'stationary_main_test.c'] +
          [app / f for f in BC], ['-DROBOCAR_WIRELESS_CALIBRATION=1', '-I' + str(s)], 18)
s = stubs('test_course_main.py')
srcs = ['course_control.c', 'motion_control.c', 'barcode_acquisition.c',
        'barcode_reader.c', 'barcode_decoder.c', 'run_diagnostics.c',
        'navigation_commands.c']
for center in (0, 1):
    ok &= run(f'course_main_center{center}', [tests / 'course_main_test.c'] +
              [app / f for f in srcs],
              ['-DROBOCAR_WIRELESS_CALIBRATION=1',
               f'-DROBOCAR_CENTER_LINE={center}', '-I' + str(s)],
              34 if center else 18)
print('ALL PASS' if ok else 'SOME FAILED')
