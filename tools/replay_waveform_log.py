#!/usr/bin/env python3
"""Verify frozen export and replay the actual stored decoder inputs.
Batch pattern replay does not prove a moving path or missing quiet zone valid.
"""
from pathlib import Path
import json, os, re, struct, subprocess, sys, argparse
parser=argparse.ArgumentParser()
parser.add_argument('--expect',help='Expected batch letters, one per log; - means no acceptance assertion')
parser.add_argument('--recovery-optics',action='store_true',help='Enable RTOS v0.4.2 single-short-black consensus path')
parser.add_argument('--latest-pass',action='store_true',help='Use latest monotonic traversal of rescanned coordinates')
parser.add_argument('files',nargs='+')
args=parser.parse_args()

root=Path(__file__).resolve().parents[1]
app=root/'firmware/app/robot_firmware'
build=root/'build/waveform-replays';build.mkdir(parents=True,exist_ok=True)
driver=build/'replay.c'
driver.write_text(r'''#include "barcode_waveform.h"
#include <assert.h>
#include <stdio.h>
static barcode_waveform_t w;
int main(int argc,char **argv){
    assert(argc==4);FILE *f=fopen(argv[1],"rb");assert(f);
    uint16_t words[2*BARCODE_WAVE_SAMPLES];
    unsigned n=(unsigned)fread(words,2,2*BARCODE_WAVE_SAMPLES,f);fclose(f);
    w.motion=argv[3][0]=='m';w.count=(uint16_t)(n/(w.motion?2u:1u));
    for(unsigned i=0;i<w.count;i++){w.sample[i]=words[i*(w.motion?2u:1u)];if(w.motion)w.position[i]=(int16_t)words[2*i+1];}
    char c=0;bool ok=barcode_waveform_decode(&w,&c);
    printf("batch_pattern=%c profiles_extrema=",ok?c:'-');
    for(unsigned i=0;i<6;i++)printf("%s%u",i?"/":"",w.profile[i].extrema);
    puts("");if(argv[2][0]!='-')assert(ok&&c==argv[2][0]);return 0;
}
''')
exe=build/('replay'+('-recovery' if args.recovery_optics else '')+('-latest' if args.latest_pass else ''))
env=dict(os.environ,SDKROOT='/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk')
subprocess.run(['clang','-std=c11','-O1','-Wall','-Wextra','-Werror',
    f'-DBARCODE_WAVE_RECOVERY_OPTICS={int(args.recovery_optics)}',
    f'-DBARCODE_WAVE_LAST_PASS={int(args.latest_pass)}',
    '-fsanitize=address,undefined','-I'+str(app),str(driver),str(app/'barcode_waveform.c'),
    str(app/'barcode_decoder.c'),'-lm','-o',str(exe)],check=True,env=env)
if args.expect and len(args.expect)!=len(args.files):parser.error('--expect must contain one letter per log')
for input_index,name in enumerate(args.files):
    text=Path(name).read_text()
    header=re.search(r'^\[WAVEFORM SNAPSHOT\].*',text,re.M)
    if not header:raise ValueError(f'{name}: missing frozen snapshot')
    meta=dict(re.findall(r'(\w+)=([^ ]+)',header[0]));words=[]
    for match in re.finditer(r'\[WAVEFORM CHUNK\] id=(\d+) offset=(\d+) count=(\d+) total=(\d+) fnv=([0-9a-f]{8}) packed=([0-9a-f]+)',text):
        ident,off,count,total,checksum,packed=match.groups()
        assert ident==meta['id'] and checksum==meta['fnv']
        assert int(off)==len(words) and len(packed)==4*int(count) and int(total)==int(meta['samples'])
        words.extend(int(packed[i:i+4],16) for i in range(0,len(packed),4))
    assert 0<len(words)==int(meta['samples'])<=16384
    checksum=2166136261
    for word in words:
        for byte in (word&255,word>>8):checksum=((checksum^byte)*16777619)&0xffffffff
    assert f'{checksum:08x}'==meta['fnv']
    packed=build/(Path(name).stem+'.bin');packed.write_bytes(struct.pack('<'+'H'*len(words),*words))
    expected=args.expect[input_index] if args.expect else (meta['letter'] if meta['result']=='ACCEPTED' else '-')
    print(json.dumps({'file':Path(name).name,'samples':len(words),'checksum':'verified',
        'saved_result':meta['result'],'saved_letter':meta['letter']},sort_keys=True),flush=True)
    subprocess.run([str(exe),str(packed),expected,'motion' if meta.get('encoding')=='ADC12_MS4_POS16' else 'time'],check=True,env=env)
