#!/usr/bin/env python3
"""Verify USB identity gate, decoder/log capture and command restrictions."""
import json
from pathlib import Path
import tempfile

from rtos_bench import Capture


def main():
    with tempfile.TemporaryDirectory(prefix="rtos-bench-") as folder:
        path=Path(folder)/"test"
        c=Capture(path)
        for value in ['f','x','r']:
            try:c.command(value)
            except ValueError:pass
            else:raise AssertionError('Command before firmware identity')
        c.line('[RTOS_INFO] version=0.1 network_state=0 uptime_ms=1')
        assert not c.ready
        c.line('[RTOS_INFO] version=0.2 network_state=0 uptime_ms=2')
        assert c.command('f')==b'f'
        for value in ['start','go','s','GP20','\x00','']:
            try:c.command(value)
            except ValueError:pass
            else:raise AssertionError('Unexpected command accepted')
        c.line('[MOTION_V1] 00000000000000000000000001 gap_us=15000')
        # This valid fixture has stop_latched false, so selection is blocked.
        try:c.command('l')
        except ValueError:pass
        else:raise AssertionError('Selection allowed while outputs armed')
        assert c.command('x')==b'x'
        c.line('[STOP_TRACE] seq=1 reason=1 request_us=100 off_us=104 alarm_gap_us=5000')
        c.line('[MOTION_V1] ff gap_us=20000')
        c.close()
        result=json.loads(path.with_suffix('.json').read_text())
        assert result['decoded_packets']==1 and result['parse_errors']==1
        assert result['stop_traces'][0]['register_write_delay_us']==4
        assert result['commands_requested']==['f','x']
        assert result['max_motion_gap_us']==15000
        try:Capture(path)
        except FileExistsError:pass
        else:raise AssertionError('Existing evidence log overwritten')
    print('PASS USB helper: firmware identity, no start command, active-selection gate, decoded logs, stop trace, no overwrite')


if __name__=='__main__':main()
