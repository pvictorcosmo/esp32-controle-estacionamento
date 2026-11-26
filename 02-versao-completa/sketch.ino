/*
 * ════════════════════════════════════════════════════════════════════
 *   CONTROLE AUTOMATIZADO DE ESTACIONAMENTO — ESP32 · v2 (completa)
 * ════════════════════════════════════════════════════════════════════
 *   Versão de referência + as 3 atividades do roteiro + extras:
 *
 *   Atividade 1 · Display LCD 16×2 I2C com ocupadas/livres, ícones das
 *                 cancelas e avisos temporários
 *   Atividade 2 · Sensores HC-SR04 (leitura por interrupção, sem
 *                 pulseIn bloqueante) com histerese e confirmação
 *   Atividade 3 · Painel Web via Wi-Fi: vagas, cancelas, sensores,
 *                 estatísticas, histórico e comandos remotos
 *
 *   Extras
 *   • Cancelas com movimento suave e máquina de estados
 *     (FECHADA → ABRINDO → ABERTA → FECHANDO)
 *   • Segurança: a cancela não fecha com veículo embaixo e reabre se
 *     um veículo for detectado durante o fechamento
 *   • Buzzer com sons distintos (entrada, saída, negado, lotado)
 *   • Contador e estatísticas salvos na flash (NVS / Preferences)
 *   • Histórico dos últimos eventos (serial, web)
 *   • Botões manuais do operador continuam funcionando (teclas E/S)
 *   • Tudo não bloqueante: nenhum delay() no loop
 *
 *   Arquivo único (pronto para o Wokwi). Seções, nesta ordem:
 *     1. Configuração (pinos, tempos, Wi-Fi)   4. Componentes (tipos)
 *     2. Página web (HTML/CSS/JS)              5. Lógica, interfaces
 *     3. Estado global                         6. setup() e loop()
 * ════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Preferences.h>
#include <stdarg.h>

// ═════════════════════════════════════════════════════════════════════
//  1. CONFIGURAÇÃO — ajuste pinos, tempos e Wi-Fi aqui
// ═════════════════════════════════════════════════════════════════════

// ─────────────────────────── Rede Wi-Fi ──────────────────────────────
// No Wokwi use "Wokwi-GUEST" sem senha (canal 6 acelera a conexão).
#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_SENHA     ""
#define WIFI_CANAL     6
#define MDNS_NOME      "estacionamento"   // http://estacionamento.local (hardware real)

// ───────────────────────────── Pinos ─────────────────────────────────
// Botões manuais do operador (ligados ao GND, INPUT_PULLUP)
constexpr uint8_t PIN_BTN_ENTRADA   = 18;
constexpr uint8_t PIN_BTN_SAIDA     = 19;

// Display LCD 16×2 I2C (barramento padrão do ESP32)
constexpr uint8_t PIN_I2C_SDA       = 21;
constexpr uint8_t PIN_I2C_SCL       = 22;
constexpr uint8_t LCD_ENDERECO      = 0x27;

// LEDs (saíram de 21/22 para liberar o I2C do LCD)
constexpr uint8_t PIN_LED_VERDE     = 32;
constexpr uint8_t PIN_LED_VERMELHO  = 33;

// Servos das cancelas (PWM 50 Hz)
constexpr uint8_t PIN_SERVO_ENTRADA = 25;
constexpr uint8_t PIN_SERVO_SAIDA   = 26;

// Sensores ultrassônicos HC-SR04
constexpr uint8_t PIN_US_ENT_TRIG   = 13;
constexpr uint8_t PIN_US_ENT_ECHO   = 14;
constexpr uint8_t PIN_US_SAI_TRIG   = 27;
constexpr uint8_t PIN_US_SAI_ECHO   = 34;   // GPIO34 é só entrada: ideal p/ ECHO

// Buzzer passivo (feedback sonoro)
constexpr uint8_t PIN_BUZZER        = 23;

// ─────────────────────────── Regras ──────────────────────────────────
constexpr int           CAPACIDADE         = 30;     // vagas
constexpr unsigned long DEBOUNCE_MS        = 40;     // botões
constexpr unsigned long TEMPO_ABERTA_MS    = 2500;   // cancela aberta

// ─────────────────────────── Cancelas ────────────────────────────────
constexpr int           ANGULO_FECHADA     = 0;
constexpr int           ANGULO_ABERTA      = 90;
constexpr int           SERVO_PULSO_MIN    = 500;    // µs (SG90)
constexpr int           SERVO_PULSO_MAX    = 2400;   // µs (SG90)
constexpr unsigned long SERVO_PASSO_MS     = 10;     // movimento suave:
constexpr int           SERVO_PASSO_GRAUS  = 3;      //  ~300 ms p/ 90°

// ───────────────────────── Sensores HC-SR04 ──────────────────────────
constexpr unsigned long US_PERIODO_MS      = 60;     // 1 medição/sensor a cada 60 ms
constexpr unsigned long US_TIMEOUT_MS      = 40;     // sem eco → fora de alcance
constexpr float         US_LIMIAR_CM       = 50.0f;  // < 50 cm → veículo presente
constexpr float         US_HISTERESE_CM    = 20.0f;  // > 70 cm → veículo saiu
constexpr uint8_t       US_CONFIRMACOES    = 3;      // leituras seguidas p/ confirmar
constexpr float         US_FORA_ALCANCE_CM = 400.0f;

// ─────────────────────────── Persistência ────────────────────────────
constexpr unsigned long NVS_INTERVALO_MS   = 2000;   // grava no máx. a cada 2 s

// ─────────────────────────── Interface ───────────────────────────────
constexpr uint32_t      SERIAL_BAUD        = 115200;
constexpr unsigned long LCD_MENSAGEM_MS    = 2000;   // duração dos avisos no LCD
constexpr uint8_t       MAX_EVENTOS        = 15;     // histórico em memória

// ─────────────────────────── Opções ──────────────────────────────────
constexpr bool          PERSISTIR_DADOS    = true;   // salva contador/estatísticas na flash

// ═════════════════════════════════════════════════════════════════════
//  2. PÁGINA WEB — painel servido pelo ESP32 (autocontido)
// ═════════════════════════════════════════════════════════════════════

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

// ═════════════════════════════════════════════════════════════════════
//  3. ESTADO GLOBAL
// ═════════════════════════════════════════════════════════════════════
int veiculos = 0;

struct Estatisticas {
  uint32_t entradas = 0;
  uint32_t saidas   = 0;
  uint32_t negados  = 0;
  int      pico     = 0;
} stats;

bool somAtivo        = true;

// Medição do eco dos HC-SR04 (escrita pelas interrupções) · [0]=entrada [1]=saída
volatile unsigned long ecoInicio[2]  = { 0, 0 };
volatile unsigned long ecoDuracao[2] = { 0, 0 };
volatile bool          ecoPronto[2]  = { false, false };
bool dadosAlterados  = false;   // há algo para gravar na NVS
bool wifiConectado   = false;
bool servidorAtivo   = false;

// ═════════════════════════════════════════════════════════════════════
//  4. COMPONENTES — tipos do sistema
// ═════════════════════════════════════════════════════════════════════

// ═════════════════════════════════════════════════════════════════════
//  Eventos
// ═════════════════════════════════════════════════════════════════════
enum TipoEvento : uint8_t { EV_SISTEMA, EV_ENTRADA, EV_SAIDA, EV_NEGADO, EV_ALERTA, EV_CANCELA, EV_TESTE };

const char* const TIPO_NOME[]  = { "sistema", "entrada", "saida", "negado", "alerta", "cancela", "teste" };
const char* const TIPO_ICONE[] = { "🔧", "🟢", "🔵", "⛔", "🚨", "🔒", "🧪" };
const char* const TIPO_TAG[]   = { "SISTEMA", "ENTRADA", "SAÍDA  ", "NEGADO ", "ALERTA ", "CANCELA", "TESTE  " };

struct Evento {
  unsigned long instante;
  TipoEvento    tipo;
  char          texto[64];
};

// ═════════════════════════════════════════════════════════════════════
//  Botão com debounce + detecção de borda
// ═════════════════════════════════════════════════════════════════════
struct Botao {
  uint8_t       pino;
  int           estadoEstavel   = HIGH;
  int           ultimaLeitura   = HIGH;
  unsigned long instanteMudanca = 0;

  explicit Botao(uint8_t p) : pino(p) {}

  void iniciar() {
    pinMode(pino, INPUT_PULLUP);
    estadoEstavel = ultimaLeitura = digitalRead(pino);
    instanteMudanca = millis();
  }

  bool foiPressionado() {
    const int leitura = digitalRead(pino);
    const unsigned long agora = millis();
    if (leitura != ultimaLeitura) {
      ultimaLeitura   = leitura;
      instanteMudanca = agora;
    }
    if ((agora - instanteMudanca) >= DEBOUNCE_MS && leitura != estadoEstavel) {
      estadoEstavel = leitura;
      return estadoEstavel == LOW;
    }
    return false;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Sensor ultrassônico HC-SR04 — eco medido por interrupção
// ═════════════════════════════════════════════════════════════════════
enum EventoSensor : int8_t { SENSOR_NADA = 0, SENSOR_CHEGOU = 1, SENSOR_SAIU = -1 };

struct SensorUltrassonico {
  const char* nome;
  uint8_t     pinoTrig;
  uint8_t     pinoEcho;

  uint8_t     id;              // 0 = entrada, 1 = saída (índice nos vetores do eco)

  unsigned long instanteDisparo = 0;
  bool          aguardandoEco   = false;
  float         distanciaCm     = US_FORA_ALCANCE_CM;
  bool          presente        = false;
  uint8_t       confirmacoes    = 0;

  SensorUltrassonico(const char* n, uint8_t i, uint8_t trig, uint8_t echo)
    : nome(n), pinoTrig(trig), pinoEcho(echo), id(i) {}


  void iniciar() {
    pinMode(pinoTrig, OUTPUT);
    digitalWrite(pinoTrig, LOW);
    pinMode(pinoEcho, INPUT);       // a interrupção é ligada no setup()
  }

  void disparar() {
    ecoPronto[id] = false;
    digitalWrite(pinoTrig, HIGH);
    delayMicroseconds(10);              // pulso de 10 µs exigido pelo HC-SR04
    digitalWrite(pinoTrig, LOW);
    instanteDisparo = millis();
    aguardandoEco   = true;
  }

  // Chamado a cada loop. Retorna CHEGOU/SAIU quando o estado muda.
  EventoSensor atualizar() {
    if (!aguardandoEco) return SENSOR_NADA;

    float d;
    if (ecoPronto[id]) {
      d = ecoDuracao[id] / 58.0f;                       // µs → cm
      if (d > US_FORA_ALCANCE_CM) d = US_FORA_ALCANCE_CM;
    } else if (millis() - instanteDisparo >= US_TIMEOUT_MS) {
      d = US_FORA_ALCANCE_CM;                       // sem eco
    } else {
      return SENSOR_NADA;                           // ainda medindo
    }
    aguardandoEco = false;
    distanciaCm   = d;
    return filtrar(d);
  }

  // Histerese (50 cm entra / 70 cm sai) + N leituras seguidas
  EventoSensor filtrar(float d) {
    const bool leitura = presente ? (d < US_LIMIAR_CM + US_HISTERESE_CM)
                                  : (d < US_LIMIAR_CM);
    if (leitura == presente) { confirmacoes = 0; return SENSOR_NADA; }
    if (++confirmacoes < US_CONFIRMACOES) return SENSOR_NADA;
    confirmacoes = 0;
    presente     = leitura;
    return presente ? SENSOR_CHEGOU : SENSOR_SAIU;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Cancela — máquina de estados com movimento suave e segurança
// ═════════════════════════════════════════════════════════════════════
enum class EstadoCancela : uint8_t { FECHADA, ABRINDO, ABERTA, FECHANDO };
enum class EventoCancela : uint8_t { NADA, FECHOU, OBSTRUIDA, REABERTURA };

struct Cancela {
  const char*         nome;
  uint8_t             pino;
  SensorUltrassonico* sensor;          // detecta veículo sob a cancela

  Servo         servo;
  EstadoCancela estado           = EstadoCancela::FECHADA;
  int           angulo           = ANGULO_FECHADA;
  unsigned long instanteAbertura = 0;
  unsigned long ultimoPasso      = 0;
  bool          avisouObstrucao  = false;
  uint32_t      aberturas        = 0;

  Cancela(const char* n, uint8_t p, SensorUltrassonico* s) : nome(n), pino(p), sensor(s) {}

  void iniciar() {
    servo.setPeriodHertz(50);
    servo.attach(pino, SERVO_PULSO_MIN, SERVO_PULSO_MAX);
    servo.write(ANGULO_FECHADA);
  }

  bool estaAberta() const { return estado != EstadoCancela::FECHADA; }
  bool obstruida()  const { return sensor != nullptr && sensor->presente; }

  // Abre (ou mantém aberta) e reinicia a contagem de 2,5 s
  void abrir() {
    instanteAbertura = millis();
    avisouObstrucao  = false;
    if (estado != EstadoCancela::ABERTA) estado = EstadoCancela::ABRINDO;
    aberturas++;
  }

  // Move o servo um passo em direção ao alvo; true quando chegou
  bool moverPara(int alvo) {
    const unsigned long agora = millis();
    if (angulo == alvo) return true;
    if (agora - ultimoPasso < SERVO_PASSO_MS) return false;
    ultimoPasso = agora;
    angulo += (alvo > angulo) ? SERVO_PASSO_GRAUS : -SERVO_PASSO_GRAUS;
    if ((alvo > ANGULO_FECHADA && angulo > alvo) || (alvo == ANGULO_FECHADA && angulo < alvo)) angulo = alvo;
    servo.write(angulo);
    return angulo == alvo;
  }

  EventoCancela atualizar() {
    const unsigned long agora = millis();
    switch (estado) {
      case EstadoCancela::FECHADA:
        break;

      case EstadoCancela::ABRINDO:
        if (moverPara(ANGULO_ABERTA)) estado = EstadoCancela::ABERTA;
        break;

      case EstadoCancela::ABERTA:
        if (agora - instanteAbertura >= TEMPO_ABERTA_MS) {
          if (obstruida()) {                       // veículo ainda embaixo
            if (!avisouObstrucao) { avisouObstrucao = true; return EventoCancela::OBSTRUIDA; }
          } else {
            estado = EstadoCancela::FECHANDO;
          }
        }
        break;

      case EstadoCancela::FECHANDO:
        if (obstruida()) {                         // apareceu veículo: reabre
          estado           = EstadoCancela::ABRINDO;
          instanteAbertura = agora;
          avisouObstrucao  = false;
          return EventoCancela::REABERTURA;
        }
        if (moverPara(ANGULO_FECHADA)) {
          estado = EstadoCancela::FECHADA;
          return EventoCancela::FECHOU;
        }
        break;
    }
    return EventoCancela::NADA;
  }
};

// ═════════════════════════════════════════════════════════════════════
//  Buzzer — sequenciador de notas não bloqueante
// ═════════════════════════════════════════════════════════════════════
struct Nota { uint16_t freq; uint16_t ms; };   // freq 0 = pausa

const Nota SOM_INICIO[]  = { {1047, 80}, {1319, 80}, {1568, 140} };
const Nota SOM_ENTRADA[] = { {1568, 70}, {0, 40}, {2093, 90} };
const Nota SOM_SAIDA[]   = { {2093, 70}, {0, 40}, {1568, 90} };
const Nota SOM_NEGADO[]  = { {392, 180}, {0, 80}, {392, 180} };
const Nota SOM_LOTADO[]  = { {880, 120}, {0, 50}, {660, 120}, {0, 50}, {440, 220} };
const Nota SOM_ALERTA[]  = { {1760, 60}, {0, 60}, {1760, 60} };

#define TOCAR(som) buzzer.tocar(som, sizeof(som) / sizeof(som[0]))

struct Buzzer {
  uint8_t       pino;
  const Nota*   sequencia = nullptr;
  uint8_t       total     = 0;
  uint8_t       indice    = 0;
  unsigned long inicio    = 0;

  explicit Buzzer(uint8_t p) : pino(p) {}

  void tocar(const Nota* seq, uint8_t n) {
    if (!somAtivo) return;
    sequencia = seq; total = n; indice = 0;
    iniciarNota();
  }

  void iniciarNota() {
    inicio = millis();
    if (sequencia[indice].freq) tone(pino, sequencia[indice].freq);
    else noTone(pino);
  }

  void atualizar() {
    if (!sequencia) return;
    if (millis() - inicio < sequencia[indice].ms) return;
    if (++indice >= total) { noTone(pino); sequencia = nullptr; }
    else iniciarNota();
  }
};

// ═════════════════════════════════════════════════════════════════════
//  5. LÓGICA
// ═════════════════════════════════════════════════════════════════════
// (a primeira função livre vem depois de todos os tipos: assim os
//  protótipos gerados pelo Arduino já enxergam Cancela, Evento etc.)
const char* nomeEstado(EstadoCancela e) {
  switch (e) {
    case EstadoCancela::FECHADA:  return "fechada";
    case EstadoCancela::ABRINDO:  return "abrindo";
    case EstadoCancela::ABERTA:   return "aberta";
    case EstadoCancela::FECHANDO: return "fechando";
  }
  return "?";
}

// ─── Interrupções do eco dos HC-SR04 ───
// Funções livres com IRAM_ATTR (padrão do ESP32). Medem quanto tempo o
// pino ECHO fica em nível alto: esse tempo é proporcional à distância.
void IRAM_ATTR isrEcoEntrada() {
  if (gpio_get_level((gpio_num_t)PIN_US_ENT_ECHO)) ecoInicio[0] = micros();
  else { ecoDuracao[0] = micros() - ecoInicio[0]; ecoPronto[0] = true; }
}

void IRAM_ATTR isrEcoSaida() {
  if (gpio_get_level((gpio_num_t)PIN_US_SAI_ECHO)) ecoInicio[1] = micros();
  else { ecoDuracao[1] = micros() - ecoInicio[1]; ecoPronto[1] = true; }
}

// ═════════════════════════════════════════════════════════════════════
//  Histórico de eventos (buffer circular)
// ═════════════════════════════════════════════════════════════════════
Evento  eventos[MAX_EVENTOS];
uint8_t eventosInicio = 0;   // índice do mais antigo
uint8_t eventosTotal  = 0;

void imprimirTimestamp(unsigned long ms) {
  char buf[16];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu.%03lu] ",
           (ms / 60000UL) % 100, (ms / 1000UL) % 60, ms % 1000UL);
  Serial.print(buf);
}

// Registra no histórico e imprime no serial (formato printf)
void registrar(TipoEvento tipo, const char* formato, ...) {
  Evento& ev = eventos[(eventosInicio + eventosTotal) % MAX_EVENTOS];
  if (eventosTotal < MAX_EVENTOS) eventosTotal++;
  else eventosInicio = (eventosInicio + 1) % MAX_EVENTOS;

  ev.instante = millis();
  ev.tipo     = tipo;
  va_list args;
  va_start(args, formato);
  vsnprintf(ev.texto, sizeof(ev.texto), formato, args);
  va_end(args);

  imprimirTimestamp(ev.instante);
  Serial.print(TIPO_ICONE[tipo]);
  Serial.print(' ');
  Serial.print(TIPO_TAG[tipo]);
  Serial.print(' ');
  Serial.println(ev.texto);
}

// ═════════════════════════════════════════════════════════════════════
//  Instâncias
// ═════════════════════════════════════════════════════════════════════
Botao              btnEntrada     { PIN_BTN_ENTRADA };
Botao              btnSaida       { PIN_BTN_SAIDA };
SensorUltrassonico sensorEntrada  { "entrada", 0, PIN_US_ENT_TRIG, PIN_US_ENT_ECHO };
SensorUltrassonico sensorSaida    { "saida",   1, PIN_US_SAI_TRIG, PIN_US_SAI_ECHO };
Cancela            cancelaEntrada { "entrada", PIN_SERVO_ENTRADA, &sensorEntrada };
Cancela            cancelaSaida   { "saída",   PIN_SERVO_SAIDA,   &sensorSaida };
Buzzer             buzzer         { PIN_BUZZER };
LiquidCrystal_I2C  lcd(LCD_ENDERECO, 16, 2);
WebServer          servidor(80);
Preferences        nvs;

// ═════════════════════════════════════════════════════════════════════
//  Display LCD 16×2
// ═════════════════════════════════════════════════════════════════════
const uint8_t ICONE_CARRO[8]   = { 0b00000, 0b00000, 0b01110, 0b11111, 0b11111, 0b01010, 0b00000, 0b00000 };
const uint8_t ICONE_FECHADA[8] = { 0b00000, 0b00000, 0b11111, 0b11111, 0b10000, 0b10000, 0b10000, 0b10000 };
const uint8_t ICONE_ABERTA[8]  = { 0b11000, 0b11000, 0b11000, 0b11000, 0b11000, 0b10000, 0b10000, 0b10000 };
constexpr char CH_CARRO = 1, CH_FECHADA = 2, CH_ABERTA = 3;

char          lcdLinhas[2][17] = { "", "" };   // o que já está na tela
char          lcdAviso[17]     = "";
unsigned long lcdAvisoAte      = 0;

void lcdEscrever(uint8_t linha, const char* texto) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16s", texto);   // completa com espaços
  if (strcmp(buf, lcdLinhas[linha]) == 0) return; // evita I2C desnecessário
  lcd.setCursor(0, linha);
  lcd.print(buf);
  strcpy(lcdLinhas[linha], buf);
}

void lcdAvisar(const char* texto) {
  strncpy(lcdAviso, texto, 16);
  lcdAviso[16] = '\0';
  lcdAvisoAte  = millis() + LCD_MENSAGEM_MS;
}

void lcdIniciar() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.createChar(CH_CARRO,   (uint8_t*)ICONE_CARRO);
  lcd.createChar(CH_FECHADA, (uint8_t*)ICONE_FECHADA);
  lcd.createChar(CH_ABERTA,  (uint8_t*)ICONE_ABERTA);
  lcdEscrever(0, " ESTACIONAMENTO");
  lcdEscrever(1, "  ESP32 \x01 v2.0");
  lcdAvisoAte = millis() + 1500;               // tela de abertura
  lcdAviso[0] = '\0';
}

void lcdAtualizar() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < 100) return;         // 10 quadros/s bastam
  ultimo = millis();
  if ((long)(lcdAvisoAte - millis()) > 0 && lcdAviso[0] == '\0') return; // splash

  char l0[17], l1[17];
  if ((long)(lcdAvisoAte - millis()) > 0) {
    snprintf(l0, sizeof(l0), "%s", lcdAviso);
  } else {
    snprintf(l0, sizeof(l0), "%c Ocupadas %2d/%d", CH_CARRO, veiculos, CAPACIDADE);
  }

  char esquerda[20];
  if (veiculos >= CAPACIDADE) snprintf(esquerda, sizeof(esquerda), "LOTADO!");
  else                        snprintf(esquerda, sizeof(esquerda), "Livres: %d", CAPACIDADE - veiculos);
  snprintf(l1, sizeof(l1), "%-11sE%c S%c", esquerda,
           cancelaEntrada.estaAberta() ? CH_ABERTA : CH_FECHADA,
           cancelaSaida.estaAberta()   ? CH_ABERTA : CH_FECHADA);

  lcdEscrever(0, l0);
  lcdEscrever(1, l1);
}

// ═════════════════════════════════════════════════════════════════════
//  Persistência (NVS)
// ═════════════════════════════════════════════════════════════════════
void nvsCarregar() {
  if (!PERSISTIR_DADOS) return;
  nvs.begin("estac", true);
  veiculos       = constrain(nvs.getInt("veic", 0), 0, CAPACIDADE);
  stats.entradas = nvs.getUInt("ent", 0);
  stats.saidas   = nvs.getUInt("sai", 0);
  stats.negados  = nvs.getUInt("neg", 0);
  stats.pico     = nvs.getInt("pico", 0);
  nvs.end();
}

void nvsSalvarSeNecessario() {
  static unsigned long ultimo = 0;
  if (!PERSISTIR_DADOS || !dadosAlterados || millis() - ultimo < NVS_INTERVALO_MS) return;
  ultimo = millis();
  nvs.begin("estac", false);
  nvs.putInt("veic", veiculos);
  nvs.putUInt("ent", stats.entradas);
  nvs.putUInt("sai", stats.saidas);
  nvs.putUInt("neg", stats.negados);
  nvs.putInt("pico", stats.pico);
  nvs.end();
  dadosAlterados = false;
}

// ═════════════════════════════════════════════════════════════════════
//  Serial: status e ajuda
// ═════════════════════════════════════════════════════════════════════
void imprimirStatus() {
  char buf[64];
  Serial.print("              ▕");
  for (int i = 0; i < CAPACIDADE; i++) Serial.print(i < veiculos ? "█" : "░");
  snprintf(buf, sizeof(buf), "▏ %2d/%d · %2d livres", veiculos, CAPACIDADE, CAPACIDADE - veiculos);
  Serial.print(buf);
  Serial.println(veiculos >= CAPACIDADE ? "  ● LOTADO" : "  ● VAGAS");
}

void imprimirAjuda() {
  Serial.println();
  Serial.println("  ┌─ Comandos do monitor serial ─────────────────────────┐");
  Serial.println("  │  E  entrada manual          S  saída manual          │");
  Serial.println("  │  T  contador = 29 (teste)   R  zera o contador       │");
  Serial.println("  │  ?  status completo         L  lista o histórico     │");
  Serial.println("  │  W  informações do Wi-Fi    M  liga/desliga o som    │");
  Serial.println("  │  Z  zera estatísticas       H  esta ajuda            │");
  Serial.println("  └──────────────────────────────────────────────────────┘");
  Serial.println();
}

void imprimirBanner() {
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║   CONTROLE AUTOMATIZADO DE ESTACIONAMENTO · v2.0       ║");
  Serial.println("║   ESP32 · 30 vagas · LCD · HC-SR04 · Painel Web        ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  imprimirAjuda();
}

void imprimirStatusCompleto() {
  char buf[96];
  Serial.println();
  imprimirStatus();
  snprintf(buf, sizeof(buf), "  Cancelas : entrada %s · saída %s",
           nomeEstado(cancelaEntrada.estado), nomeEstado(cancelaSaida.estado));
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  Sensores : entrada %.0f cm%s · saída %.0f cm%s",
           sensorEntrada.distanciaCm, sensorEntrada.presente ? " (veículo)" : "",
           sensorSaida.distanciaCm,   sensorSaida.presente   ? " (veículo)" : "");
  Serial.println(buf);
  snprintf(buf, sizeof(buf), "  Totais   : %lu entradas · %lu saídas · %lu negados · pico %d",
           (unsigned long)stats.entradas, (unsigned long)stats.saidas,
           (unsigned long)stats.negados, stats.pico);
  Serial.println(buf);
  Serial.print("  Wi-Fi    : ");
  Serial.println(wifiConectado ? WiFi.localIP().toString() : String("desconectado"));
  Serial.println();
}

void imprimirHistorico() {
  Serial.println();
  Serial.println("  ── Últimos eventos ──");
  for (uint8_t i = 0; i < eventosTotal; i++) {
    const Evento& ev = eventos[(eventosInicio + i) % MAX_EVENTOS];
    Serial.print("  ");
    imprimirTimestamp(ev.instante);
    Serial.print(TIPO_ICONE[ev.tipo]);
    Serial.print(' ');
    Serial.println(ev.texto);
  }
  Serial.println();
}

// ═════════════════════════════════════════════════════════════════════
//  Sinalização e contador
// ═════════════════════════════════════════════════════════════════════
void atualizarLeds() {
  const bool lotado = (veiculos >= CAPACIDADE);
  digitalWrite(PIN_LED_VERMELHO, lotado ? HIGH : LOW);
  digitalWrite(PIN_LED_VERDE,    lotado ? LOW  : HIGH);
}

void definirVeiculos(int valor) {
  veiculos = constrain(valor, 0, CAPACIDADE);    // RF09
  if (veiculos > stats.pico) stats.pico = veiculos;
  dadosAlterados = true;
  atualizarLeds();
}

// ═════════════════════════════════════════════════════════════════════
//  Regras de negócio (origem: "sensor", "botão", "web", "serial")
// ═════════════════════════════════════════════════════════════════════
void processarEntrada(const char* origem) {
  if (veiculos >= CAPACIDADE) {
    stats.negados++;
    dadosAlterados = true;
    registrar(EV_NEGADO, "Acesso negado (%s): estacionamento LOTADO", origem);
    imprimirStatus();
    lcdAvisar("X ACESSO NEGADO");
    TOCAR(SOM_NEGADO);
    return;
  }
  definirVeiculos(veiculos + 1);
  stats.entradas++;
  cancelaEntrada.abrir();
  registrar(EV_ENTRADA, "Veículo autorizado (%s) - cancela ABERTA", origem);
  imprimirStatus();
  if (veiculos >= CAPACIDADE) {
    registrar(EV_ALERTA, "Capacidade máxima atingida - entrada bloqueada");
    lcdAvisar("! LOTOU  30/30");
    TOCAR(SOM_LOTADO);
  } else {
    lcdAvisar("> BEM-VINDO!");
    TOCAR(SOM_ENTRADA);
  }
}

void processarSaida(const char* origem) {
  if (veiculos <= 0) {
    registrar(EV_ALERTA, "Saída ignorada (%s): nenhum veículo", origem);
    imprimirStatus();
    lcdAvisar("! SEM VEICULOS");
    TOCAR(SOM_NEGADO);
    return;
  }
  const bool estavaLotado = (veiculos >= CAPACIDADE);
  definirVeiculos(veiculos - 1);
  stats.saidas++;
  cancelaSaida.abrir();
  registrar(EV_SAIDA, "Veículo liberado (%s) - cancela ABERTA", origem);
  imprimirStatus();
  if (estavaLotado) registrar(EV_ENTRADA, "Vaga liberada - entrada desbloqueada");
  lcdAvisar("< ATE LOGO!");
  TOCAR(SOM_SAIDA);
}

void tratarEventoCancela(Cancela& c, EventoCancela ev) {
  switch (ev) {
    case EventoCancela::FECHOU:
      registrar(EV_CANCELA, "Cancela de %s FECHADA", c.nome);
      break;
    case EventoCancela::OBSTRUIDA:
      registrar(EV_ALERTA, "Veículo sob a cancela de %s - fechamento adiado", c.nome);
      TOCAR(SOM_ALERTA);
      break;
    case EventoCancela::REABERTURA:
      registrar(EV_ALERTA, "Reabertura de segurança na cancela de %s", c.nome);
      TOCAR(SOM_ALERTA);
      break;
    case EventoCancela::NADA:
      break;
  }
}

// Medições alternadas: um sensor por vez evita que um capte o eco do outro
void agendarSensores() {
  static unsigned long ultimo = 0;
  static bool vezEntrada = true;
  if (millis() - ultimo < US_PERIODO_MS / 2) return;
  ultimo = millis();
  SensorUltrassonico& s = vezEntrada ? sensorEntrada : sensorSaida;
  if (!s.aguardandoEco) s.disparar();
  vezEntrada = !vezEntrada;
}

// ═════════════════════════════════════════════════════════════════════
//  Comandos (compartilhados pelo serial e pela web)
// ═════════════════════════════════════════════════════════════════════
bool executarComando(const String& acao, const char* origem) {
  if (acao == "entrada")            processarEntrada(origem);
  else if (acao == "saida")         processarSaida(origem);
  else if (acao == "abrir_entrada") { cancelaEntrada.abrir(); registrar(EV_CANCELA, "Abertura manual da entrada (%s)", origem); }
  else if (acao == "abrir_saida")   { cancelaSaida.abrir();   registrar(EV_CANCELA, "Abertura manual da saída (%s)", origem); }
  else if (acao == "teste")         { definirVeiculos(CAPACIDADE - 1); registrar(EV_TESTE, "Contador ajustado para %d (%s)", CAPACIDADE - 1, origem); imprimirStatus(); }
  else if (acao == "zerar")         { definirVeiculos(0); registrar(EV_TESTE, "Contador zerado (%s)", origem); imprimirStatus(); }
  else if (acao == "zerar_stats")   { stats = Estatisticas(); stats.pico = veiculos; dadosAlterados = true; registrar(EV_TESTE, "Estatísticas zeradas (%s)", origem); }
  else if (acao == "som")           { somAtivo = !somAtivo; if (!somAtivo) noTone(PIN_BUZZER); registrar(EV_SISTEMA, "Som %s (%s)", somAtivo ? "ligado" : "desligado", origem); }
  else return false;
  return true;
}

void processarSerial() {
  while (Serial.available() > 0) {
    switch (toupper(Serial.read())) {
      case 'E': executarComando("entrada", "serial");     break;
      case 'S': executarComando("saida", "serial");       break;
      case 'T': executarComando("teste", "serial");       break;
      case 'R': executarComando("zerar", "serial");       break;
      case 'Z': executarComando("zerar_stats", "serial"); break;
      case 'M': executarComando("som", "serial");         break;
      case '?': imprimirStatusCompleto();                 break;
      case 'L': imprimirHistorico();                      break;
      case 'H': imprimirAjuda();                          break;
      case 'W':
        Serial.print("  Wi-Fi: ");
        if (wifiConectado) {
          Serial.printf("%s · IP %s · RSSI %d dBm\n", WIFI_SSID,
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else {
          Serial.println("conectando...");
        }
        break;
      default: break;
    }
  }
}

// ═════════════════════════════════════════════════════════════════════
//  Web: API JSON + página
// ═════════════════════════════════════════════════════════════════════
void jsonTexto(String& out, const char* s) {     // string JSON com escape
  out += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { out += '\\'; out += *s; }
    else if ((uint8_t)*s < 0x20) out += ' ';
    else out += *s;
  }
  out += '"';
}

void jsonCancela(String& out, const Cancela& c) {
  out += "{\"estado\":\"";  out += nomeEstado(c.estado);
  out += "\",\"aberta\":";  out += c.estaAberta() ? "true" : "false";
  out += ",\"angulo\":";    out += c.angulo;
  out += ",\"obstruida\":"; out += c.obstruida() ? "true" : "false";
  out += ",\"aberturas\":"; out += c.aberturas;
  out += ",\"distancia\":"; out += String(c.sensor->distanciaCm, 0);
  out += '}';
}

String montarEstadoJson() {
  String j;
  j.reserve(2600);
  j += "{\"veiculos\":";    j += veiculos;
  j += ",\"capacidade\":";  j += CAPACIDADE;
  j += ",\"livres\":";      j += CAPACIDADE - veiculos;
  j += ",\"lotado\":";      j += veiculos >= CAPACIDADE ? "true" : "false";
  j += ",\"som\":";         j += somAtivo ? "true" : "false";
  j += ",\"uptime\":";      j += millis() / 1000;
  j += ",\"rssi\":";        j += wifiConectado ? WiFi.RSSI() : 0;
  j += ",\"cancelas\":{\"entrada\":"; jsonCancela(j, cancelaEntrada);
  j += ",\"saida\":";                 jsonCancela(j, cancelaSaida);
  j += "},\"stats\":{\"entradas\":";  j += stats.entradas;
  j += ",\"saidas\":";      j += stats.saidas;
  j += ",\"negados\":";     j += stats.negados;
  j += ",\"pico\":";        j += stats.pico;
  j += "},\"eventos\":[";
  for (int i = eventosTotal - 1; i >= 0; i--) {   // mais recente primeiro
    const Evento& ev = eventos[(eventosInicio + i) % MAX_EVENTOS];
    j += "{\"t\":";  j += ev.instante / 1000;
    j += ",\"tipo\":\""; j += TIPO_NOME[ev.tipo];
    j += "\",\"msg\":"; jsonTexto(j, ev.texto);
    j += '}';
    if (i > 0) j += ',';
  }
  j += "]}";
  return j;
}

void enviarEstado() {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "application/json", montarEstadoJson());
}

void configurarServidor() {
  servidor.on("/", HTTP_GET, []() {
    servidor.send(200, "text/html; charset=utf-8", PAGINA_HTML);
  });
  servidor.on("/api/estado", HTTP_GET, enviarEstado);
  servidor.on("/api/comando", HTTP_POST, []() {
    const String acao = servidor.arg("acao");
    if (!executarComando(acao, "web")) {
      servidor.send(400, "application/json", "{\"erro\":\"acao invalida\"}");
      return;
    }
    enviarEstado();
  });
  servidor.onNotFound([]() {
    servidor.send(404, "application/json", "{\"erro\":\"rota nao encontrada\"}");
  });
  servidor.begin();
  servidorAtivo = true;
}

// Conexão Wi-Fi não bloqueante: o sistema funciona mesmo sem rede
void atualizarWifi() {
  static unsigned long ultimo = 0;
  if (millis() - ultimo < 500) return;
  ultimo = millis();

  const bool conectado = (WiFi.status() == WL_CONNECTED);
  if (conectado && !wifiConectado) {
    wifiConectado = true;
    if (!servidorAtivo) {
      configurarServidor();
      MDNS.begin(MDNS_NOME);
    }
    const String ip = WiFi.localIP().toString();
    registrar(EV_SISTEMA, "Wi-Fi conectado - painel em http://%s", ip.c_str());
    char aviso[17];
    snprintf(aviso, sizeof(aviso), "IP %s", ip.c_str());
    lcdAvisar(aviso);
  } else if (!conectado && wifiConectado) {
    wifiConectado = false;
    registrar(EV_ALERTA, "Wi-Fi desconectado - tentando reconectar");
  }
}

// ═════════════════════════════════════════════════════════════════════
//  Setup & Loop
// ═════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  btnEntrada.iniciar();
  btnSaida.iniciar();
  sensorEntrada.iniciar();
  sensorSaida.iniciar();
  attachInterrupt(digitalPinToInterrupt(PIN_US_ENT_ECHO), isrEcoEntrada, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_US_SAI_ECHO), isrEcoSaida,   CHANGE);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  cancelaEntrada.iniciar();
  cancelaSaida.iniciar();

  lcdIniciar();
  nvsCarregar();
  definirVeiculos(veiculos);
  dadosAlterados = false;

  imprimirBanner();
  registrar(EV_SISTEMA, "Sistema pronto - %d/%d veículos%s", veiculos, CAPACIDADE,
            (PERSISTIR_DADOS && veiculos > 0) ? " (restaurado da flash)" : "");
  imprimirStatus();
  TOCAR(SOM_INICIO);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_SENHA, WIFI_CANAL);
  registrar(EV_SISTEMA, "Conectando ao Wi-Fi \"%s\"...", WIFI_SSID);
}

void loop() {
  // 1. Entradas: botões manuais (debounce) e sensores ultrassônicos
  if (btnEntrada.foiPressionado()) processarEntrada("botão");
  if (btnSaida.foiPressionado())   processarSaida("botão");

  agendarSensores();
  if (sensorEntrada.atualizar() == SENSOR_CHEGOU) processarEntrada("sensor");
  if (sensorSaida.atualizar()   == SENSOR_CHEGOU) processarSaida("sensor");

  // 2. Cancelas (movimento + temporização + segurança)
  tratarEventoCancela(cancelaEntrada, cancelaEntrada.atualizar());
  tratarEventoCancela(cancelaSaida,   cancelaSaida.atualizar());

  // 3. Interfaces
  buzzer.atualizar();
  lcdAtualizar();
  processarSerial();
  atualizarWifi();
  if (servidorAtivo) servidor.handleClient();

  // 4. Persistência
  nvsSalvarSeNecessario();
}
