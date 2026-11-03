#!/usr/bin/env python3
"""Sanitized centred-line bend continuation and existing steering regressions."""
from pathlib import Path
import os
import subprocess
root = Path(__file__).resolve().parents[1]
app = root / 'firmware/app/robot_firmware'
build = root / 'build/center-bend-host-tests'
build.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
for name in ['center_bend_test', 'center_course_test']:
    exe = build / name
    subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I'+str(app),
                    str(root / 'firmware/tests' / (name+'.c')),
                    str(app / 'motion_control.c'), '-lm', '-o', str(exe)], check=True, env=env)
    subprocess.run([str(exe)], check=True, env=env)
