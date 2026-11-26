/*
 * ════════════════════════════════════════════════════════════════════
 *   pagina_web.h — Painel de monitoramento (HTML + CSS + JS)
 *   Autocontido: não depende de internet/CDN. Atualiza via /api/estado.
 * ════════════════════════════════════════════════════════════════════
 */
#pragma once
#include <Arduino.h>

const char PAGINA_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Estacionamento · ESP32</title>
<link rel="icon" href="data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' rx='22' fill='%2322c55e'/><text x='50' y='72' font-size='64' text-anchor='middle' font-family='Arial' font-weight='700' fill='white'>P</text></svg>">
<style>
:root{--bg:#0a0e16;--card:#111827;--line:#1f2a3c;--txt:#e5edf7;--mut:#8796ad;
--ok:#22c55e;--bad:#ef4444;--warn:#f59e0b;--info:#3b82f6;--r:16px}
*{box-sizing:border-box;margin:0}
body{font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:
radial-gradient(1200px 600px at 10% -10%,#16233a 0,transparent 60%),var(--bg);
color:var(--txt);min-height:100vh;padding:20px}
.wrap{max-width:1100px;margin:auto;display:grid;gap:16px}
header{display:flex;align-items:center;gap:14px;flex-wrap:wrap}
.logo{width:44px;height:44px;border-radius:12px;background:var(--ok);display:grid;place-items:center;
font-weight:800;font-size:24px;color:#fff;box-shadow:0 6px 24px #22c55e55}
h1{font-size:20px;letter-spacing:.3px}
.sub{color:var(--mut);font-size:13px}
.sp{flex:1}
.pill{padding:6px 12px;border-radius:999px;font-size:12px;font-weight:700;letter-spacing:.8px;
background:#22c55e22;color:var(--ok);border:1px solid #22c55e55}
.pill.bad{background:#ef444422;color:var(--bad);border-color:#ef444466;animation:pulse 1.4s infinite}
@keyframes pulse{50%{box-shadow:0 0 0 6px #ef444411}}
.net{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--mut)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--bad)}
.dot.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.card{background:linear-gradient(180deg,#131c2c,var(--card));border:1px solid var(--line);
border-radius:var(--r);padding:18px}
.card h2{font-size:12px;text-transform:uppercase;letter-spacing:1.2px;color:var(--mut);margin-bottom:14px;font-weight:600}
.top{display:grid;grid-template-columns:260px 1fr;gap:16px}
@media(max-width:760px){.top,.gates{grid-template-columns:1fr!important}}
.gauge{display:grid;place-items:center;position:relative}
.gauge svg{width:200px;height:200px;transform:rotate(-90deg)}
.gauge .c{position:absolute;text-align:center}
.big{font-size:46px;font-weight:800;font-variant-numeric:tabular-nums;line-height:1}
.big small{font-size:18px;color:var(--mut);font-weight:600}
#arc{transition:stroke-dashoffset .6s cubic-bezier(.4,1.4,.6,1),stroke .3s}
.slots{display:grid;grid-template-columns:repeat(10,1fr);gap:8px}
.slot{aspect-ratio:1/1.35;border-radius:8px;border:1.5px dashed #2a3a54;display:grid;place-items:center;
font-size:10px;color:#4b5d7a;transition:.35s;position:relative}
.slot.on{border:1.5px solid #22c55e66;background:#22c55e14;color:var(--ok)}
.lot .slot.on{border-color:#ef444466;background:#ef444414;color:var(--bad)}
.slot svg{width:70%;opacity:0;transform:translateY(-6px);transition:.35s}
.slot.on svg{opacity:1;transform:none}
.slot b{position:absolute;top:3px;left:5px;font-weight:600}
.legend{display:flex;gap:18px;margin-top:14px;font-size:13px;color:var(--mut)}
.legend strong{color:var(--txt);font-size:18px;margin-right:4px}
.gates{display:grid;grid-template-columns:1fr 1fr;gap:16px}
.gate{display:grid;grid-template-columns:150px 1fr;gap:16px;align-items:center}
.gate svg{width:150px}
#armE,#armS{transition:transform .25s linear;transform-origin:30px 70px}
.st{font-size:22px;font-weight:800;text-transform:capitalize}
.st.open{color:var(--ok)} .st.move{color:var(--warn)} .st.closed{color:var(--mut)}
.meter{height:8px;border-radius:9px;background:#1b2537;margin:8px 0 4px;position:relative;overflow:hidden}
.meter i{position:absolute;inset:0 auto 0 0;background:var(--info);border-radius:9px;transition:width .3s}
.meter.near i{background:var(--warn)}
.meter u{position:absolute;top:0;bottom:0;width:2px;background:#fff6;left:25%}
.small{font-size:12px;color:var(--mut)}
.tag{display:inline-block;margin-top:8px;font-size:11px;font-weight:700;padding:3px 8px;border-radius:6px;
background:#f59e0b22;color:var(--warn);visibility:hidden}
.tag.on{visibility:visible}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}
@media(max-width:560px){.stats{grid-template-columns:repeat(2,1fr)}}
.stat{background:#0d1422;border:1px solid var(--line);border-radius:12px;padding:14px}
.stat .v{font-size:28px;font-weight:800;font-variant-numeric:tabular-nums}
.bottom{display:grid;grid-template-columns:1fr 1.3fr;gap:16px}
@media(max-width:760px){.bottom{grid-template-columns:1fr}}
.btns{display:grid;grid-template-columns:1fr 1fr;gap:10px}
button{font:inherit;font-size:14px;font-weight:600;padding:12px;border-radius:10px;border:1px solid var(--line);
background:#172133;color:var(--txt);cursor:pointer;transition:.15s}
button:hover{background:#1d2a41;transform:translateY(-1px)}
button:active{transform:none}
button.g{background:#22c55e;border-color:#22c55e;color:#04210f}
button.b{background:#3b82f6;border-color:#3b82f6;color:#fff}
button.r{color:#fca5a5;border-color:#ef444455}
.log{list-style:none;padding:0;max-height:300px;overflow:auto;display:grid;gap:6px}
.log li{display:grid;grid-template-columns:62px 74px 1fr;gap:8px;align-items:center;font-size:13px;
padding:8px 10px;background:#0d1422;border-radius:8px;border-left:3px solid var(--mut);animation:in .3s}
@keyframes in{from{opacity:0;transform:translateX(-6px)}}
.log time{color:var(--mut);font-variant-numeric:tabular-nums}
.log .k{font-size:10px;font-weight:800;letter-spacing:.6px;text-transform:uppercase}
.entrada{border-color:var(--ok)!important}.entrada .k{color:var(--ok)}
.saida{border-color:var(--info)!important}.saida .k{color:var(--info)}
.negado{border-color:var(--bad)!important}.negado .k{color:var(--bad)}
.alerta{border-color:var(--warn)!important}.alerta .k{color:var(--warn)}
.cancela .k,.sistema .k,.teste .k{color:var(--mut)}
footer{text-align:center;color:#4b5d7a;font-size:12px;padding:8px}
.toast{position:fixed;bottom:20px;left:50%;transform:translate(-50%,20px);background:#1d2a41;
border:1px solid var(--line);padding:10px 16px;border-radius:10px;font-size:13px;transition:.3s;
opacity:0;pointer-events:none}
.toast.on{transform:translate(-50%,0);opacity:1}
@media(max-width:560px){body{padding:12px}.slots{grid-template-columns:repeat(6,1fr)}
.gate{grid-template-columns:110px 1fr}.gate svg{width:110px}
.log li{grid-template-columns:44px 1fr;row-gap:2px}.log li span:last-child{grid-column:1/-1}}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div class="logo">P</div>
    <div><h1>Controle de Estacionamento</h1><div class="sub">ESP32 · monitoramento em tempo real</div></div>
    <div class="sp"></div>
    <div class="net"><span class="dot" id="dot"></span><span id="net">conectando…</span></div>
    <span class="pill" id="pill">VAGAS</span>
  </header>

  <section class="top">
    <div class="card gauge">
      <svg viewBox="0 0 120 120">
        <circle cx="60" cy="60" r="52" fill="none" stroke="#1b2537" stroke-width="10"/>
        <circle id="arc" cx="60" cy="60" r="52" fill="none" stroke="#22c55e" stroke-width="10"
          stroke-linecap="round" stroke-dasharray="326.7" stroke-dashoffset="326.7"/>
      </svg>
      <div class="c"><div class="big" id="occ">0<small>/30</small></div>
      <div class="small" id="free">30 vagas livres</div></div>
    </div>
    <div class="card" id="mapa">
      <h2>Mapa de vagas</h2>
      <div class="slots" id="slots"></div>
      <div class="legend"><span><strong id="lOcc">0</strong>ocupadas</span>
      <span><strong id="lFree">30</strong>livres</span><span><strong id="lPct">0%</strong>ocupação</span></div>
    </div>
  </section>

  <section class="gates">
    <div class="card"><h2>Cancela de entrada</h2><div class="gate">
      <svg viewBox="0 0 150 90"><rect x="20" y="62" width="20" height="26" rx="3" fill="#2a3a54"/>
      <g id="armE"><rect x="26" y="64" width="118" height="12" rx="6" fill="#e5edf7"/>
      <rect x="50" y="64" width="16" height="12" fill="#ef4444"/><rect x="82" y="64" width="16" height="12" fill="#ef4444"/>
      <rect x="114" y="64" width="16" height="12" fill="#ef4444"/></g><circle cx="30" cy="70" r="7" fill="#8796ad"/></svg>
      <div><div class="st closed" id="stE">fechada</div>
      <div class="meter" id="mE"><i></i><u></u></div><div class="small" id="dE">sensor: — cm</div>
      <span class="tag" id="oE">VEÍCULO SOB A CANCELA</span></div></div></div>
    <div class="card"><h2>Cancela de saída</h2><div class="gate">
      <svg viewBox="0 0 150 90"><rect x="20" y="62" width="20" height="26" rx="3" fill="#2a3a54"/>
      <g id="armS"><rect x="26" y="64" width="118" height="12" rx="6" fill="#e5edf7"/>
      <rect x="50" y="64" width="16" height="12" fill="#ef4444"/><rect x="82" y="64" width="16" height="12" fill="#ef4444"/>
      <rect x="114" y="64" width="16" height="12" fill="#ef4444"/></g><circle cx="30" cy="70" r="7" fill="#8796ad"/></svg>
      <div><div class="st closed" id="stS">fechada</div>
      <div class="meter" id="mS"><i></i><u></u></div><div class="small" id="dS">sensor: — cm</div>
      <span class="tag" id="oS">VEÍCULO SOB A CANCELA</span></div></div></div>
  </section>

  <section class="card"><h2>Estatísticas</h2><div class="stats">
    <div class="stat"><div class="small">Entradas</div><div class="v" id="sE">0</div></div>
    <div class="stat"><div class="small">Saídas</div><div class="v" id="sS">0</div></div>
    <div class="stat"><div class="small">Acessos negados</div><div class="v" id="sN">0</div></div>
    <div class="stat"><div class="small">Pico de ocupação</div><div class="v" id="sP">0</div></div>
  </div></section>

  <section class="bottom">
    <div class="card"><h2>Comandos</h2><div class="btns">
      <button class="g" data-a="entrada">▲ Simular entrada</button>
      <button class="b" data-a="saida">▼ Simular saída</button>
      <button data-a="abrir_entrada">Abrir entrada</button>
      <button data-a="abrir_saida">Abrir saída</button>
      <button data-a="teste">Teste: 29 carros</button>
      <button class="r" data-a="zerar">Zerar contador</button>
      <button data-a="som" id="bSom">🔊 Som ligado</button>
      <button class="r" data-a="zerar_stats">Zerar estatísticas</button>
    </div></div>
    <div class="card"><h2>Histórico de eventos</h2><ul class="log" id="log"></ul></div>
  </section>
  <footer id="up">ESP32 · ativo há —</footer>
</div>
<div class="toast" id="toast"></div>
<script>
const $=id=>document.getElementById(id), CAR='<svg viewBox="0 0 24 16"><path fill="currentColor" d="M4 7l2.2-4.4A2 2 0 0 1 8 1.5h8a2 2 0 0 1 1.8 1.1L20 7h1a2 2 0 0 1 2 2v4h-2.2a2.5 2.5 0 0 1-4.6 0H7.8a2.5 2.5 0 0 1-4.6 0H1V9a2 2 0 0 1 2-2zm2.2 0h11.6l-1.6-3.3a1 1 0 0 0-.9-.5H8.7a1 1 0 0 0-.9.5z"/></svg>';
let cap=30, ok=false, lastLog='';
function slots(n){let h='';for(let i=0;i<n;i++)h+=`<div class="slot"><b>${String(i+1).padStart(2,'0')}</b>${CAR}</div>`;$('slots').innerHTML=h}
slots(cap);
const fmt=s=>{const h=Math.floor(s/3600),m=Math.floor(s%3600/60),x=s%60;return(h?h+'h ':'')+m+'min '+x+'s'};
const mmss=s=>String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');
const ROT={entrada:'entrada',saida:'saída',negado:'negado',alerta:'alerta',cancela:'cancela',sistema:'sistema',teste:'teste'};
const esc=t=>t.replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
function gate(k,g){
  $('arm'+k).style.transform=`rotate(${-g.angulo}deg)`;
  const st=$('st'+k);st.textContent=g.estado;
  st.className='st '+(g.estado=='aberta'?'open':g.estado=='fechada'?'closed':'move');
  const d=Math.min(g.distancia,200),m=$('m'+k);
  m.querySelector('i').style.width=(d/200*100)+'%';m.classList.toggle('near',g.distancia<50);
  $('d'+k).textContent=`sensor: ${g.distancia>=400?'livre (> 4 m)':g.distancia+' cm'} · ${g.aberturas} aberturas`;
  $('o'+k).classList.toggle('on',g.obstruida&&g.estado!='fechada');
}
function render(s){
  if(s.capacidade!==cap){cap=s.capacidade;slots(cap)}
  const p=s.veiculos/cap;
  $('arc').style.strokeDashoffset=326.7*(1-p);
  $('arc').style.stroke=s.lotado?'#ef4444':p>.8?'#f59e0b':'#22c55e';
  $('occ').innerHTML=`${s.veiculos}<small>/${cap}</small>`;
  $('free').textContent=s.lotado?'estacionamento lotado':`${s.livres} vaga${s.livres==1?'':'s'} livre${s.livres==1?'':'s'}`;
  document.querySelectorAll('.slot').forEach((e,i)=>e.classList.toggle('on',i<s.veiculos));
  $('mapa').classList.toggle('lot',s.lotado);
  $('lOcc').textContent=s.veiculos;$('lFree').textContent=s.livres;$('lPct').textContent=Math.round(p*100)+'%';
  const pill=$('pill');pill.textContent=s.lotado?'LOTADO':'VAGAS';pill.classList.toggle('bad',s.lotado);
  gate('E',s.cancelas.entrada);gate('S',s.cancelas.saida);
  $('sE').textContent=s.stats.entradas;$('sS').textContent=s.stats.saidas;
  $('sN').textContent=s.stats.negados;$('sP').textContent=s.stats.pico;
  $('bSom').textContent=s.som?'🔊 Som ligado':'🔇 Som desligado';
  const key=JSON.stringify(s.eventos);
  if(key!==lastLog){lastLog=key;$('log').innerHTML=s.eventos.map(e=>
    `<li class="${e.tipo}"><time>${mmss(e.t)}</time><span class="k">${ROT[e.tipo]||e.tipo}</span><span>${esc(e.msg)}</span></li>`).join('')}
  $('up').textContent=`ESP32 · ativo há ${fmt(s.uptime)}`+(s.rssi?` · Wi-Fi ${s.rssi} dBm`:'');
}
function online(v){ok=v;$('dot').classList.toggle('on',v);$('net').textContent=v?'online':'sem conexão'}
async function tick(){
  try{const r=await fetch('/api/estado',{cache:'no-store'});render(await r.json());online(true)}
  catch(e){online(false)}
  setTimeout(tick,700);
}
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('on');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('on'),1800)}
document.querySelectorAll('button[data-a]').forEach(b=>b.onclick=async()=>{
  try{const r=await fetch('/api/comando',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'acao='+b.dataset.a});
  if(!r.ok)throw 0;render(await r.json());toast('✓ '+b.textContent.trim())}catch(e){toast('Falha ao enviar comando')}
});
tick();
</script>
</body>
</html>)rawliteral";
