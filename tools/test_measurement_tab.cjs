// Host test for the Measurements tab in firmware/app/robot_firmware/wireless/calibration.html
// and its firmware contract. No browser or npm packages: the tab script runs in a node vm with a
// stub DOM and a scripted fetch(). Run from the repository root:  node tools/test_measurement_tab.cjs
'use strict';
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const path = require('path');

const root = path.resolve(__dirname, '..');
const read = (p) => fs.readFileSync(path.join(root, p), 'utf8');
const html = read('firmware/app/robot_firmware/wireless/calibration.html');
const measC = read('firmware/app/robot_firmware/measurement.c');
const robotC = read('firmware/app/robot_firmware/robot_firmware.c');
const coreC = read('firmware/app/robot_firmware/wireless/calibration_core.c');
const wifiC = read('firmware/app/robot_firmware/wireless/calibration_wifi.c');
const script = /<script id="measure-script">([\s\S]*?)<\/script>/.exec(html)[1];

// ---------------------------------------------------------------- stub DOM
const elements = {};
function makeElement(id) {
  let text = '';
  return {
    id, value: '', className: '', children: [], dataset: {}, hidden: false, disabled: false, attrs: {},
    get textContent() { return text; },
    set textContent(v) { text = String(v); this.children = []; },
    appendChild(c) { this.children.push(c); return c; },
    setAttribute(k, v) { this.attrs[k] = String(v); },
  };
}
const buttons = ['g', 'h', 'p', 'w'].map((m) => { const b = makeElement('btn-' + m); b.dataset.m = m; return b; });
const document = {
  getElementById(id) { if (!elements[id]) { elements[id] = makeElement(id); } return elements[id]; },
  createElement(tag) { return makeElement(tag); },
  querySelectorAll(sel) { return sel === '.mbtn' ? buttons : []; },
};
const calls = [];
let measureBody = 'boot=11111111\n';
let idleHeader = '1';
let bootHeader = '11111111';
let commandStatus = 202;
const fetchStub = async (url, opts = {}) => {
  calls.push({ url, method: opts.method || 'GET', headers: opts.headers || {} });
  if (url === '/measure') {
    return { ok: true, status: 200, headers: { get: (h) => (h === 'X-Idle' ? idleHeader : h === 'X-Boot' ? bootHeader : null) }, text: async () => measureBody };
  }
  return { ok: commandStatus < 300, status: commandStatus, headers: { get: () => null }, text: async () => (commandStatus < 300 ? 'Measurement command queued.\n' : 'rejected') };
};
const timers = [];
const context = vm.createContext({
  document, console, fetch: fetchStub,
  setTimeout: (fn, ms) => { if (ms === 700) { timers.push(fn); } return 1; }, clearTimeout() {},
  setInterval: () => 1, clearInterval() {},
  AbortController: class { constructor() { this.signal = {}; } abort() {} },
  Blob: class { constructor(parts) { this.parts = parts; } }, URL: { createObjectURL: () => '', revokeObjectURL() {} },
  window: { dispatchEvent() { context.resized = (context.resized || 0) + 1; } }, Event: class {},
  Date, Math, Number, Map, Array, Object, JSON, String, Error, Promise,
});
const tick = async () => { for (let i = 0; i < 8; i++) { await new Promise((r) => setImmediate(r)); } };
document.getElementById('m-dist').value = '1000';
vm.runInContext(script, context, { filename: 'calibration.html#measure-script' });
const text = (id) => elements[id].textContent;
const rowsOf = (id) => elements[id].children.find((c) => c.id === 'tbody').children.map((tr) => tr.children.map((td) => td.textContent));
// makeElement ids for created tags are 'thead'/'tr'/'td'; find the tbody
const tbodyRows = (id) => {
  const body = elements[id].children.filter((c) => c.id === 'tbody')[0];
  return body.children.map((tr) => ({ cls: tr.className, cells: tr.children.map((td) => td.textContent) }));
};

// ---------------------------------------------------------------- fixtures: the real results from the first measurement session
const REAL = `boot=11111111
[MEAS LIVE] dist=IDLE left=- right=- inv_left=0 inv_right=0 imu=OK x=-78.0 y=57.0 z=1149.2 pitch=2.83 roll=-3.88 tilt=2.99 roll_chg=-4.86 pose=1 capturing=0
[MEAS RESULT] seq=2 DIST_RUN run=1 left=12003 right=11811 raw_left=-12003 raw_right=11811 inv_left=0 inv_right=0
[MEAS RESULT] seq=3 DIST_RUN run=2 left=12021 right=11986 raw_left=-12021 raw_right=11986 inv_left=0 inv_right=0
[MEAS RESULT] seq=4 DIST_RUN run=3 left=12047 right=11891 raw_left=-12047 raw_right=11891 inv_left=0 inv_right=0
[MEAS RESULT] seq=5 DIST_RUN run=4 left=12028 right=11822 raw_left=-12028 raw_right=11822 inv_left=0 inv_right=0
[MEAS RESULT] seq=6 DIST_RUN run=5 left=12030 right=11832 raw_left=-12030 raw_right=11832 inv_left=0 inv_right=0
[MEAS RESULT] seq=7 IMU_POSE capture=1 pose=1 x=19.85 y=-8.06 z=1152.28 sx=2.17 sy=2.12 sz=3.02 mag=1152.48 pitch=-0.401 roll=0.987 tilt=0.000 roll_chg=0.000 axis=X exp_roll=5.184 exp_pitch=3.562 retry=0 verdict=BASELINE
[MEAS RESULT] seq=8 IMU_LEVEL vertical=Z value=1152.3 level_pitch=-0.401 level_roll=0.987 warn=NONE
[MEAS RESULT] seq=12 IMU_POSE capture=3 pose=3 x=18.43 y=-9.11 z=1153.06 sx=2.16 sy=1.86 sz=2.97 mag=1153.24 pitch=-0.453 roll=0.916 tilt=0.052 roll_chg=-0.071 axis=X exp_roll=5.184 exp_pitch=3.562 retry=1 verdict=TOO SMALL: raise the front further, then capture again
[MEAS RESULT] seq=20 IMU_POSE capture=7 pose=3 x=3.45 y=-147.20 z=1150.46 sx=1.95 sy=2.32 sz=2.77 mag=1159.84 pitch=-7.291 roll=0.170 tilt=6.891 roll_chg=-0.816 axis=Y exp_roll=5.184 exp_pitch=3.562 retry=0 verdict=PASS: front raised = positive tilt, forward axis = Y
[MEAS RESULT] seq=28 IMU_POSE capture=11 pose=7 x=-77.99 y=57.66 z=1149.72 sx=2.48 sy=2.39 sz=3.17 mag=1153.80 pitch=2.864 roll=-3.876 tilt=-3.265 roll_chg=-4.863 axis=X exp_roll=5.184 exp_pitch=3.562 retry=0 verdict=PASS: matches model; caster is at the FRONT (front lowered)
[MEAS RESULT] seq=30 IMU_NEXT pose=1 total=7 text=1/7 LEVEL: car on the flat floor, all wheels and the caster down
`;
const approx = (a, b, eps) => assert(Math.abs(a - b) < eps, `${a} != ${b}`);

(async () => {
  // ---- 1. nothing happens until the tab is opened
  assert.equal(calls.length, 0, 'no network traffic before the tab is opened');
  assert.equal(elements['m-summary'].textContent.includes('No runs latched yet') || elements['m-summary'].textContent.includes('Enter'), true);

  // ---- 2. opening the tab activates the car ('m') and polls /measure; Testing tab content is hidden/shown
  measureBody = REAL;
  elements['tab-measure'].onclick();
  await tick();
  assert.equal(elements['test-tab'].hidden, true);
  assert.equal(elements['measure-tab'].hidden, false);
  assert.equal(elements['tab-measure'].attrs['aria-selected'], 'true');
  assert.equal(elements['tab-test'].attrs['aria-selected'], 'false');
  assert.deepEqual(calls[0], { url: '/command/m', method: 'POST', headers: { 'X-Calibrate': '1' } });
  assert(calls.some((c) => c.url === '/measure'));

  // ---- 3. distance maths with the real five runs (marked distance 1000 mm)
  assert.match(text('m-s-n'), /^5$/);
  assert.match(text('m-s-r'), /^11\.8684 counts\/mm · 0\.08426 mm\/count$/);
  assert.match(text('m-s-l'), /^12\.0258 counts\/mm · 0\.08315 mm\/count$/);
  assert.match(text('m-s-sp'), /L 0\.37% · R 1\.47%/);
  assert.match(text('m-s-d'), /^1\.32%$/);
  const runs = tbodyRows('m-runs');
  assert.equal(runs.length, 5);
  assert.equal(runs[0].cls, 'ok');
  assert.equal(runs[0].cells[0], '1');
  assert.equal(runs[0].cells[4], '11.8110');
  // changing the marked distance rescales
  elements['m-dist'].value = '2000'; elements['m-dist'].oninput();
  assert.match(text('m-s-r'), /^5\.9342 counts\/mm/);
  assert(/CHECK|check/.test(tbodyRows('m-runs')[0].cells[6]), 'far-from-nominal flag at 2000 mm');
  elements['m-dist'].value = '1000'; elements['m-dist'].oninput();

  // ---- 4. IMU poses, retry row, level, pose 7 model and implied height
  const poses = tbodyRows('m-poses');
  assert.equal(poses.length, 7);
  assert.equal(poses[0].cells[4], 'BASELINE');
  assert.equal(poses[2].cls, 'ok');                       // retry capture 12 was superseded by pose 3 PASS (seq 20)
  assert.match(poses[2].cells[4], /^PASS: front raised/);
  assert.equal(poses[3].cells[4], 'Not captured yet');
  assert.equal(poses[6].cls, 'ok');
  assert.equal(text('m-e-end'), 'Front (front lowered)');
  assert.equal(text('m-e-exp'), '5.18° / 3.56°');
  assert.equal(text('m-e-meas'), '4.86° / 3.27°');
  // independent hand calculation of the inverse model for roll 4.863 deg, tilt 3.265 deg
  const r = Math.sin(4.863 * Math.PI / 180), p = Math.sin(3.265 * Math.PI / 180), k = 1 / Math.sqrt(1 - r * r - p * p);
  approx(Number(text('m-e-hr').replace(' mm', '')), 110 * r * k, 0.006);
  approx(Number(text('m-e-hp').replace(' mm', '')), 160 * p * k, 0.006);
  assert.match(text('m-summary'), /USE FOR THE HUMP \(right wheel\): 11\.8684 counts\/mm/);
  assert.match(text('m-summary'), /Implied block height/);
  assert.match(text('m-next'), /^Next pose · 1\/7 LEVEL/);

  // ---- 5. live readout
  assert.equal(text('m-dstate'), 'Idle');
  assert.equal(text('m-i-s'), 'Ready');
  assert.equal(text('m-i-p'), '2.83°');
  assert.equal(text('m-i-c'), '-4.86°');
  measureBody = REAL.replace(/dist=IDLE left=- right=-/, 'dist=COUNTING left=1234 right=1210').replace('inv_left=0 inv_right=0 imu', 'inv_left=1 inv_right=0 imu').replace('capturing=0', 'capturing=1');
  await timers.pop()(); await tick();
  assert.equal(text('m-dstate'), 'Counting');
  assert.equal(text('m-dl'), '1234');
  assert.equal(text('m-ds'), '1 / 0');
  assert.equal(text('m-i-s'), 'Capturing…');

  // ---- 6. buttons: enabled when online+idle, disabled when the car is busy
  assert.equal(buttons.every((b) => !b.disabled), true);
  idleHeader = '0';
  await timers.pop()(); await tick();
  assert.equal(buttons.every((b) => b.disabled), true);
  assert.match(text('m-message'), /running or settling/);
  idleHeader = '1';
  await timers.pop()(); await tick();
  assert.equal(buttons.every((b) => !b.disabled), true);

  // ---- 7. each button sends exactly its command with the control header
  for (const b of buttons) {
    const before = calls.length;
    await b.onclick(); await tick();
    assert.deepEqual(calls[before], { url: '/command/' + b.dataset.m, method: 'POST', headers: { 'X-Calibrate': '1' } });
  }
  commandStatus = 409;
  await buttons[0].onclick(); await tick();
  assert.match(text('m-message'), /Not accepted/);
  commandStatus = 202;

  // ---- 8. a car restart (new boot id) clears results and re-activates the car
  bootHeader = '22222222'; measureBody = 'boot=22222222\n';
  const before = calls.length;
  await timers.pop()(); await tick();
  assert.equal(tbodyRows('m-runs').length, 0);
  assert.match(text('m-message'), /restarted/);
  assert(calls.slice(before).some((c) => c.url === '/command/m'));

  // ---- 9. leaving the tab switches the car off and restores the Testing tab
  const beforeLeave = calls.length;
  elements['tab-test'].onclick(); await tick();
  assert.equal(elements['test-tab'].hidden, false);
  assert.equal(elements['measure-tab'].hidden, true);
  assert.deepEqual(calls[beforeLeave], { url: '/command/e', method: 'POST', headers: { 'X-Calibrate': '1' } });
  assert.equal(context.resized, 1, 'resize event redraws the optical plot');
  assert.equal(buttons.every((b) => b.disabled), true);

  // ---- 10. firmware / page / transport contract
  const sentChars = Array.from(new Set((script.match(/sendCommand\('(\w)'/g) || []).map((s) => s[13])));
  ['m', 'e'].forEach((c) => assert(sentChars.includes(c), 'page sends ' + c));
  html.match(/data-m="(\w)"/g).map((s) => s[8]).concat(['m', 'e']).forEach((c) => {
    assert(measC.includes(`ch != '${c}'`), `measurement.c handles ${c}`);
    assert(coreC.includes(`command == '${c}'`), `calibration_command whitelists ${c}`);
    assert(wifiC.includes(c), `transport mentions ${c}`);
  });
  assert(wifiC.includes('strchr("mepghw"'), 'transport acknowledges exactly m e p g h w');
  assert(/strcmp\(path, "\/measure"\)/.test(wifiC), 'GET /measure served');
  assert(robotC.includes('measurement_command(ch,') && robotC.includes('measurement_poll('), 'main loop hooks present');
  assert(!/(motors_|pwm_|set_motor|servo_)/.test(measC.replace(/\/\*[\s\S]*?\*\//g, '')), 'measurement.c has no motor or servo calls');
  // the page's retained-result line formats match the firmware printf formats
  ['DIST_RUN run=%u left=%lld right=%lld', 'IMU_POSE capture=%u pose=%u', 'IMU_LEVEL vertical=%s', 'IMU_NEXT pose=%u total=%u text=%s', 'IMU_REJECT capture=%u pose=%u reason=%s', 'IMU_RESTART', 'IMU_COMPLETE'].forEach((f) => assert(measC.includes(f), 'format ' + f));
  const liveFmt = measC.slice(measC.indexOf('"[MEAS LIVE] dist='));
  ['dist=', 'left=', 'right=', 'inv_left=', 'inv_right=', 'imu=', 'x=', 'y=', 'z=', 'pitch=', 'roll=', 'tilt=', 'roll_chg=', 'pose=', 'capturing='].forEach((k) => assert(liveFmt.includes(k), 'live field ' + k));
  assert(!/id="(log|result|download|message|connection)"[^>]*>[\s\S]*id="\1"/.test(html), 'no duplicate ids for core elements');
  const ids = html.match(/ id="[^"]+"/g);
  assert.equal(new Set(ids).size, ids.length, 'every id is unique');
  console.log('measurement tab: all checks passed');
})().catch((e) => { console.error(e); process.exit(1); });
