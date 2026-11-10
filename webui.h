#pragma once
#include <Arduino.h>

// The whole web page. No external files, no external URLs.
static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fa" dir="rtl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Miner Monitoring</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--tx:#1b1f24;--mut:#667085;--bd:#e2e5ea;--ac:#2563eb;--ok:#16a34a;--bad:#dc2626;--warn:#d97706;--row:#f0f4ff}
@media (prefers-color-scheme:dark){:root{--bg:#0f1318;--card:#171c23;--tx:#e8ecf1;--mut:#9aa4b2;--bd:#2a313b;--ac:#5b8cff;--ok:#34d399;--bad:#f87171;--warn:#fbbf24;--row:#1d2740}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font:14px/1.6 Tahoma,Segoe UI,sans-serif}
.wrap{max-width:1100px;margin:0 auto;padding:16px}
h1{font-size:18px;margin:0}
header{display:flex;flex-wrap:wrap;gap:8px 16px;align-items:center;justify-content:space-between;margin-bottom:12px}
.chips{display:flex;flex-wrap:wrap;gap:6px}
.chip{background:var(--card);border:1px solid var(--bd);border-radius:999px;padding:2px 10px;font-size:12px;color:var(--mut)}
.chip b{color:var(--tx);font-weight:600;direction:ltr;unicode-bidi:embed}
.chip.warn{color:var(--warn);border-color:var(--warn)}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--bad);margin-inline-end:6px}
.dot.on{background:var(--ok)}
nav{display:flex;gap:4px;margin-bottom:12px}
nav button{border:1px solid var(--bd);background:var(--card);color:var(--tx);padding:6px 16px;border-radius:8px;cursor:pointer;font:inherit}
nav button.act{background:var(--ac);border-color:var(--ac);color:#fff}
.card{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:14px;margin-bottom:12px}
.card h2{font-size:14px;margin:0 0 10px;color:var(--mut);font-weight:600}
.row{display:flex;flex-wrap:wrap;gap:10px;align-items:end}
label{display:flex;flex-direction:column;font-size:12px;color:var(--mut);gap:3px}
input,select{font:inherit;padding:6px 8px;border:1px solid var(--bd);border-radius:8px;background:var(--bg);color:var(--tx);min-width:150px}
input[type=checkbox]{min-width:0}
.chk{flex-direction:row;align-items:center;gap:6px;color:var(--tx);font-size:13px}
.ltr{direction:ltr;text-align:left}
button.btn{font:inherit;padding:7px 14px;border-radius:8px;border:1px solid var(--bd);background:var(--card);color:var(--tx);cursor:pointer}
button.pri{background:var(--ac);border-color:var(--ac);color:#fff}
button.dng{color:var(--bad)}
button:disabled{opacity:.5;cursor:default}
.bar{height:8px;background:var(--bd);border-radius:6px;overflow:hidden;margin-top:12px}
.bar i{display:block;height:100%;width:0;background:var(--ac);transition:width .3s}
.small{font-size:12px;color:var(--mut);margin-top:6px}
table{width:100%;border-collapse:collapse}
th,td{padding:8px 6px;text-align:start;border-bottom:1px solid var(--bd);white-space:nowrap}
th{font-size:12px;color:var(--mut);font-weight:600}
td.ltr,th.ltr{direction:ltr;text-align:left}
tbody tr{cursor:pointer}
tbody tr:hover,tbody tr.sel{background:var(--row)}
.scroll{overflow-x:auto}
.tag{display:inline-block;padding:0 8px;border-radius:6px;background:var(--bg);border:1px solid var(--bd);font-size:12px}
.grid2{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px}
.kv{display:flex;justify-content:space-between;gap:12px;padding:4px 0;border-bottom:1px dashed var(--bd)}
.kv span:last-child{direction:ltr;font-weight:600}
.empty{color:var(--mut);text-align:center;padding:24px}
.sect{margin-top:14px;font-weight:600;font-size:13px}
.warn{color:var(--warn)}
.hide{display:none}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1 data-i="title"></h1>
    <div class="chips" id="chips"></div>
  </header>
  <nav>
    <button id="t-scan" class="act" data-i="tabScan"></button>
    <button id="t-set" data-i="tabSet"></button>
  </nav>

  <section id="p-scan">
    <div class="card">
      <h2 data-i="scanTitle"></h2>
      <div class="row">
        <label><span data-i="from"></span><input class="ltr" id="from"></label>
        <label><span data-i="to"></span><input class="ltr" id="to"></label>
        <label class="chk"><input type="checkbox" id="autoRange"><span data-i="autoRange"></span></label>
        <button class="btn pri" id="start" data-i="start"></button>
        <button class="btn dng hide" id="stop" data-i="stop"></button>
      </div>
      <div class="row" style="margin-top:10px">
        <label class="chk"><input type="checkbox" id="autoscan"><span data-i="autoScan"></span></label>
        <label><input type="number" id="autoMin" min="1" max="1440" style="min-width:70px"></label>
        <span class="small" data-i="minutes"></span>
        <span style="flex:1"></span>
        <label><span data-i="manualIp"></span><input class="ltr" id="manual" placeholder="192.168.1.50"></label>
        <button class="btn" id="add" data-i="add"></button>
      </div>
      <div class="bar"><i id="prog"></i></div>
      <div class="small" id="scanTxt"></div>
    </div>

    <div class="card">
      <div class="row" style="justify-content:space-between;align-items:center">
        <h2 style="margin:0"><span data-i="found"></span> <span class="tag" id="cnt">0</span></h2>
        <div class="row">
          <label class="chk"><input type="checkbox" id="poll"><span data-i="autoPoll"></span></label>
          <button class="btn dng" id="clear" data-i="clear"></button>
        </div>
      </div>
      <div class="scroll">
        <table>
          <thead><tr>
            <th data-i="cStatus"></th><th class="ltr">IP</th><th data-i="cBrand"></th><th data-i="cModel"></th>
            <th class="ltr">TH/s</th><th class="ltr" data-i="cTemp"></th><th class="ltr" data-i="cPower"></th><th data-i="cPorts"></th>
          </tr></thead>
          <tbody id="tb"></tbody>
        </table>
      </div>
      <div class="empty hide" id="none" data-i="none"></div>
    </div>

    <div class="card hide" id="detail">
      <div class="row" style="justify-content:space-between;align-items:center">
        <h2 style="margin:0" id="dTitle"></h2>
        <button class="btn" id="dClose" data-i="close"></button>
      </div>
      <div class="small" id="dMsg"></div>
      <div class="grid2" id="dRows" style="margin-top:10px"></div>
      <div id="dBoardsBox">
        <div class="sect" data-i="boards"></div>
        <div class="scroll"><table><thead><tr><th>#</th><th class="ltr" data-i="cTemp"></th><th class="ltr">TH/s</th><th data-i="cStatus"></th></tr></thead><tbody id="dBoards"></tbody></table></div>
      </div>
      <div id="dPoolsBox">
        <div class="sect" data-i="pools"></div>
        <div class="scroll"><table><thead><tr><th class="ltr">URL</th><th class="ltr">Worker</th><th data-i="cStatus"></th></tr></thead><tbody id="dPools"></tbody></table></div>
      </div>
      <div class="sect" data-i="errors"></div>
      <div id="dErr" class="small"></div>
      <div class="small" data-i="readOnly"></div>
    </div>
  </section>

  <section id="p-set" class="hide">
    <div class="card">
      <h2 data-i="netTitle"></h2>
      <div class="grid2">
        <label><span data-i="wifiRole"></span><select id="s-role"></select></label>
        <label><span data-i="apSsid"></span><input id="s-apssid"></label>
        <label><span data-i="apPass"></span><input type="password" id="s-ap" data-ph="keep"></label>
        <label><span data-i="wifiSsid"></span><input id="s-ssid"></label>
        <label><span data-i="wifiPass"></span><input type="password" id="s-wpass" data-ph="keep"></label>
        <label class="chk"><input type="checkbox" id="s-static"><span data-i="ethStatic"></span></label>
        <label>IP<input class="ltr" id="s-eip"></label>
        <label>Gateway<input class="ltr" id="s-egw"></label>
        <label>Subnet<input class="ltr" id="s-emask"></label>
      </div>
    </div>
    <div class="card">
      <h2 data-i="scanSet"></h2>
      <div class="grid2">
        <label><span data-i="ports"></span><input class="ltr" id="s-ports"></label>
        <label><span data-i="timeout"></span><input type="number" id="s-to"></label>
        <label><span data-i="parallel"></span><input type="number" id="s-par" min="1" max="6"></label>
        <label><span data-i="pollSec"></span><input type="number" id="s-poll"></label>
        <label class="chk"><input type="checkbox" id="s-unk"><span data-i="unknown"></span></label>
      </div>
    </div>
    <div class="card">
      <h2 data-i="secTitle"></h2>
      <div class="grid2">
        <label><span data-i="user"></span><input class="ltr" id="s-user"></label>
        <label><span data-i="newPass"></span><input type="password" id="s-pass" data-ph="keep"></label>
      </div>
      <div class="small warn" data-i="passWarn"></div>
    </div>
    <div class="row">
      <button class="btn pri" id="save" data-i="save"></button>
      <button class="btn" id="reboot" data-i="reboot"></button>
      <span class="small" id="saveTxt"></span>
    </div>
  </section>
</div>

<script>
(function(){
  var T={
    title:'Miner Monitoring',tabScan:'اسکن و دستگاه‌ها',tabSet:'تنظیمات',scanTitle:'اسکن شبکه',from:'از IP',to:'تا IP',
    autoRange:'رنج خودکار از تنظیمات LAN',start:'شروع اسکن',stop:'توقف',autoScan:'اسکن خودکار هر',minutes:'دقیقه',
    manualIp:'افزودن دستی IP',add:'افزودن',found:'دستگاه‌های پیدا شده',autoPoll:'خواندن خودکار اطلاعات',clear:'پاک کردن لیست',
    cStatus:'وضعیت',cBrand:'برند',cModel:'مدل',cTemp:'دما',cPower:'توان',cPorts:'پورت‌ها',none:'هنوز دستگاهی پیدا نشده. اسکن را شروع کنید.',
    close:'بستن',boards:'بردها',pools:'استخرها',errors:'خطاها',readOnly:'فقط خواندن — هیچ فرمانی به ماینر ارسال نمی‌شود.',
    netTitle:'شبکه',wifiSsid:'WiFi مودم (برای آینده)',wifiPass:'رمز WiFi مودم',apPass:'رمز WiFi دستگاه',wifiRole:'حالت WiFi',roleAp:'هات‌اسپات خود دستگاه (پیش‌فرض)',roleSta:'اتصال به مودم',roleBoth:'هر دو',apSsid:'نام WiFi دستگاه',ethStatic:'IP ثابت برای Ethernet',
    scanSet:'اسکن',ports:'پورت‌ها (با کاما)',timeout:'تایم‌اوت اتصال (ms)',parallel:'اتصال موازی (۱ تا ۶)',pollSec:'فاصله‌ی خواندن (ثانیه)',
    unknown:'نمایش دستگاه‌های ناشناس (غیر ماینر)',secTitle:'امنیت صفحه‌ی وب',user:'نام کاربری',newPass:'رمز جدید',
    passWarn:'رمز پیش‌فرض را حتماً عوض کنید.',save:'ذخیره',reboot:'ری‌استارت دستگاه',
    online:'آنلاین',offline:'آفلاین',ready:'آماده',scanning:'در حال اسکن',of:'از',foundN:'پیدا شده',scanDone:'اسکن تمام شد',
    sec:'ثانیه',saved:'ذخیره شد.',needReboot:'برای اعمال تنظیمات شبکه، دستگاه را ری‌استارت کنید.',keep:'بدون تغییر',
    loading:'در حال دریافت...',noErr:'بدون خطا',sameNet:'WiFi و Ethernet در یک رنج‌اند',noEth:'کابل Ethernet وصل نیست',
    lastScan:'آخرین اسکن',rebooting:'در حال ری‌استارت...',sure:'لیست پاک شود؟'
  };
  var $=function(s){return document.querySelector(s)};
  function h(tag,attrs,kids){var e=document.createElement(tag);if(attrs)for(var k in attrs){if(k==='cls')e.className=attrs[k];else e.setAttribute(k,attrs[k]);}(kids||[]).forEach(function(c){e.appendChild(typeof c==='string'?document.createTextNode(c):c)});return e}
  document.querySelectorAll('[data-i]').forEach(function(e){e.textContent=T[e.getAttribute('data-i')]||''});
  document.querySelectorAll('[data-ph]').forEach(function(e){e.placeholder=T[e.getAttribute('data-ph')]});
  function j(path,body){
    var o=body===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)};
    return fetch(path,o).then(function(r){return r.json()});
  }
  var devs=[],sel=null,st=null,detTimer=null,wasRunning=false;

  function chips(){
    var c=$('#chips');c.textContent='';if(!st)return;
    function chip(label,val,on,w){
      var e=h('span',{cls:'chip'+(w?' warn':'')});
      if(on!==undefined)e.appendChild(h('span',{cls:'dot'+(on?' on':'')}));
      e.appendChild(document.createTextNode(label+' '));e.appendChild(h('b',null,[val]));c.appendChild(e)
    }
    chip('Ethernet',st.eth.up?st.eth.ip:'—',st.eth.up);
    chip('WiFi',st.wifi.ip&&st.wifi.ip!=='0.0.0.0'?st.wifi.ip:(st.wifi.ap?'AP '+st.wifi.apIp:'—'),st.wifi.ap||st.wifi.ip!=='0.0.0.0');
    chip('RAM',st.heap+' KB');
    if(st.sameSubnet)c.appendChild(h('span',{cls:'chip warn'},[T.sameNet]));
    if(!st.eth.up)c.appendChild(h('span',{cls:'chip warn'},[T.noEth]));
  }
  function fmt(v,u,d){return(v===null||v===undefined||v<0)?'—':v.toFixed(d)+' '+u}
  function renderList(){
    var tb=$('#tb');tb.textContent='';
    devs.forEach(function(d){
      var tr=h('tr',{cls:sel===d.ip?'sel':''},[
        h('td',null,[h('span',{cls:'dot'+(d.online?' on':'')}),d.online?T.online:T.offline]),
        h('td',{cls:'ltr'},[d.ip]),h('td',null,[d.brand]),h('td',null,[d.model||d.info||'—']),
        h('td',{cls:'ltr'},[d.hasStats?fmt(d.ths,'',2):'—']),h('td',{cls:'ltr'},[d.hasStats?fmt(d.temp,'°C',1):'—']),
        h('td',{cls:'ltr'},[d.hasStats?fmt(d.power,'W',0):'—']),h('td',{cls:'ltr'},[d.ports])
      ]);
      tr.addEventListener('click',function(){openDetail(d.ip)});
      tb.appendChild(tr);
    });
    $('#cnt').textContent=devs.length;
    $('#none').classList.toggle('hide',devs.length>0);
  }
  function refreshDevices(){j('/api/devices').then(function(a){devs=a;renderList()}).catch(function(){})}
  function refreshStatus(){
    j('/api/status').then(function(s){
      st=s;chips();applyRange();
      var sc=s.scan;
      $('#start').classList.toggle('hide',sc.running);$('#stop').classList.toggle('hide',!sc.running);
      var pct=sc.total?sc.done/sc.total*100:0;
      $('#prog').style.width=(sc.running?pct:(sc.total?100:0))+'%';
      if(sc.running)$('#scanTxt').textContent=T.scanning+' '+sc.done+' '+T.of+' '+sc.total+' — '+T.foundN+': '+sc.found;
      else if(sc.total)$('#scanTxt').textContent=T.lastScan+': '+sc.lastSec+' '+T.sec+' — '+T.foundN+': '+sc.found;
      else $('#scanTxt').textContent=T.ready;
      if(wasRunning&&!sc.running)refreshDevices();
      wasRunning=sc.running;
    }).catch(function(){});
  }
  function openDetail(ip){
    sel=ip;renderList();
    $('#detail').classList.remove('hide');
    $('#dTitle').textContent=ip;$('#dMsg').textContent=T.loading;
    loadDetail(true);
    clearInterval(detTimer);detTimer=setInterval(function(){loadDetail(false)},10000);
  }
  function loadDetail(scroll){
    if(!sel)return;var ip=sel;
    j('/api/detail?ip='+encodeURIComponent(ip)).then(function(d){
      if(sel!==ip)return;
      $('#dTitle').textContent=d.brand+' '+(d.model||'')+' — '+d.ip;
      $('#dMsg').textContent=d.ok?'':(d.message||'');
      var r=$('#dRows');r.textContent='';
      var rows=d.rows||[],half=Math.ceil(rows.length/2);
      [rows.slice(0,half),rows.slice(half)].forEach(function(col){
        var box=h('div');col.forEach(function(kv){box.appendChild(h('div',{cls:'kv'},[h('span',null,[kv[0]]),h('span',null,[String(kv[1])])]))});r.appendChild(box)});
      var b=$('#dBoards');b.textContent='';
      (d.boards||[]).forEach(function(x){b.appendChild(h('tr',null,[h('td',null,[String(x[0])]),h('td',{cls:'ltr'},[String(x[1])]),h('td',{cls:'ltr'},[Number(x[2]).toFixed(2)]),h('td',null,[String(x[3])])]))});
      $('#dBoardsBox').classList.toggle('hide',!(d.boards&&d.boards.length));
      var p=$('#dPools');p.textContent='';
      (d.pools||[]).forEach(function(x){p.appendChild(h('tr',null,[h('td',{cls:'ltr'},[String(x[0])]),h('td',{cls:'ltr'},[String(x[1])]),h('td',null,[String(x[2])])]))});
      $('#dPoolsBox').classList.toggle('hide',!(d.pools&&d.pools.length));
      $('#dErr').textContent=(d.errors&&d.errors.length)?d.errors.join('، '):T.noErr;
      if(scroll)$('#detail').scrollIntoView({behavior:'smooth',block:'nearest'});
      refreshDevices();
    }).catch(function(){$('#dMsg').textContent='—'});
  }
  function closeDetail(){sel=null;clearInterval(detTimer);$('#detail').classList.add('hide');renderList()}

  $('#start').addEventListener('click',function(){
    j('/api/scan',{auto:$('#autoRange').checked,from:$('#from').value.trim(),to:$('#to').value.trim()}).then(function(r){
      if(!r.ok)$('#scanTxt').textContent=r.error||'error';refreshStatus();
    });
  });
  $('#stop').addEventListener('click',function(){j('/api/scan/stop',{}).then(refreshStatus)});
  function applyRange(){
    var a=$('#autoRange').checked;$('#from').disabled=a;$('#to').disabled=a;
    if(a&&st&&st.suggest){$('#from').value=st.suggest.from;$('#to').value=st.suggest.to}
  }
  $('#autoRange').addEventListener('change',function(){applyRange();j('/api/settings',{scanAuto:$('#autoRange').checked})});
  $('#add').addEventListener('click',function(){
    var v=$('#manual').value.trim();if(!v)return;
    j('/api/add',{ip:v}).then(function(r){if(r.ok){$('#manual').value='';refreshDevices()}else $('#scanTxt').textContent=r.error||'error'});
  });
  $('#clear').addEventListener('click',function(){if(!confirm(T.sure))return;j('/api/clear',{}).then(function(){closeDetail();refreshDevices()})});
  $('#dClose').addEventListener('click',closeDetail);
  $('#autoscan').addEventListener('change',function(){j('/api/settings',{autoScan:$('#autoscan').checked})});
  $('#autoMin').addEventListener('change',function(){j('/api/settings',{autoScanMin:parseInt($('#autoMin').value,10)||30})});
  $('#poll').addEventListener('change',function(){j('/api/settings',{autoPoll:$('#poll').checked})});

  function fillSettings(c){
    $('#from').value=c.scanFrom;$('#to').value=c.scanTo;$('#autoRange').checked=c.scanAuto;$('#s-role').value=String(c.wifiRole);$('#s-apssid').value=c.apSsid;applyRange();
    $('#autoscan').checked=c.autoScan;$('#autoMin').value=c.autoScanMin;$('#poll').checked=c.autoPoll;
    $('#s-ssid').value=c.wifiSsid;$('#s-static').checked=c.ethStatic;
    $('#s-eip').value=c.ethIp;$('#s-egw').value=c.ethGw;$('#s-emask').value=c.ethMask;
    $('#s-ports').value=c.ports;$('#s-to').value=c.timeoutMs;$('#s-par').value=c.parallel;
    $('#s-poll').value=c.pollSec;$('#s-unk').checked=c.listUnknown;$('#s-user').value=c.authUser;
  }
  $('#save').addEventListener('click',function(){
    var o={wifiRole:parseInt($('#s-role').value,10),apSsid:$('#s-apssid').value.trim(),scanAuto:$('#autoRange').checked,wifiSsid:$('#s-ssid').value,ethStatic:$('#s-static').checked,ethIp:$('#s-eip').value.trim(),
      ethGw:$('#s-egw').value.trim(),ethMask:$('#s-emask').value.trim(),ports:$('#s-ports').value.trim(),
      timeoutMs:parseInt($('#s-to').value,10),parallel:parseInt($('#s-par').value,10),
      pollSec:parseInt($('#s-poll').value,10),listUnknown:$('#s-unk').checked,authUser:$('#s-user').value.trim(),
      scanFrom:$('#from').value.trim(),scanTo:$('#to').value.trim()};
    if($('#s-wpass').value)o.wifiPass=$('#s-wpass').value;
    if($('#s-ap').value)o.apPass=$('#s-ap').value;
    if($('#s-pass').value)o.authPass=$('#s-pass').value;
    j('/api/settings',o).then(function(r){
      $('#saveTxt').textContent=r.ok?(T.saved+(r.reboot?' '+T.needReboot:'')):(r.error||'error');
      ['#s-wpass','#s-ap','#s-pass'].forEach(function(s){$(s).value=''});
    });
  });
  $('#reboot').addEventListener('click',function(){j('/api/reboot',{});$('#saveTxt').textContent=T.rebooting});

  function tab(w){
    $('#p-scan').classList.toggle('hide',w!=='scan');$('#p-set').classList.toggle('hide',w!=='set');
    $('#t-scan').classList.toggle('act',w==='scan');$('#t-set').classList.toggle('act',w==='set');
  }
  $('#t-scan').addEventListener('click',function(){tab('scan')});
  $('#t-set').addEventListener('click',function(){tab('set')});

  [[0,T.roleAp],[1,T.roleSta],[2,T.roleBoth]].forEach(function(o){
    var op=document.createElement('option');op.value=String(o[0]);op.textContent=o[1];$('#s-role').appendChild(op)});
  j('/api/settings').then(fillSettings);
  refreshStatus();refreshDevices();
  setInterval(refreshStatus,2000);
  setInterval(refreshDevices,4000);
})();
</script>
</body>
</html>
)HTML";
