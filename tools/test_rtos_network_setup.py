#!/usr/bin/env python3
"""Validate local setup, C escaping, real password authentication and role ACLs."""
import asyncio
import json
import logging
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import warnings

from rtos_network_setup import PREFIX, validate, write_settings, broker_config


async def console_quit_during_restart(settings,folder):
    """Drive the actual input/finally path; only USB is replaced, never the broker."""
    import rtos_network_session as session
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    commands=[]
    class USBFixture:
        def __init__(self,capture):self.capture=capture
        def poll(self):pass
        def command(self,key):commands.append(key)
        def close(self):pass
    original=(session.ROOT,session.USB,sys.stdin)
    read_fd,write_fd=os.pipe();reader=os.fdopen(read_fd)
    task=None
    try:
        session.ROOT=Path(folder)/'console-quit'
        session.USB=USBFixture;sys.stdin=reader
        task=asyncio.create_task(session.run(dict(settings,broker_ip='127.0.0.1',port=port),
            Path(folder)/'fixture.uf2',folder,flash_hint=False))
        # Wait for the real worker readiness before driving b then q.
        for _ in range(100):
            if list(session.ROOT.glob('serial-output/*/broker.log')):
                await asyncio.sleep(.3);break
            await asyncio.sleep(.03)
        os.write(write_fd,b'b\n');await asyncio.sleep(.15)
        os.write(write_fd,b'q\n')
        await asyncio.wait_for(task,5)
        logs=list(session.ROOT.glob('serial-output/*'));assert len(logs)==1
        for name in ['usb.log','usb.json','mqtt.jsonl','mqtt-summary.json']:
            assert (logs[0]/name).exists(),name
        events=[json.loads(l)['event'] for l in (logs[0]/'mqtt.jsonl').read_text().splitlines()]
        assert 'broker_restart_begin' in events and 'broker_restart_cancelled' in events
        assert commands==['x'],'Quit did not request exactly one USB stop'
        with socket.socket() as probe:
            assert probe.connect_ex(('127.0.0.1',port))!=0,'Quit left a broker listening'
    finally:
        if task is not None and not task.done():
            task.cancel()
            try:await task
            except asyncio.CancelledError:pass
        session.ROOT,session.USB,sys.stdin=original
        reader.close();os.close(write_fd)


async def console_reconnect(settings,folder):
    from rtos_network_session import Link,Telemetry
    import paho.mqtt.client as mqtt
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    local=dict(settings,broker_ip='127.0.0.1',port=port)
    telemetry=Telemetry(Path(folder)/'console-mqtt.jsonl')
    link=Link(local,folder,telemetry)
    robot=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id='fixture-robot')
    robot.username_pw_set(settings['robot_user'],settings['robot_password'])
    robot.will_set(PREFIX+'link',b'\x01',qos=1,retain=True)
    robot.on_connect=lambda client,*args:client.subscribe(PREFIX+'command/#',qos=1)
    idle_writer=None;pump=None
    async def traffic():
        while True:
            if robot.is_connected():robot.publish(PREFIX+'motion',bytes.fromhex('00000000000000000000000001'))
            await asyncio.sleep(.03)
    try:
        await link.start()
        for _ in range(100):
            if telemetry.summary['connections']>=1:break
            await asyncio.sleep(.03)
        assert telemetry.summary['connections']==1
        robot.connect_async('127.0.0.1',port,keepalive=15);robot.loop_start()
        pump=asyncio.create_task(traffic())
        for _ in range(150):
            if telemetry.summary['valid_motion_packets']:break
            await asyncio.sleep(.03)
        assert telemetry.summary['valid_motion_packets']
        # An accepted TCP peer that never sends MQTT CONNECT was absent from
        # the old test. It can keep Server.wait_closed() blocked indefinitely.
        idle_reader,idle_writer=await asyncio.open_connection('127.0.0.1',port)
        await asyncio.sleep(.05)
        first_worker=link.broker
        await asyncio.wait_for(link.restart(),8)
        assert first_worker.returncode is not None
        assert await asyncio.wait_for(idle_reader.read(1),2)==b''
        for _ in range(200):
            if telemetry.summary['connections']>=2 and robot.is_connected():break
            await asyncio.sleep(.03)
        assert telemetry.summary['connections']>=2
        assert robot.is_connected()
        assert telemetry.summary['broker_restarts_requested']==1
        # A second outage must also recover real binary traffic automatically.
        count=telemetry.summary['valid_motion_packets']
        await asyncio.wait_for(link.restart(),8)
        for _ in range(200):
            if telemetry.summary['valid_motion_packets']>count:break
            await asyncio.sleep(.03)
        assert telemetry.summary['valid_motion_packets']>count
        assert telemetry.summary['broker_restarts_requested']==2
        # Quitting during the pause must cancel promptly, not await shutdown.
        restart=asyncio.create_task(link.restart())
        await asyncio.sleep(.1);restart.cancel()
        try:await asyncio.wait_for(restart,2)
        except asyncio.CancelledError:pass
        await link.start_broker()
        orphan=link.broker
        orphan.stdin.close()
        await asyncio.wait_for(orphan.wait(),3)
        assert orphan.returncode==0,'Worker remained alive after parent stdin closed'
    finally:
        if pump is not None:
            pump.cancel()
            try:await pump
            except asyncio.CancelledError:pass
        if idle_writer is not None:
            idle_writer.close();await idle_writer.wait_closed()
        robot.disconnect();await asyncio.to_thread(robot.loop_stop)
        await asyncio.wait_for(link.close(),6)
        assert link.broker is None
        telemetry.save(Path(folder)/'console-summary.json')


async def authenticated_roundtrip(settings, folder):
    from amqtt.broker import Broker
    import paho.mqtt.client as mqtt
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    settings=dict(settings,broker_ip='127.0.0.1',port=port)
    with warnings.catch_warnings():
        warnings.simplefilter('ignore',DeprecationWarning)
        warnings.filterwarnings('ignore',message="The 'acl' option is deprecated.*",category=UserWarning)
        broker=Broker(broker_config(settings,folder))
    await broker.start()
    loop=asyncio.get_running_loop()
    clients=[];received=[];commands=[]

    async def connect(role=None, wrong=False):
        client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,reconnect_on_failure=False)
        clients.append(client)
        complete=asyncio.Event();outcome=[];subscribed=asyncio.Event()
        if role:client.username_pw_set(settings[role+'_user'],'wrong' if wrong else settings[role+'_password'])
        def connected(c,u,f,reason,properties):
            del u,f,properties
            outcome.append(not reason.is_failure)
            if not reason.is_failure:
                c.subscribe(PREFIX+('+' if role=='operator' else 'command/#'),qos=1)
            loop.call_soon_threadsafe(complete.set)
        client.on_connect=connected
        # amqtt closes a rejected authentication without sending CONNACK.
        def disconnected(*args):
            if not outcome:
                outcome.append(False);loop.call_soon_threadsafe(complete.set)
        client.on_disconnect=disconnected
        client.on_connect_fail=disconnected
        client.on_subscribe=lambda *args:loop.call_soon_threadsafe(subscribed.set)
        client.on_message=lambda c,u,m:(received if role=='operator' else commands).append((m.topic,bytes(m.payload)))
        client.connect_async('127.0.0.1',port);client.loop_start()
        await asyncio.wait_for(complete.wait(),5)
        if outcome[0]:await asyncio.wait_for(subscribed.wait(),5)
        return client,outcome[0]
    try:
        _,allowed=await connect();assert not allowed,'Anonymous broker login accepted'
        _,allowed=await connect('operator',wrong=True);assert not allowed,'Bad password accepted'
        observer,allowed=await connect('operator');assert allowed
        robot,allowed=await connect('robot');assert allowed
        payload=bytes.fromhex('00000000000000000000000001')
        info=robot.publish(PREFIX+'motion',payload,qos=1)
        await asyncio.to_thread(info.wait_for_publish,3)
        for _ in range(100):
            if received:break
            await asyncio.sleep(.01)
        assert received==[(PREFIX+'motion',payload)]
        # The real role ACL must permit the new binary diagnostic topic.
        diagnostic=bytes.fromhex('05010000') # run 1, no attempts/snapshot
        info=robot.publish(PREFIX+'scan_diag',diagnostic,qos=1)
        await asyncio.to_thread(info.wait_for_publish,3)
        for _ in range(100):
            if len(received)>=2:break
            await asyncio.sleep(.01)
        assert received==[(PREFIX+'motion',payload),(PREFIX+'scan_diag',diagnostic)]
        runtime_fixture=bytes.fromhex(json.loads((Path(__file__).resolve().parents[1]/'build/rtos-v0_5-tests/runtime-fixtures.json').read_text())[1])
        info=robot.publish(PREFIX+'runtime_diag',runtime_fixture,qos=1)
        await asyncio.to_thread(info.wait_for_publish,3)
        for _ in range(100):
            if len(received)>=3:break
            await asyncio.sleep(.01)
        assert received==[(PREFIX+'motion',payload),(PREFIX+'scan_diag',diagnostic),(PREFIX+'runtime_diag',runtime_fixture)]
        info=observer.publish(PREFIX+'command/stop',b'',qos=1)
        await asyncio.to_thread(info.wait_for_publish,3)
        for _ in range(100):
            if commands:break
            await asyncio.sleep(.01)
        assert commands==[(PREFIX+'command/stop',b'')]
        # Operator credentials cannot forge robot telemetry.
        observer.publish(PREFIX+'motion',b'forged',qos=0)
        await asyncio.sleep(.2)
        assert received==[(PREFIX+'motion',payload),(PREFIX+'scan_diag',diagnostic),(PREFIX+'runtime_diag',runtime_fixture)]
    finally:
        for c in clients:c.disconnect()
        for c in clients:await asyncio.to_thread(c.loop_stop)
        await broker.shutdown()


async def console_quit_drains_scan(settings,folder):
    """Quit after stop must retain a download arriving after the STOP ACK."""
    import rtos_network_session as session
    import paho.mqtt.client as mqtt
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    local=dict(settings,broker_ip='127.0.0.1',port=port)
    fixture=json.loads((Path(__file__).resolve().parents[1]/'build/rtos-scan-fixtures.json').read_text())['AAZ']
    robot=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,client_id='diagnostic-fixture')
    robot.username_pw_set(settings['robot_user'],settings['robot_password'])
    robot.on_connect=lambda c,*args:c.subscribe(PREFIX+'command/#',qos=1)
    robot.on_message=lambda c,u,m:c.publish(PREFIX+'command_result',b'\x07',qos=1) if m.topic.endswith('/stop') else None
    class USBFixture:
        def __init__(self,capture):pass
        def poll(self):pass
        def command(self,key):assert key=='x'
        def close(self):pass
    original=(session.ROOT,session.USB,sys.stdin)
    read_fd,write_fd=os.pipe();reader=os.fdopen(read_fd);task=None;started=False
    try:
        session.ROOT=Path(folder)/'console-drain';session.USB=USBFixture;sys.stdin=reader
        task=asyncio.create_task(session.run(local,Path(folder)/'fixture.uf2',folder,flash_hint=False))
        for _ in range(100):
            if list(session.ROOT.glob('serial-output/*/broker.log')):break
            await asyncio.sleep(.03)
        robot.connect_async('127.0.0.1',port);robot.loop_start();started=True
        for _ in range(200):
            if robot.is_connected():break
            await asyncio.sleep(.03)
        assert robot.is_connected()
        await asyncio.sleep(.2)
        async def send(payload,topic='scan_diag'):
            info=robot.publish(PREFIX+topic,payload,qos=1)
            await asyncio.to_thread(info.wait_for_publish,3)
        await send(bytes(4)+(123).to_bytes(4,'little')+bytes([7,16]),'status')
        await send(bytes.fromhex(fixture[0]))
        await asyncio.sleep(.1);os.write(write_fd,b'q\n')
        # This delay exceeds the ordinary final USB polling window.
        await asyncio.sleep(1)
        for packet in fixture[1:]:await send(bytes.fromhex(packet))
        await asyncio.wait_for(task,8)
        logs=list(session.ROOT.glob('serial-output/*'));assert len(logs)==1
        summary=json.loads((logs[0]/'mqtt-summary.json').read_text())
        assert summary['mqtt_stop_acks']==1 and summary['decode_errors']==0
        assert summary['scan_runs']['123-1']['complete'] and summary['scan_runs']['123-1']['checksum_verified']
        assert (logs[0]/'scan-123-1-waveform.txt').exists()
    finally:
        robot.disconnect()
        if started:await asyncio.to_thread(robot.loop_stop)
        if task is not None and not task.done():
            task.cancel()
            try:await task
            except asyncio.CancelledError:pass
        session.ROOT,session.USB,sys.stdin=original;reader.close();os.close(write_fd)


def main():
    logging.basicConfig(level=logging.CRITICAL)
    settings=dict(broker_ip='192.168.1.9',port=1883,ssid='test "WiFi" \\ 网络',wifi_password='fixture\\"password',
                  prefix=PREFIX,robot_user='robot',robot_password='fixture-robot-password',
                  operator_user='observer',operator_password='fixture-observer-password')
    validate(settings)
    for changes in [dict(broker_ip='8.8.8.8'),dict(broker_ip='127.0.0.1'),dict(port=0),
                    dict(ssid='x'*33),dict(ssid='x\0'),dict(wifi_password='short')]:
        try:validate(dict(settings,**changes))
        except ValueError:pass
        else:raise AssertionError(changes)
    with tempfile.TemporaryDirectory(prefix='rtos-provision-') as name:
        folder=Path(name)/'private'
        write_settings(settings,folder)
        assert folder.stat().st_mode&0o777==0o700
        for p in folder.iterdir():assert p.stat().st_mode&0o777==0o600
        assert json.loads((folder/'settings.json').read_text())==settings
        probe=folder/'probe.c'
        probe.write_text('#include <stdio.h>\n#include "network_settings.h"\n'
                         'int main(void){puts(RTOS_WIFI_SSID);puts(RTOS_WIFI_PASSWORD);return 0;}\n')
        subprocess.run([shutil.which('cc'),str(probe),'-o',str(folder/'probe')],check=True)
        actual=subprocess.check_output([str(folder/'probe')]).decode().splitlines()
        assert actual==[settings['ssid'],settings['wifi_password']]
        print('PASS local setup: private file modes, validated settings, UTF-8/quote/backslash C round trip')
        asyncio.run(authenticated_roundtrip(settings,folder))
        print('PASS real broker: anonymous/wrong-password denied, binary telemetry and empty STOP delivered, telemetry spoof blocked')
        asyncio.run(console_reconnect(settings,folder))
        print('PASS actual operator console: two bounded restarts with live binary traffic, LWT and idle TCP peer; robot/observer reconnect; cancellation, cleanup and parent-EOF exit')
        asyncio.run(console_quit_during_restart(settings,folder))
        print('PASS actual console b/q input: pending restart cancelled, USB STOP requested, four capture files saved, no broker left listening')
        asyncio.run(console_quit_drains_scan(settings,folder))
        print('PASS actual console q download: stopped AAZ export delivered after STOP ACK, waited for all chunks, checksum verified and waveform saved')


if __name__=='__main__':main()
