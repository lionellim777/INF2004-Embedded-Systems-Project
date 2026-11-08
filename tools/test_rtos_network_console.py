#!/usr/bin/env python3
"""Check console command gates, packet evidence and hidden setup boundary."""
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import contextlib
import hashlib
import io
import os
from unittest.mock import patch

from rtos_bench import Capture
from rtos_network_session import Telemetry, Link, USB,CommandInput,console_command
from rtos_network_setup import PREFIX


def delays_and_command_input():
    with tempfile.TemporaryDirectory(prefix='rtos-delays-') as name:
        folder=Path(name);t=Telemetry(folder/'mqtt.jsonl');output=io.StringIO()
        payload=bytes.fromhex('00000000000000000000000001')
        with patch('rtos_network_session.time.monotonic',side_effect=[10,12,14,15]),contextlib.redirect_stdout(output):
            t.packet(PREFIX+'motion',payload)
            t.packet(PREFIX+'motion',payload)
            t.packet(PREFIX+'motion',payload)
            t.interruption('broker test')
            t.packet(PREFIX+'motion',payload)
        assert output.getvalue().count('delayed telemetry detected')==1
        assert output.getvalue().count('resumed after the interruption')==1
        assert t.summary['motion_delay_events']==2 and t.summary['reconnections_confirmed']==1
        assert t.summary['max_motion_delay_seconds']==2
        barcode_output=io.StringIO()
        with contextlib.redirect_stdout(barcode_output):
            t.packet(PREFIX+'barcode',bytes([0,0,0,0,33]))
            t.packet(PREFIX+'barcode',bytes([0,0,0,0,33]))
            t.packet(PREFIX+'barcode',bytes([1,0,0,0,33]))
        assert len(t.summary['barcode_reads'])==2
        assert barcode_output.getvalue().count('MQTT barcode: A')==2
        t.save(folder/'summary.json')
        events=[json.loads(l) for l in (folder/'mqtt.jsonl').read_text().splitlines()]
        assert sum(e['event']=='motion_delivery_delay' for e in events)==2
    read_fd,write_fd=os.pipe()
    with os.fdopen(read_fd) as stream:
        try:
            source=CommandInput(stream)
            assert source.readline() is None
            os.write(write_fd,b'bb\nq\n')
            assert console_command(source.readline())=='b'
            assert console_command(source.readline())=='q'
            os.close(write_fd);write_fd=None
            assert source.readline()==''
        finally:
            if write_fd is not None:os.close(write_fd)
    assert console_command(' \n') is None
    for key in ['start','bm','go']:
        try:console_command(key)
        except ValueError:pass
        else:raise AssertionError('Unexpected multi-command or START accepted')
    print('PASS delay/input UI: ordinary delays logged without reconnect spam; real outage resumes once; repeated b and buffered q accepted; no START')


def reuse_address_guard():
    import rtos_network_session as session
    with tempfile.TemporaryDirectory(prefix='rtos-reuse-') as name:
        root=Path(name);private=root/'private';private.mkdir()
        image=root/'build/rtos-network/robocar_rtos_network_v0_3.uf2'
        image.parent.mkdir(parents=True);image.write_bytes(b'fixture image')
        settings={'broker_ip':'192.168.1.9'}
        (private/'settings.json').write_text(json.dumps(settings))
        (private/'firmware-info.json').write_text(json.dumps(dict(
            sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
            settings_sha256=hashlib.sha256(json.dumps(settings,sort_keys=True).encode()).hexdigest())))
        invoked=[]
        async def runner(*args,**kwargs):invoked.append(kwargs)
        with patch.object(session,'ROOT',root),patch.object(session,'PRIVATE',private), \
             patch.object(session,'run',runner),patch('sys.argv',['helper','--reuse']):
            for current,expected in [('', ['Could not detect','retry']),
                                     ('192.168.1.5',['192.168.1.9','192.168.1.5','flash'])]:
                error=io.StringIO()
                with patch('rtos_network_setup.mac_wifi_ip',return_value=current),contextlib.redirect_stderr(error):
                    try:session.main()
                    except SystemExit as result:assert result.code==2
                    else:raise AssertionError('Reuse continued with unavailable or changed IP')
                assert all(word in error.getvalue() for word in expected)
                assert not invoked
            with patch('rtos_network_setup.mac_wifi_ip',return_value=settings['broker_ip']):session.main()
            assert invoked==[{'flash_hint':False}]
    print('PASS reuse guard: missing IP distinguished from genuine change; both addresses and reflash shown; matching image/settings start without flashing')


def main():
    with tempfile.TemporaryDirectory(prefix='rtos-console-') as name:
        folder=Path(name)
        capture=Capture(folder/'usb')
        usb=USB(capture)
        writes=[]
        usb.device=SimpleNamespace(write=lambda p:writes.append(p))
        capture.line('[RTOS_INFO] version=0.2 network_state=0 uptime_ms=1')
        try:usb.command('f')
        except ValueError:pass
        else:raise AssertionError('Network test selected on old firmware')
        usb.command('x');assert writes==[b'x']
        capture.line('[RTOS_INFO] version=0.3 network_state=1 network_error=0 uptime_ms=2')
        usb.command('f');assert writes[-1]==b'f'
        try:usb.command('t')
        except ValueError:pass
        else:raise AssertionError('Track mode accepted on old firmware')
        capture.line('[RTOS_INFO] version=0.4 network_state=1 network_error=0 uptime_ms=3')
        usb.command('t');assert writes[-1]==b't'
        capture.line('[RTOS_INFO] version=0.4.1 network_state=1 network_error=0 uptime_ms=4')
        usb.command('t');assert writes[-1]==b't'
        for key in ['start','go','s']:
            try:usb.command(key)
            except ValueError:pass
            else:raise AssertionError('Unexpected USB command')
        telemetry=Telemetry(folder/'mqtt.jsonl')
        settings=dict(operator_user='fixture',operator_password='fixture-password',prefix=PREFIX)
        link=Link(settings,folder,telemetry)
        sent=[]
        link.client=SimpleNamespace(is_connected=lambda:False)
        try:link.stop()
        except ValueError:pass
        else:raise AssertionError('STOP reported queued while disconnected')
        def publish(topic,payload,qos):
            sent.append((topic,payload,qos));return SimpleNamespace(rc=0)
        link.client=SimpleNamespace(is_connected=lambda:True,publish=publish)
        link.stop()
        assert sent==[(PREFIX+'command/stop',b'',1)]
        telemetry.packet(PREFIX+'motion',bytes.fromhex('00000000000000000000000001'))
        telemetry.packet(PREFIX+'command_result',b'\x07')
        telemetry.packet(PREFIX+'motion',b'bad')
        telemetry.save(folder/'mqtt-summary.json');capture.close()
        r=json.loads((folder/'mqtt-summary.json').read_text())
        assert r['valid_motion_packets']==1 and r['decode_errors']==1
        assert r['mqtt_stops_requested']==1 and r['mqtt_stop_acks']==1
        assert r['physical_observations_pending']
        assert all('fixture-password' not in p.read_text() for p in folder.iterdir())
    print('PASS network console: v0.3/v0.4 selection gates, no START, exact empty MQTT STOP, telemetry/ack logs, no password in evidence')
    reuse_address_guard()
    delays_and_command_input()


if __name__=='__main__':main()
