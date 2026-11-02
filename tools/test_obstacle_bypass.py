#!/usr/bin/env python3
"""Host controller simulations with sanitizers; no hardware validation claim."""
from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1];app=root/'firmware/app/robot_firmware'
build=root/'build/obstacle-host-tests';build.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
exe=build/'obstacle_bypass_test'
subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-DROBOCAR_CENTER_LINE=1','-fsanitize=address,undefined','-I'+str(app),str(root/'firmware/tests/obstacle_bypass_test.c')]+[str(app/s) for s in ['obstacle_bypass.c','motion_control.c','course_control.c']]+['-lm','-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
