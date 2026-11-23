"""Decode compact scan diagnostics and verify/reassemble stopped waveforms."""
import json
from pathlib import Path

REASONS=['NONE','ACCEPTED','TOO_FEW_EXTREMA','TOO_MANY_EXTREMA','RUN_TOO_SHORT',
         'RUN_TOO_LONG','CODE39_PATTERN_OR_WIDTH','AMBIGUOUS','FRAME_LIMIT',
         'BUFFER_FULL','SAMPLE_TIMING','STOPPED_PARTIAL','NO_VALID_FRAMED_WINDOW','MOTION_PATH_INVALID']
WIDTH_REASONS=['OK','RANGE','RANK_TIE','NONPOSITIVE_OFFSET','WIDE_NARROW_SEPARATION',
               'NARROW_SPREAD','WIDE_SPREAD','OUTER_BAR']

class Reader:
    def __init__(self,payload):self.word=int.from_bytes(payload,'little');self.bits=8*len(payload);self.at=0
    def get(self,bits):
        if self.at+bits>self.bits:raise ValueError('Truncated scan diagnostic')
        value=(self.word>>self.at)&((1<<bits)-1);self.at+=bits;return value
    def zero(self,bits):
        if self.get(bits):raise ValueError('Nonzero scan diagnostic padding')
    def end(self):
        if self.bits-self.at>=8:raise ValueError('Oversized scan diagnostic')
        self.zero(self.bits-self.at)

def _reason(r):
    value=r.get(4)
    if value>=len(REASONS):raise ValueError('Unknown scan reason')
    return REASONS[value]

def _letter(r):
    value=r.get(5)
    if value>26:raise ValueError('Invalid diagnostic letter')
    return chr(64+value) if value else None

def _profile(r):
    d=dict(extrema=r.get(7),windows=r.get(6),best_window=r.get(6),reason=_reason(r),
           bad_run=r.get(5),bad_width=r.get(32),patterns=[r.get(9) for _ in range(3)])
    d['width_reasons']=[WIDTH_REASONS[r.get(3)] for _ in range(3)]
    d.update(guards=r.get(2),separators=r.get(2),reversed=bool(r.get(1)),score=r.get(4),
             runs=[r.get(32) for _ in range(29)])
    if d['extrema']>95 or d['windows']>33 or d['best_window']>32 or d['bad_run']>28 or d['score']>11:
        raise ValueError('Out-of-range scan profile')
    return d

def decode_scan(payload):
    if not 3<=len(payload)<=192:raise ValueError('Invalid scan diagnostic length')
    r=Reader(payload);kind=r.get(8);run=r.get(16);d=dict(type='scan_diag',run=run)
    if kind==0:
        d['record']='state'
        for name in ['captured_ms','samples','frames','rejected','decode_max_us','adc_max_us']:d[name]=r.get(32)
        d.update(reads=r.get(16),queue_peak=r.get(9),faults=r.get(3),enabled=bool(r.get(1)),
                 recording=bool(r.get(1)),sealed=bool(r.get(1)))
        r.zero(1);d.update(attempts=r.get(6),ready=bool(r.get(1)));r.zero(1)
        for name in ['attempt_overflow','recoveries','stop_us']:d[name]=r.get(32)
        d['stop_discarded_samples']=r.get(9)
        if d['attempts']>32 or (d['sealed'] and (d['enabled'] or not d['ready'])):raise ValueError('Invalid scan state')
    elif kind==1:
        d.update(record='attempt',index=r.get(5),start_us=r.get(32),duration_us=r.get(32),
                 samples=r.get(14),raw_min=r.get(12),raw_max=r.get(12),lateral_mm=r.get(16),yaw_deg=r.get(16),
                 reason=_reason(r),letter=_letter(r),coherent_mask=r.get(6),geometry_mask=r.get(6),best=_profile(r))
        if not 1<=d['samples']<=8192 or d['raw_min']>d['raw_max']:raise ValueError('Invalid scan attempt')
    elif kind==2:
        d.update(record='snapshot',count=r.get(15),motion=bool(r.get(1)),paper=r.get(12),signal_range=r.get(12),
                 reason=_reason(r),letter=_letter(r),coherent_mask=r.get(6),geometry_mask=r.get(6))
        for name in ['start_us','leading_us','white_us','duration_us','checksum']:d[name]=r.get(32)
        if not 1<=d['count']<=16384 or (d['motion'] and d['count']%2):raise ValueError('Invalid waveform sample count')
    elif kind==3:
        d.update(record='profile',index=r.get(3),profile=_profile(r))
        if d['index']>5:raise ValueError('Invalid profile index')
    elif kind==4:
        d.update(record='words',index=r.get(8),count=r.get(7));r.zero(1)
        if not 1<=d['count']<=64:raise ValueError('Invalid waveform chunk size')
        d['words']=[r.get(16) for _ in range(d['count'])]
    elif kind==5:
        d.update(record='done',attempts=r.get(6),snapshot=bool(r.get(1)))
        if d['attempts']>32:raise ValueError('Invalid scan attempt count')
    else:raise ValueError('Unknown scan diagnostic kind')
    r.end();return d

def checksum(words):
    value=2166136261
    for word in words:
        for byte in (word&255,word>>8):value=((value^byte)*16777619)&0xffffffff
    return value

class ScanCapture:
    def __init__(self,folder):self.folder=Path(folder);self.boot=None;self.runs={};self.latest=None
    def boot_id(self,value):
        # A token is unique within one boot. Never combine different boots.
        if self.boot!=value:self.boot=value;self.latest=None
    def accept(self,d):
        if not d['run']:return []
        key=f'{self.boot if self.boot is not None else "unknown"}-{d["run"]}'
        # Firmware publishes status before diagnostics on every connection.
        run=self.runs.setdefault(key,dict(boot_id=self.boot,run=d['run'],attempts={},profiles={},chunks={},complete=False))
        record=d['record'];messages=[]
        if record=='state' or self.latest is None:self.latest=key
        if record=='state':
            prior=run.get('state');run['state']=d
            if d['sealed'] and (prior is None or not prior['sealed']):
                messages.append(f'Scan stopped: {d["reads"]} reads, {d["rejected"]} rejected captures, {d["recoveries"]} line recoveries. Downloading diagnostic capture...')
        elif record=='attempt':
            prior=run['attempts'].get(str(d['index']));run['attempts'][str(d['index'])]=d
            if prior!=d:messages.append(f'Scan attempt {d["index"]+1}: {d["reason"]}'+(f' ({d["letter"]})' if d['letter'] else '')+f'; {d["samples"]} samples, yaw {d["yaw_deg"]} degrees, lateral {d["lateral_mm"]} mm')
        elif record=='profile':run['profiles'][str(d['index'])]=d['profile']
        elif record=='snapshot':
            prior=run.get('snapshot')
            if prior is not None and prior!=d:
                run['profiles'].clear();run['chunks'].clear();run['complete']=False
            run['snapshot']=d
        elif record=='words':run['chunks'][str(d['index'])]=d['words']
        elif record=='done':
            run['done']=d
            attempts=all(str(i) in run['attempts'] for i in range(d['attempts']))
            if d['snapshot']:
                meta=run.get('snapshot',{});count=meta.get('count',0);total=(count+63)//64
                words=[v for i in range(total) for v in run['chunks'].get(str(i),[])]
                full=bool(count) and len(words)==count and all(str(i) in run['profiles'] for i in range(6))
                correct=full and checksum(words)==meta['checksum']
                run['complete']=bool(attempts and correct)
                run['checksum_verified']=bool(correct)
                if run['complete']:
                    run['words']=words;self._text(run)
            else:run['complete']=attempts
            if run['complete']:messages.append('Scan diagnostics saved'+('; waveform checksum verified.' if d['snapshot'] else '; no waveform was captured.'))
            else:messages.append('Scan diagnostic download is incomplete or failed its checksum; keep the car powered and logger open.')
            self.save()
        return messages
    def pending(self):
        if self.latest is None:return False
        run=self.runs[self.latest];state=run.get('state',{})
        return bool(state.get('enabled') or state.get('sealed') or run.get('snapshot')) and not run['complete']
    def save(self):
        self.folder.mkdir(parents=True,exist_ok=True)
        for key,run in self.runs.items():
            (self.folder/f'scan-{key}.json').write_text(json.dumps(run,indent=2)+'\n')
    def summary(self):
        return {key:{k:run[k] for k in ('boot_id','run','complete','checksum_verified','state') if k in run}
                for key,run in self.runs.items()}
    def _text(self,run):
        s=run['snapshot'];words=run['words'];ident=s['start_us'];fnv=f'{s["checksum"]:08x}'
        encoding='ADC12_MS4_POS16' if s['motion'] else 'ADC12_MS4'
        lines=[f'[WAVEFORM SNAPSHOT] id={ident} samples={s["count"]} readings={s["count"]//(2 if s["motion"] else 1)} paper={s["paper"]} start_us={ident} duration_us={s["duration_us"]} leading_us={s["leading_us"]} trailing_us={s["white_us"]} result={s["reason"]} letter={s["letter"] or "-"} fnv={fnv} encoding={encoding}']
        for i in range(0,len(words),64):
            chunk=words[i:i+64];packed=''.join(f'{v:04x}' for v in chunk)
            lines.append(f'[WAVEFORM CHUNK] id={ident} offset={i} count={len(chunk)} total={len(words)} fnv={fnv} packed={packed}')
        self.folder.mkdir(parents=True,exist_ok=True)
        (self.folder/f'scan-{run["boot_id"]}-{run["run"]}-waveform.txt').write_text('\n'.join(lines)+'\n')
