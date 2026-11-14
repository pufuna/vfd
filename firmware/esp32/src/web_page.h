/* Встроенная веб-страница ПЧ-1500 */
#pragma once
#include <pgmspace.h>
static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ПЧ-1500</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1c1f24;--mut:#69707d;--acc:#0b6bcb;--bad:#c62828;--ok:#2e7d32;--line:#dde1e7}
@media(prefers-color-scheme:dark){:root{--bg:#14171c;--card:#1e2229;--fg:#e8eaee;--mut:#9aa3b2;--acc:#5aa9f5;--bad:#ef6b6b;--ok:#6fcf74;--line:#2f3540}}
*{box-sizing:border-box}body{margin:0;font:16px/1.4 system-ui,sans-serif;background:var(--bg);color:var(--fg)}
main{max-width:860px;margin:0 auto;padding:16px}h1{font-size:20px;margin:4px 0 12px}h2{font-size:16px;margin:0 0 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:10px}
.tile b{display:block;font-size:22px;font-variant-numeric:tabular-nums}.tile span{color:var(--mut);font-size:13px}
#state{font-weight:600;padding:2px 10px;border-radius:12px;border:1px solid var(--line)}
.bad{color:var(--bad)}.ok{color:var(--ok)}
button,input,select{font:inherit;padding:8px 12px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg)}
button{cursor:pointer}button.go{background:var(--ok);color:#fff;border:0}button.stop{background:var(--bad);color:#fff;border:0}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.tw{overflow-x:auto}table{border-collapse:collapse;width:100%;font-size:14px}td,th{padding:6px 8px;border-bottom:1px solid var(--line);text-align:left;white-space:nowrap}
td input,td select{padding:4px 6px;width:120px}small{color:var(--mut)}
</style></head><body><main>
<h1>Преобразователь частоты ПЧ-1500 <span id="state">…</span></h1>
<div class="card"><div class="grid">
<div class="tile"><b id="f_out">–</b><span>Выходная частота, Гц</span></div>
<div class="tile"><b id="f_ref">–</b><span>Задание, Гц</span></div>
<div class="tile"><b id="i">–</b><span>Ток, А</span></div>
<div class="tile"><b id="u">–</b><span>Напряжение, В</span></div>
<div class="tile"><b id="udc">–</b><span>Шина DC, В</span></div>
<div class="tile"><b id="rpm">–</b><span>Скорость, об/мин</span></div>
<div class="tile"><b id="torque">–</b><span>Момент, %</span></div>
<div class="tile"><b id="p">–</b><span>Мощность, Вт</span></div>
<div class="tile"><b id="t">–</b><span>Радиатор, °C</span></div>
<div class="tile"><b id="hours">–</b><span>Наработка, ч</span></div>
</div><p id="flt" class="bad"></p><small id="log"></small></div>
<div class="card"><h2>Управление</h2><div class="row">
<input id="fset" type="number" step="0.1" min="0" max="200" value="50" style="width:110px"> Гц
<button onclick="cmd('f='+fset.value)">Задать</button>
<button class="go" onclick="cmd('run=1&rev=0')">Пуск вперёд</button>
<button class="go" onclick="cmd('run=1&rev=1')">Пуск назад</button>
<button class="stop" onclick="cmd('run=0')">Стоп</button>
<button onclick="cmd('reset=1')">Сброс аварии</button></div>
<p><small>Пуск и задание из веб-интерфейса действуют, когда P00 «Источник команд» и P01 «Источник задания» установлены в MODBUS.</small></p></div>
<div class="card"><h2>Параметры</h2><div class="tw"><table id="pt"><tr><th>№</th><th>Параметр</th><th>Значение</th><th>Диапазон</th></tr></table></div>
<p class="row"><button onclick="cmd('save=1')">Сохранить в память привода</button><button onclick="loadP()">Обновить</button><span id="msg"></span></p></div>
<div class="card"><h2>Wi-Fi</h2><div class="row"><input id="ssid" placeholder="Имя сети"><input id="pass" type="password" placeholder="Пароль сети">
<input id="appass" type="password" placeholder="Новый пароль точки доступа (от 8 знаков)">
<button onclick="fetch('/api/wifi?ssid='+encodeURIComponent(ssid.value)+'&pass='+encodeURIComponent(pass.value)+(appass.value?'&appass='+encodeURIComponent(appass.value):''))">Сохранить и перезагрузить модуль</button></div></div>
</main><script>
const FL=['','КЗ / сверхток силового модуля','Перегрузка по току','Перенапряжение шины','Низкое напряжение шины','Перегрузка двигателя','Перегрев радиатора','Нет связи Modbus','Ошибка автонастройки','Внешняя авария','Шина не заряжена','Датчики тока'];
const ERR={'-1':'нет связи с приводом',2:'нет такого регистра',3:'значение вне диапазона',4:'запрещено при работе'};
const $=id=>document.getElementById(id);
function say(e){$('msg').textContent=e?('Ошибка: '+(ERR[e]||e)):'Готово';$('msg').className=e?'bad':'ok'}
async function cmd(q){try{const r=await(await fetch('/api/cmd?'+q)).json();say(r.err)}catch(e){say(-1)}}
async function wr(reg,val){try{const r=await(await fetch('/api/write?reg='+reg+'&val='+val)).json();say(r.err);if(r.err)loadP()}catch(e){say(-1)}}
function fmt(v,d){return (v/Math.pow(10,d)).toFixed(d)}
async function loadP(){let ps;try{ps=await(await fetch('/api/params')).json()}catch(e){return}
const t=$('pt');while(t.rows.length>1)t.deleteRow(1);
ps.forEach((p,i)=>{const r=t.insertRow();r.insertCell().textContent='P'+String(i).padStart(2,'0');r.insertCell().textContent=p.n;const c=r.insertCell();
if(p.o){const s=document.createElement('select');p.o.split('|').forEach((o,k)=>{const e=document.createElement('option');e.value=k+p.min;e.textContent=o;s.appendChild(e)});s.value=p.v;s.onchange=()=>wr(i,s.value);c.appendChild(s);r.insertCell().textContent=''}
else{const n=document.createElement('input');n.type='number';n.step=Math.pow(10,-p.dec);n.value=fmt(p.v,p.dec);n.onchange=()=>wr(i,Math.round(n.value*Math.pow(10,p.dec)));c.appendChild(n);c.append(' '+p.u);r.insertCell().textContent=fmt(p.min,p.dec)+' … '+fmt(p.max,p.dec)}})}
async function tick(){try{const s=await(await fetch('/api/status')).json();
['f_out','f_ref','i','u','udc','rpm','torque','p','t','hours'].forEach(k=>$(k).textContent=s[k]);
const st=$('state');if(!s.link){st.textContent='нет связи с приводом';st.className='bad'}
else if(s.word&8){st.textContent='АВАРИЯ';st.className='bad'}else if(s.word&64){st.textContent='автонастройка';st.className='ok'}
else if(s.word&2){st.textContent='работа';st.className='ok'}else if(s.word&1){st.textContent='готов';st.className=''}else{st.textContent='заряд шины';st.className=''}
$('flt').textContent=s.fault?('E'+String(s.fault).padStart(2,'0')+' — '+FL[s.fault]):'';
$('log').textContent='Журнал аварий: '+s.log.map(x=>x?'E'+String(x).padStart(2,'0'):'—').join(', ')+' · версия ПО '+(s.ver/100).toFixed(2)}
catch(e){$('state').textContent='нет связи с модулем';$('state').className='bad'}}
loadP();tick();setInterval(tick,1000);
</script></body></html>)HTML";
