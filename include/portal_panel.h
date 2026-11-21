#pragma once
// Inserted directly into WiFiManager's native root menu on the shared port 80.
const char PORTAL_PANEL[] = R"PANEL(
<style>.fan-panel{background:#f0f6ff;border:1px solid #b7c8df;border-radius:12px;padding:16px;margin:16px 0;color:#17263a;text-align:left}.fan-panel h2{font-size:20px;margin:0 0 12px}.fan-panel p{font-size:14px;line-height:1.6}.fan-panel .fan-actions{display:flex;gap:6px;flex-wrap:wrap}.fan-panel button{width:auto;flex:1;padding:10px;font-size:14px;margin:4px 0}.fan-panel a{display:block;text-align:center;padding:10px;color:#174d88}.fan-panel #portal-notice{font-size:13px;min-height:20px;overflow-wrap:anywhere}.fan-panel small{line-height:1.7;display:block}</style>
<section class="fan-panel" aria-label="风扇管理">
<h2>F50 风扇管理</h2>
<p id="portal-state" aria-live="polite">正在读取风扇状态…</p>
<p id="portal-link">正在检查 F50 通信…</p>
<div class="fan-actions"><button type="button" data-fan="on">手动开启</button><button type="button" data-fan="off">手动关闭</button></div>
<div class="fan-actions"><button type="button" data-fan="auto">恢复自动</button><button type="button" id="portal-test">连接测试</button></div>
<p id="portal-notice" role="status" aria-live="polite"></p>
<a href="/fan">详细状态与管理 →</a>
<small>手动控制持续 10 分钟。配网请使用下方 Configure WiFi。长按 BOOT 5 秒可清除已保存 Wi-Fi；短按不清除。热点密码：12345678。</small>
</section>
<script>
(()=>{
 let token='',busy=false;
 const el=id=>document.getElementById(id);
 const show=s=>{
  token=s.token;
  el('portal-state').textContent='风扇输出：'+(s.fan?'开启':'关闭')+' · '+(s.manual?'手动（剩余 '+s.manualRemaining+' 秒）':'F50 自动');
  el('portal-link').textContent=!s.usb?'F50：未检测到 USB 主机':s.recent?'F50：近期通信正常':'F50：USB 已连接，等待消息';
  el('portal-test').disabled=s.test==='waiting';
  const messages={waiting:'正在等待新命令或心跳（15 秒）…',received:'通过：已收到新的串口消息。',no_usb:'未检测到 USB，请检查数据连接。',timeout:'没有新消息；可在 F50 执行 esp_send.sh PING 重试。'};
  if(messages[s.test])el('portal-notice').textContent=messages[s.test];
 };
 async function request(path,action){
  const opts={cache:'no-store',signal:AbortSignal.timeout(5000)};
  if(action!==undefined){opts.method='POST';opts.headers={'X-Control-Token':token,'Content-Type':'application/x-www-form-urlencoded'};opts.body=new URLSearchParams({action});}
  const r=await fetch(path,opts);if(!r.ok)throw Error('HTTP '+r.status+'，请刷新门户');show(await r.json());
 }
 async function mutate(path,action){
  if(busy||!token)return;busy=true;
  try{await request(path,action);}catch(e){el('portal-notice').textContent=e.message;}finally{busy=false;}
 }
 document.querySelectorAll('[data-fan]').forEach(b=>b.onclick=()=>mutate('/api/fan',b.dataset.fan));
 el('portal-test').onclick=()=>mutate('/api/test','start');
 async function poll(){
  if(!busy){try{await request('/api/status');}catch(e){el('portal-notice').textContent='设备连接中断，请检查网络。';}}
  setTimeout(poll,2000);
 }
 poll();
})();
</script>
)PANEL";
