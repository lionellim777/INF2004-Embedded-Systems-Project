#!/usr/bin/env python3
"""Measure the pre-integration representation baseline; never build/flash firmware.

Uses Clang only to enumerate C declarations, ARM GCC/objcopy for actual target
layout, and the existing linked ELF for static RAM. Branches are read at pinned
commits in temporary directories. No network, credentials, or device access.
"""

import argparse
import csv
import hashlib
import json
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APP = "firmware/app/robot_firmware"
TOOLCHAIN = ROOT / "build/dependencies/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi"
INCLUDES = ["obstacle_bypass.h", "barcode_waveform.h", "barcode_signal.h",
            "barcode_optical.h", "motor_guard.h", "terrain.h",
            "navigation_commands.h", "run_diagnostics.h", "wireless/calibration_core.h"]


def run(*args, cwd=ROOT):
    return subprocess.check_output([str(x) for x in args], cwd=cwd, text=True)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git_bytes(commit, path):
    return subprocess.check_output(["git", "show", f"{commit}:{path}"], cwd=ROOT)


def walk(node):
    yield node
    for child in node.get("inner", []):
        yield from walk(child)


def bits_for_values(values):
    low, high = min(values), max(values)
    if low >= 0:
        return max(1, high.bit_length())
    bits = 1
    while low < -(1 << (bits - 1)) or high >= (1 << (bits - 1)):
        bits += 1
    return bits


def enum_values(node):
    values, previous = {}, -1
    for member in node.get("inner", []):
        if member.get("kind") != "EnumConstantDecl":
            continue
        constants = [int(n["value"]) for n in walk(member)
                     if n.get("kind") == "ConstantExpr" and "value" in n]
        previous = constants[0] if constants else previous + 1
        values[member["name"]] = previous
    return values


def export_headers(commit, prefix, dest):
    hashes = {}
    for path in run("git", "ls-tree", "-r", "--name-only", commit, "--", prefix).splitlines():
        # Application declarations only; omit generated pages and SDK-dependent transport headers.
        if not path.endswith(".h") or path.endswith(("calibration_page.h", "stationary_page.h", "dhcpserver.h", "lwipopts.h")):
            continue
        data = git_bytes(commit, path)
        relative = Path(path).relative_to(prefix)
        target = dest / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        hashes[path] = digest(data)
    return hashes


def policy(scope, type_name, field, qual, enums):
    """Known ranges only. OPEN means an owner still has to justify the range."""
    key = f"{type_name}.{field}"
    scalar = "[" not in qual
    if qual in enums:
        values = enums[qual]["values"]
        return (f"declared enum values {min(values.values())}..{max(values.values())}",
                enums[qual]["required_bits"], "CODE_RANGE", "Pack only after shared layout and invalid-code rules are agreed")
    if qual in ("bool", "_Bool"):
        return ("false/true", 1, "CODE_RANGE", "One bit; shared-word updates still require synchronization")
    if type_name == "barcode_runs_t" and field == "bytes":
        return ("29 widths, each 0..750000 at existing resolution", 580,
                "ALREADY_PACKED", "20 bits per width, 73 bytes incl. four tail bits; do not change units")
    if type_name == "barcode_signal_t":
        known = {"samples": ("12 samples of 0..4095", 144),
                 "sum": ("0..49140 = 12*4095", 16),
                 "next": ("0..11", 4), "count": ("0..12", 4)}
        if field in known:
            value, bits = known[field]
            return value, bits, "ADC12_ASSUMPTION", "Requires retaining the 12-bit ADC input contract; no firmware change made"
    if type_name == "encoder_sample_t":
        return ("uint32 modulo counter; full bit pattern retained", 32,
                "KEEP_PENDING_LIFETIME_REVIEW", "Never reset shared raw counts to satisfy a proposed relative-message width")
    if type_name == "waveform_sample_t":
        if field == "raw":
            return "0..4095 filtered ADC", 12, "ADC12_ASSUMPTION", "Target struct padding measured separately; preserve position and sample timing"
        return "uint32 timestamp or modulo wheel counter", 32, "KEEP_PENDING_LIFETIME_REVIEW", "Bounded delta rules must survive wrapping"
    if type_name == "course_config_t":
        if field.endswith("action"):
            return "11 action choices", 4, "RECODING_PROPOSAL", "Explicit code mapping required; current ASCII is unchanged"
        return "60..250 mm", 8, "CODE_RANGE", "Direct unsigned representation; no change to controller thresholds"
    if scalar and (field in ("letter", "last_letter", "decoded", "barcode") or
                   key == "telemetry_line_t.barcode") and qual == "char":
        return ("proposed none + A..Z + unreadable (28 codes)", 5,
                "RECODING_PROPOSAL", "Current bytes are ASCII; decide reserved/invalid codes with navigation and MQTT owners")
    if "*" in qual:
        return "address/reference; not a portable wire value", "", "STRUCTURAL", "Consider replacing application state strings with enum codes; do not transmit pointers"
    if not scalar:
        return "array/container; inspect elements or referenced type", "", "CONTAINER_REVIEW", "Do not add nested container and child sizes when estimating RAM"
    if qual.startswith(("union ", "struct ")):
        return "anonymous record container", "", "CONTAINER_REVIEW", "Review member types separately; do not count container twice"
    if qual in ("float", "double"):
        return "range/resolution/error budget OPEN", "", "OWNER_DECISION", "Retain working precision until quantified error/range tests justify a fixed-point representation"
    if qual.endswith("_t") and qual not in ("uint8_t", "uint16_t", "uint32_t", "uint64_t", "int8_t", "int16_t", "int32_t", "int64_t", "size_t"):
        return "nested application type", "", "CONTAINER_REVIEW", "See field rows for referenced type"
    return "range/lifetime/reset/overflow policy OPEN", "", "OWNER_DECISION", "No minimum width claimed solely from native C type"


def owner(scope, name):
    if scope == "wifi" or name == "calibration_core_t":
        return "Justin / integration owner"
    if scope == "terrain" or name == "terrain_status_t":
        return "Ezra"
    if scope == "obstacle" or name.startswith(("obstacle_", "avoid_")):
        return "Lionel"
    if name.startswith(("barcode_", "ir_", "line_", "navigation_")) or name == "waveform_sample_t":
        return "Jing Hai / Yixuan baseline"
    if name.startswith(("course_", "center_")):
        return "Jing Hai / Yixuan"
    return "Yixuan / integration owner"


def measure_layout(scope, directory, includes, sources, cc, clang, objcopy):
    typedef_files = {}
    for relative in sources:
        file = directory / relative
        if file.suffix == ".h":
            for name in re.findall(r"}\s*(\w+)\s*;", file.read_text()):
                typedef_files[name] = relative
            for name in re.findall(r"typedef\s+u?int\d+_t\s+(\w+)\s*;", file.read_text()):
                typedef_files[name] = relative
    probe = directory / "audit_probe.c"
    prefix = "#include <stddef.h>\n#include <stdint.h>\n" + "".join(f'#include "{h}"\n' for h in includes)
    probe.write_text(prefix)
    ast = json.loads(run(clang, "--target=arm-none-eabi", "-mcpu=cortex-m0plus", "-mthumb", "-std=c11",
                         "-isystem", cc.parent.parent / "arm-none-eabi/include", "-I", directory,
                         "-Xclang", "-ast-dump=json", "-fsyntax-only", probe))
    nodes = list(walk(ast))
    records = {n["id"]: n for n in nodes if n.get("kind") in ("RecordDecl", "EnumDecl")
               and n.get("inner")}
    definitions, enums = {}, {}
    for node in ast.get("inner", []):
        name = node.get("name")
        if node.get("kind") != "TypedefDecl" or name not in typedef_files:
            continue
        tags = [n["ownedTagDecl"]["id"] for n in walk(node) if "ownedTagDecl" in n]
        if not tags:
            definitions[name] = {"kind": "ScalarAlias", "inner": []}
            continue
        if tags[0] not in records:
            continue
        definition = records[tags[0]]
        definitions[name] = definition
        if definition["kind"] == "EnumDecl":
            values = enum_values(definition)
            enums[name] = {"values": values, "required_bits": bits_for_values(list(values.values()))}
    if not definitions:
        raise ValueError(f"No application types found for {scope}")
    expressions, labels = [], []
    for name, definition in sorted(definitions.items()):
        expressions.append(f"sizeof({name})")
        labels.append((name, "", "size"))
        for field in definition.get("inner", []):
            if field.get("kind") == "FieldDecl" and field.get("name") and not field.get("isBitfield"):
                for kind, expression in (("size", f"sizeof((({name} *)0)->{field['name']})"),
                                         ("offset", f"offsetof({name}, {field['name']})")):
                    expressions.append(expression)
                    labels.append((name, field["name"], kind))
    probe.write_text(prefix + '\nconst uint32_t audit_values[] __attribute__((section(".audit"))) = {\n' + ",\n".join(expressions) + "\n};\n")
    obj, raw = directory / "audit_probe.o", directory / "audit_probe.bin"
    run(cc, "-std=c11", "-mcpu=cortex-m0plus", "-mthumb", "-Wall", "-Wextra", "-Werror", "-I", directory, "-c", probe, "-o", obj)
    run(objcopy, "-O", "binary", "-j", ".audit", obj, raw)
    data = raw.read_bytes()
    if len(data) != 4 * len(labels):
        raise ValueError("ARM layout probe size mismatch")
    values = dict(zip(labels, struct.unpack(f"<{len(labels)}I", data)))
    types, rows = {}, []
    for name, definition in sorted(definitions.items()):
        types[name] = {"bytes": values[(name, "", "size")], "source": typedef_files[name]}
        for field in definition.get("inner", []):
            if field.get("kind") != "FieldDecl" or not field.get("name"):
                continue
            field_name, qual = field["name"], field["type"]["qualType"]
            for prefix in (str(directory) + "/", str(directory.resolve()) + "/"):
                qual = qual.replace(prefix, "")
            is_bitfield = field.get("isBitfield", False)
            if is_bitfield:
                field_bits = next(int(n["value"]) for n in walk(field) if n.get("kind") == "ConstantExpr")
                offset = "bitfield; see explicit encoder"
            else:
                field_bits = 8 * values[(name, field_name, "size")]
                offset = values[(name, field_name, "offset")]
            valid_range, minimum, status, action = policy(scope, name, field_name, qual, enums)
            if is_bitfield:
                status, action = "PACKED_OWNER_REVIEW", "Width already packed; owner must justify range/resolution and explicit wire offsets"
            rows.append(dict(scope=scope, source=typedef_files[name], type=name, field=field_name,
                             owner_review=owner(scope, name),
                             declared_type=qual, current_storage_bits=field_bits, byte_offset=offset,
                             range_or_domain=valid_range, required_or_proposed_bits=minimum,
                             status=status, next_action=action))
    return {"types": types, "enums": enums, "field_rows": len(rows)}, rows


def mqtt_cases(directory, commit, host_cc):
    (directory / "telemetry.c").write_bytes(git_bytes(commit, "src/telemetry.c"))
    driver = directory / "audit_json.c"
    driver.write_text(r'''
#include "telemetry.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    telemetry_snapshot_t s = {0};
    char out[TELEMETRY_JSON_SIZE];
    if (!telemetry_format_snapshot(&s, 1, 1, 0, out, sizeof out)) return 1;
    puts(out);
    s.mission = (telemetry_mission_t){true, true, 12345, TELEMETRY_MISSION_RUNNING};
    s.motion = (telemetry_motion_t){true, true, 12345, 50, 50, -10000, 10000, 250};
    s.line = (telemetry_line_t){true, true, 12345, 300, 300, 200, true, 'A'};
    s.terrain = (telemetry_terrain_t){true, true, 12345, TELEMETRY_MOTION_STATIONARY, 20, 40};
    s.obstacle = (telemetry_obstacle_t){true, true, 12345, 1, 300, 100, 400, 400};
    if (!telemetry_format_snapshot(&s, 1, 2, 12345, out, sizeof out)) return 2;
    puts(out);
    s.line.barcode = 'Z';
    if (!telemetry_format_snapshot(&s, 1, 3, 12345, out, sizeof out)) return 3;
    puts(out);
    telemetry_event_t event = {TELEMETRY_EVENT_SCAN_POINT, 12345, 1, 300, -45000};
    if (!telemetry_format_event(&event, 1, 4, out, sizeof out)) return 4;
    puts(out);
    return 0;
}
''')
    binary = directory / "audit_json"
    run(host_cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", directory,
        directory / "telemetry.c", driver, "-o", binary)
    lines = run(binary).splitlines()
    if len(lines) != 4:
        raise ValueError("Serializer did not produce four measurement cases")
    decoded = [json.loads(line) for line in lines]
    if decoded[1]["line"]["barcode"] != "A" or decoded[3]["value"] != 300:
        raise ValueError("Independent JSON interpretation failed")
    names = ["all_producers_invalid", "representative_all_producers_A", "same_values_Z_input", "representative_scan_event"]
    return dict(method="Real branch serializer executed on host, independently parsed with Python JSON; synthetic fixtures, not network traffic",
                terminator_in_payload=False, mqtt_topic_and_protocol_overhead_included=False,
                cases=[dict(name=name, payload_bytes=len(line.encode("utf-8")), decoded=value)
                       for name, line, value in zip(names, lines, decoded)],
                z_preserved=(decoded[2]["line"]["barcode"] == "Z"))


def linked_memory(elf, release, cc, directory):
    size_tool, nm, objcopy = [cc.with_name("arm-none-eabi-" + name) for name in ("size", "nm", "objcopy")]
    size_line = run(size_tool, elf).splitlines()[1].split()
    text, data, bss = map(int, size_line[:3])
    symbols = []
    for line in run(nm, "-S", "--size-sort", elf).splitlines():
        match = re.fullmatch(r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([bBdD])\s+(.+)", line)
        if match:
            address, count, kind, name = match.groups()
            symbols.append(dict(name=name, bytes=int(count, 16), address="0x" + address, kind=kind))
    uf2 = ROOT / release["file"]
    raw = uf2.read_bytes()
    if digest(raw) != release["sha256"] or len(raw) % 512:
        raise ValueError("Frozen release UF2 failed its recorded hash/size check")
    chunks = {}
    for offset in range(0, len(raw), 512):
        block = raw[offset:offset+512]
        magic0, magic1, flags, address, length, index, total, family = struct.unpack_from("<8I", block)
        if (magic0, magic1, struct.unpack_from("<I", block, 508)[0]) != (0x0A324655, 0x9E5D5157, 0x0AB16F30):
            raise ValueError("Bad UF2 magic")
        if length != 256 or flags != 0x2000 or family != 0xE48BFF56 or total != len(raw)//512 or index != offset//512 or address in chunks:
            raise ValueError("Unexpected UF2 family/block metadata")
        chunks[address] = block[32:32+length]
    addresses = sorted(chunks)
    if addresses != list(range(0x10000000, 0x10000000 + 256*len(chunks), 256)):
        raise ValueError("UF2 flash image is not contiguous")
    elf_bin = directory / "linked_image.bin"
    run(objcopy, "-O", "binary", elf, elf_bin)
    image, flash = elf_bin.read_bytes(), b"".join(chunks[a] for a in addresses)
    if not flash.startswith(image) or any(flash[len(image):]):
        raise ValueError("ELF flash contents differ from the accepted UF2")
    if (text, data, bss) != (release["linked_text_bytes"], release["linked_data_bytes"], release["linked_bss_bytes"]):
        raise ValueError("ELF sizes differ from the accepted release report")
    return dict(text_bytes=text, data_bytes=data, bss_bytes=bss,
                static_data_plus_bss_bytes=data+bss, chip_sram_bytes=264*1024,
                arithmetic_remainder_bytes=264*1024-data-bss,
                remainder_is_free_runtime_ram=False,
                main_stack_reservation_bytes=release["main_stack_reservation_bytes"],
                kernel_pool_on_wifi_branch_bytes=96*1024,
                naive_bss_plus_kernel_pool_bytes=bss+96*1024,
                naive_sum_is_combined_link_measurement=False,
                largest_ram_symbols=sorted(symbols, key=lambda x:x["bytes"], reverse=True)[:25],
                elf_sha256=digest(elf.read_bytes()), frozen_uf2_sha256=digest(raw),
                elf_flash_matches_frozen_uf2=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--main-ref", default="origin/main")
    parser.add_argument("--wifi-ref", default="origin/wifi-buddy-1")
    parser.add_argument("--terrain-ref", default="origin/buddy4-terrain")
    parser.add_argument("--obstacle-ref", default="origin/buddy-5")
    parser.add_argument("--elf", type=Path, default=ROOT / "build/motion-wireless/robot_firmware.elf")
    parser.add_argument("--toolchain", type=Path, default=TOOLCHAIN)
    parser.add_argument("--clang", default=shutil.which("clang"))
    parser.add_argument("--host-cc", default=shutil.which("cc"))
    parser.add_argument("--output", type=Path, default=ROOT / "build/representation-audit")
    args = parser.parse_args()
    cc = args.toolchain.resolve() / "bin/arm-none-eabi-gcc"
    if not cc.is_file() or not args.clang or not args.host_cc or not args.elf.is_file():
        parser.error("Need ARM GCC, Clang, host C compiler and the preserved v5.63 ELF; see docs/representation-audit.md")
    refs = {name: run("git", "rev-parse", f"{ref}^{{commit}}").strip()
            for name, ref in (("main", args.main_ref), ("wifi", args.wifi_ref),
                              ("terrain", args.terrain_ref), ("obstacle", args.obstacle_ref))}
    result = {"schema": 1, "scope": "Pre-change baseline; no runtime/RTOS/packing changes",
              "measurement_tool_sha256": digest(Path(__file__).read_bytes()),
              "commits": refs, "compiler": run(cc, "--version").splitlines()[0],
              "ast_compiler": run(args.clang, "--version").splitlines()[0],
              "target_flags": ["-std=c11", "-mcpu=cortex-m0plus", "-mthumb"],
              "after_change_measurements": None, "hardware_tests_performed": False,
              "coverage": "Named public application records/enums/scalar aliases plus three copied private main layouts; excludes automatic locals, private transport internals and third-party kernel/SDK fields",
              "source_sha256": {}, "layouts": {}}
    rows = []
    with tempfile.TemporaryDirectory(prefix="inf2004-representation-") as temporary:
        base = Path(temporary)
        for scope in refs:
            directory = base / scope
            directory.mkdir()
            if scope == "main":
                hashes = export_headers(refs[scope], APP, directory)
                sources = [str(Path(p).relative_to(APP)) for p in hashes]
                source = git_bytes(refs[scope], APP + "/robot_firmware.c").decode()
                for path in run("git", "ls-tree", "-r", "--name-only", refs[scope], "--", APP).splitlines():
                    if path.endswith(".c"):
                        hashes[path] = digest(git_bytes(refs[scope], path))
                layouts = []
                for name in ("motor_t", "button_t", "waveform_sample_t"):
                    match = re.search(r"typedef struct\s*\{[^}]*}\s*" + name + r";", source)
                    if not match:
                        raise ValueError(f"Private layout moved: {name}; update extractor")
                    layouts.append(match.group())
                (directory / "private_layouts.h").write_text("typedef unsigned int uint;\n" + "\n".join(layouts))
                sources.append("private_layouts.h")
                includes = INCLUDES + ["private_layouts.h"]
                result["source_sha256"][scope] = hashes
            else:
                file = {"wifi": "include/telemetry.h", "terrain": APP + "/terrain.h",
                        "obstacle": APP + "/obstacle_data.h"}[scope]
                target = Path(file).name
                data = git_bytes(refs[scope], file)
                (directory / target).write_bytes(data)
                includes, sources = [target], [target]
                result["source_sha256"][scope] = {file: digest(data)}
            layout, fields = measure_layout(scope, directory, includes, sources, cc, args.clang,
                                            cc.with_name("arm-none-eabi-objcopy"))
            if scope == "main":
                for entry in layout["types"].values():
                    if entry["source"] == "private_layouts.h":
                        entry["source"] = "robot_firmware.c (exact copied declaration)"
                for row in fields:
                    if row["source"] == "private_layouts.h":
                        row["source"] = "robot_firmware.c"
            result["layouts"][scope] = layout
            rows.extend(fields)
            if scope == "wifi":
                result["mqtt_before"] = mqtt_cases(directory, refs[scope], args.host_cc)
                result["source_sha256"][scope]["src/telemetry.c"] = digest(git_bytes(refs[scope], "src/telemetry.c"))
        release = json.loads(git_bytes(refs["main"], "firmware/releases/build-report-v5_63.json"))
        for name, expected in release["source_sha256"].items():
            path = APP + "/" + name
            if digest(git_bytes(refs["main"], path)) != expected:
                raise ValueError(f"Source differs from accepted v5.63 report: {path}")
        result["release_report_source_hashes_verified"] = True
        result["memory_before"] = linked_memory(args.elf, release, cc, base)
        symbols = {s["name"]: s["bytes"] for s in result["memory_before"]["largest_ram_symbols"]}
        for symbol, name in (("waveform", "barcode_waveform_t"), ("core", "calibration_core_t"),
                             ("avoidance", "obstacle_bypass_t"), ("course", "course_control_t")):
            if symbols[symbol] != result["layouts"]["main"]["types"][name]["bytes"]:
                raise ValueError(f"ARM probe disagrees with linked symbol {symbol}")
    result["inventory_rows"] = len(rows)
    result["inventory_status_counts"] = {status: sum(r["status"] == status for r in rows)
                                           for status in sorted({r["status"] for r in rows})}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "baseline.json").write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    with (args.output / "fields.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    print(f"PASS: measured {len(rows)} fields across {sum(len(v['types']) for v in result['layouts'].values())} target types")
    print(f"Static data+BSS: {result['memory_before']['static_data_plus_bss_bytes']} bytes; ELF matches frozen v5.63 UF2")
    print("MQTT baseline payload bytes: " + ", ".join(str(c["payload_bytes"]) for c in result["mqtt_before"]["cases"]))
    print(f"Z retained by current serializer: {result['mqtt_before']['z_preserved']}")
    print(args.output)


if __name__ == "__main__":
    main()
