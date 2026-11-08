#!/usr/bin/env python3
"""Production MQTT service under SDK mocks, independent decoder, local broker E2E."""
import asyncio
import json
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile

from rtos_mqtt_observer import decode

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "firmware/app/mission_rtos"
TEST = ROOT / "firmware/tests/mission_rtos"


async def broker_roundtrip(packets):
    from amqtt.broker import Broker
    import paho.mqtt.client as mqtt
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0)); port = sock.getsockname()[1]
    config = {"listeners": {"default": {"type": "tcp", "bind": f"127.0.0.1:{port}"}},
              "plugins": {"amqtt.plugins.authentication.AnonymousAuthPlugin": {"allow_anonymous": True}}}
    broker = Broker(config)
    await broker.start()
    received, errors = [], []
    subscriber = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    publisher = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    subscribed = asyncio.Event()
    loop = asyncio.get_running_loop()
    def connect(c, context, flags, reason, properties):
        del context, flags, properties
        if reason != 0: errors.append(str(reason))
        c.subscribe("inf2004/aai10/robot1/v1/+", qos=1)
    def subscription(c, context, mid, reasons, properties):
        del c, context, mid, reasons, properties
        loop.call_soon_threadsafe(subscribed.set)
    def message(c, context, m):
        del c, context
        try:
            decode(m.topic, m.payload)
            received.append((m.topic, bytes(m.payload)))
        except ValueError as error:
            errors.append(str(error))
    subscriber.on_connect=connect; subscriber.on_subscribe=subscription; subscriber.on_message=message
    try:
        subscriber.connect_async("127.0.0.1", port); subscriber.loop_start()
        await asyncio.wait_for(subscribed.wait(),5)
        publisher.connect("127.0.0.1",port); publisher.loop_start()
        for topic,payload in packets:
            info=publisher.publish(topic,payload,qos=1)
            await asyncio.to_thread(info.wait_for_publish,3)
        end=loop.time()+5
        while len(received)<len(packets) and loop.time()<end: await asyncio.sleep(0.02)
        assert not errors,errors
        assert received==packets,(len(received),len(packets))
        # Exercise real client reconnection/resubscription after a broker restart.
        subscribed.clear()
        await broker.shutdown()
        broker=Broker(config)
        await broker.start()
        await asyncio.wait_for(subscribed.wait(),8)
        end=loop.time()+8
        while not publisher.is_connected() and loop.time()<end: await asyncio.sleep(0.02)
        assert publisher.is_connected()
        extra=[next((t,p) for t,p in packets if t.endswith('/motion')),
               next((t,p) for t,p in packets if decode(t,p).get('barcode')=='Z')]
        for topic,payload in extra:
            info=publisher.publish(topic,payload,qos=1)
            await asyncio.to_thread(info.wait_for_publish,3)
        end=loop.time()+5
        while len(received)<len(packets)+2 and loop.time()<end: await asyncio.sleep(0.02)
        assert received==packets+extra
    finally:
        publisher.disconnect(); subscriber.disconnect()
        await asyncio.to_thread(publisher.loop_stop); await asyncio.to_thread(subscriber.loop_stop)
        await broker.shutdown()
    return len(received)


def main():
    with tempfile.TemporaryDirectory(prefix="rtos-network-") as directory:
        temp=Path(directory)
        headers=["hardware/sync.h","pico/cyw43_arch.h","pico/rand.h","pico/stdlib.h",
                 "lwip/apps/mqtt.h","lwip/ip_addr.h"]
        for path in headers:
            file=temp/path;file.parent.mkdir(parents=True,exist_ok=True)
            file.write_text('#include "network_test_sdk.h"\n')
        (temp/"network_settings.h").write_text((APP/"network_settings.example.h").read_text())
        subprocess.run([shutil.which("clang"),"-std=c11","-g","-O1","-Wall","-Wextra","-Werror","-DRTOS_TRACK=1",
                        "-fsanitize=address,undefined","-fno-omit-frame-pointer",
                        "-I",str(APP),"-I",str(ROOT/'firmware/app/robot_firmware'),"-I",str(TEST),"-I",str(temp),str(TEST/"network_test.c"),
                        str(APP/"binary_protocol.c"),str(APP/"motion_record.c"),str(APP/'scan_diagnostic.c'),"-o",str(temp/"test")],check=True)
        output=subprocess.check_output([str(temp/"test")],text=True)
        print("\n".join(line for line in output.splitlines() if line.startswith("PASS")))
        packets=[]
        for line in output.splitlines():
            if line.startswith("PACKET "):
                _,topic,hex_data=line.split();payload=bytes.fromhex(hex_data)
                decode(topic,payload);packets.append((topic,payload))
        assert any(decode(t,p).get("barcode")=="Z" for t,p in packets)
        assert any(len(p)==13 and b"\0" in p for t,p in packets)
        assert any(decode(t,p).get('raw_adc')==[191,2908,141] for t,p in packets)
        fixtures=ROOT/'build/rtos-scan-fixtures.json'
        if not fixtures.exists():raise RuntimeError('Run tools/test_rtos_track.py first for actual scan export fixtures')
        for label in ('AAZ','stopped-partial','max-wave'):
            packets.extend(('inf2004/aai10/robot1/v1/scan_diag',bytes.fromhex(h)) for h in json.loads(fixtures.read_text())[label])
        for topic,data in [("inf2004/aai10/robot1/v1/barcode",b"\0"*4+b"\xff"),
                           ("inf2004/aai10/robot1/v1/status",b"\xff"*10),
                           ("inf2004/aai10/robot1/v1/motion",b"\0")]:
            try: decode(topic,data)
            except ValueError: pass
            else: raise AssertionError("Malformed packet accepted")
        count=asyncio.run(broker_roundtrip(packets))
        result=dict(production_service_sdk_mock="PASS",independent_binary_decode="PASS",
                    local_broker_binary_roundtrip_packets=count,hardware_network_tested=False,
                    local_broker_restart_cycles=1,
                    packet_sizes={kind:sorted({len(p) for t,p in packets if t.endswith('/'+kind)})
                                  for kind in ["motion","line","barcode","status","link","command_result","scan_diag"]})
        (ROOT/"build/rtos-network-tests.json").write_text(json.dumps(result,indent=2)+"\n")
        print(f"PASS local MQTT broker: {count} binary packets retained byte-for-byte, including Z and zero bytes")


if __name__=="__main__": main()
