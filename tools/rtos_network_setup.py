#!/usr/bin/env python3
"""Local-only credential provisioning and authenticated broker configuration."""
import getpass
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "build/rtos-network-local"
PREFIX = "inf2004/aai10/robot1/v1/"


def private_write(path, text):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    path.parent.chmod(0o700)
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as stream:
        name = Path(stream.name)
        os.chmod(name, 0o600)
        stream.write(text)
    name.replace(path)


def validate(settings):
    ip = ipaddress.IPv4Address(settings["broker_ip"])
    if not ip.is_private or ip.is_loopback or ip.is_link_local or ip.is_unspecified:
        raise ValueError("Use this Mac's private Wi-Fi IPv4 address")
    if not 1 <= int(settings["port"]) <= 65535:
        raise ValueError("Invalid broker port")
    ssid, password = settings["ssid"], settings["wifi_password"]
    if not 1 <= len(ssid.encode()) <= 32 or '\0' in ssid:
        raise ValueError("Wi-Fi name must contain 1..32 UTF-8 bytes, without NUL")
    if not 8 <= len(password.encode()) <= 63 or '\0' in password:
        raise ValueError("Use an 8..63-byte WPA2 Wi-Fi password")


def mac_wifi_ip():
    try:
        return subprocess.check_output(["ipconfig", "getifaddr", "en0"], text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def write_settings(settings, folder):
    """Passwords never enter command arguments or printed output."""
    from amqtt.plugins.authentication import pwd_context
    folder = Path(folder)
    private_write(folder / "settings.json", json.dumps(settings, indent=2) + "\n")
    strings = {"RTOS_WIFI_SSID": settings["ssid"], "RTOS_WIFI_PASSWORD": settings["wifi_password"],
               "RTOS_BROKER_IPV4": settings["broker_ip"], "RTOS_BROKER_USER": settings["robot_user"],
               "RTOS_BROKER_PASSWORD": settings["robot_password"], "RTOS_TOPIC_PREFIX": settings["prefix"]}
    header = '#ifndef RTOS_NETWORK_SETTINGS_H\n#define RTOS_NETWORK_SETTINGS_H\n#define RTOS_NETWORK_CONFIGURED 1\n#define RTOS_WAIT_FOR_USB 0\n'
    header += f'#define RTOS_BROKER_PORT {int(settings["port"])}\n'
    # Fixed-width octal C escapes preserve UTF-8 and quotes without allowing
    # a network name/password to become source code or a universal escape.
    def c_string(value):
        return '"' + ''.join('\\%03o' % byte for byte in value.encode('utf-8')) + '"'
    header += ''.join(f'#define {key} {c_string(value)}\n' for key, value in strings.items())
    private_write(folder / "network_settings.h", header + '#endif\n')
    passwords = ''.join(f'{settings[role+"_user"]}:{pwd_context.hash(settings[role+"_password"], scheme="sha512_crypt", rounds=10000)}\n'
                        for role in ("robot", "operator"))
    private_write(folder / "passwords", passwords)


def broker_config(settings, folder):
    robot, operator, prefix = settings["robot_user"], settings["operator_user"], settings["prefix"]
    return {"listeners": {"default": {"type": "tcp", "bind": f'{settings["broker_ip"]}:{settings["port"]}'}},
            "plugins": {
                "amqtt.plugins.authentication.AnonymousAuthPlugin": {"allow_anonymous": False},
                "amqtt.plugins.authentication.FileAuthPlugin": {"password_file": str(Path(folder)/"passwords")},
                # amqtt0.11.3 Config calls the subscribe ACL 'acl'.
                "amqtt.plugins.topic_checking.TopicAccessControlListPlugin": {
                    "acl": {robot: [prefix+"command/#"], operator: [prefix+"+"]},
                    "publish_acl": {robot: [prefix+name for name in ("motion","line","barcode","status","link","command_result","scan_diag","runtime_diag")],
                                    operator: [prefix+"command/#"]}}}}


def prepare(folder=PRIVATE):
    folder = Path(folder)
    path = folder / "settings.json"
    settings = json.loads(path.read_text()) if path.exists() else None
    ip = mac_wifi_ip()
    reuse = settings and input("Reuse the saved local Wi-Fi settings? [Y/n]: ").strip().lower() != "n"
    if not reuse:
        print("Enter the Pico's 2.4 GHz Wi-Fi name. This Mac may use another band on the same home router. Password entry stays hidden.")
        ssid = input("Wi-Fi name (SSID): ")
        password = getpass.getpass("Wi-Fi password: ")
        settings = dict(ssid=ssid, wifi_password=password, broker_ip=ip, port=1883, prefix=PREFIX,
                        robot_user="aai10_robot", robot_password=secrets.token_hex(16),
                        operator_user="aai10_observer", operator_password=secrets.token_hex(16))
    default = ip or settings["broker_ip"]
    settings["broker_ip"] = input(f"This Mac's broker address [{default}]: ").strip() or default
    validate(settings)
    write_settings(settings, folder)
    print("Local settings saved. Passwords are not printed or added to git.")
    print("Building the configured Wi-Fi firmware; this may take a moment...")
    env = dict(os.environ, ROBOT_BUILD_DIR=str(ROOT/"build/rtos-network-v0_5_1"),
               ROBOT_NETWORK_SETTINGS=str(folder/"network_settings.h"),
               ROBOT_OUTPUT_NAME="robocar_rtos_network_v0_5_1.uf2",
               ROBOT_COMPACT_ADC_STATE="OFF",ROBOT_COMPACT_CORE_STATE="OFF",ROBOT_COMPACT_CONTROL_STATE="OFF",
               ROBOT_FIRST_FAILURE_CACHE="ON")
    log = folder / "build.log"
    with log.open("w") as output:
        os.chmod(log, 0o600)
        result = subprocess.run(["sh", str(ROOT/"tools/build_rtos_motion.sh")], cwd=ROOT,
                                env=env, stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f"Build failed; private build log: {log}")
    image = ROOT / "build/rtos-network-v0_5_1/robocar_rtos_network_v0_5_1.uf2"
    private_write(folder / "firmware-info.json", json.dumps(dict(file=str(image),
        sha256=hashlib.sha256(image.read_bytes()).hexdigest(),broker_ip=settings["broker_ip"],
        settings_sha256=hashlib.sha256(json.dumps(settings,sort_keys=True).encode()).hexdigest(),
        port=settings["port"],network_configured=True,credentials_embedded=True,
        firmware_version='0.5.1',track_integration=True,scan_diagnostics=True,runtime_measurements=True,requires_usb_logger=False,
        default_selection='track_scan'), indent=2)+'\n')
    print("Firmware ready:", image)
    return settings, image
