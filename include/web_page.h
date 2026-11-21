#pragma once
#include <pgmspace.h>
const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>F50 风扇管理</title><style>
:root{color-scheme:dark;font-family:system-ui,-apple-system,"Microsoft YaHei",sans-serif;background:#0b1220;color:#e5edf8}*{box-sizing:border-box}body{margin:0 auto;padding:32px 18px;max-width:940px}header{display:flex;justify-content:space-between;align-items:center;gap:12px}h1{font-size:27px;margin:0}h2{font-size:17px;margin:0 0 18px}p{color:#a8b9cf;line-height:1.7;font-size:14px}.tag{border:1px solid #34465e;border-radius:30px;padding:7px 12px;font-size:12px;color:#b4c5db}.grid{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-top:20px}.card{background:#131f31;border:1px solid #27364e;border-radius:18px;padding:23px}.wide{grid-column:1/-1}.value{font-size:33px;font-weight:650;margin:8px 0}.ok{color:#67e8b0}.warn{color:#f4c67a}.muted{color:#a8b9cf}dl{margin:0}dl>div{display:flex;justify-content:space-between;gap:16px;padding:10px 0;border-bottom:1px solid #27364e;font-size:14px}dt{color:#a8b9cf}dd{margin:0;text-align:right;overflow-wrap:anywhere}button,a.button{font:inherit;font-size:14px;border:1px solid #445672;background:#24364e;color:#e5edf8;border-radius:10px;padding:12px 17px;cursor:pointer;text-decoration:none;display:inline-block}button.primary{background:#67e8b0;color:#0c2820;border-color:#67e8b0}button:disabled{opacity:.4;cursor:wait}.actions{display:flex;flex-wrap:wrap;gap:10px;margin-top:20px}#notice{min-height:28px;font-size:14px;color:#f4c67a;margin-top:14px}table{border-collapse:collapse;width:100%;font-size:13px}td,th{text-align:left;border-bottom:1px solid #27364e;padding:11px 5px}th{color:#a8b9cf}code{background:#0b1220;padding:3px 6px;border-radius:4px}footer{font-size:12px;color:#8da1bb;margin-top:24px;line-height:1.8}@media(max-width:620px){.grid{grid-template-columns:1fr}.card{padding:19px}header{align-items:flex-start}h1{font-size:24px}}
</style></head><body>
<header><div><h1>F50 风扇管理</h1><p>ESP32-C3 · USB 串口控制与连接诊断</p></div><span class="tag" id="live">正在连接…</span></header>
<div id="notice" role="status" aria-live="polite"></div>
<main class="grid">
<section class="card"><h2>风扇输出</h2><div class="value" id="fan">—</div><span class="tag" id="mode">—</span><p id="intent">读取控制状态中…</p><div class="actions"><button class="primary" data-action="on">手动开启</button><button data-action="off">手动关闭</button><button data-action="auto">恢复 F50 自动</button></div><p>手动模式持续 10 分钟，到期交回 F50。此处显示 GPIO 输出状态，未接转速反馈，不能判断叶片是否实际转动。</p></section>
<section class="card"><h2>F50 连接</h2><div class="value" id="link">—</div><dl><div><dt>USB 主机信号</dt><dd id="usb">—</dd></div><div><dt>最后有效串口命令</dt><dd id="last">—</dd></div><div><dt>距上次命令</dt><dd id="age">—</dd></div><div><dt>本次连接接收计数</dt><dd id="count">—</dd></div></dl><div class="actions"><button id="test">测试串口接收</button></div><p id="testResult">测试会等待 15 秒，确认期间是否收到新命令或心跳。</p></section>
<section class="card wide"><h2>网络与配网</h2><dl><div><dt>Wi-Fi 状态</dt><dd id="wifi">—</dd></div><div><dt>网络 / 配网热点</dt><dd id="ssid">—</dd></div><div><dt>管理地址</dt><dd id="address">—</dd></div><div><dt>信号强度</dt><dd id="rssi">—</dd></div><div><dt>运行时间</dt><dd id="uptime">—</dd></div></dl><div class="actions"><button id="provision">开启 WiFiManager 配网</button><a class="button" id="portal" href="/">打开配网页</a></div><p>配网与风扇管理统一使用 80 端口，门户首页可直接控制风扇。热点为 F50-FAN-xxxx，密码 12345678；未连接 Wi-Fi 时 15 秒后自动开启热点，串口控制继续工作。运行中长按 BOOT 5 秒清除保存的 Wi-Fi 并重新配网；短按无效。</p></section>
<section class="card wide"><h2>状态灯说明</h2><table><thead><tr><th>外接灯</th><th>灯效</th><th>含义</th></tr></thead><tbody><tr><td>GPIO4 · F50</td><td>常亮</td><td>USB 主机存在，30 秒内收到有效命令</td></tr><tr><td>GPIO4 · F50</td><td>每 2 秒双闪</td><td>USB 主机存在，暂无近期通信</td></tr><tr><td>GPIO4 · F50</td><td>每 2 秒短闪</td><td>未检测到 USB 主机信号</td></tr><tr><td>GPIO5 · Wi-Fi</td><td>常亮 / 快闪 / 慢闪</td><td>已联网 / 配网热点开启 / 正在重连</td></tr></tbody></table><p>GPIO4、GPIO5 各通过 330Ω～1kΩ 电阻接 LED 正极，负极接 GND。板载 GPIO8 与风扇共用，不能独立表示连接状态。</p></section>
</main><footer>USB 主机信号不等于 F50 温控程序存活；有效命令也可能来自电脑串口工具。旧 F50 脚本只在温度跨阈值时发送命令，长期安静不代表故障。启用配套心跳脚本后可持续监测通信。自动温控阈值仍由 F50 决定。</footer>
<script>
let token='',busy=false;
const el=id=>document.getElementById(id);
const text=(id,value)=>el(id).textContent=value;
const tests={idle:'测试会等待 15 秒，确认期间是否收到新命令或心跳。',waiting:'等待新的 F50 命令 / 心跳…也可在 F50 执行 esp_send.sh PING。',received:'通过：测试期间收到了新的有效串口消息。',no_usb:'未检测到 USB 主机，请检查数据线、供电与连接方向。',timeout:'15 秒内没有新消息。旧温控脚本可能只是没有发送；请执行 PING 或启用心跳。'};
function render(s){
 token=s.token;text('live','实时 · 每 2 秒刷新');
 text('fan',s.fan?'已开启':'已关闭');el('fan').className='value '+(s.fan?'ok':'muted');
 text('mode',s.manual?'手动 · 剩余 '+s.manualRemaining+' 秒':'F50 自动');
 text('intent',s.autoKnown?'F50 最近要求：'+(s.autoFan?'开启':'关闭')+(s.manual?'，恢复自动时应用。':'。'):'尚未收到 F50 风扇指令；恢复自动后保持当前输出，等待 F50 下发。');
 text('link',!s.usb?'USB 未连接':s.recent?'近期通信正常':'USB 已连接 · 待通信');el('link').className='value '+(s.recent?'ok':'warn');
 text('usb',s.usb?'已检测到':'未检测到');text('last',s.lastCommand);text('age',s.rxAge===null?'尚未收到':s.rxAge+' 秒');text('count',s.rxCount);text('testResult',tests[s.test]||s.test);el('test').disabled=s.test==='waiting';
 text('wifi',s.portal?'配网热点已开启'+(s.wifi?' · Wi-Fi 已连接':''):s.wifi?'已连接':'正在连接');text('ssid',s.ssid);text('address','http://'+location.hostname+'/fan');text('rssi',s.wifi?s.rssi+' dBm':'—');text('uptime',Math.floor(s.uptime/3600)+' 时 '+Math.floor(s.uptime%3600/60)+' 分 '+s.uptime%60+' 秒');
 el('portal').href='http://'+location.hostname+'/';
}
async function request(path,action){
 const opts={cache:'no-store',signal:AbortSignal.timeout(5000)};
 if(action!==undefined){opts.method='POST';opts.headers={'X-Control-Token':token,'Content-Type':'application/x-www-form-urlencoded'};opts.body=new URLSearchParams({action});}
 const r=await fetch(path,opts);if(!r.ok)throw Error('HTTP '+r.status+'，请刷新页面');const s=await r.json();render(s);return s;
}
async function mutate(path,action){if(busy||!token)return;busy=true;try{await request(path,action);text('notice','操作已完成');return true;}catch(e){text('notice',e.message);return false;}finally{busy=false;}}
document.querySelectorAll('[data-action]').forEach(b=>b.onclick=()=>mutate('/api/fan',b.dataset.action));
el('test').onclick=()=>mutate('/api/test','start');
el('provision').onclick=async()=>{if(await mutate('/api/provision','start'))text('notice','请连接配网热点后访问 http://192.168.4.1/；已保存的 Wi-Fi 不会被清除。');};
async function poll(){if(!busy){try{await request('/api/status');}catch(e){text('live','设备连接中断');text('notice','无法刷新状态，请检查网络。'+e.message);}}setTimeout(poll,2000);}poll();
</script></body></html>)HTML";

