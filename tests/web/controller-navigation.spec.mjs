import {test,expect} from '@playwright/test';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import fs from 'node:fs';
const script=fileURLToPath(new URL('../../src/UIPlatform/Web/meridian-input.js',import.meta.url));
test.beforeEach(async({page})=>{
 await page.setContent(`<style>
 main{width:600px;display:grid;grid-template-columns:200px 200px;gap:20px}button,input,select{height:40px}
 #scroll{height:90px;overflow:auto;grid-column:1 / 3}#scroll button{display:block;height:45px}
 </style><main id="root"><button id="a">A</button><button id="b">B</button>
 <button id="c">C</button><button id="disabled" disabled>Disabled</button>
 <input id="text"><input id="range" type="range" min="0" max="10" value="5">
 <select id="select"><option>One</option><option>Two</option><option disabled>Three</option><option>Four</option></select>
 <div id="scroll">${Array.from({length:8},(_,i)=>'<button id="row'+i+'">Row '+i+'</button>').join('')}</div></main>
 <div id="modal" hidden><button id="modalA">Modal A</button><button id="modalB">Modal B</button></div>`);
 await page.addScriptTag({path:script});
 await page.evaluate(()=>{
  window.requests=[];window.__meridianInstallInput((t,n,p)=>requests.push(JSON.parse(p)),'token');
  window.seq=0;window.clicks=0;window.backs=0;
  document.querySelector('#a').onclick=()=>++window.clicks;
  window.disposeMain=MeridianInput.attachNavigation({root:document.querySelector('#root'),initialFocus:'a',onBack:()=>++window.backs});
  window.send=(action='accept',phase='press',control='south',extra={})=>MeridianInput.__receive({
   version:1,page:requests[0].page,generation:1,sequence:++window.seq,active:true,
   enabled:true,connected:true,mode:'navigation',device:'gamepad',glyphFamily:'xbox',dt:.016,
   events:[{action,phase,control,x:0,y:0,value:1}],...extra});
 });
});
test('initial focus and one activation per press',async({page})=>{
 await expect(page.locator('#a')).toBeFocused();
 await page.evaluate(()=>{send();send('accept','release');send('accept','repeat');});
 expect(await page.evaluate(()=>window.clicks)).toBe(1);
});
test('directional geometry and explicit neighbors',async({page})=>{
 await page.evaluate(()=>send('right','repeat','leftStick'));
 await expect(page.locator('#b')).toBeFocused();
 await page.evaluate(()=>{document.querySelector('#b').dataset.meridianDown='c';send('down','repeat','leftStick');});
 await expect(page.locator('#c')).toBeFocused();
});
test('disabled hidden inert negative tabindex are excluded',async({page})=>{
 await page.evaluate(()=>{
  document.querySelector('#b').setAttribute('inert','');document.querySelector('#c').hidden=true;
  document.querySelector('#text').tabIndex=-1;send('right','repeat','leftStick');
 });
 const selected=await page.evaluate(()=>document.activeElement.id);
 expect(['a','b','c','disabled','text']).not.toContain(selected);
 expect(selected).not.toBe('');
});
test('modal scope traps and restores parent selection',async({page})=>{
 await page.evaluate(()=>{
  send('right','repeat','leftStick');document.querySelector('#modal').hidden=false;
  window.disposeModal=MeridianInput.attachNavigation({root:document.querySelector('#modal'),initialFocus:'modalA',
   onBack:()=>{disposeModal();document.querySelector('#modal').hidden=true;}});
 });
 await expect(page.locator('#modalA')).toBeFocused();
 await page.evaluate(()=>send('cancel','press','east'));
 await expect(page.locator('#b')).toBeFocused();
 expect(await page.evaluate(()=>window.backs)).toBe(0);
});
test('selection survives replacement by stable id',async({page})=>{
 await page.evaluate(()=>{
  document.querySelector('#a').outerHTML='<button id="a">Replacement</button>';
 });
 await expect(page.locator('#a')).toBeFocused();
});
test('removed selection recovers and scrolls into view',async({page})=>{
 await page.evaluate(()=>{
  document.querySelector('#a').dataset.meridianDown='row7';send('down','repeat','leftStick');
 });
 await expect(page.locator('#row7')).toBeFocused();
 expect(await page.locator('#scroll').evaluate(e=>e.scrollTop)).toBeGreaterThan(0);
 await page.locator('#row7').evaluate(e=>e.remove());
 await expect(page.locator('#a')).toBeFocused();
});
test('text edit navigation does not change text or selection',async({page})=>{
 await page.evaluate(()=>{
  document.querySelector('#a').dataset.meridianDown='text';send('down','repeat','leftStick');
  send();document.querySelector('#text').value='Test';send('right','repeat','leftStick');
 });
 await expect(page.locator('#text')).toBeFocused();await expect(page.locator('#text')).toHaveValue('Test');
 await page.evaluate(()=>send('cancel','press','east'));
 expect(await page.evaluate(()=>window.backs)).toBe(0);
 await page.evaluate(()=>send('cancel','press','east'));
 expect(await page.evaluate(()=>window.backs)).toBe(1);
});
test('range edit adjusts bounded value with input event',async({page})=>{
 await page.evaluate(()=>{
  window.changes=0;const range=document.querySelector('#range');range.oninput=()=>++window.changes;
  document.querySelector('#a').dataset.meridianDown='range';send('down','repeat','leftStick');send();
  send('right','repeat','leftStick');
 });
 await expect(page.locator('#range')).toHaveValue('6');
 expect(await page.evaluate(()=>window.changes)).toBe(1);
});
test('cursor mode never also invokes DOM accept',async({page})=>{
 await page.evaluate(()=>send('accept','press','south',{mode:'cursor'}));
 expect(await page.evaluate(()=>window.clicks)).toBe(0);
});
test('custom handler can own analog or navigation actions',async({page})=>{
 await page.evaluate(()=>{MeridianInput.onAction(e=>e.control==='leftStick');send('right','repeat','leftStick');});
 await expect(page.locator('#a')).toBeFocused();
});
test('disposed and hidden scopes cannot activate',async({page})=>{
 await page.evaluate(()=>{disposeMain();send();});
 expect(await page.evaluate(()=>window.clicks)).toBe(0);
});
test('shoulder actions select enabled tabs through existing click handlers',async({page})=>{
 await page.evaluate(()=>{
  const a=document.querySelector('#a'),b=document.querySelector('#b');
  a.setAttribute('role','tab');a.setAttribute('aria-selected','true');b.setAttribute('role','tab');
  b.onclick=()=>{a.setAttribute('aria-selected','false');b.setAttribute('aria-selected','true');};
  send('nextTab','press','rightShoulder');
 });
 await expect(page.locator('#b')).toBeFocused();await expect(page.locator('#b')).toHaveAttribute('aria-selected','true');
});
test('mouse focus becomes the controller selection',async({page})=>{
 await page.locator('#b').click();
 await page.evaluate(()=>{document.querySelector('#b').onclick=()=>++window.clicks;send();});
 expect(await page.evaluate(()=>window.clicks)).toBe(1);
});
test('select edit skips disabled options and cancel exits one level',async({page})=>{
 await page.evaluate(()=>{
  document.querySelector('#a').dataset.meridianDown='select';send('down','press','dpadDown');send();
  send('down','repeat','dpadDown');send('down','repeat','dpadDown');
 });
 await expect(page.locator('#select')).toHaveValue('Four');
 await page.evaluate(()=>send('cancel','press','east'));
 expect(await page.evaluate(()=>window.backs)).toBe(0);
});
test('lifecycle cancellation reaches custom scope handlers',async({page})=>{
 await page.evaluate(()=>{
  window.cancels=0;disposeMain();
  MeridianInput.attachNavigation({root:document.querySelector('#root'),onAction:e=>{if(e.phase==='cancel')++window.cancels;}});
  send();send('accept','press','south',{active:false,events:[]});
 });
 expect(await page.evaluate(()=>window.cancels)).toBe(1);
});
test('right stick scrolls the selected clipped list',async({page})=>{
 await page.evaluate(()=>{
  document.querySelector('#a').dataset.meridianDown='row0';send('down','press','dpadDown');
  send('none','change','rightStick',{dt:0.05,events:[{action:'none',control:'rightStick',phase:'change',x:0,y:-1,value:0}]});
 });
 expect(await page.locator('#scroll').evaluate(e=>e.scrollTop)).toBeGreaterThan(0);
});
test('standalone fixture uses shared navigation and native command callbacks',async({page})=>{
 await page.goto('about:blank');
 const html=fs.readFileSync(new URL('../../src/InputTest/web/index.html',import.meta.url),'utf8');
 await page.setContent(html.replace(/<script[\s\S]*?<\/script>/g,''));
 await page.addScriptTag({path:script});
 await page.evaluate(()=>{
  window.fixtureRequests=[];window.nativeCommands=[];window.nativeCloses=0;window.fixtureSeq=0;
  window.inputTestCommand=c=>nativeCommands.push(c);window.inputTestClose=()=>++window.nativeCloses;
  window.__meridianInstallInput((t,n,p)=>fixtureRequests.push(JSON.parse(p)),'fixture');
  window.fixtureSend=action=>MeridianInput.__receive({version:1,page:fixtureRequests[0].page,
   generation:1,sequence:++window.fixtureSeq,active:true,mode:'navigation',dt:.016,
   events:[{action,phase:'press',control:action==='cancel'?'east':'south',x:0,y:0,value:1}]});
 });
 await page.addScriptTag({path:fileURLToPath(new URL('../../src/InputTest/web/input-test.js',import.meta.url))});
 await page.evaluate(()=>fixtureSend('accept'));
 await expect(page.locator('#result')).toHaveText('Accept count: 1');
 await page.locator('#modalOpen').click();
 await expect(page.locator('#modalAccept')).toBeFocused();
 await page.evaluate(()=>fixtureSend('cancel'));
 await expect(page.locator('#modal')).toBeHidden();
 await page.locator('[data-command="unpaused"]').click();
 expect(await page.evaluate(()=>window.nativeCommands)).toEqual(['unpaused']);
 await page.evaluate(()=>fixtureSend('cancel'));
 expect(await page.evaluate(()=>window.nativeCloses)).toBe(1);
});
test('diagnostic consumer retains mouse and Escape without Input/1',async({page})=>{
 await page.goto('about:blank');
 const html=fs.readFileSync(new URL('../../src/InputTest/web/index.html',import.meta.url),'utf8');
 await page.setContent(html.replace(/<script[\s\S]*?<\/script>/g,''));
 await page.evaluate(()=>{
  window.nativeCloses=0;window.inputTestClose=()=>++window.nativeCloses;window.inputTestCommand=()=>{};
 });
 await page.addScriptTag({path:fileURLToPath(new URL('../../src/InputTest/web/input-test.js',import.meta.url))});
 await expect(page.locator('#cursor')).toBeDisabled();
 await page.locator('#accept').click();
 await expect(page.locator('#result')).toHaveText('Accept count: 1');
 await page.locator('#modalOpen').click();
 await page.keyboard.press('Escape');
 await expect(page.locator('#modal')).toBeHidden();
 expect(await page.evaluate(()=>window.nativeCloses)).toBe(0);
 await page.locator('#close').click();
 expect(await page.evaluate(()=>window.nativeCloses)).toBe(1);
});
