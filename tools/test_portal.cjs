const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const panel = fs.readFileSync(path.join(__dirname, '../include/portal_panel.h'), 'utf8');
const script = panel.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = new Map();
const el = id => {
  if (!elements.has(id)) elements.set(id, {textContent:'', disabled:false});
  return elements.get(id);
};
const buttons = ['on','off','auto'].map(fan => ({dataset:{fan}}));
const calls = [];
const sample = {token:'portal-token', fan:true, manual:false, manualRemaining:0,
  usb:true, recent:true, test:'idle'};
const context = vm.createContext({document:{getElementById:el, querySelectorAll:()=>buttons},
  URLSearchParams, AbortSignal, setTimeout:()=>{}, fetch:async (url,opts)=>{
    calls.push({url,opts}); return {ok:true,json:async()=>({...sample})};
  }});
const tick = () => new Promise(resolve => setImmediate(resolve));
(async()=>{
  vm.runInContext(script,context);
  await tick();
  assert.match(el('portal-state').textContent,/开启.*F50 自动/);
  assert.match(el('portal-link').textContent,/近期通信正常/);
  assert.equal(calls[0].url,'/api/status');
  await buttons[1].onclick();
  assert.equal(calls.at(-1).url,'/api/fan');
  assert.equal(calls.at(-1).opts.body.toString(),'action=off');
  assert.equal(calls.at(-1).opts.headers['X-Control-Token'],'portal-token');
  sample.test='waiting';
  await el('portal-test').onclick();
  assert.equal(calls.at(-1).url,'/api/test');
  assert.equal(el('portal-test').disabled,true);
  assert.match(el('portal-notice').textContent,/等待/);
  sample.usb=false; sample.test='no_usb';
  await buttons[2].onclick();
  assert.equal(calls.at(-1).opts.body.toString(),'action=auto');
  assert.match(el('portal-link').textContent,/未检测到 USB/);
  assert.match(el('portal-notice').textContent,/未检测到 USB/);
  context.fetch=async()=>({ok:false,status:403});
  await buttons[0].onclick();
  assert.match(el('portal-notice').textContent,/403/);
  assert.match(panel,/href="\/fan"/);
  assert.ok(!panel.includes('8080'));
  console.log('PASS: embedded portal status, all control actions, receive test, shared-origin API/token and error handling');
})().catch(e=>{console.error(e);process.exitCode=1;});
