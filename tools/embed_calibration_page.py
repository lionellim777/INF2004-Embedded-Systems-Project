#!/usr/bin/env python3
"""Generate flash-resident pages; no external dependencies."""
from pathlib import Path
import json
folder=Path(__file__).resolve().parents[1]/'firmware/app/robot_firmware/wireless'
for name in ['calibration','stationary']:
    text=(folder/(name+'.html')).read_text()
    (folder/(name+'_page.h')).write_text(
        '/* Generated; edit '+name+'.html. */\n#ifndef CALIBRATION_PAGE_H\n#define CALIBRATION_PAGE_H\n'
        'static const char calibration_page[] =\n'+
        '\n'.join(json.dumps(line,ensure_ascii=True) for line in text.splitlines(keepends=True))+';\n#endif\n')
