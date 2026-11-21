const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const page = fs.readFileSync(path.join(__dirname, '../include/web_page.h'), 'utf8');
const source = page.match(/<script>([\s\S]*?)<\/script>/)[1].replace(/poll\(\);\s*$/, '');
const elements = new Map();
const element = id => {
  if (!elements.has(id)) elements.set(id, {textContent: '', className: '', disabled: false});
  return elements.get(id);
};
const sample = {token: 'test-token', fan: false, manual: false, autoKnown: false,
  autoFan: false, manualRemaining: 0, usb: true, recent: false, rxCount: 0,
  rxAge: null, lastCommand: 'NONE', test: 'idle', wifi: false, portal: true,
  ssid: '<img src=x onerror=alert(1)>', ip: '192.168.4.1', rssi: 0, uptime: 3601};
const calls = [];
const context = vm.createContext({document: {getElementById: element, querySelectorAll: () => []},
  location: {hostname: '192.168.4.1'}, URLSearchParams, AbortSignal, setTimeout: () => {},
  fetch: async (url, opts) => {calls.push({url, opts}); return {ok: true, json: async () => sample};}});
vm.runInContext(source, context);
context.sample = sample;
vm.runInContext('render(sample)', context);
assert.equal(element('link').textContent, 'USB 已连接 · 待通信');
assert.equal(element('ssid').textContent, sample.ssid); // SSID rendered as text, never HTML.
assert.match(element('intent').textContent, /尚未收到/);
assert.equal(element('portal').href, 'http://192.168.4.1/');
sample.usb = false;
vm.runInContext('render(sample)', context);
assert.equal(element('link').textContent, 'USB 未连接');
sample.usb = true; sample.recent = true; sample.manual = true;
sample.autoKnown = true; sample.autoFan = true; sample.manualRemaining = 42;
sample.test = 'waiting';
vm.runInContext('render(sample)', context);
assert.equal(element('link').textContent, '近期通信正常');
assert.equal(element('mode').textContent, '手动 · 剩余 42 秒');
assert.match(element('intent').textContent, /开启/);
assert.equal(element('test').disabled, true);
(async () => {
  await vm.runInContext("request('/api/fan','off')", context);
  const post = calls.at(-1);
  assert.equal(post.opts.method, 'POST');
  assert.equal(post.opts.headers['X-Control-Token'], 'test-token');
  assert.equal(post.opts.body.toString(), 'action=off');
  context.fetch = async () => ({ok: false, status: 403});
  await assert.rejects(vm.runInContext("request('/api/fan','on')", context), /403/);
  console.log('PASS: USB/receive distinction, manual intent, text-safe SSID, test wait, POST token and error handling');
})().catch(e => {console.error(e); process.exitCode = 1;});
