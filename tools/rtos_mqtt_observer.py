#!/usr/bin/env python3
"""Receive versioned binary robot telemetry; never publishes motion commands."""
import argparse
from datetime import datetime, timezone
import getpass
import json
import os
from pathlib import Path
import time

from decode_motion_record import decode as decode_motion


def decode(topic, payload):
    if "/v1/" not in topic:
        raise ValueError("Unknown schema version")
    kind = topic.rsplit("/", 1)[-1]
    if kind == "motion":
        return dict(type=kind, **decode_motion(payload))
    if kind == "barcode":
        if len(payload) != 5 or payload[4] & 128:
            raise ValueError("Invalid barcode length/padding")
        code, valid, simulated = payload[4] & 31, bool(payload[4] & 32), bool(payload[4] & 64)
        if code > 27 or (not valid and (code or simulated)):
            raise ValueError("Invalid barcode code/validity")
        return dict(type=kind, captured_ms=int.from_bytes(payload[:4], "little"),
                    barcode=chr(64+code) if 1 <= code <= 26 else None,
                    unreadable=code == 27, valid=valid, simulated=simulated)
    if kind == "status":
        if len(payload) != 10:
            raise ValueError("Invalid status length")
        word = int.from_bytes(payload, "little")
        available, simulated, state = (word >> 64) & 31, (word >> 69) & 31, (word >> 74) & 7
        if word >> 77 or state > 5 or simulated & ~available:
            raise ValueError("Invalid status flags")
        return dict(type=kind, captured_ms=word & 0xffffffff, boot_id=(word >> 32) & 0xffffffff,
                    available_mask=available, simulated_mask=simulated, network_state=state)
    if kind == "link":
        if len(payload) != 1 or payload[0] > 2:
            raise ValueError("Invalid link code")
        return dict(type=kind, state=["unknown", "offline", "online"][payload[0]])
    if kind == "command_result":
        if len(payload) != 1 or payload[0] & 0xf8:
            raise ValueError("Invalid result code")
        return dict(type=kind, accepted=bool(payload[0] & 1), command=(payload[0] >> 1) & 3)
    raise ValueError("Unknown telemetry topic")


def main():
    import paho.mqtt.client as mqtt
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--broker", required=True)
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--prefix", default="inf2004/aai10/robot1/v1/")
    parser.add_argument("--user")
    parser.add_argument("--password-prompt", action="store_true")
    parser.add_argument("--log", type=Path, default=Path("serial-output/mqtt-binary.jsonl"))
    parser.add_argument("--seconds", type=float)
    args = parser.parse_args()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    with args.log.open("a", buffering=1) as stream:
        def record(value):
            value = dict(received_utc=datetime.now(timezone.utc).isoformat(), **value)
            line = json.dumps(value)
            print(line, flush=True); stream.write(line + "\n")
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        if args.user:
            password = getpass.getpass("Broker password: ") if args.password_prompt else os.environ.get("MQTT_PASSWORD")
            if password is None:
                parser.error("Set MQTT_PASSWORD locally or use --password-prompt")
            client.username_pw_set(args.user, password)
        def connected(c, context, flags, reason, properties):
            del context, flags, properties
            record(dict(event="connection", result=str(reason)))
            if reason == 0:
                c.subscribe(args.prefix.rstrip("/") + "/+", qos=1)
        def received(c, context, message):
            del c, context
            try:
                value = decode(message.topic, message.payload)
                record(dict(topic=message.topic, bytes=len(message.payload), retained=message.retain, **value))
            except ValueError as error:
                record(dict(topic=message.topic, rejected=str(error), payload_hex=message.payload.hex()))
        client.on_connect = connected; client.on_message = received
        client.connect_async(args.broker, args.port, keepalive=15)
        client.loop_start()
        end = time.monotonic() + args.seconds if args.seconds else float("inf")
        try:
            while time.monotonic() < end:
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass
        finally:
            client.disconnect(); client.loop_stop()


if __name__ == "__main__":
    main()
