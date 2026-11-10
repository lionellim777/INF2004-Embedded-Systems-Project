#!/usr/bin/env python3
"""Measure already-built minimum-bit candidates; never build, flash or move.

Private configured images are read only to compare their linked contents and
hashes. Output contains public layouts/symbol sizes, not image bytes/settings.
The default-control build must use the reference build date (SOURCE_DATE_EPOCH)
so automatic Pico binary-info date text does not confound exact comparison.
"""
import argparse,hashlib,json,re,struct,subprocess,tempfile
from datetime import datetime,timezone
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/dependencies/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi/bin'
APP=ROOT/'firmware/app/mission_rtos';BASE=ROOT/'firmware/app/robot_firmware'
def run(*args):return subprocess.check_output([str(a) for a in args],cwd=ROOT,text=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def image(folder,uf2):
    elf=folder/'robocar_rtos_motion.elf';raw=(folder/uf2).read_bytes()
    text,data,bss=map(int,run(BIN/'arm-none-eabi-size',elf).splitlines()[1].split()[:3])
    chunks=[];assert len(raw)%512==0
    for offset in range(0,len(raw),512):
        block=raw[offset:offset+512];a,b,flags,address,length,index,total,family=struct.unpack_from('<8I',block)
        assert (a,b,flags,length,index,total,family,struct.unpack_from('<I',block,508)[0])==(0x0a324655,0x9e5d5157,0x2000,256,offset//512,len(raw)//512,0xe48bff56,0x0ab16f30)
        assert address==0x10000000+offset//512*256
        chunks.append(block[32:288])
    flash=b''.join(chunks);linked=(folder/'robocar_rtos_motion.bin').read_bytes()
    assert flash[:len(linked)]==linked and not any(flash[len(linked):])
    # Duplicate private names exist: SDK filter4 vs ADC filter21. Measure the
    # owning object explicitly, never overwrite names in an ELF symbol dict.
    def symbols(path):
        rows=[]
        for line in run(BIN/'arm-none-eabi-nm','-S',path).splitlines():
            m=re.fullmatch(r'([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[bBdD]\s+(\w+)',line)
            if m:rows.append((m[3],int(m[2],16)))
        return rows
    selected={}
    for label in ('runtime','queue','kernel_pool','waveform','capture','first_words','first_profiles','first_failure'):
        found=[n for name,n in symbols(elf) if name==label]
        assert len(found)<=1
        if found:selected[label]=found[0]
    selected['ADC_filter']=dict(symbols(folder/'CMakeFiles/robocar_rtos_motion.dir/track_sensor.c.o'))['filter']
    return dict(text=text,data=data,bss=bss,sha256=sha(folder/uf2),
        private_path=str((folder/uf2).relative_to(ROOT)),symbols=selected,UF2_matches_linked_binary=True)
def layouts():
    names=['motion_wheel_t','motion_control_t','barcode_progress_t','center_search_t','center_trim_t','course_control_t','track_runtime_t','motion_runtime_t']
    results={}
    with tempfile.TemporaryDirectory(prefix='rtos-layout-') as tmp:
        folder=Path(tmp);source=folder/'probe.c'
        source.write_text('#include "motion_runtime.h"\nconst unsigned sizes[] __attribute__((section(".sizes")))={'+','.join('sizeof('+n+')' for n in names)+'};\n')
        for label,flags in [('accepted',[]),('stage2',['-DCENTER_BASELINE_COMPACT=1']),('stage3',['-DCENTER_BASELINE_COMPACT=1','-DROBOCAR_COMPACT_CONTROL_STATE=1'])]:
            run(BIN/'arm-none-eabi-gcc','-mcpu=cortex-m0plus','-mthumb','-std=c11','-Wall','-Wextra','-Werror','-DRTOS_TRACK=1','-I'+str(APP),'-I'+str(BASE),*flags,'-c',source,'-o',folder/'probe.o')
            run(BIN/'arm-none-eabi-objcopy','-O','binary','-j','.sizes',folder/'probe.o',folder/'probe.bin')
            results[label]=dict(zip(names,struct.unpack('<'+str(len(names))+'I',(folder/'probe.bin').read_bytes())))
    return results
def frames(folder):
    wanted={'sample','track_runtime_begin','track_service','motion_runtime_step','motion_poll','course_step'};result={}
    for path in folder.glob('CMakeFiles/robocar_rtos_motion.dir/**/*.su'):
        for line in path.read_text().splitlines():
            fields=line.split('\t');name=fields[0].split(':')[-1]
            if name in wanted:result[name]=int(fields[1])
    assert set(result)==wanted
    return result
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/rtos-minbits-stage3-measurement.json')
    args=parser.parse_args()
    full=ROOT/'build/rtos-minbits-stage3-candidate';lean=ROOT/'build/rtos-minbits-stage3-lean';control=ROOT/'build/rtos-minbits-stage3-default-control'
    accepted=ROOT/'build/rtos-network-v0_5_1'
    assert sha(accepted/'robocar_rtos_network_v0_5_1.uf2')=='b7836015b55e8636b6f154a3139b693851651b810b654da642c85b5a62e2624d'
    assert (accepted/'robocar_rtos_motion.bin').read_bytes()==(control/'robocar_rtos_motion.bin').read_bytes()
    assert json.loads((ROOT/'build/rtos-network-local/firmware-info.json').read_text())['firmware_version']=='0.5.1'
    candidates=dict(full=image(full,'robocar_rtos_minbits_stage3_candidate.uf2'),lean=image(lean,'robocar_rtos_minbits_stage3_lean.uf2'))
    default=image(control,'robocar_rtos_default_control.uf2');sizes=layouts()
    assert default['bss']-candidates['full']['bss']==176
    assert candidates['full']['bss']-candidates['lean']['bss']==33664
    assert sizes['stage2']['motion_runtime_t']-sizes['stage3']['motion_runtime_t']==120
    source_paths=list(APP.glob('*.c'))+list(BASE/n for n in ['control_representation.h','motion_control.h','course_control.h','center_search.h','center_trim.h','barcode_drive.h'])
    source_paths+=[APP/'CMakeLists.txt',ROOT/'tools/test_rtos_control_bits.py',ROOT/'tools/test_rtos_minimum_bits.py',ROOT/'firmware/tests/mission_rtos/control_state_trace.c']
    evidence=dict(recorded_utc=datetime.now(timezone.utc).isoformat(),branch='yixuan/rtos-minimum-bit-state',base_stage2_commit='47a0a43',
        status='Stage3 opt-in candidates built/software verified; not installed',installed_version='0.5.1',
        candidates=candidates,default_control=default,ARM_layout_bytes=sizes,
        savings_bytes=dict(stage3_incremental_representation=120,total_representation=176,optional_lean_feature=33664),
        default_binary_identical_to_accepted_with_reference_build_date=True,
        reference_build_date_control=dict(SOURCE_DATE_EPOCH=int(datetime(2026,11,9,12,tzinfo=timezone.utc).timestamp()),date='Nov  9 2026',reason='SDK embeds compiler __DATE__; no functional source or binary patch'),
        compiler_frame_bytes=dict(accepted=frames(control),candidate=frames(full),interpretation='Individual compiler frames, not runtime stack watermarks or whole call-chain bounds'),
        verification=dict(motion_scenarios=320,course_scenarios=240,independent_behaviour_test_programs=8,
            ADC_updates_equal=1102600,baseline_queries_equal=120601,independent_option_combinations=8,full_exports_byte_identical=True,lean_retention_tradeoff_verified=True,sanitizers='ASan/UBSan',warning_as_error_ARM_build=True,normal_provisioning_forces_accepted_flags=True,candidate_console_no_START=True),
        header_inventory=json.loads((ROOT/'build/rtos-representation-stage3/inventory.json').read_text())['status_counts'],
        logs=['build/rtos-minbits-stage3-'+n+'.log' for n in ('tests','control-tests','build','lean-build','default-build','bench-tests','console-tests','setup-tests')],
        source_sha256={str(p.relative_to(ROOT)):sha(p) for p in source_paths if not p.name.startswith('network_settings')},
        limits=['No candidate hardware timing, stack or course acceptance yet; installed v0.5.1 unchanged.',
            'Header declarations are not independent live allocations or a completion percentage.',
            'Packing is internal and target compiler verified; no raw bitfield structs are transported.',
            'Motion owns changed controller flags; this single-core owner proof is not an SMP locking contract.',
            'Raw encoder/time domains, controller arithmetic, barcode validators, MQTT schemas and task stacks are unchanged.',
            'Optional lean-cache saving is a feature-budget tradeoff, not minimum-bit credit.',
            'Teammate algorithms and submitted Tribunal1 are unchanged.'])
    # Pull the digest from the actual successful trace log, rather than trusting
    # a hand-entered hash. The full byte comparison is performed by that test.
    log=(ROOT/'build/rtos-minbits-stage3-control-tests.log').read_text()
    match=re.search(r'complete logical traces: (\d+) bytes equal, SHA256 ([0-9a-f]{64})',log);assert match
    evidence['verification'].update(logical_trace_bytes_equal=int(match[1]),logical_trace_sha256=match[2])
    required={
        'tests':'PASS all eight ADC/core/control combinations',
        'bench-tests':'PASS USB helper:',
        'console-tests':'PASS telemetry health:',
        'setup-tests':'PASS default provisioning overrides inherited experimental flags',
    }
    for label,marker in required.items():
        assert marker in (ROOT/('build/rtos-minbits-stage3-'+label+'.log')).read_text(),label
    args.output.write_text(json.dumps(evidence,indent=2)+'\n')
    print('PASS ARM layouts, linked sizes/symbol attribution, UF2 reconstruction and date-controlled default binary identity; installed metadata still v0.5.1')
if __name__=='__main__':main()
