const {chromium}=require('playwright');
const fs=require('fs');const assert=require('assert');
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
try {
const page=await browser.newPage({viewport:{width:1100,height:1000}});
let boot='00112233',offline=false,idle=true,commands=[],errors=[],reply='',continuous=false,resultRequests=0;
page.on('pageerror',e=>errors.push(e.message));
await page.route('http://192.168.4.1/**',async r=>{
 const u=new URL(r.request().url());
 if(offline){await r.abort();return;}
 if(u.pathname==='/'){await r.fulfill({contentType:'text/html',body:fs.readFileSync('firmware/app/robot_firmware/wireless/calibration.html','utf8')});return;}
 if(u.pathname==='/logs'){
  let body=u.searchParams.get('since')==='0'?'AAI10 MOTION INTEGRATION v5.32 WIRELESS\nRun=0 State=STOPPED\n':'';
  body+=reply;reply='';await r.fulfill({headers:{'X-Boot':boot,'X-Next':'80','X-Gap':'0','X-Idle':idle?'1':'0'},body});return;
 }
 if(u.pathname==='/result'){resultRequests++;await r.fulfill({body:continuous?'[MOTION STOP] GP20\n[MOTION SETTLED] CONTINUOUS_SCAN\n[BARCODE RESULT] mode=CONTINUOUS reads=2 sequence=AZ\n':'[OPTICAL RESULT] motors=OFF samples=5000 raw=172..2782 max_gap_us=1017 empty_bins=0 aborted=0 sampling_ok=1\n'});return;}
 if(u.pathname.startsWith('/command/')||u.pathname.startsWith('/mission/')){
  assert.equal(r.request().method(),'POST');assert.equal(r.request().headers()['x-calibrate'],'1');commands.push(u.pathname);await r.fulfill({status:202,body:'Request queued.'});return;
 }
 await r.fulfill({status:404,body:'Not found'});
});
await page.goto('http://192.168.4.1/');await page.waitForFunction(()=>document.getElementById('connection').className==='online');
assert.equal(await page.locator('button:visible').count(),3);assert.equal(await page.locator('#recover').count(),0);
assert.equal(await page.locator('#test-mode').inputValue(),'v');assert(!(await page.locator('#technical').getAttribute('open')));
await page.locator('#prepare').click();assert.equal(commands.at(-1),'/command/v');
idle=false;reply='[OPTICAL START] motors=OFF\n';
await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Recording now'));
assert(await page.locator('#prepare').isDisabled());assert(await page.locator('#test-mode').isDisabled());assert(!(await page.locator('#stop').isDisabled()));
reply='[OPTICAL RESULT] motors=OFF samples=5000 raw=172..2782 max_gap_us=1017 empty_bins=0 aborted=0 sampling_ok=1\n[OPTICAL DATA] bin=0 190:210 200:';
await page.waitForFunction(()=>document.getElementById('optical-summary').textContent.includes('5000 samples'));
reply='1000 - 215:219\n[OPTICAL END] window_ms=10\n';idle=true;
await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Recording saved'));
assert.deepEqual(await page.evaluate(()=>opticalBins.slice(0,4)),[[190,210],[200,1000],null,[215,219]]);
assert((await page.locator('#optical-summary').innerText()).includes('1.02 ms'));assert(resultRequests>=2);
await page.screenshot({path:'build/wireless-host-tests/calibration-desktop.png',fullPage:true});
await page.setViewportSize({width:390,height:844});await page.evaluate(()=>window.scrollTo(0,0));await page.waitForTimeout(200);await page.screenshot({path:'build/wireless-host-tests/calibration-mobile.png',fullPage:true});
assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
const downloadPromise=page.waitForEvent('download');await page.locator('#download').click();await(await downloadPromise).saveAs('build/wireless-host-tests/browser-test-download.txt');
const saved=fs.readFileSync('build/wireless-host-tests/browser-test-download.txt','utf8');assert(saved.includes('v5.32'));assert(saved.includes('[OPTICAL RESULT]'));assert(saved.includes('[OPTICAL DATA]'));assert(saved.includes('Physical observations supplied separately in chat.'));
for(const ch of ['f','l','r','u']){await page.locator('#test-mode').selectOption(ch);await page.locator('#prepare').click();assert.equal(commands.at(-1),'/command/'+ch);assert((await page.locator('#start-help').innerText()).includes('press GP20'));}
await page.locator('#test-mode').selectOption('course');assert(await page.locator('#optical-panel').isHidden());
await page.locator('#prepare').click();assert.equal(commands.at(-1),'/mission/s/s/120');
await page.locator('#barcode-actions summary').click();assert.equal(await page.locator('#y-action option').count(),11);assert.equal(await page.locator('#z-action option').count(),11);
await page.locator('#y-action').selectOption('b');await page.locator('#z-action').selectOption('L');await page.locator('#prepare').click();assert.equal(commands.at(-1),'/mission/b/L/120');
const before=commands.length;await page.locator('#route-advance').fill('0');await page.locator('#prepare').click();assert.equal(commands.length,before);assert((await page.locator('#message').innerText()).includes('60–250'));
await page.locator('#route-advance').fill('120');await page.locator('#prepare').click();
reply='[SELECT Y/Z] Y=STRAIGHT Z=STRAIGHT\n';await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Ready. Press GP20'));
await page.setViewportSize({width:1100,height:1000});await page.screenshot({path:'build/wireless-host-tests/calibration-mission.png',fullPage:true});
idle=false;await page.waitForFunction(()=>document.getElementById('prepare').disabled);assert(await page.locator('#y-action').isDisabled());assert(await page.locator('#z-action').isDisabled());assert(!(await page.locator('#stop').isDisabled()));
await page.locator('#stop').click();assert.equal(commands.at(-1),'/command/x');
reply='[OPTICAL START] source=DRIVE recorded_at_ms=1200 motors=OFF; retained first-activity trace, emitted after stop.\n';
await page.waitForFunction(()=>document.getElementById('message').textContent.includes('driving sensor recording'));
assert(!(await page.locator('#message').innerText()).includes('slide'));
reply='[OPTICAL RESULT] motors=OFF samples=5000 raw=180..2400 max_gap_us=1010 empty_bins=0 aborted=0 sampling_ok=1 source=DRIVE\n[OPTICAL DATA] bin=0 190:240\n[OPTICAL END] window_ms=10\n';
await page.waitForFunction(()=>document.getElementById('message').textContent.includes('Recording saved'));
assert.deepEqual(await page.evaluate(()=>opticalBins[0]),[190,240]);
continuous=true;const streamDownload=page.waitForEvent('download');await page.locator('#download').click();await(await streamDownload).saveAs('build/wireless-host-tests/browser-stream-download.txt');assert(fs.readFileSync('build/wireless-host-tests/browser-stream-download.txt','utf8').includes('reads=2 sequence=AZ'));assert(fs.readFileSync('build/wireless-host-tests/browser-stream-download.txt','utf8').includes('source=DRIVE'));
offline=true;await page.waitForFunction(()=>document.getElementById('connection').className==='offline');assert(await page.locator('#prepare').isDisabled());
offline=false;idle=true;boot='44556677';await page.waitForFunction(()=>document.getElementById('log').textContent.includes('CAR POWER RESET'));await page.waitForFunction(()=>document.getElementById('connection').className==='online');assert.equal(await page.evaluate(()=>opticalBins.length),0);assert(await page.locator('#optical-plot').isHidden());assert.deepEqual(errors,[]);
console.log('Simplified UI passed: three buttons, all modes, fragmented optical trace, auto recovery, one combined download, mission configuration, input validation, running lockout, stop, disconnect/reset and mobile width. Mock robot only.');
} finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1)});
