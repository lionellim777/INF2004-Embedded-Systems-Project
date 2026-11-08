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
import os
from pathlib import Path
import select
import subprocess
import sys
import threading
import time
from zoneinfo import ZoneInfo

from rtos_bench import Capture,PHASES
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
                          mqtt_stop_acks=0,connections=0,disconnections=0,physical_observations_pending=True,
                          motion_delay_events=0,max_motion_delay_seconds=0,reconnections_confirmed=0)
        self.last_valid_motion_at=None
        self.last_source_ms=None
        self.recovery_pending=False

    def interruption(self,reason):
        with self.lock:self.recovery_pending=True
        self.event('telemetry_interruption',reason=reason)

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
        message=None;delay=None;controller_message=None
        with self.lock:
            self.summary['packets']+=1
            if value['type']=='motion':
                previous=self.summary.get('last_motion')
                if previous is None or any(previous[k]!=value[k] for k in ['phase','fault','stop_latched']):
                    controller_message=f"MQTT controller: {PHASES[value['phase']]} | stop latched: {value['stop_latched']} | fault code: {value['fault']}"
                if value['valid']:
                    self.summary['valid_motion_packets']+=1
                    now=time.monotonic()
                    if self.summary['valid_motion_packets']==1:
                        message='MQTT: valid motion data received from the Pico.'
                    elif self.recovery_pending:
                        self.summary['reconnections_confirmed']+=1
                        message='MQTT: motion telemetry resumed after the interruption.'
                    elif self.last_valid_motion_at is not None and now-self.last_valid_motion_at>1:
                        gap=now-self.last_valid_motion_at
                        self.summary['motion_delay_events']+=1
                        self.summary['max_motion_delay_seconds']=max(self.summary['max_motion_delay_seconds'],gap)
                        delay=dict(seconds=round(gap,6),source_interval_ms=(value['captured_ms']-self.last_source_ms)&0xffffffff)
                        if self.summary['motion_delay_events']==1:
                            message='MQTT: delayed telemetry detected; timing details are being logged.'
                    self.recovery_pending=False
                    self.last_valid_motion_at=now
                    self.last_source_ms=value['captured_ms']
                if value['fault'] not in self.summary['fault_codes']: self.summary['fault_codes'].append(value['fault'])
                self.summary['last_motion']=value
            elif value['type']=='status':
                if value['boot_id'] not in self.summary['boot_ids']: self.summary['boot_ids'].append(value['boot_id'])
                self.summary['last_status']=value
            elif value['type']=='line':self.summary['last_line']=value
            elif value['type']=='barcode' and value['valid']:
                previous=self.summary.get('last_barcode')
                if previous!=value:
                    self.summary.setdefault('barcode_reads',[]).append(value)
                    label=value['barcode'] or 'unreadable'
                    message=f"MQTT barcode: {label} at {value['captured_ms']/1000:.1f}s"+(' [SIMULATED]' if value['simulated'] else '')
                self.summary['last_barcode']=value
            elif value['type']=='link' and value['state']=='offline':
                self.recovery_pending=True
            elif value['type']=='command_result' and value['accepted'] and value['command']==3:
                self.summary['mqtt_stop_acks']+=1
                message='MQTT: Pico acknowledged STOP. Check USB reports aborted/stop latched.'
        if delay is not None:self.event('motion_delivery_delay',**delay)
        if message: print(message,flush=True)
        if controller_message:print(controller_message,flush=True)

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
        self.telemetry.interruption('observer disconnected')

    async def start_broker(self):
        log=Path(self.telemetry.stream.name).parent/'broker.log'
        with log.open('a') as errors:
            self.broker=await asyncio.create_subprocess_exec(sys.executable,
                str(Path(__file__).with_name('rtos_broker_worker.py')),stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,stderr=errors)
        try:
            self.broker.stdin.write((json.dumps(broker_config(self.settings,self.folder))+'\n').encode())
            await self.broker.stdin.drain()
            ready=await asyncio.wait_for(self.broker.stdout.readline(),5)
            if ready!=b'BROKER_READY\n':
                raise RuntimeError(f'Broker did not start; details: {log}')
            self.broker_started=True
            self.telemetry.event('broker_process_started',pid=self.broker.pid)
        except BaseException:
            await self.stop_broker()
            raise

    async def stop_broker(self):
        process=self.broker
        if process is not None and process.returncode is None:
            try:process.terminate()
            except ProcessLookupError:pass
            try:await asyncio.wait_for(process.wait(),3)
            except asyncio.TimeoutError:
                try:process.kill()
                except ProcessLookupError:pass
                await asyncio.wait_for(process.wait(),2)
        if process is not None and process.stdin is not None:process.stdin.close()
        self.broker=None
        self.broker_started=False

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
        self.telemetry.interruption('requested broker restart')
        print('Pausing the broker for two seconds. GP20 and USB stop remain available.',flush=True)
        try:
            await self.stop_broker()
            await asyncio.sleep(2)
            await self.start_broker()
            self.telemetry.event('broker_restart_complete')
            print('Broker restarted. Waiting for automatic reconnection...',flush=True)
        except asyncio.CancelledError:
            self.telemetry.event('broker_restart_cancelled')
            raise

    async def close(self):
        self.client.disconnect()
        await asyncio.to_thread(self.client.loop_stop)
        await self.stop_broker()


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
                print('USB connected. Waiting for RTOS identity...',flush=True)
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
        if key!='x' and self.capture.summary.get('firmware_version') not in ('0.3','0.4'):
            raise ValueError('Flash the configured RTOS firmware before selecting a test')
        self.device.write(self.capture.command(key))

    def close(self):
        if self.device is not None:self.device.close()


class CommandInput:
    """Read complete lines without TextIO read-ahead hiding queued commands."""
    def __init__(self,stream):
        self.fd=stream.fileno();self.pending=bytearray();self.eof=False

    def readline(self):
        if b'\n' not in self.pending and not self.eof:
            if not select.select([self.fd],[],[],0)[0]:return None
            chunk=os.read(self.fd,4096)
            if chunk:self.pending.extend(chunk)
            else:self.eof=True
        if b'\n' in self.pending:
            line,_,rest=self.pending.partition(b'\n');self.pending=bytearray(rest)
            return line.decode(errors='replace')+'\n'
        if self.eof:
            line=self.pending.decode(errors='replace');self.pending.clear();return line
        return None


def console_command(line):
    key=line.strip().lower()
    if not key:return None
    # Repeating a single key before Enter still requests only one action.
    if len(set(key))==1:key=key[0]
    if key not in ('f','l','r','u','t','x','m','b','q','?'):
        raise ValueError('Enter one command letter, then press Enter: f, x, m, b or q.')
    return key


async def run(settings,image,folder,flash_hint=True):
    stamp=datetime.now(ZoneInfo('Asia/Singapore')).strftime('%Y%m%d-%H%M%S')
    logs=ROOT/'serial-output'/('rtos-network-'+stamp);logs.mkdir(parents=True)
    capture=Capture(logs/'usb')
    telemetry=Telemetry(logs/'mqtt.jsonl')
    link=Link(settings,folder,telemetry);usb=USB(capture)
    restart=None
    commands=CommandInput(sys.stdin)
    try:
        await link.start()
        print('\nKeep the car stopped. Raise both powered wheels for bench tests; use the prepared track for t.')
        if flash_hint:print('Flash this file using BOOTSEL, then reconnect normally:',image)
        else:print('Reusing the configured RTOS image. No flashing requested; waiting for the Pico.')
        print('t=track scan (v0.4 default) | f=30cm bench | x=USB stop | m=MQTT stop | b=restart broker | q=save/exit')
        print('Type a command letter, then press Enter.')
        print('There is no remote START. GP20 starts and stops physically. Logs:',logs,flush=True)
        if flash_hint and sys.platform=='darwin':subprocess.Popen(['open','-R',str(image)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        while True:
            usb.poll()
            line=commands.readline()
            if line is not None:
                if not line:break
                try:
                    key=console_command(line)
                    if key is None:continue
                    telemetry.event('console_command',key=key)
                    if key=='q':break
                    if key=='m':link.stop()
                    elif key=='b':
                        if restart is None or restart.done():restart=asyncio.create_task(link.restart())
                        else:print('Broker restart is already in progress.',flush=True)
                    elif key in ('f','l','r','u','t','x','?'):usb.command(key)
                    else:print('Use f, x, m, b or q; start with GP20.',flush=True)
                except (ValueError,OSError) as error:print(error,flush=True)
            if restart is not None and restart.done():
                try:restart.result()
                except Exception as error:print('Broker restart failed:',error,'; USB/GP20 stop remain available.',flush=True)
                restart=None
            await asyncio.sleep(0.03)
    finally:
        try:usb.command('x')
        except (ValueError,OSError):pass
        if link.client.is_connected():
            try:
                before=telemetry.summary['mqtt_stop_acks'];link.stop()
                until=time.monotonic()+5
                while telemetry.summary['mqtt_stop_acks']==before and time.monotonic()<until:
                    usb.poll();await asyncio.sleep(.03)
                if telemetry.summary['mqtt_stop_acks']==before:
                    print('MQTT STOP was not acknowledged. Use GP20 and confirm drive is off.',flush=True)
            except ValueError:pass
        if restart is not None:
            if not restart.done():restart.cancel()
            try:await restart
            except asyncio.CancelledError:pass
            except Exception as error:print('Broker restart failed:',type(error).__name__,flush=True)
        for _ in range(10):
            usb.poll();await asyncio.sleep(0.03)
        usb.close();capture.close()
        await link.close()
        telemetry.save(logs/'mqtt-summary.json')
        print('Saved USB and MQTT logs in:',logs)
        print('Confirm drive is off before handling the car.')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reuse',action='store_true',help='Use existing private settings and build without prompts')
    args=parser.parse_args()
    logging.basicConfig(level=logging.ERROR)
    try:
        if args.reuse:
            settings=json.loads((PRIVATE/'settings.json').read_text())
            metadata=json.loads((PRIVATE/'firmware-info.json').read_text())
            image=Path(metadata.get('file',ROOT/'build/rtos-network/robocar_rtos_network_v0_3.uf2'))
            if not image.exists():parser.error('Run setup once before --reuse')
            from rtos_network_setup import mac_wifi_ip
            current_ip=mac_wifi_ip()
            if not current_ip:
                parser.error('Could not detect this Mac\'s Wi-Fi IPv4 address; connect to the home Wi-Fi and retry')
            if current_ip!=settings['broker_ip']:
                parser.error(f"Mac Wi-Fi IP changed from {settings['broker_ip']} to {current_ip}; rerun without --reuse, then flash the newly built firmware")
            if hashlib.sha256(image.read_bytes()).hexdigest()!=metadata['sha256'] or \
               hashlib.sha256(json.dumps(settings,sort_keys=True).encode()).hexdigest()!=metadata['settings_sha256']:
                parser.error('Saved image/settings changed; rerun without --reuse to rebuild')
        else:settings,image=prepare()
        asyncio.run(run(settings,image,PRIVATE,flash_hint=not args.reuse))
    except KeyboardInterrupt:pass
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'Setup stopped: {error}\n')


if __name__=='__main__':main()
