#!/usr/bin/env python3
"""Measure current declared application records; leave unknown contracts open.

No network, credentials, flashing or robot movement. Header inventory is a
review checklist, not proof that every declared type has a live allocation.
"""
import argparse,csv,hashlib,json,shutil,tempfile
from collections import Counter
from datetime import datetime,timezone
from pathlib import Path
from audit_data_representation import measure_layout

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=ROOT/'build/rtos-representation-stage2')
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
toolchain=ROOT/'build/dependencies/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi'
includes=['audit_config.h','adc12_filter.h','track_control.h','board.h','binary_protocol.h',
          'motion_runtime.h','network_service.h','runtime_report.h','scan_diagnostic.h','track_sensor.h']
with tempfile.TemporaryDirectory(prefix='rtos-fields-') as name:
    folder=Path(name);sources=[];hashes={}
    for directory in [ROOT/'firmware/app/robot_firmware',ROOT/'firmware/app/mission_rtos']:
        for p in directory.glob('*.h'):
            if p.name.startswith('network_settings') or p.name in ['calibration_page.h','stationary_page.h','lwipopts.h','dhcpserver.h']:continue
            dest=folder/p.name
            if dest.exists():raise ValueError('Header-name collision: '+p.name)
            shutil.copyfile(p,dest);sources.append(p.name)
            hashes[str(p.relative_to(ROOT))]=hashlib.sha256(p.read_bytes()).hexdigest()
    (folder/'audit_config.h').write_text('#define RTOS_TRACK 1\n#define RTOS_COMPACT_ADC_STATE 1\n#define RTOS_COMPACT_CORE_STATE 1\n#define CENTER_BASELINE_COMPACT 1\n#define BARCODE_WAVE_LAST_PASS 1\n#define BARCODE_WAVE_RECOVERY_OPTICS 1\n')
    data,rows=measure_layout('rtos_candidate',folder,includes,sources,
        toolchain/'bin/arm-none-eabi-gcc',shutil.which('clang'),toolchain/'bin/arm-none-eabi-objcopy')
    for row in rows:
        # Older audit heuristics are not accepted as current contracts. Keep
        # only the explicit bounds below and declared enum/Boolean domains.
        row.update(range_or_domain='Unresolved',required_or_proposed_bits='',status='REVIEW_REQUIRED',
                   next_action='Confirm range, precision, lifetime, allocation and writer/access contract')
        key=(row['type'],row['field'])
        known={
            ('adc12_filter_t','history'):('12 readings x12 bits,0..4095 each',144),
            ('adc12_filter_t','sum'):('0..49140',16),
            ('adc12_filter_t','state'):('next0..11:4 bits; filled0..12:4 bits',8),
            ('center_baseline_t','packed'):('31 levels0..1000 x10 bits; count5 and next5',320),
            ('center_baseline_t','last_us'):('uint64 capture time; no new mission-duration cap',64),
        }
        if row['type']=='track_control_t':
            bounds={'head':(511,9),'tail':(511,9),'event_head':(7,3),'event_tail':(7,3),'enabled':(1,1),'divider':(3,2)}
            high,bits=bounds[row['field']];known[key]=(f'0..{high}',bits)
        if key in known:
            domain,bits=known[key];row.update(range_or_domain=domain,required_or_proposed_bits=bits,
                status='CANDIDATE_SOFTWARE_VERIFIED',next_action='Hardware timing pending; validate owner/access proof before deployment')
        elif row['type']=='encoder_sample_t':
            row.update(range_or_domain='uint32 modulo counter, all bit patterns retained',required_or_proposed_bits=32,
                status='PRESERVED_COUNTER_CONTRACT',next_action='Do not truncate/reset shared raw counters; no assumed short mission lifetime')
        elif row['declared_type'] in ('bool','_Bool'):
            row.update(range_or_domain='false/true',required_or_proposed_bits=1,status='DOMAIN_KNOWN_REVIEW_STORAGE',
                next_action='Review writer ownership before grouping flags; a native Boolean is not automatically packed')
        elif row['declared_type'] in data['enums']:
            enum=data['enums'][row['declared_type']]
            row.update(range_or_domain='Declared enum values; check invalid input/transition handling',
                required_or_proposed_bits=enum['required_bits'],status='ENUM_DOMAIN_REVIEW_STORAGE',
                next_action='Confirm producer validation before narrowing; do not silently truncate invalid inputs')
        elif row['declared_type'].split('[')[0] in data['types']:
            row.update(status='CONTAINER_REVIEW_ALLOCATION',next_action='Review child type; do not add container and nested sizes twice')
        if row['type']=='barcode_signal_t':
            row.update(status='REFERENCE_DECLARATION',range_or_domain='Legacy general-purpose filter retained; candidate allocates adc12_filter_t',
                required_or_proposed_bits='',next_action='Keep reference behavior; do not count this declaration as a simultaneous candidate allocation')
        row['source']='candidate header: '+row['source']
    result=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),scope='Selected current application header declarations, compact ADC/core enabled',
        types=data['types'],enums=data['enums'],field_rows=len(rows),status_counts=dict(Counter(r['status'] for r in rows)),
        source_sha256=hashes,limits=[
            'This is a declared-type checklist, not whole-application compliance or a count of allocated objects.',
            'Private C-file records/globals, automatic locals, included-but-unused types and precision/lifetime proof require separate review.',
            'Candidate verified status means software equivalence/domain evidence; hardware timing is still pending.',
            'Native validation/staging records may need a wider domain than the checked internal/wire representation; document that purpose.',
            'Kernel/SDK records are not classified as application data here; task stack/pool budgeting remains separately measured.'])
    (args.output/'inventory.json').write_text(json.dumps(result,indent=2)+'\n')
    with (args.output/'fields.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]),lineterminator='\n');writer.writeheader();writer.writerows(rows)
    print(json.dumps(dict(types=len(data['types']),declared_fields=len(rows),status_counts=result['status_counts'],output=str(args.output))))
