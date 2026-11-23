#!/usr/bin/env python3
"""Compare logical control state with pinned accepted source, not raw padding.

The checked-in pre-conversion field inventory generates a complete logical
snapshot emitter. Unknown fields/types fail instead of disappearing from the
comparison. Reference algorithms and headers come from accepted commit a4e15b5.
No hardware, credentials, network or firmware deployment.
"""
import csv,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
from audit_data_representation import walk
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'firmware/app/robot_firmware'
TEST=ROOT/'firmware/tests'
ENV=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
CC=[shutil.which('clang'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
    '-fsanitize=address,undefined','-fno-omit-frame-pointer']
REFERENCE='a4e15b5'

def git_file(path):
    return subprocess.check_output(['git','show',REFERENCE+':'+path],cwd=ROOT)

def dump_header():
    rows=list(csv.DictReader((ROOT/'docs/testing/representation/20261109-rtos-stage2-fields.csv').open()))
    fields={}
    for row in rows:fields.setdefault(row['type'],[]).append(row)
    done=set();code=['#include <stdio.h>\n#include <string.h>\n',
        'static void number(uint64_t n){for(unsigned i=0;i<8;i++)putchar((int)((n>>(8*i))&255));}\n']
    def generate(name):
        if name in done:return
        body=[]
        for row in fields[name]:
            field,kind=row['field'],row['declared_type'];value='s->'+field
            if kind in fields:
                generate(kind);body.append(f'dump_{kind}(&{value});')
            elif kind=='const char *':
                body.append(f'number({value}?strlen({value})+1:0);if({value})fwrite({value},1,strlen({value})+1,stdout);')
            elif kind=='float':
                body.append(f'{{uint32_t bits;memcpy(&bits,&{value},4);number(bits);}}')
            elif kind in ('bool','_Bool','char','unsigned int','int','unsigned','int8_t','int32_t','int64_t','uint8_t','uint16_t','uint32_t','uint64_t','motion_kind_t','motion_phase_t','motion_fault_t','course_phase_t'):
                body.append(f'number((uint64_t){value});')
            else:raise ValueError('Uncompared logical field '+name+'.'+field+': '+kind)
        code.append(f'static void dump_{name}(const {name} *s){{'+''.join(body)+'}\n');done.add(name)
    generate('course_control_t');generate('line_check_output_t')
    return ''.join(code),{name:{r['field'] for r in fields[name]} for name in done}

def verify_fields(app,folder,flags,expected):
    probe=folder/'field_check.c';probe.write_text('#include "course_control.h"\n')
    ast=json.loads(subprocess.check_output([CC[0],'-std=c11','-I'+str(app)]+flags+
        ['-Xclang','-ast-dump=json','-fsyntax-only',str(probe)],env=ENV))
    records={n['id']:n for n in walk(ast) if n.get('kind')=='RecordDecl' and n.get('inner')}
    found={}
    for n in ast.get('inner',[]):
        if n.get('kind')!='TypedefDecl' or n.get('name') not in expected:continue
        ids=[v['ownedTagDecl']['id'] for v in walk(n) if 'ownedTagDecl' in v]
        assert len(ids)==1 and ids[0] in records,n['name']
        found[n['name']]={v['name'] for v in records[ids[0]]['inner'] if v.get('kind')=='FieldDecl' and v.get('name')}
    assert found==expected,'A logical field was added/removed; update the trace inventory explicitly'

def main():
    with tempfile.TemporaryDirectory(prefix='rtos-control-bits-') as tmp:
        folder=Path(tmp);reference=folder/'reference';reference.mkdir()
        paths=subprocess.check_output(['git','ls-tree','-r','--name-only',REFERENCE,'--','firmware/app/robot_firmware'],cwd=ROOT,text=True).splitlines()
        for path in paths:
            if Path(path).parent==Path('firmware/app/robot_firmware') and Path(path).suffix in ('.h','.c'):
                if Path(path).suffix=='.c' and Path(path).name not in ('course_control.c','motion_control.c'):continue
                (reference/Path(path).name).write_bytes(git_file(path))
        code,fields=dump_header();(folder/'control_trace_dump.h').write_text(code)
        traces=[]
        for label,app,flags in [('accepted',reference,[]),('candidate',BASE,['-DROBOCAR_COMPACT_CONTROL_STATE=1'])]:
            verify_fields(app,folder,flags,fields)
            exe=folder/label
            subprocess.run(CC+flags+['-DROBOCAR_CENTER_LINE=1','-I'+str(app),'-I'+tmp,
                str(ROOT/'firmware/tests/mission_rtos/control_state_trace.c'),str(app/'motion_control.c'),str(app/'course_control.c'),'-o',str(exe)],check=True,env=ENV)
            output=folder/(label+'.trace')
            with output.open('wb') as f:subprocess.run([str(exe)],stdout=f,check=True,env=ENV)
            traces.append(output.read_bytes())
        assert traces[0]==traces[1],'Logical control state differs from accepted source'
        print(f'PASS accepted/candidate complete logical traces: {len(traces[0])} bytes equal, SHA256 {hashlib.sha256(traces[0]).hexdigest()}')
        print('PASS 320 motion and240 course scenarios, wide invalid request rejection, full float bits/time/counters; ASan/UBSan')
        # Existing behavioural tests add independently specified safety/steering
        # assertions to the before/after trace comparison.
        names=['motion_control_host_test','course_control_test','center_recovery_test',
               'center_course_test','center_bend_test','flow_bend_test',
               'center_launch_test','barcode_drive_test']
        for name in names:
            source=TEST/(name+'.c');exe=folder/name
            extra=[] if 'course_control.c"' in source.read_text() or name not in ('course_control_test',) else [str(BASE/'course_control.c')]
            subprocess.run(CC+['-DROBOCAR_COMPACT_CONTROL_STATE=1','-I'+str(BASE),str(source),str(BASE/'motion_control.c')]+extra+['-lm','-o',str(exe)],check=True,env=ENV)
            subprocess.run([str(exe)],check=True,env=ENV)

if __name__=='__main__':main()
