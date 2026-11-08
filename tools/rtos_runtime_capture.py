"""Decode observed runtime evidence. Values are not worst-case guarantees."""
from rtos_scan_capture import Reader

TASKS=['motion','decoder','network','observer']
SOURCES=['boot/none','stop API entry','guard observation','output rejection']

def decode_runtime(payload):
    if len(payload) not in (34,36):raise ValueError('Invalid runtime record length')
    r=Reader(payload);kind=r.get(8);d=dict(type='runtime_diag',run=r.get(16),captured_ms=r.get(32))
    if kind==0:
        if len(payload)!=36:raise ValueError('Invalid runtime summary length')
        d.update(record='summary',armed=bool(r.get(1)),stop_source=r.get(2),stop_reason=r.get(3));r.zero(2)
        if d['stop_reason']>6:raise ValueError('Invalid runtime stop reason')
        for name in ['stop_sequence','requested_us','disabled_us','guard_gap_max_us','service_gap_max_us','monitor_work_max_us','stack_scans']:d[name]=r.get(32)
        d['request_to_pwm_writes_us']=(d['disabled_us']-d['requested_us'])&0xffffffff
        d['stop_source_label']=SOURCES[d['stop_source']]
    elif kind==1:
        if len(payload)!=34:raise ValueError('Invalid runtime task length')
        d.update(record='task',task_id=r.get(2),priority=r.get(5),valid=bool(r.get(1)))
        d.update(requested_bytes=r.get(14),allocated_bytes=r.get(14),untouched_bytes=r.get(14),
                 low_guard_intact=bool(r.get(1)),observed=bool(r.get(1)));r.zero(4)
        for name in ['iterations','gap_max_us','work_max_us','armed_gap_max_us','armed_work_max_us']:d[name]=r.get(32)
        if d['untouched_bytes']>d['allocated_bytes'] or d['requested_bytes']>d['allocated_bytes'] or d['priority']>16:
            raise ValueError('Invalid runtime stack range')
        if d['valid'] and (not d['priority'] or d['requested_bytes']<128 or d['allocated_bytes']<128):raise ValueError('Invalid runtime stack metadata')
        if not d['valid'] and any(d[k] for k in ['priority','requested_bytes','allocated_bytes','untouched_bytes','low_guard_intact','observed']):raise ValueError('Unavailable runtime stack has values')
        d['task']=TASKS[d['task_id']]
        d['observed_watermark_bytes']=d['allocated_bytes']-d['untouched_bytes'] if d['observed'] else None
    else:raise ValueError('Unknown runtime record kind')
    r.end();return d

class RuntimeCapture:
    def __init__(self):self.boot=None;self.runs={}
    def boot_id(self,value):self.boot=value
    def accept(self,d):
        key=f'{self.boot if self.boot is not None else "session"}-{d["run"]}'
        run=self.runs.setdefault(key,dict(boot_id=self.boot,run=d['run'],tasks={}))
        target=run if d['record']=='summary' else run['tasks']
        item='summary' if d['record']=='summary' else d['task']
        old=target.get(item)
        # Ignore older delayed snapshots in the same bounded clock window.
        if old and ((d['captured_ms']-old['captured_ms'])&0xffffffff)>=0x80000000:return
        target[item]=d
    def summary(self):return self.runs
