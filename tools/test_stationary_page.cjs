const {chromium}=require('playwright');
const fs=require('fs');const assert=require('assert');
(async()=>{
const browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
const page=await browser.newPage({viewport:{width:1100,height:1000}});let boot='00112233', offline=false,idle=true;let commands=[];let errors=[];
page.on('pageerror',e=>errors.push(e.message));
await page.route('http://192.168.4.1/**',async r=>{const u=new URL(r.request().url());
 if(offline){await r.abort();return;}
 if(u.pathname==='/'){await r.fulfill({contentType:'text/html',body:fs.readFileSync('firmware/app/robot_firmware/wireless/stationary.html','utf8')});return;}
 if(u.pathname==='/logs'){const body=u.searchParams.get('since')==='0'?'AAI10 STATIONARY BARCODE v5.7\n[SCAN LIVE] active=0 motors=OFF IR_GP26=200\n':'';await r.fulfill({headers:{'X-Boot':boot,'X-Next':'80','X-Gap':'0','X-Idle':idle?'1':'0'},body});return;}
 if(u.pathname==='/result'){await r.fulfill({body:'[SCAN STOP] reason=BARCODE_READ motors=OFF\n[BARCODE RESULT] decoded=A valid=1\n[SCAN EDGES 1] total=30 time_ms:level=300:B,700:W\n'});return;}
 if(u.pathname.startsWith('/command/')){assert.equal(r.request().method(),'POST');assert.equal(r.request().headers()['x-calibrate'],'1');commands.push(u.pathname);await r.fulfill({status:202,body:'Selection queued. Wait for [SELECT], then GP20.'});return;}
 await r.fulfill({status:404,body:'Not found'});
});
await page.goto('http://192.168.4.1/');await page.getByRole('button',{name:'Stationary scan · motors off',exact:true}).waitFor();
await page.waitForFunction(()=>document.getElementById('connection').className==='online');
await page.getByRole('button',{name:'Stationary scan · motors off',exact:true}).click();assert.deepEqual(commands,['/command/s']);
assert.equal(await page.locator('.test').count(),1);
await page.getByRole('button',{name:'Finish scan',exact:true}).click();assert.deepEqual(commands,['/command/s','/command/x']);
assert.equal(await page.locator('input,select,textarea').count(),0);
const downloadPromise=page.waitForEvent('download');await page.locator('#download').click();const download=await downloadPromise;await download.saveAs('build/wireless-host-tests/stationary-test-download.txt');
const saved=fs.readFileSync('build/wireless-host-tests/stationary-test-download.txt','utf8');assert(saved.includes('Physical observations supplied separately in chat.'));assert(saved.includes('[SCAN STOP]'));assert(saved.includes('[BARCODE RESULT] decoded=A valid=1'));assert(saved.includes('v5.7'));assert(!saved.includes('Measured travel (cm)'));
await page.screenshot({path:'build/wireless-host-tests/stationary-desktop.png',fullPage:true});
idle=false;await page.waitForFunction(()=>document.querySelector('.test').disabled);
offline=true;await page.waitForFunction(()=>document.getElementById('connection').className==='offline');
offline=false;idle=true;boot='44556677';await page.waitForFunction(()=>document.getElementById('log').textContent.includes('CAR POWER RESET'));
await page.waitForFunction(()=>document.getElementById('connection').className==='online');
await page.setViewportSize({width:390,height:844});await page.screenshot({path:'build/wireless-host-tests/stationary-mobile.png',fullPage:true});
assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));assert.deepEqual(errors,[]);
console.log('Stationary page passed: selection POST, download, running lockout, disconnect/reconnect, power-reset label, mobile width, no JS errors. Mock robot only.');
await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
