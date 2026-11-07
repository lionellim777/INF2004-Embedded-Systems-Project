#!/usr/bin/env python3
"""One local console: provision/build, authenticated broker, USB and MQTT logs.

No remote START. f/l/r/u select over USB; GP20 starts physically; x stops over
USB; m stops over MQTT; b restarts the owned broker; q requests stops/exits.
"""
import argparse
import asyncio
import hashlib
from datetime import datetime, timezone
import json
import logging
from pathlib import Path
import select
import subprocess
import sys
import threading
import time
import warnings
from zoneinfo import ZoneInfo

from rtos_bench import Capture
from rtos_mqtt_observer import decode
from rtos_network_setup import ROOT, PRIVATE, broker_config, prepare

STATES = ['unconfigured','joining Wi-Fi','connecting to broker','subscribing','online','retrying']
ERRORS = ['none','radio initialization','association request','Wi-Fi/DHCP timeout','Wi-Fi link lost',
          'broker address/client allocation','TCP/MQTT connect request','broker login rejected',
          'subscription failed','broker connection timeout','subscription timeout','broker disconnected']
WIFI_LINKS = {-3:'authentication failed',-2:'Wi-Fi name not found',-1:'connection failed',
              0:'down',1:'joining',2:'waiting for IP address',3:'connected with IP address'}


class Telemetry:
    def __init__(self, path):
        self.stream=Path(path).open('x',buffering=1)
        self.lock=threading.Lock()
        self.summary=dict(packets=0,decode_errors=0,valid_motion_packets=0,boot_ids=[],
                          fault_codes=[],broker_restarts_requested=0,mqtt_stops_requested=0,
                          mqtt_stop_acks=0,connections=0,disconnections=0,physical_observations_pending=True)
        self.last_valid_motion_at=None

    def event(self, event_type, **values):
        with self.lock:
            self.stream.write(json.dumps(dict(received_utc=datetime.now(timezone.utc).isoformat(),
                event=event_type,**values))+'\n')

    def packet(self, topic, payload, retained=False):
        try:
            value=decode(topic,payload)
        except ValueError as error:
            with self.lock: self.summary['decode_errors']+=1
            self.event('rejected',topic=topic,error=str(error),payload_hex=payload.hex())
            return
        self.event('packet',topic=topic,payload_hex=payload.hex(),retained=bool(retained),**value)
        message=None
        with self.lock:
            self.summary['packets']+=1
            if value['type']=='motion':
                if value['valid']:
                    self.summary['valid_motion_packets']+=1
                    now=time.monotonic()
                    if self.summary['valid_motion_packets']==1:
                        message='MQTT: valid motion data received from the Pico.'
                    elif self.last_valid_motion_at is not None and now-self.last_valid_motion_at>1:
                        message='MQTT: motion telemetry resumed after the interruption.'
                    self.last_valid_motion_at=now
                if value['fault'] not in self.summary['fault_codes']: self.summary['fault_codes'].append(value['fault'])
                self.summary['last_motion']=value
            elif value['type']=='status':
                if value['boot_id'] not in self.summary['boot_ids']: self.summary['boot_ids'].append(value['boot_id'])
                self.summary['last_status']=value
            elif value['type']=='command_result' and value['accepted'] and value['command']==3:
                self.summary['mqtt_stop_acks']+=1
                message='MQTT: Pico acknowledged STOP. Check USB reports aborted/stop latched.'
        if message: print(message,flush=True)

    def save(self,path):
        with self.lock:
            Path(path).write_text(json.dumps(self.summary,indent=2)+'\n')
            self.stream.close()


class Link:
    def __init__(self,settings,folder,telemetry):
        import paho.mqtt.client as mqtt
        self.settings,self.folder,self.telemetry=settings,Path(folder),telemetry
        self.broker=None
        self.broker_started=False
        self.client=mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.username_pw_set(settings['operator_user'],settings['operator_password'])
        self.client.on_connect=self.connected
        self.client.on_disconnect=self.disconnected
        self.client.on_message=lambda c,u,m:telemetry.packet(m.topic,bytes(m.payload),m.retain)
        self.client.reconnect_delay_set(min_delay=1,max_delay=5)

    def connected(self,client,context,flags,reason,properties):
        del context,flags,properties
        self.telemetry.event('observer_connection',result=str(reason))
        if reason==0:
            with self.telemetry.lock:self.telemetry.summary['connections']+=1
            client.subscribe(self.settings['prefix']+'+',qos=1)
            print('Laptop broker/observer connected. Waiting for the Pico...',flush=True)
        else:print('Broker login failed; rerun setup rather than changing robot wiring.',flush=True)

    def disconnected(self,client,context,flags,reason,properties):
        del client,context,flags,properties
        with self.telemetry.lock:self.telemetry.summary['disconnections']+=1
        self.telemetry.event('observer_disconnect',reason=str(reason))

    async def start_broker(self):
        from amqtt.broker import Broker
        with warnings.catch_warnings():
            warnings.simplefilter('ignore',DeprecationWarning)
            warnings.filterwarnings('ignore',message="The 'acl' option is deprecated.*",category=UserWarning)
            self.broker=Broker(broker_config(self.settings,self.folder))
        await self.broker.start()
        self.broker_started=True

    async def start(self):
        await self.start_broker()
        self.client.connect_async(self.settings['broker_ip'],self.settings['port'],keepalive=15)
        self.client.loop_start()

    def stop(self):
        if not self.client.is_connected():
            raise ValueError('MQTT is disconnected; use x over USB or GP20 on the car')
        result=self.client.publish(self.settings['prefix']+'command/stop',b'',qos=1)
        if result.rc!=0:raise ValueError('MQTT STOP could not be queued; use GP20')
        with self.telemetry.lock:self.telemetry.summary['mqtt_stops_requested']+=1
        self.telemetry.event('mqtt_stop_requested')
        print('MQTT STOP queued. Check the acknowledgement and that drive stays off.',flush=True)

    async def restart(self):
        with self.telemetry.lock:self.telemetry.summary['broker_restarts_requested']+=1
        self.telemetry.event('broker_restart_begin')
        print('Pausing the broker for two seconds. GP20 and USB stop remain available.',flush=True)
        await self.broker.shutdown()
        self.broker_started=False
        await asyncio.sleep(2)
        await self.start_broker()
        self.telemetry.event('broker_restart_complete')
        print('Broker restarted. Waiting for automatic reconnection...',flush=True)

    async def close(self):
        self.client.disconnect()
        await asyncio.to_thread(self.client.loop_stop)
        if self.broker_started:
            await self.broker.shutdown();self.broker_started=False


class USB:
    def __init__(self,capture):
        self.capture=capture;self.device=None;self.pending=bytearray();self.last_network=None

    def poll(self):
        import serial
        from serial.tools import list_ports
        if self.device is None:
            ports=[p.device for p in list_ports.comports() if p.vid==0x2e8a]
            if len(ports)!=1:return
            candidate=None
            try:
                candidate=serial.Serial(ports[0],115200,timeout=0,write_timeout=0.5)
                candidate.dtr=True;self.device=candidate;self.pending.clear()
                print('USB connected. Looking for v0.3 after flashing...',flush=True)
            except (OSError,serial.SerialException):
                if candidate is not None:candidate.close()
                return
        try:
            self.pending.extend(self.device.read(min(self.device.in_waiting,4096)))
            if len(self.pending)>16384:raise ValueError('oversized USB line')
            while b'\n' in self.pending:
                line,_,rest=self.pending.partition(b'\n');self.pending=bytearray(rest)
                text=line.rstrip(b'\r').decode(errors='replace')
                message=self.capture.line(text)
                if message:print(message,flush=True)
                if text.startswith('[RTOS_INFO]'):
                    fields=dict(item.split('=',1) for item in text.split()[1:] if '=' in item)
                    state,error=int(fields.get('network_state',0)),int(fields.get('network_error',0))
                    wifi=int(fields.get('wifi_link',0))
                    key=(fields.get('version'),state,error,wifi)
                    if key!=self.last_network:
                        print(f"Pico v{key[0]}: {STATES[state] if state<len(STATES) else state}; error: {ERRORS[error] if error<len(ERRORS) else error}; Wi-Fi: {WIFI_LINKS.get(wifi,wifi)}",flush=True)
                        self.last_network=key
        except (OSError,serial.SerialException,ValueError):
            self.device.close();self.device=None;self.capture.ready=False
            print('USB disconnected; waiting for the flashed Pico to return.',flush=True)

    def command(self,key):
        if self.device is None:raise ValueError('USB not connected')
        if key!='x' and self.capture.summary.get('firmware_version')!='0.3':
            raise ValueError('Flash the configured v0.3 firmware before selecting a test')
        self.device.write(self.capture.command(key))

    def close(self):
        if self.device is not None:self.device.close()


async def run(settings,image,folder):
    stamp=datetime.now(ZoneInfo('Asia/Singapore')).strftime('%Y%m%d-%H%M%S')
    logs=ROOT/'serial-output'/('rtos-network-'+stamp);logs.mkdir(parents=True)
    capture=Capture(logs/'usb')
    telemetry=Telemetry(logs/'mqtt.jsonl')
    link=Link(settings,folder,telemetry);usb=USB(capture)
    restart=None
    try:
        await link.start()
        print('\nKeep BOTH powered wheels clear of the table. No track is needed.')
        print('Flash this file using BOOTSEL, then reconnect normally:',image)
        print('f=select 30cm | x=USB stop | m=MQTT stop | b=restart broker | q=save/exit')
        print('There is no remote START. GP20 starts and stops physically. Logs:',logs,flush=True)
        if sys.platform=='darwin':subprocess.Popen(['open','-R',str(image)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        while True:
            usb.poll()
            if select.select([sys.stdin],[],[],0)[0]:
                line=sys.stdin.readline()
                if not line:break
                key=line.strip().lower()
                try:
                    if key=='q':break
                    if key=='m':link.stop()
                    elif key=='b':
                        if restart is None or restart.done():restart=asyncio.create_task(link.restart())
                        else:print('Broker restart is already in progress.',flush=True)
                    elif key in ('f','l','r','u','x','?'):usb.command(key)
                    else:print('Use f, x, m, b or q; start with GP20.',flush=True)
                except (ValueError,OSError) as error:print(error,flush=True)
            if restart is not None and restart.done():
                restart.result();restart=None
            await asyncio.sleep(0.03)
    finally:
        try:usb.command('x')
        except (ValueError,OSError):pass
        if link.client.is_connected():
            try:link.stop()
            except ValueError:pass
        if restart is not None:
            try:await restart
            except Exception as error:print('Broker restart failed:',type(error).__name__,flush=True)
        for _ in range(10):
            usb.poll();await asyncio.sleep(0.03)
        usb.close();capture.close()
        await link.close()
        telemetry.save(logs/'mqtt-summary.json')
        print('Saved USB and MQTT logs in:',logs)
        print('Confirm the wheels are undriven before lowering the car.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reuse',action='store_true',help='Use existing private settings and build without prompts')
    args=parser.parse_args()
    logging.basicConfig(level=logging.ERROR)
    try:
        if args.reuse:
            settings=json.loads((PRIVATE/'settings.json').read_text())
            metadata=json.loads((PRIVATE/'firmware-info.json').read_text())
            image=ROOT/'build/rtos-network/robocar_rtos_network_v0_3.uf2'
            if not image.exists():parser.error('Run setup once before --reuse')
            from rtos_network_setup import mac_wifi_ip
            if mac_wifi_ip()!=settings['broker_ip']:parser.error('Mac address changed; rerun without --reuse to rebuild')
            if hashlib.sha256(image.read_bytes()).hexdigest()!=metadata['sha256'] or \
               hashlib.sha256(json.dumps(settings,sort_keys=True).encode()).hexdigest()!=metadata['settings_sha256']:
                parser.error('Saved image/settings changed; rerun without --reuse to rebuild')
        else:settings,image=prepare()
        asyncio.run(run(settings,image,PRIVATE))
    except KeyboardInterrupt:pass
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'Setup stopped: {error}\n')


if __name__=='__main__':main()
