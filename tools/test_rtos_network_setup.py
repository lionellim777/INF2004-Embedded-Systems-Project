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
import tempfile
import warnings

from rtos_network_setup import PREFIX, validate, write_settings, broker_config


async def console_reconnect(settings,folder):
    from rtos_network_session import Link,Telemetry
    with socket.socket() as sock:
        sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    local=dict(settings,broker_ip='127.0.0.1',port=port)
    telemetry=Telemetry(Path(folder)/'console-mqtt.jsonl')
    link=Link(local,folder,telemetry)
    try:
        await link.start()
        for _ in range(100):
            if telemetry.summary['connections']>=1:break
            await asyncio.sleep(.03)
        assert telemetry.summary['connections']==1
        await link.restart()
        for _ in range(200):
            if telemetry.summary['connections']>=2:break
            await asyncio.sleep(.03)
        assert telemetry.summary['connections']>=2
        assert telemetry.summary['broker_restarts_requested']==1
    finally:
        await link.close();telemetry.save(Path(folder)/'console-summary.json')


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
        info=observer.publish(PREFIX+'command/stop',b'',qos=1)
        await asyncio.to_thread(info.wait_for_publish,3)
        for _ in range(100):
            if commands:break
            await asyncio.sleep(.01)
        assert commands==[(PREFIX+'command/stop',b'')]
        # Operator credentials cannot forge robot telemetry.
        observer.publish(PREFIX+'motion',b'forged',qos=0)
        await asyncio.sleep(.2)
        assert received==[(PREFIX+'motion',payload)]
    finally:
        for c in clients:c.disconnect()
        for c in clients:await asyncio.to_thread(c.loop_stop)
        await broker.shutdown()


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
        print('PASS actual operator console: authenticated connection, owned broker restart and automatic observer reconnect')


if __name__=='__main__':main()
