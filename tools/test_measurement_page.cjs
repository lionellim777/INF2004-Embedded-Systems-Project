// Host test for firmware/app/robot_firmware/wireless/measurement.html.
// No browser or npm packages: the page script runs in a node vm with a stub DOM.
// Run from the repository root:  node tools/test_measurement_page.cjs
'use strict';
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const path = require('path');

const root = path.resolve(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'firmware/app/robot_firmware/wireless/measurement.html'), 'utf8');
const firmware = fs.readFileSync(path.join(root, 'firmware/app/robot_firmware/measurement_diag.c'), 'utf8');
const script = /<script>([\s\S]*)<\/script>/.exec(html)[1];

// ---------------------------------------------------------------- stub DOM
function makeElement(id) {
  // Like the real DOM, assigning textContent replaces (clears) all child nodes.
  let text = '';
  return {
    id, value: '', className: '', children: [], dataset: {}, scrollTop: 0, scrollHeight: 0,
    get textContent() { return text; },
    set textContent(v) { text = String(v); this.children = []; },
    appendChild(c) { this.children.push(c); return c; },
  };
}
const elements = {};
const document = {
  getElementById(id) { if (!elements[id]) { elements[id] = makeElement(id); } return elements[id]; },
  createElement(tag) { return makeElement(tag); },
  querySelectorAll() { return []; },
};
const timers = [];
const context = vm.createContext({
  document, console,
  fetch: () => Promise.reject(new Error('offline in host test')),
  setTimeout: (fn) => { timers.push(fn); return timers.length; },
  clearTimeout() {}, AbortController: class { constructor() { this.signal = {}; } abort() {} },
  Blob: class {}, URL: { createObjectURL: () => '', revokeObjectURL() {} },
  Date, Math, Number, Map, Array, Object, JSON, String, Error, Promise,
});
document.getElementById('dist').value = '1000';
vm.runInContext(script, context, { filename: 'measurement.html' });
const run = (code) => vm.runInContext(code, context);
const textOf = (id) => elements[id].textContent;
const reset = () => run('events=new Map();live=null;carry="";');

// ---------------------------------------------------------------- fixtures (mirror the firmware printf formats)
let seq = 0;
const distRun = (run, left, right, il = 0, ir = 0) =>
  `[MEAS RESULT] seq=${++seq} DIST_RUN run=${run} left=${left} right=${right} raw_left=${-left} raw_right=${right} inv_left=${il} inv_right=${ir}`;
const poseLine = (pose, tilt, roll, axis, retry, verdict, x = 12.3, y = -4.1, z = 998.2) =>
  `[MEAS RESULT] seq=${++seq} IMU_POSE capture=${seq} pose=${pose} x=${x.toFixed(2)} y=${y.toFixed(2)} z=${z.toFixed(2)} ` +
  `sx=1.10 sy=1.20 sz=1.50 mag=998.30 pitch=-0.235 roll=0.706 tilt=${tilt.toFixed(3)} roll_chg=${roll.toFixed(3)} ` +
  `axis=${axis} exp_roll=5.184 exp_pitch=3.562 retry=${retry} verdict=${verdict}`;
const nextLine = (pose, text) => `[MEAS RESULT] seq=${++seq} IMU_NEXT pose=${pose} total=7 text=${text}`;
const liveLine = (extra = '') =>
  '[MEAS LIVE] dist=COUNTING left=1234 right=1240 inv_left=0 inv_right=0 imu=OK x=1.0 y=2.0 z=998.0 pitch=0.11 roll=0.06 ' +
  'tilt=- roll_chg=- pose=1 capturing=0 gp20=DIST' + extra;
const approx = (a, b, eps = 1e-9) => assert(Math.abs(a - b) < eps, `${a} != ${b}`);

// ---------------------------------------------------------------- 1. line parsing
{
  const ev = run(`parseResult(${JSON.stringify(poseLine(7, -3.5, 5.1, 'Y', 0, 'PASS: matches model; caster is at the FRONT (front lowered)'))})`);
  assert.equal(ev.kind, 'IMU_POSE');
  assert.equal(ev.f.pose, '7');
  assert.equal(ev.f.exp_roll, '5.184');
  assert.equal(ev.f.retry, '0');
  assert.equal(ev.tail, 'PASS: matches model; caster is at the FRONT (front lowered)');
  const nx = run(`parseResult(${JSON.stringify(nextLine(3, '3/7 FRONT RAISED by about 5 to 10 degrees (front = hump-first end)'))})`);
  assert.equal(nx.f.pose, '3');
  assert.equal(nx.tail, '3/7 FRONT RAISED by about 5 to 10 degrees (front = hump-first end)');
  assert.equal(run('parseResult("[MEAS INFO] hello")'), null);
  assert.equal(run('parseResult("[MEAS LIVE] dist=IDLE")'), null);
}

// ---------------------------------------------------------------- 2. distance analysis
reset();
{
  const lines = [
    distRun(1, 13491, 13520),
    distRun(2, 13400, 13380),
    distRun(3, -50, 13000),          // rejected: negative
    distRun(4, 13000, 13000, 2, 0),  // rejected: skipped transitions
    distRun(5, 10000, 10050),        // clean but >15% below nominal 13.491
  ];
  lines.forEach((l) => run(`ingestLine(${JSON.stringify(l)})`));
  const d = run('derive()');
  assert.equal(d.runs.length, 5);
  const rows = run('derive().runs.map(e=>analyseRun(e,1000))');
  approx(rows[0].cl, 13.491); approx(rows[0].cr, 13.52);
  assert.equal(rows[0].clean, true);
  assert.equal(rows[2].clean, false); assert(/zero or negative/.test(rows[2].status));
  assert.equal(rows[3].clean, false); assert(/skipped/.test(rows[3].status));
  assert.equal(rows[4].far, true);
  assert.equal(rows[0].far, false);
  const s = run('summarise(derive().runs.map(e=>analyseRun(e,1000)))');
  assert.equal(s.n, 3);
  approx(s.r.mean, (13.52 + 13.38 + 10.05) / 3);
  approx(s.r.min, 10.05); approx(s.r.max, 13.52);
  approx(s.r.spreadPct, 100 * (13.52 - 10.05) / ((13.52 + 13.38 + 10.05) / 3));
  // changing the marked distance rescales counts/mm; raw runs are unchanged
  const rows2000 = run('derive().runs.map(e=>analyseRun(e,2000))');
  approx(rows2000[0].cr, 13520 / 2000);
  const text = run('distanceText(derive().runs.map(e=>analyseRun(e,1000)),1000)');
  assert(/USE FOR THE HUMP \(right wheel\)/.test(text));
  assert(/clean runs: 3/.test(text));
  assert(/fewer than 5 clean runs/.test(text));
  assert(/Enter the marked distance/.test(run('distanceText([],0)')));
  // invalid distance must not raise the nominal flag
  assert.equal(run('analyseRun(derive().runs[0],0).far'), false);
}

// ---------------------------------------------------------------- 3. IMU sequence, retry, restart
reset();
{
  const feed = (l) => run(`ingestLine(${JSON.stringify(l)})`);
  feed(nextLine(1, '1/7 LEVEL'));
  feed(poseLine(1, 0, 0, 'Z', 0, 'BASELINE'));
  feed(`[MEAS RESULT] seq=${++seq} IMU_LEVEL vertical=Z value=998.2 level_pitch=-0.235 level_roll=0.706 warn=NONE`);
  feed(poseLine(2, 0.1, 0.05, 'Z', 0, 'PASS: level repeats'));
  feed(poseLine(3, 1.0, 0, 'Y', 1, 'TOO SMALL: raise the front further, then capture again'));
  feed(poseLine(3, 6.1, 0.2, 'Y', 0, 'PASS: front raised = positive tilt, forward axis = Y'));
  feed(poseLine(4, -6.0, 0, 'Y', 0, 'PASS: front lowered = negative tilt, forward axis = Y'));
  feed(poseLine(5, 0, -6.5, 'X', 0, 'RECORD: LEFT raised gives NEGATIVE roll'));
  feed(poseLine(6, 0, 6.4, 'X', 0, 'RECORD: RIGHT raised gives POSITIVE roll'));
  feed(`[MEAS RESULT] seq=${++seq} IMU_REJECT capture=${seq} pose=7 reason=CAR_MOVED`);
  feed(poseLine(7, -3.45, 5.12, 'X', 0, 'PASS: matches model; caster is at the FRONT (front lowered)'));
  feed(nextLine(1, '1/7 LEVEL'));
  let d = run('derive()');
  assert.deepEqual(Object.keys(d.poses).sort(), ['1', '2', '3', '4', '5', '6', '7']);
  assert.equal(d.poses[3].f.retry, '0', 'retry capture is replaced by the accepted one');
  assert.equal(d.rejects, 1); assert.equal(d.lastReject.f.reason, 'CAR_MOVED');
  assert.equal(d.level.f.vertical, 'Z');
  assert.equal(d.next.f.pose, '1');
  const model = run('modelText(derive().poses[7])');
  assert(/caster at the FRONT/.test(model));
  assert(/expected \|roll\| 5\.184 deg, \|pitch\| 3\.562 deg/.test(model));
  assert(/error -0\.064/.test(model), model);   // 5.12 - 5.184
  assert(/error -0\.112/.test(model), model);   // 3.45 - 3.562
  assert.equal(run('verdictClass("PASS: x")'), 'ok');
  assert.equal(run('verdictClass("FAIL: x")'), 'bad');
  assert.equal(run('verdictClass("TOO SMALL: x")'), 'warn');
  assert.equal(run('verdictClass("RECORD: x")'), 'info');

  // restart clears the pose table (and only the pose table)
  feed(distRun(1, 13491, 13520));
  feed(`[MEAS RESULT] seq=${++seq} IMU_RESTART capture=${seq}`);
  d = run('derive()');
  assert.equal(Object.keys(d.poses).length, 0);
  assert.equal(d.runs.length, 1);
  assert.equal(d.rejects, 0);
}

// ---------------------------------------------------------------- 4. replay safety: duplicates and out-of-order
reset();
{
  const lines = [
    distRun(1, 13491, 13520),
    poseLine(1, 0, 0, 'Z', 0, 'BASELINE'),
    poseLine(2, 0.1, 0.05, 'Z', 0, 'PASS: level repeats'),
    distRun(2, 13480, 13500),
  ];
  const snapshot = () => JSON.stringify(run('(()=>{const d=derive();return{runs:d.runs.map(e=>e.f.run),poses:Object.keys(d.poses)}})()'));
  lines.forEach((l) => run(`ingestLine(${JSON.stringify(l)})`));
  const once = snapshot();
  lines.forEach((l) => run(`ingestLine(${JSON.stringify(l)})`));      // duplicate replay (log + /result)
  assert.equal(snapshot(), once);
  reset();
  lines.slice().reverse().forEach((l) => run(`ingestLine(${JSON.stringify(l)})`)); // out of order
  assert.equal(snapshot(), once);
  assert.deepEqual(JSON.parse(once).runs, ['1', '2']);
}

// ---------------------------------------------------------------- 5. streaming with partial lines
reset();
{
  const l1 = distRun(1, 13491, 13520), l2 = poseLine(1, 0, 0, 'Z', 0, 'BASELINE');
  const stream = l1 + '\n' + liveLine() + '\r\n' + l2 + '\n';
  const cut = Math.floor(l1.length / 2);
  run(`feed(${JSON.stringify(stream.slice(0, cut))})`);
  assert.equal(run('events.size'), 0, 'partial line must not be parsed');
  run(`feed(${JSON.stringify(stream.slice(cut, l1.length + 20))})`);
  run(`feed(${JSON.stringify(stream.slice(l1.length + 20))})`);
  assert.equal(run('events.size'), 2);
  assert.equal(run('live.dist'), 'COUNTING');
  assert.equal(run('carry'), '');
}

// ---------------------------------------------------------------- 6. rendering
reset();
{
  document.getElementById('dist').value = '1000';
  run(`ingestLine(${JSON.stringify(distRun(1, 13491, 13520))})`);
  run(`ingestLine(${JSON.stringify(poseLine(2, 0.1, 0.05, 'Z', 0, 'PASS: level repeats'))})`);
  run(`ingestLine(${JSON.stringify(nextLine(3, '3/7 FRONT RAISED'))})`);
  run(`ingestLine(${JSON.stringify(liveLine())})`);
  run('render()');
  assert(/RUN IN PROGRESS\s+LEFT 1234\s+RIGHT 1240/.test(textOf('distlive')));
  assert(/pitch 0\.11/.test(textOf('imulive')));
  assert.equal(textOf('imunext'), 'NEXT POSE 3/7 FRONT RAISED');
  assert.equal(elements.runs.children.length, 2);                       // thead + tbody
  assert.equal(elements.runs.children[1].children.length, 1);           // one run row
  assert.equal(elements.poses.children[1].children.length, 7);          // seven pose rows
  assert(/USE FOR THE HUMP/.test(textOf('diststats')));
  assert(/=== ENCODER DISTANCE ===/.test(textOf('summary')) && /=== IMU ORIENTATION ===/.test(textOf('summary')));
  // IMU error state
  run('ingestLine("[MEAS LIVE] dist=IDLE left=- right=- inv_left=0 inv_right=0 imu=ERR x=- y=- z=- pitch=- roll=- tilt=- roll_chg=- pose=1 capturing=0 gp20=IMU")');
  run('render()');
  assert(/not responding/.test(textOf('imulive')));
  assert(/Idle\./.test(textOf('distlive')));
  // no data-driven innerHTML anywhere in the page script
  assert(!/innerHTML|insertAdjacentHTML|document\.write/.test(script));
}

// ---------------------------------------------------------------- 7. page <-> firmware contract
// Every key the page reads must be emitted by a printf format string in the firmware.
{
  const needed = {
    DIST_RUN: ['run', 'left', 'right', 'inv_left', 'inv_right'],
    IMU_POSE: ['capture', 'pose', 'x', 'y', 'z', 'pitch', 'roll', 'tilt', 'roll_chg', 'axis', 'exp_roll', 'exp_pitch', 'retry', 'verdict'],
    IMU_LEVEL: ['vertical', 'value', 'level_pitch', 'level_roll', 'warn'],
    IMU_REJECT: ['pose', 'reason'],
    IMU_NEXT: ['pose', 'total', 'text'],
    IMU_RESTART: [],
  };
  for (const [kind, keys] of Object.entries(needed)) {
    const start = firmware.indexOf(`seq=%u ${kind}`);
    assert(start >= 0, `firmware never prints a ${kind} result`);
    const stmt = firmware.slice(start, firmware.indexOf(');', start));
    keys.forEach((k) => assert(new RegExp(`\\b${k}=%`).test(stmt), `firmware ${kind} lacks ${k}=%`));
  }
  const liveStart = firmware.indexOf('printf("[MEAS LIVE]');
  assert(liveStart >= 0, 'firmware never prints a live line');
  const liveStmt = firmware.slice(liveStart, firmware.indexOf(');', liveStart));
  ['dist', 'left', 'right', 'inv_left', 'inv_right', 'imu', 'x', 'y', 'z', 'pitch', 'roll', 'tilt', 'roll_chg', 'pose', 'capturing', 'gp20']
    .forEach((k) => assert(new RegExp(`\\b${k}=%`).test(liveStmt.replace(/\s+/g, ' ')), `live line lacks ${k}=%`));
  // commands the page can send must be accepted by the firmware dispatcher and the transport whitelist
  const core = fs.readFileSync(path.join(root, 'firmware/app/robot_firmware/wireless/calibration_core.c'), 'utf8');
  const buttons = [...html.matchAll(/data-command="(.)"/g)].map((m) => m[1]);
  assert.deepEqual([...new Set(buttons)].sort(), ['d', 'g', 'i', 'n', 'p', 'x']);
  buttons.forEach((b) => {
    assert(new RegExp(`case '${b}':`).test(firmware), `firmware does not handle '${b}'`);
    assert(b === 'x' || core.includes(`command == '${b}'`), `transport does not whitelist '${b}'`);
  });
  // result-line prefix retained by the transport equals what the firmware prints
  assert(core.includes('"[MEAS RESULT] "') && firmware.includes('[MEAS RESULT] seq='));
}

console.log('Measurement page passed: line parsing, distance maths/rejection/rescale, IMU sequence+retry+restart, ' +
  'duplicate/out-of-order replay, partial-line streaming, rendering, and page<->firmware contract. Stub DOM only; not a browser run.');
