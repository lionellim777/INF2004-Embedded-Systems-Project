#!/usr/bin/env python3
"""Reproduce the robot's proposed data budget; does not modify/flash firmware.

Uses checked-out hardware code, fetched teammate headers and the Week 6
architecture. Policies introduced by this proposal are explicitly labelled.
"""
from __future__ import annotations

import argparse
import json
import math
import pathlib
import random
import re
import subprocess
import tempfile
from collections import OrderedDict

ROOT = pathlib.Path(__file__).resolve().parents[1]
RECORDS: dict[str, dict] = OrderedDict()
SESSION_MS = 1_800_000  # existing Buddy 1 history contract
CPR = 2543
WHEEL_MM = 60
TRACK_MM = 115
SPEED_MAX = 500
TICKS_MM = CPR / (math.pi * WHEEL_MM)
RELATIVE_MAX = math.ceil(SPEED_MAX * 12.050 * TICKS_MM)
CUMULATIVE_MAX = math.ceil(SPEED_MAX * (SESSION_MS / 1000) * TICKS_MM)
TOKEN_MAX = SESSION_MS // 10  # proposed: one new token per Safety release
TX_MAX = SESSION_MS // 5  # proposed: at most one publication per comm release


def field(name, lo, hi, unit='', count=1, reason='', status='derived'):
    assert isinstance(lo, int) and isinstance(hi, int) and lo <= hi
    # Offset encoding, rather than relying on C signed bit-field behaviour.
    bits = (hi - lo).bit_length()
    return dict(name=name, minimum=lo, maximum=hi, unit=unit, count=count,
                bits_each=bits, logical_bits=bits * count, reason=reason,
                status=status)


def record(name, owner, fields, copies=1, note=''):
    bits = sum(f['logical_bits'] for f in fields)
    RECORDS[name] = dict(owner=owner, fields=fields, bits=bits,
                         bytes=(bits + 7) // 8, copies=copies, note=note)


def define_records():
    f = field
    stamp = lambda: f('captured_ms', 0, SESSION_MS, 'ms', reason='30-minute session in wifi-buddy-1')
    token = lambda name: f(name, 0, TOKEN_MAX, reason='one allocation per 10 ms Safety release; stop before session expiry', status='proposed enforced policy')
    record('mission_snapshot', 'Safety / Mission', [stamp(),
        f('mission_state', 0, 8, reason='nine states in docs/architecture.md'),
        f('fault_mask', 0, 65535, reason='16 independently settable causes, listed in report'),
        f('stop_latched', 0, 1)])
    record('motion_snapshot', 'Buddy 2', [stamp(),
        f('wheel_speed', -500, 500, 'mm/s', 2, 'Tribunal measured-speed proposal; reject invalid/overspeed samples', 'provisional physical envelope'),
        f('cumulative_count', -CUMULATIVE_MAX, CUMULATIVE_MAX, 'ticks', 2,
          'ceil(500 mm/s * 1800 s * 2543/(pi*60)); counted travel, not displacement', 'provisional physical envelope'),
        f('motion_state', 0, 5, reason='IDLE/RUNNING/PAUSED/DONE/ABORTED/FAULT', status='proposed enum'),
        token('command_id'), token('generation'),
        f('valid', 0, 1), f('moving', 0, 1), f('slipping', 0, 1)])
    record('line_snapshot', 'Buddy 3', [stamp(),
        f('raw_adc', 0, 4095, 'ADC counts', 3, '12-bit ADC input; preserve full sensor output'),
        f('position', -1000, 1000, 'normalised', reason='proposed bounded line-position interface', status='proposed numeric contract'),
        f('sensor_pattern', 0, 7, reason='three independently thresholded sensors'),
        f('barcode', 0, 27, reason='NONE, A-Z, UNREADABLE: preserve existing A-Z decoder'),
        f('valid', 0, 1), f('junction_candidate', 0, 1)])
    record('terrain_snapshot', 'Buddy 4', [stamp(),
        f('pitch_tenths', -1800, 1800, '0.1 degree', reason='calibrated tilt is difference of two +/-90 degree angles', status='proposed quantisation'),
        f('rise_mm', -5000, 5000, 'mm', reason='10 s event * 500 mm/s bounds absolute integrated rise', status='proposed numeric contract'),
        f('highest_rise_mm', 0, 5000, 'mm', reason='same bound as current completed event', status='proposed numeric contract'),
        f('yaw_tenths', -math.ceil(1000 / TRACK_MM * 180 / math.pi * 10),
          math.ceil(1000 / TRACK_MM * 180 / math.pi * 10), '0.1 degree/s',
          reason='(vR-vL)/115 mm; each wheel +/-500 mm/s', status='provisional physical envelope'),
        f('motion_class', 0, 6, reason='seven values in telemetry_motion_state_t'),
        f('hump_count', 0, 3000, count=2, reason='count and highest index; 30 min / (300 ms climb + 300 ms descent)', status='requires enforcement in integrated state machine'),
        f('validity', 0, 1, count=6, reason='available/calibrated/pitch/rise/yaw/class'),
        f('phase', 0, 2, reason='WAITING/CLIMBING/DESCENDING')])
    record('obstacle_snapshot', 'Buddy 5', [stamp(), token('scan_id'), token('generation'),
        f('closest_mm', 0, 1000, 'mm', reason='existing obstacle_data.h'),
        f('width_mm', 0, math.ceil(2 * 1000 * math.sin(math.radians(75 / 2))), 'mm',
          reason='maximum endpoint chord for 1 m ranges spanning 50..125 degrees; projected width <= chord', status='geometry envelope; physical width remains estimated'),
        f('side_clearance_mm', 0, 1000, 'mm', 2, 'same radial range envelope; no-echo remains unavailable'),
        f('coverage_edge_index', 0, 15, count=2, reason='first and last covered fine angle'),
        f('nearest_index', 0, 15), f('status', 0, 3),
        f('scan_state', 0, 4, reason='idle/settling/waiting/coarse/fine', status='proposed enum')])
    # Timestamps are retained at their real precision. This is not the untested
    # 10-bit-ms proposal copied into the Tribunal document.
    record('barcode_capture', 'Buddy 3', [
        f('stripe_us', 0, 750000, 'us', 29, 'existing 750 ms timeout, unchanged microsecond precision'),
        f('edge_us', 0, SESSION_MS * 1000, 'us'),
        f('last_emit_us', 0, SESSION_MS * 1000, 'us'),
        f('run_count', 0, 29), f('last_letter', 0, 26),
        f('black_active_armed', 0, 1, count=3)])
    record('acceleration_filter', 'Buddy 4', [
        f('raw_acceleration', -2048, 2047, 'sensor LSB', 24,
          'eight samples x three signed 12-bit axes; existing right-shift by four is lossless')])
    record('terrain_working_state', 'Buddy 4', [
        f('filter_next', 0, 7), f('filter_count', 0, 8),
        f('level_pitch_hundredths', -9000, 9000, '0.01 degree',
          reason='atan2 pitch +/-90 degrees; <=0.005 degree stored-offset rounding', status='proposed quantisation'),
        f('next_event_phase_level_ms', 0, SESSION_MS + 10000, 'ms', 4,
          'next sample / event start / phase start / level-since; explicit active flags replace zero sentinels'),
        f('level_timer_active', 0, 1),
        f('climb_descent_tenths', -1800, 1800, '0.1 degree', 2, status='proposed quantisation'),
        f('rise_hundredths_mm', -500000, 500000, '0.01 mm',
          reason='10 s x 500 mm/s; <=500 updates x 0.005 mm = 2.5 mm rounding bound', status='proposed accumulator precision')])
    record('motion_working_state', 'Buddy 2', [
        f('baseline_count', -CUMULATIVE_MAX, CUMULATIVE_MAX, 'ticks', 2,
          'subtract stable cumulative baselines; do not reset counters in the ISR'),
        f('relative_count', -RELATIVE_MAX, RELATIVE_MAX, 'ticks', 2,
          '12 s manoeuvre plus proposed 50 ms stop allowance at +/-500 mm/s; overflow guard required', 'provisional physical envelope'),
        f('previous_control_ms', 0, SESSION_MS, 'ms'),
        f('integral_millicounts', -2500000, 2500000, '0.001 count', 2,
          'existing integral clamp +/-2500; <=1200 steps x 0.0005 count x Ki 0.000015 is <0.06 PWM tick at 6250 ticks', 'proposed quantisation; PI tests required'),
        f('motor_pwm_level', 0, 6250, 'PWM ticks', 2,
          '125 MHz / 20 kHz = 6250 levels; pin/check clock before applying'),
        f('motor_direction', 0, 1, count=2),
        f('next_motor_release_ms', 0, SESSION_MS + 10, 'ms')])
    # Each ISR wheel uses two words, not an unsafe cross-word shared bitstream.
    record('encoder_isr_wheel', 'Buddy 2 ISR', [
        f('count', -CUMULATIVE_MAX, CUMULATIVE_MAX, 'ticks'),
        f('previous_ab', 0, 3), f('invalid_seen', 0, 1),
        f('last_edge_us', 0, SESSION_MS * 1000, 'us')], copies=2,
        note='Allocate 8 B per wheel: count/AB/flag word and edge-time word; snapshot under short ISR-safe exclusion. Diagnostic transition totals need a separate optional debug counter.')
    record('scan_points', 'Buddy 5', [
        f('distance_mm', 0, 1000, 'mm', 16), f('range_status', 0, 3, count=16)],
        note='Index is implicit: angle=50+5*index. 16 x 12 bits = 24 B. A transmitted individual point carries its index and uses 16 bits / 2 B.')
    record('sonar_transaction', 'Buddy 5 ISR / task', [
        f('echo_start_us', 0, SESSION_MS * 1000, 'us'),
        f('echo_width_us', 0, 30000, 'us', reason='existing hard timeout, not the shorter 25 ms design target'),
        f('transaction_state', 0, 3), f('close_readings', 0, 2),
        f('point_count', 0, 16), f('servo_index', 0, 15),
        f('next_range_settle_deadline_ms', 0, SESSION_MS + 30000, 'ms', 3)])
    record('mission_working_state', 'Safety / Mission', [token('active_id'), token('generation'),
        f('lease_deadline_ms', 0, SESSION_MS + 100, 'ms'),
        f('last_line_ms', 0, SESSION_MS, 'ms'),
        f('previous_buttons', 0, 1, count=2), f('last_seen_side', 0, 2),
        f('line_ever_seen', 0, 1), f('recovery_attempt', 0, 3,
            reason='proposed maximum three attempts; each still needs a time/distance bound', status='proposed enforced policy')])
    record('wheel_history', 'Buddy 1', [
        f('speed', -500, 500, 'mm/s', 26),
        f('capture_ms', 0, SESSION_MS, 'ms', 26),
        f('head', 0, 25), f('count', 0, 26), f('recovery_pending', 0, 1)], copies=2,
        note='Existing capacity and behaviour preserved; 26 x 31 + 11 = 817 bits -> 103 B each. Existing word-array implementation is 108 B each on ARM.')
    record('comm_working_state', 'Buddy 1', [
        f('state', 0, 5), f('publish_kind', 0, 3),
        f('flags', 0, 1, count=11, reason='eleven booleans in comm_runtime_t'),
        f('sequence', 0, TX_MAX),
        f('absolute_deadlines', 0, SESSION_MS + 30000, 'ms', 6,
          'deadline/retry/online-since/status/telemetry timestamps plus receive expiry'),
        f('retry_stage', 0, 5, reason='1,2,4,8,16,30 second backoff: six values'),
        f('current_and_pending_period', 200, 5000, 'ms', 2),
        f('subscription_count', 0, 2),
        f('reconnect_count', 0, 1801, reason='one-second minimum backoff during 30 min plus initial attempt'),
        f('dropped_count', 0, 837000, reason='proposed <=465 producer records/s x 1800 s; reject above envelope', status='proposed enforced policy'),
        f('last_error', 0, 9, reason='actual comm_fail codes 1..9 and zero'),
        f('task_ticks', 0, TX_MAX, reason='at most one release per 5 ms'),
        f('event_head', 0, 15), f('event_count_and_peak', 0, 16, count=2),
        f('boot_id', 0, 2**32 - 1, reason='retain existing get_rand_32 full-domain boot identity'),
        f('rx_received_and_expected', 0, 12, 'bytes', 2,
          'new largest binary command size; reject bigger frames before copying', 'proposed protocol')])
    record('motion_command', 'Safety -> Motion', [
        f('operation', 0, 7, reason='NONE/FORWARD/REVERSE/LEFT/RIGHT/UTURN/PAUSE/STOP; parse failure is a separate result'),
        f('distance_mm', 0, 1000, 'mm'), f('speed_mm_s', -150, 150, 'mm/s'),
        f('angle_deg', 0, 180, 'degree', reason='retain arbitrary bypass turns, not only barcode 90/180; direction in operation', status='proposed precision'),
        token('id'), token('generation'), f('issued_ms', 0, SESSION_MS, 'ms')])
    record('scan_command', 'Safety -> Scan', [token('id'), token('generation'), stamp()])
    record('event_record', 'Subsystems -> Safety / Comms', [
        f('kind', 0, 7, reason='barcode/hump/scan-point/profile/fault/motion-done/line-lost/line-found'),
        f('source', 0, 4), stamp(), token('id'), token('generation'),
        f('variant_payload', 0, 2**45 - 1,
          reason='largest variant: 10-bit closest + 11-bit width + two 10-bit clearances + four validity bits; other variants zero unused bits', status='proposed discriminated union')])
    record('mqtt_header', 'Buddy 1 -> subscriber', [
        f('type', 0, 7), f('boot_id', 0, 2**32 - 1),
        f('sequence', 0, TX_MAX), stamp()],
        note='Version is fixed by a /v1/ topic, not repeated as an arbitrary integer. Explicit bytes and lengths, never strlen on binary data.')
    record('mqtt_command_payload', 'Subscriber -> Buddy 1', [
        f('kind', 0, 2, reason='STATUS / PERIOD / STOP; no remote steering'),
        f('period_offset', 0, 4800, 'ms', reason='actual 200..5000 interval; encode period-200')])


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True)


def arm_baseline_sizes():
    """Compile actual public headers for Cortex-M0+; no SDK or host ABI guess."""
    with tempfile.TemporaryDirectory(prefix='inf2004-layout-') as td:
        p = pathlib.Path(td)
        for h in ('telemetry.h', 'command.h', 'comm.h'):
            (p / h).write_text(run('git', 'show', f'origin/wifi-buddy-1:include/{h}'))
        types = ['telemetry_wheel_history_t', 'telemetry_mission_t', 'telemetry_motion_t',
                 'telemetry_line_t', 'telemetry_terrain_t', 'telemetry_obstacle_t',
                 'telemetry_snapshot_t', 'telemetry_sample_t', 'telemetry_event_t',
                 'command_request_t', 'command_frame_t', 'comm_config_t', 'comm_status_t',
                 'barcode_reader_t', 'navigation_manoeuvre_t', 'terrain_status_t']
        src = '#include "comm.h"\n#include "command.h"\n#include "barcode_reader.h"\n#include "navigation_commands.h"\n#include "terrain.h"\n'
        src += '\n'.join(f'const unsigned audit_{t} = sizeof({t});' for t in types)
        (p / 'probe.c').write_text(src)
        app = ROOT / 'firmware/app/robot_firmware'
        subprocess.run(['clang', '--target=arm-none-eabi', '-mcpu=cortex-m0plus', '-mthumb',
                        '-ffreestanding', '-std=c11', '-I', str(p), '-I', str(app),
                        '-S', str(p / 'probe.c'), '-o', str(p / 'probe.s')], check=True)
        asm = (p / 'probe.s').read_text()
        return {t: int(re.search(r'^audit_' + t + r':\s*\n\s*\.long\s+(\d+)', asm, re.M)[1]) for t in types}


def self_check():
    rng = random.Random(2004)
    fields = 0
    for r in RECORDS.values():
        offsets = []
        cursor = 0
        for f in r['fields']:
            lo, hi, width = f['minimum'], f['maximum'], f['bits_each']
            assert (1 << width) >= hi - lo + 1
            assert width == 0 or (1 << (width - 1)) < hi - lo + 1
            for value in (lo, hi, (lo + hi) // 2, rng.randint(lo, hi)):
                encoded = value - lo
                decoded = encoded + lo
                assert 0 <= encoded < (1 << width) and decoded == value
            # Unused code points are invalid, not silently clamped.
            assert hi + 1 - lo > hi - lo and lo - 1 - lo < 0
            fields += f['count']
            for index in range(f['count']):
                offsets.append((cursor, f))
                cursor += width
        # Full-record binary round trips exercise signed offsets and crossings
        # of byte/word boundaries, not just the arithmetic bit totals.
        for trial in range(12):
            packed = 0
            expected = []
            for offset, f in offsets:
                value = (f['minimum'] if trial == 0 else f['maximum'] if trial == 1
                         else rng.randint(f['minimum'], f['maximum']))
                packed |= (value - f['minimum']) << offset
                expected.append(value)
            wire = packed.to_bytes(r['bytes'], 'little')
            independent = int.from_bytes(wire, 'little')
            for (offset, f), value in zip(offsets, expected):
                code = (independent >> offset) & ((1 << f['bits_each']) - 1)
                assert code <= f['maximum'] - f['minimum']
                assert code + f['minimum'] == value
            assert independent >> r['bits'] == 0
            # Independent validation of malformed records, not just valid
            # round trips. A value in spare codes must never look valid.
            def valid_wire(data):
                if len(data) != r['bytes']:
                    return False
                x = int.from_bytes(data, 'little')
                if x >> r['bits']:
                    return False
                return all(((x >> off) & ((1 << fld['bits_each']) - 1)) <=
                           fld['maximum'] - fld['minimum'] for off, fld in offsets)
            assert valid_wire(wire)
            assert not valid_wire(wire[:-1])
            assert not valid_wire(wire + b'\0')
            if r['bits'] % 8:
                assert not valid_wire((packed | (1 << r['bits'])).to_bytes(r['bytes'], 'little'))
            for off, fld in offsets:
                bad_code = fld['maximum'] - fld['minimum'] + 1
                if bad_code < (1 << fld['bits_each']):
                    mask = ((1 << fld['bits_each']) - 1) << off
                    malformed = (packed & ~mask) | (bad_code << off)
                    assert not valid_wire(malformed.to_bytes(r['bytes'], 'little'))
    assert RELATIVE_MAX > 32767 and (2 * RELATIVE_MAX).bit_length() == 18
    assert RECORDS['scan_points']['bytes'] == 24
    assert RECORDS['wheel_history']['bytes'] == 103
    # Independently verify the complete concatenated MQTT snapshot length,
    # rather than summing padded in-memory record sizes.
    wire_bits = sum(RECORDS[n]['bits'] for n in ('mqtt_header','mission_snapshot',
                    'motion_snapshot','line_snapshot','terrain_snapshot','obstacle_snapshot'))
    assert wire_bits == 552 and (wire_bits + 7) // 8 == 69
    return fields


def storage_budget():
    persistent = ['mission_snapshot', 'motion_snapshot', 'line_snapshot',
                  'terrain_snapshot', 'obstacle_snapshot', 'barcode_capture',
                  'acceleration_filter', 'terrain_working_state', 'motion_working_state',
                  'encoder_isr_wheel', 'scan_points', 'sonar_transaction',
                  'mission_working_state', 'wheel_history', 'comm_working_state']
    rows = []
    for name in persistent:
        r = RECORDS[name]
        size = 8 if name == 'encoder_isr_wheel' else r['bytes']
        rows.append(dict(name=name, copies=r['copies'], bytes_each=size,
                         total=size*r['copies'], allocation='application static'))
    for name, depth in [('motion_command', 4), ('scan_command', 4), ('event_record', 8)]:
        size = RECORDS[name]['bytes']
        stride = 4 + ((size + 3) // 4) * 4
        rows.append(dict(name=name + '_MBF', copies=depth, bytes_each=stride,
                         total=depth*stride, allocation='inside kernel pool'))
    size = RECORDS['event_record']['bytes']
    rows.append(dict(name='network_event_ring', copies=16, bytes_each=size,
                     total=16*size, allocation='application static'))
    tx_bits = RECORDS['mqtt_header']['bits'] + sum(RECORDS[n]['bits'] for n in persistent[:5])
    tx = (tx_bits + 7) // 8
    rx = (RECORDS['mqtt_header']['bits'] + RECORDS['mqtt_command_payload']['bits'] + 7) // 8
    # Preserve the current API string capacities; do not inspect credentials.
    for name, size in [('network_configuration', 241), ('topic_scratch', 80),
                       ('persistent_will_topic', 80), ('client_id', 48),
                       ('mqtt_output', tx), ('mqtt_input', rx),
                       ('kernel_handles', 15*4)]:
        rows.append(dict(name=name, copies=1, bytes_each=size, total=size,
                         allocation='application static'))
    return rows, tx, rx


def write_outputs(outdir):
    define_records()
    checked = self_check()
    sizes = arm_baseline_sizes()
    rows, tx, rx = storage_budget()
    app_unaligned = sum(r['total'] for r in rows if r['allocation'] == 'application static')
    app = (app_unaligned + 3) // 4 * 4
    mbf = sum(r['total'] for r in rows if r['allocation'] == 'inside kernel pool')
    revisions = {b: run('git','rev-parse',b).strip() for b in
                 ('origin/main','origin/buddy-5','origin/buddy3-barcode','origin/wifi-buddy-1')}
    result = dict(scope='PROPOSED integrated application storage, not a linked firmware measurement',
                  source_revisions=revisions, bounds=dict(session_ms=SESSION_MS,
                  counts_per_revolution=CPR,wheel_diameter_mm=WHEEL_MM,
                  relative_count_max=RELATIVE_MAX,cumulative_count_max=CUMULATIVE_MAX),
                  records=RECORDS,arm_abi_current_types=sizes,allocations=rows,
                  application_static_bytes=app,application_field_allocation_bytes=app_unaligned,
                  aggregate_alignment_bytes=app-app_unaligned,kernel_message_buffers_bytes=mbf,
                  mqtt_snapshot_bytes=tx,mqtt_command_bytes=rx,
                  checked_field_instances=checked)
    outdir.mkdir(parents=True,exist_ok=True)
    (outdir/'minimum-bit-budget.json').write_text(json.dumps(result,indent=2)+'\n')
    lines = ['# Minimum-bit data budget: AAI10', '',
             '**Status: proposed full layout; partial implementation only, not hardware-validated.**', '',
             'See [implementation progress](implementation-progress.md) for changes actually made and checks completed. The 1,352-byte target is not a linked firmware measurement.', '',
             'Generated by `python3 tools/bit_budget.py`. Current public struct sizes were cross-compiled for ARM Cortex-M0+, not inferred from the Mac ABI.', '',
             '## Calculated allocation', '',
             f'- Application-owned persistent records, queues, config and working buffers: **{app} bytes ({app*8} storage bits)**: {app_unaligned} field/buffer bytes + {app-app_unaligned} aggregate alignment bytes.',
             f'- Three µT-Kernel message buffers: **{mbf} bytes**, allocated inside the existing 96 KiB kernel pool; do not add them to the pool twice.',
             '- Six initial task stack allowances: 5 x 2 KiB + 8 KiB = **18,432 bytes**, also inside that pool. The 1,024-byte init-task stack and dynamic allocator overhead also consume that pool. Kernel control-block tables and the 256-byte temporary stack are separate static storage; do not hide them inside the pool.',
             f'- Kernel pool + calculated application static allocation: **{98304+app} bytes**. This is a subtotal, NOT total firmware RAM; separate kernel tables, SDK/CYW43/lwIP objects, SDK main/interrupt stack and C heap are additional.',
             f'- SRAM remaining after that subtotal: **{270336-98304-app} bytes** for those additional allocations and free margin. This is not a measurement of free RAM.',
             f'- Complete five-source MQTT snapshot: **{tx} application payload bytes**; event packet: **{(RECORDS["mqtt_header"]["bits"]+RECORDS["event_record"]["bits"]+7)//8} bytes**; largest diagnostic/STOP command: **{rx} bytes**. MQTT/TCP/IP/Wi-Fi headers are extra.', '',
             'The payload is not the previously discussed 7-byte/55-bit subset. It includes all five timestamped subsystem snapshots and replay identity. Owner records are byte-aligned in memory; the wire serializer removes internal padding, and unused final bits must be zero.', '',
             '## Limits that determine the numbers', '',
             f'- Wheel circumference = pi x 60 = {math.pi*60:.6f} mm; counts/mm = 2543/circumference = {TICKS_MM:.9f}. A 1,000 mm command needs up to {math.ceil(1000*TICKS_MM):,} ticks.',
             f'- A 12 s manoeuvre plus 50 ms stopping allowance at 500 mm/s needs +/-{RELATIVE_MAX:,} ticks -> **18 bits**. At 150 mm/s and 12 s it needs +/-{math.ceil(150*12*TICKS_MM):,}; the Tribunal +/-30,000 proposal works only for that narrower envelope.',
             f'- A 30-minute session at 500 mm/s gives +/-{CUMULATIVE_MAX:,} cumulative ticks -> **25 bits per wheel**. This is total counted travel, not just net displacement. Enforce session/rate/range checks; the existing code does not enforce this envelope.',
             '- Maximum wheel speed 500 mm/s is a proposed software envelope, not a measured hardware maximum. Encoder glitches can violate the envelope; range checks and a latched fault must run before arithmetic overflows. Hardware qualification is still needed.',
             '- Preserve barcode precision: 0..750,000 microseconds -> **20 bits**, 29 widths -> **580 bits / 73 bytes**. The full reader state is listed below. The 10-bit millisecond alternative is deferred until its quantisation error is validated.',
             '- Keep the current A-Z decoder: NONE + 26 letters + UNREADABLE = 28 values -> **5 bits**, not 3. A-D navigation commands remain 3 bits for six states.',
             '- Highest rise cannot safely be capped at 63 mm just to use six bits. At the current 10 s event limit and proposed 500 mm/s maximum, path length and therefore absolute rise are bounded by **5,000 mm**: 13 bits for nonnegative peak, 14 bits for signed current rise.',
             '- Raw accelerometer samples already contain 12 signed bits per axis. The eight-sample, three-axis filter needs **288 bits / 36 bytes**, preserving current raw resolution instead of storing 96 bytes of floats.',
             '- 16 fine-scan points: each has 10 distance bits + 2 status bits. Store the angle implicitly by array position: **192 bits / 24 bytes**. Individual transmitted points include their 4-bit index: **16 bits / 2 bytes**.', '',
             '## Record sizes', '', '| Record | Owner | Useful bits per record | Packed bytes | Stored copies |', '|---|---|---:|---:|---:|']
    for name,r in RECORDS.items():
        lines.append(f'| {name} | {r["owner"]} | {r["bits"]} | {r["bytes"]} | {r["copies"]} |')
    lines += ['', 'Do not sum every row as a single telemetry frame: working state stays local; command/event/header rows have separate capacities below.', '',
              '## Actual storage allocation, including multiplicity', '', '| Allocation | Copies | Bytes each | Total bytes | Location |', '|---|---:|---:|---:|---|']
    for r in rows:
        lines.append(f'| {r["name"]} | {r["copies"]} | {r["bytes_each"]} | {r["total"]} | {r["allocation"]} |')
    lines += ['', 'µT-Kernel at commit 15ed232 uses a 4-byte message length and rounds payload storage to four bytes: `slot = 4 + 4*ceil(payload_bytes/4)`. Kernel object tables are separate static objects. Dynamic pool allocator overhead is additional to the three buffer payload capacities, within the pool.', '',
              '## Current ARM type sizes (compiled from the fetched headers)', '', '| Current C type | ARM bytes |', '|---|---:|']
    for n,size in sizes.items(): lines.append(f'| {n} | {size} |')
    lines += ['', f'The five current telemetry structs total {sizes["telemetry_snapshot_t"]} bytes; the proposed five owner records total {sum(RECORDS[n]["bytes"] for n in ("mission_snapshot","motion_snapshot","line_snapshot","terrain_snapshot","obstacle_snapshot"))} bytes. Their concatenated fields plus the wire header occupy {tx} bytes. This is a proposed storage/protocol comparison, not a measured JSON payload comparison.',
              f'The current barcode reader is {sizes["barcode_reader_t"]} bytes; the proposed reader is {RECORDS["barcode_capture"]["bytes"]} bytes. Two current histories use {2*sizes["telemetry_wheel_history_t"]} bytes; the proposed bitstream histories use {2*RECORDS["wheel_history"]["bytes"]} bytes.', '',
              '## Complete proposed field manifest', '', '| Record.field | Count | Inclusive encoded range | Unit | Bits each | Basis / status |', '|---|---:|---|---|---:|---|']
    for n,r in RECORDS.items():
        for f in r['fields']:
            lines.append(f'| {n}.{f["name"]} | {f["count"]} | {f["minimum"]}..{f["maximum"]} | {f["unit"]} | {f["bits_each"]} | {f["reason"]}; {f["status"]} |')
    lines += ['', '## Implementation rules and unresolved measurements', '',
              '1. This manifest defines offset encoding for signed fields: code=value-minimum. Preserve Justin\'s existing two\'s-complement speed history until both endpoints explicitly migrate. Reject reserved code points, malformed lengths, stale generations and out-of-range values; do not truncate or clamp silently.',
              '2. The thirty-minute bound is inherited from Buddy 1 history. Add a coordinated session-expiry policy: stop while keeping diagnostics alive, clear queues/history while stopped, establish a new boot/session identity and rearm explicitly. The current branch merely rejects late valid history samples; it does not stop the car. If continuous operation is required, redesign clock/counter epochs rather than quietly imposing this limit.',
              '3. Count both encoder wheels and ISR timestamps consistently under a short single-core interrupt-safe critical section. Keep two 32-bit words per wheel; mask/validate the meaningful bits. Control owners alone update packed state; readers copy under short locks and release before networking. Packing does not itself prove concurrency safety.',
              '4. Sixteen fault bits: left stall, right stall, left encoder, right encoder, motion timeout, representation range, IR, IMU read, calibration, sonar hardware, Wi-Fi loss, buffer overflow, stale input, protocol, brownout/reset indication and missed deadline. Missing sonar echo is a range status, not automatic hardware failure. Wi-Fi-loss severity must not stop otherwise safe local control.',
              '5. Width/clearance and rise/yaw figures are estimates with independent validity. Test geometry, acceleration artefacts, slip and freshness. The width bound applies to the stated radial scan geometry, not proof of a physically clear bypass.',
              '6. Decoder sorting scratch, calibration sums, PI multiply/divide intermediates, printf/atan2 library work and API structs live on task stacks or in registers. Do not add them again to static RAM. The separately listed transient-value inventory gives their domains. Float library work is not a claim of minimum-bit completion; retain until fixed-point error bounds and execution cost are validated.',
              '7. Constants (GPIOs, fixed PWM slice/channel mappings, scan angle table, PI gains, IR calibration constants and Code39 patterns) should remain in flash or be derived. Flash tables are not a RAM saving when repacked. Dynamic calibration records need their own persistent flash format when implemented.',
              '8. Keep native pointer/kernel-ID/lwIP types as required by their APIs. Application payloads use explicit byte arrays; a compiler-generated C bitfield layout is not a portable network format. The 60-byte kernel handle allowance assumes six task IDs, five snapshot mutex IDs plus one communications/history mutex, and three MBF IDs. The pinned BSP currently allows only four mutexes: raise CNF_MAX_MTXID to at least six in the generated integrated configuration. SDK client objects/IP structures and kernel object tables are excluded from the application subtotal and require the integrated link map.',
              '9. Firmware images are currently separate: main robot polling prototype, standalone calibrated motion test and Buddy 1 RTOS/Wi-Fi demo. There is no linked six-task end-state image to measure yet. Do not add their BSS totals as if simultaneously resident. Existing 157,676-byte BSS in Buddy 1 docs is historical, not a fresh total for its latest commit.',
              '10. Integrate the owner modules, build before/after with identical SDK/compiler/flags, report .data+.bss+other allocated SRAM, inspect fixed pool use, heap and stack peaks under load, then record actual message lengths and timing. None of those hardware results has been claimed by this calculation.', '',
              '## Transient-value and constant inventory', '',
              '| Value family in existing code | Bound / smallest mathematical width | Storage treatment |',
              '|---|---|---|',
              '| Eight ADC sample sum | 0..8*4095 = 32760: 15 bits | Checked integer temporary; inside Line task stack/registers |',
              '| Blackness numerator | (raw-white)*1000; conservative -4095000..4095000: 23 bits | Existing int32 arithmetic is sufficient; 10-bit result stored only if needed |',
              '| Left-right line error | -1000..1000: 11 bits | Derive from the two normalised values |',
              '| Threshold count | 0..3: 2 bits | Derive sensor pattern/popcount; no extra snapshot field |',
              '| Barcode sorting and direction buffers | 9 widths at 20 bits: 23 B each; three thresholds at 20 bits: 8 B; three patterns at 9 bits: 4 B | Task scratch; use getter-based decoder to avoid expanding a 73 B packed capture into a 116 B duplicate |',
              '| Barcode ratio product | 750000*18 = 13500000: 24 unsigned bits | Checked uint32 temporary is sufficient; no uint64 needed for these bounded inputs |',
              '| Code39 pattern ROM | 26 patterns * 9 bits = 234 bits / 30 B | Existing 52 B table is flash const; savings here are FLASH, not SRAM |',
              '| Calibration vector sum | 100 signed 12-bit samples: -204800..204700, 19 bits/axis | Three checked int32 accumulators; do not retain 100 samples |',
              '| Eight-sample vector sum | -16384..16376, 15 bits/axis | Three checked int32 temporaries, derive mean |',
              '| I2C input transaction | Six bytes supplied by the sensor | Fixed hardware/API transport buffer; unpack three signed 12-bit values afterward |',
              '| Sonar conversion product | 30000*343+1000 = 10291000: 24 bits | Checked uint32 temporary; bounded echo input |',
              '| PI error, gains and output calculation | Depends on measured CPS, actual dt and chosen fixed-point scaling | Constants stay in flash; clamp/check intermediates. Integral-storage precision is proposed above; final arithmetic/stack/runtime proof remains pending |',
              '| State strings / requested barcode angle | Nine vehicle states: 4 bits; six navigation states: 3 bits; A/B/C/D angles derived from command | Diagnostic string lookup in flash, not a writable pointer/string state; avoid storing a duplicate floating-point turn angle for these four commands |',
              '| GPIO IDs, PWM slices/channels, motor polarity | Compile-time assigned wiring | Derive/register constants; no writable per-motor config struct required for fixed wiring |',
              '| PWM wrap, duty and servo pulse | At pinned 125 MHz: wrap6249/duty0..6250 ->13 bits; servo500..2500 us ->11 bits | Configure clocks once; keep native hardware call argument widths at the call boundary; derive servo angle/pulse from fine index |',
              '| SDK/kernel pointers and API return/error values | Defined by actual external API | Native ABI width; not truncated into application fields. Convert to bounded application status only at the interface |',
              '| Calibration constants and standalone flash test logs | Existing IR constants/gains/geometry are compile-time or flash data | No RAM charge for flash constants; standalone flash result recorder is not part of the integrated production image |', '',
              'This is the complete proposed persistent record set plus a source-based transient/constant inventory, not a claim that all arithmetic, SDK storage or physical accuracy has already been qualified.', '',
              '## Next executable work', '',
              '- Agree/enforce the session, speed and count envelope; replace unsafe signed cumulative overflow and define coordinated stopped-session reset.',
              '- Implement shared codecs from this manifest, including reserved-code and truncation tests. Start with existing obstacle packing and lossless barcode/ADC/raw-acceleration packing.',
              '- Port calibrated nonblocking Motion into the six-task architecture; keep generation/stop/lease handling separate from network queues.',
              '- Replace JSON and strlen-based MQTT publication with explicit binary lengths and a matching laptop decoder. Replace simulated samples one owner at a time.',
              '- Measure integrated RAM/stack/heap and control deadlines; resize the 96 KiB pool only after allocator and stack evidence.', '',
              '## Source revisions', '']
    lines += [f'- `{b}`: `{sha}`' for b,sha in revisions.items()]
    lines += ['', 'Sources: repository C/header files above; [Week 6 report](https://docs.google.com/document/d/1HhiiZjk1UzZ13rGOs-Ni5hidsrhiB7DuXsaZjLgL0co/edit) task/queue/stack/interface definitions; Change Requirement 1.docx; [official micro T-Kernel port](https://github.com/tron-forum/mtk3_bsp/tree/15ed232c08f2d89e54e514db79de150e224307e3), messagebuf.h/.c and config.h. Proposed fields and limits are labelled individually. No hardware was flashed or moved.', '']
    (outdir/'minimum-bit-budget.md').write_text('\n'.join(lines))
    # Emit byte-array layout with ARM static assertions; this is a planning
    # proof only and intentionally not linked into robot_firmware.
    c = ['#include <stdint.h>', '/* Generated proposal; not production firmware. */']
    for n,r in RECORDS.items():
        c += [f'typedef struct {{ uint8_t data[{r["bytes"]}]; }} budget_{n}_t;',
              f'_Static_assert(sizeof(budget_{n}_t)=={r["bytes"]}, "{n}");']
    c += ['typedef struct {', '    volatile uint32_t encoder_words[4];',
          '    uint32_t kernel_handles[15];']
    for a in rows:
        if a['allocation'] == 'application static' and a['name'] not in ('encoder_isr_wheel','kernel_handles'):
            c.append(f'    uint8_t {a["name"]}[{a["total"]}];')
    c += ['} budget_application_storage_t;',
          f'_Static_assert(sizeof(budget_application_storage_t)=={app}, "aggregate ARM storage budget");']
    (outdir/'minimum-bit-layout-probe.c').write_text('\n'.join(c)+'\n')
    subprocess.run(['clang','--target=arm-none-eabi','-mcpu=cortex-m0plus','-mthumb',
                    '-ffreestanding','-std=c11','-fsyntax-only',str(outdir/'minimum-bit-layout-probe.c')],check=True)
    print(json.dumps(dict(checked_field_instances=checked,record_layouts=len(RECORDS),
                         application_static_bytes=app,kernel_message_buffers_bytes=mbf,
                         mqtt_snapshot_bytes=tx,mqtt_command_bytes=rx,
                         relative_count_max=RELATIVE_MAX,cumulative_count_max=CUMULATIVE_MAX),indent=2))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=pathlib.Path,default=ROOT/'docs/data-budget')
    args=parser.parse_args()
    write_outputs(args.out)
