/*
 * ============================================================
 *  Relais Circulateur — ESP32 WebApp
 *  WiFi provisioning + NTP + WebSocket + Basic Auth + LittleFS
 * ============================================================
 *  Bibliothèques requises (Library Manager) :
 *    - ESPAsyncWebServer  (me-no-dev)
 *    - AsyncTCP           (me-no-dev)
 *    - ArduinoJson        (Benoit Blanchon)
 * ============================================================
 *  PREMIER DÉMARRAGE (ou après reset WiFi) :
 *    1. Connecte-toi au réseau Wi-Fi  "RELAI-SETUP"  (sans mot de passe)
 *    2. Ouvre http://192.168.4.1
 *    3. Choisis ton réseau, saisis le mot de passe, valide
 *    4. L'ESP32 redémarre et se connecte à ta box
 *
 *  RESET WiFi :
 *    → Bouton "Reconfigurer le WiFi" dans la webapp
 *    → OU maintenir bouton BOOT (GPIO0) pendant 5 s
 *
 *  Accès local  : http://<IP_ESP32>/
 *  Accès distant: http://<domaine-gnudip>:54789/
 *  (NAT/PAT Livebox : WAN 54789 → IP_ESP32:80)
 * ============================================================
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <time.h>

// ── Paramètres ───────────────────────────────────────────────
#define HTTP_USER        "admin"
#define HTTP_PASS        "relai2024"

#define RELAY_PIN        4
#define BOOT_PIN         0        // Bouton BOOT intégré sur toutes les cartes ESP32
#define CONFIG_FILE      "/config.json"
#define WIFI_FILE        "/wifi.json"
#define AP_SETUP_SSID    "RELAI-SETUP"

// ── NTP / France ─────────────────────────────────────────────
#define NTP_SERVER1      "pool.ntp.org"
#define NTP_SERVER2      "time.nist.gov"
#define TZ_EUROPE_PARIS  "CET-1CEST,M3.5.0,M10.5.0/3"

// ── Timings ──────────────────────────────────────────────────
const unsigned long BROADCAST_MS    = 1000;
const unsigned long NTP_RETRY_MS    = 10000;
const unsigned long WIFI_TIMEOUT_MS = 20000;

// ── Objets globaux ────────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer      dnsServer;

// ── Config relai ─────────────────────────────────────────────
String cfgMode    = "off";
String cfgS1Start = "08:00";
String cfgS1End   = "12:00";
String cfgS2Start = "14:00";
String cfgS2End   = "18:00";

bool   relayState    = false;
bool   ntpSynced     = false;
bool   configMode    = false;   // true = on est dans le portail de config

unsigned long lastBroadcast  = 0;
unsigned long lastNtpRetry   = 0;

// ── Bouton BOOT ──────────────────────────────────────────────
unsigned long bootPressStart = 0;
bool          bootWasPressed = false;


// ═════════════════════════════════════════════════════════════
//  PAGE HTML — PORTAIL DE CONFIGURATION WIFI
// ═════════════════════════════════════════════════════════════
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='fr'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Configuration WiFi — Relais Circulateur</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Exo+2:wght@400;600;800&display=swap');
:root{--bg:#0d0f1a;--surface:#141728;--surface2:#1c2040;
  --accent:#00e5ff;--green:#00e676;--amber:#ffab00;--text:#dde3f0;--muted:#7a84a8;}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Exo 2',sans-serif;background:var(--bg);color:var(--text);
  min-height:100vh;display:flex;flex-direction:column;align-items:center;
  justify-content:center;padding:20px;}
h1{font-size:1.6rem;font-weight:800;letter-spacing:3px;text-transform:uppercase;
  color:var(--accent);text-shadow:0 0 18px rgba(0,229,255,.4);text-align:center;margin-bottom:6px;}
p.sub{text-align:center;color:var(--muted);font-size:.85rem;margin-bottom:28px;letter-spacing:1px;}
.card{background:var(--surface);border:1px solid rgba(0,229,255,.12);border-radius:16px;
  padding:28px 24px;width:100%;max-width:420px;}
label{display:block;font-size:.8rem;color:var(--muted);letter-spacing:1px;
  text-transform:uppercase;margin-bottom:6px;}
select,input[type=password]{width:100%;padding:11px 14px;background:var(--surface2);
  border:1px solid rgba(0,229,255,.2);border-radius:10px;color:var(--accent);
  font-family:'Exo 2',sans-serif;font-size:1rem;outline:none;margin-bottom:20px;
  transition:border-color .2s;}
select:focus,input[type=password]:focus{border-color:var(--accent);
  box-shadow:0 0 0 3px rgba(0,229,255,.1);}
select option{background:var(--surface2);color:var(--text);}
.btn-connect{width:100%;padding:14px;
  background:linear-gradient(135deg,rgba(0,229,255,.2),rgba(0,229,255,.05));
  border:1px solid var(--accent);border-radius:10px;color:var(--accent);
  font-family:'Exo 2',sans-serif;font-size:1rem;font-weight:700;letter-spacing:1px;
  cursor:pointer;transition:all .2s;}
.btn-connect:hover{background:linear-gradient(135deg,rgba(0,229,255,.35),rgba(0,229,255,.1));}
.btn-connect:disabled{opacity:.4;cursor:not-allowed;}
.msg{margin-top:16px;padding:12px;border-radius:10px;text-align:center;
  font-weight:600;font-size:.9rem;display:none;}
.msg.ok {background:rgba(0,230,118,.12);border:1px solid var(--green);color:var(--green);}
.msg.err{background:rgba(255,23,68,.12);border:1px solid #ff1744;color:#ff1744;}
.refresh{background:none;border:none;color:var(--muted);cursor:pointer;font-size:.8rem;
  letter-spacing:1px;text-decoration:underline;margin-bottom:16px;display:block;}
</style>
</head>
<body>
<h1>&#9881; Configuration WiFi</h1>
<p class='sub'>Relais Circulateur &mdash; Configuration</p>
<div class='card'>
  <button class='refresh' onclick='scanNetworks()'>&#8635; Actualiser la liste</button>
  <label>R&eacute;seau WiFi</label>
  <select id='ssid'><option>Scan en cours&hellip;</option></select>
  <label>Mot de passe</label>
  <input type='password' id='pass' placeholder='Mot de passe WiFi' autocomplete='off'>
  <button class='btn-connect' id='btnSave' onclick='save()'>
    &#128225; &nbsp;Connecter &amp; red&eacute;marrer
  </button>
  <div class='msg' id='msg'></div>
</div>
<script>
function signalBars(rssi){
  if(rssi>-60) return '&#9679;&#9679;&#9679;';
  if(rssi>-75) return '&#9679;&#9679;&#9675;';
  return '&#9679;&#9675;&#9675;';
}
function scanNetworks(){
  const sel=document.getElementById('ssid');
  sel.innerHTML='<option>Scan en cours&hellip;</option>';
  sel.disabled=true;
  fetch('/scan/start')
    .then(()=>new Promise(r=>setTimeout(r,4000)))
    .then(()=>fetch('/scan/results'))
    .then(r=>r.json())
    .then(nets=>{
      sel.innerHTML=''; sel.disabled=false;
      if(!nets.length){sel.innerHTML='<option>Aucun r&eacute;seau trouv&eacute;</option>';return;}
      nets.forEach(n=>{
        const o=document.createElement('option');
        o.value=n.ssid;
        o.innerHTML=n.ssid+'&nbsp;&nbsp;'+signalBars(n.rssi);
        sel.appendChild(o);
      });
    })
    .catch(()=>{sel.disabled=false;sel.innerHTML='<option>Erreur de scan</option>';});
}
function save(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value;
  if(!ssid||ssid==='Scan en cours\u2026') return;
  const btn=document.getElementById('btnSave');
  btn.disabled=true; btn.innerHTML='&#9203; Enregistrement&hellip;';
  fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid,pass})})
  .then(r=>r.json())
  .then(d=>{
    const msg=document.getElementById('msg');
    msg.style.display='block';
    if(d.ok){msg.className='msg ok';
      msg.innerHTML='&#10003; Enregistr&eacute; &mdash; red&eacute;marrage dans 3 s&hellip;';}
    else{msg.className='msg err';msg.textContent='Erreur &mdash; r\u00e9essaie';
      btn.disabled=false;btn.innerHTML='&#128225; Connecter &amp; red\u00e9marrer';}
  })
  .catch(()=>{btn.disabled=false;btn.innerHTML='&#128225; Connecter &amp; red\u00e9marrer';});
}
scanNetworks();
</script>
</body>
</html>
)rawliteral";


// ═════════════════════════════════════════════════════════════
//  PAGE HTML — APPLICATION RELAIS
// ═════════════════════════════════════════════════════════════
const char RELAY_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='fr'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1.0'>
<title>Relais Circulateur</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@400;600;800&display=swap');
:root{
  --bg:#0d0f1a;--surface:#141728;--surface2:#1c2040;
  --accent:#00e5ff;--green:#00e676;--red:#ff1744;--amber:#ffab00;
  --text:#dde3f0;--muted:#7a84a8;--radius:14px;
  --glow-g:0 0 14px rgba(0,230,118,.55);
  --glow-r:0 0 14px rgba(255,23,68,.55);
  --glow-a:0 0 14px rgba(0,229,255,.55);
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Exo 2',sans-serif;background:var(--bg);color:var(--text);
  min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:20px 14px 40px;}
header{text-align:center;margin-bottom:22px;}
header h1{font-size:clamp(1.5rem,5vw,2.1rem);font-weight:800;letter-spacing:3px;
  text-transform:uppercase;color:var(--accent);text-shadow:0 0 18px rgba(0,229,255,.45);}
header p.subtitle{font-family:'Share Tech Mono',monospace;font-size:.8rem;
  color:var(--muted);margin-top:4px;letter-spacing:2px;}
.card{background:var(--surface);border:1px solid rgba(0,229,255,.1);
  border-radius:var(--radius);padding:22px 20px;width:100%;max-width:460px;
  margin-bottom:16px;position:relative;overflow:hidden;}
.card::before{content:'';position:absolute;inset:0;border-radius:var(--radius);
  background:linear-gradient(135deg,rgba(0,229,255,.04) 0%,transparent 60%);pointer-events:none;}
.status-bar{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;}
.status-bar .label{font-size:.85rem;color:var(--muted);text-transform:uppercase;letter-spacing:1px}
.badge{font-family:'Share Tech Mono',monospace;font-size:.82rem;padding:4px 10px;
  border-radius:20px;border:1px solid currentColor;font-weight:600;}
.badge.on  {color:var(--green);border-color:var(--green);text-shadow:var(--glow-g)}
.badge.off {color:var(--red);border-color:var(--red);text-shadow:var(--glow-r)}
.badge.sched{color:var(--amber);border-color:var(--amber)}
.clock{font-family:'Share Tech Mono',monospace;font-size:clamp(2.2rem,8vw,3rem);
  letter-spacing:4px;color:var(--accent);text-shadow:var(--glow-a);
  text-align:center;margin:14px 0 10px;}
.relay-indicator{display:flex;align-items:center;gap:10px;justify-content:center;margin-top:4px;}
.dot{width:16px;height:16px;border-radius:50%;background:#333;
  transition:background .4s,box-shadow .4s;flex-shrink:0;}
.dot.on {background:var(--green);box-shadow:var(--glow-g)}
.dot.off{background:var(--red);  box-shadow:var(--glow-r)}
.relay-label{font-size:.9rem;color:var(--muted)}.relay-label span{font-weight:700}
.card-title{font-size:.75rem;letter-spacing:2px;text-transform:uppercase;
  color:var(--muted);margin-bottom:16px;}
.mode-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;}
.btn{padding:13px 8px;border:2px solid transparent;border-radius:10px;
  font-family:'Exo 2',sans-serif;font-size:.88rem;font-weight:700;cursor:pointer;
  transition:all .2s;display:flex;flex-direction:column;align-items:center;gap:6px;
  background:var(--surface2);color:var(--muted);}
.btn .icon{font-size:1.4rem;line-height:1}
.btn:hover:not(:disabled){color:var(--text);transform:translateY(-2px)}
.btn:disabled{opacity:.35;cursor:not-allowed}
.btn-on.active  {color:var(--green);border-color:var(--green);
  box-shadow:inset 0 0 20px rgba(0,230,118,.12),var(--glow-g)}
.btn-off.active {color:var(--red);border-color:var(--red);
  box-shadow:inset 0 0 20px rgba(255,23,68,.12),var(--glow-r)}
.btn-sched.active{color:var(--amber);border-color:var(--amber);
  box-shadow:inset 0 0 20px rgba(255,171,0,.12)}
.schedule-section{overflow:hidden;max-height:0;opacity:0;
  transition:max-height .45s ease,opacity .35s ease,margin .3s;}
.schedule-section.visible{max-height:600px;opacity:1;margin-top:18px;}
.slot{background:var(--surface2);border-radius:10px;padding:16px;
  margin-bottom:12px;border-left:3px solid var(--amber);}
.slot-title{font-size:.8rem;color:var(--amber);letter-spacing:1.5px;
  text-transform:uppercase;margin-bottom:12px;font-weight:700;}
.time-row{display:grid;grid-template-columns:60px 1fr;align-items:center;
  gap:10px;margin-bottom:10px;}
.time-row:last-of-type{margin-bottom:0}
.time-row label{font-size:.8rem;color:var(--muted);letter-spacing:1px;}
input[type='time']{width:100%;padding:9px 12px;background:#0d0f1a;
  border:1px solid rgba(0,229,255,.2);border-radius:8px;color:var(--accent);
  font-family:'Share Tech Mono',monospace;font-size:1rem;outline:none;transition:border-color .2s;}
input[type='time']:focus{border-color:var(--accent);box-shadow:0 0 0 3px rgba(0,229,255,.1)}
input[type='time']::-webkit-calendar-picker-indicator{
  filter:invert(.6) sepia(1) saturate(3) hue-rotate(155deg)}
.save-btn{width:100%;padding:13px;margin-top:14px;
  background:linear-gradient(135deg,rgba(255,171,0,.2),rgba(255,171,0,.05));
  border:1px solid var(--amber);border-radius:10px;color:var(--amber);
  font-family:'Exo 2',sans-serif;font-size:.95rem;font-weight:700;letter-spacing:1px;
  cursor:pointer;transition:all .2s;}
.save-btn:hover{background:linear-gradient(135deg,rgba(255,171,0,.35),rgba(255,171,0,.1));
  transform:translateY(-1px)}
.reset-btn{width:100%;padding:12px;background:rgba(255,23,68,.08);
  border:1px solid rgba(255,23,68,.4);border-radius:10px;color:#ff6b6b;
  font-family:'Exo 2',sans-serif;font-size:.9rem;font-weight:700;letter-spacing:1px;
  cursor:pointer;transition:all .2s;}
.reset-btn:hover{background:rgba(255,23,68,.18);border-color:#ff1744;}
.ntp-warn{width:100%;max-width:460px;background:rgba(255,171,0,.12);
  border:1px solid var(--amber);color:var(--amber);border-radius:10px;
  padding:10px 16px;font-size:.85rem;font-weight:600;text-align:center;
  margin-bottom:14px;display:none;}
.toast{position:fixed;bottom:28px;left:50%;transform:translateX(-50%) translateY(20px);
  background:rgba(0,230,118,.15);border:1px solid var(--green);color:var(--green);
  padding:11px 28px;border-radius:30px;font-size:.9rem;font-weight:700;letter-spacing:1px;
  opacity:0;transition:opacity .3s,transform .3s;pointer-events:none;
  white-space:nowrap;backdrop-filter:blur(6px);}
.toast.show{opacity:1;transform:translateX(-50%) translateY(0)}
.ws-dot{position:fixed;top:12px;right:14px;width:8px;height:8px;
  border-radius:50%;background:#333;transition:background .4s;}
.ws-dot.connected{background:var(--green);box-shadow:var(--glow-g)}
@media(max-width:340px){
  .mode-grid{grid-template-columns:1fr;gap:8px}
  .btn{flex-direction:row;padding:12px}
}
</style>
</head>
<body>

<div class='ws-dot' id='wsDot'></div>

<header>
  <h1>Relais Circulateur</h1>
  <p class='subtitle' id='dateTime'>&mdash; &nbsp;&middot;&nbsp; &mdash;</p>
</header>

<div class='ntp-warn' id='ntpWarn'>&#9203; Synchronisation NTP en cours&hellip;</div>

<div class='card'>
  <div class='status-bar'>
    <span class='label'>Mode</span>
    <span class='badge' id='modeBadge'>&mdash;</span>
  </div>
  <div class='clock' id='clock'>--:--:--</div>
  <div class='relay-indicator'>
    <div class='dot' id='relayDot'></div>
    <span class='relay-label'>Relais : <span id='relayTxt'>&mdash;</span></span>
  </div>
</div>

<div class='card'>
  <div class='card-title'>Contr&ocirc;le</div>
  <div class='mode-grid'>
    <button class='btn btn-on'    id='btnOn'    onclick='setMode("on")'>
      <span class='icon'>&#9889;</span>Forcer ON
    </button>
    <button class='btn btn-sched' id='btnSched' onclick='setMode("schedule")'>
      <span class='icon'>&#128336;</span>Programm&eacute;
    </button>
    <button class='btn btn-off'   id='btnOff'   onclick='setMode("off")'>
      <span class='icon'>&#9209;</span>Forcer OFF
    </button>
  </div>
  <div class='schedule-section' id='schedSection'>
    <div class='slot'>
      <div class='slot-title'>Tranche 1</div>
      <div class='time-row'><label>D&eacute;but</label><input type='time' id='s1start'></div>
      <div class='time-row'><label>Fin</label>        <input type='time' id='s1end'></div>
    </div>
    <div class='slot'>
      <div class='slot-title'>Tranche 2</div>
      <div class='time-row'><label>D&eacute;but</label><input type='time' id='s2start'></div>
      <div class='time-row'><label>Fin</label>        <input type='time' id='s2end'></div>
    </div>
    <button class='save-btn' onclick='saveSchedule()'>
      &#128190; &nbsp;Enregistrer les horaires
    </button>
  </div>
</div>

<div class='card' style='border-color:rgba(255,23,68,.2)'>
  <div class='card-title' style='color:#ff6b6b'>Administration</div>
  <button class='reset-btn' onclick='confirmReset()'>
    &#8635; &nbsp;Reconfigurer le WiFi
  </button>
</div>

<div class='toast' id='toast'></div>

<script>
let ws, reconnectTimer;
const modeLabels     = { on:'FORC\u00c9 ON', off:'FORC\u00c9 OFF', schedule:'PROGRAMM\u00c9' };
const modeBadgeClass = { on:'on', off:'off', schedule:'sched' };

function connect() {
  clearTimeout(reconnectTimer);
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen  = () => document.getElementById('wsDot').classList.add('connected');
  ws.onclose = () => {
    document.getElementById('wsDot').classList.remove('connected');
    reconnectTimer = setTimeout(connect, 2500);
  };
  ws.onerror  = () => ws.close();
  ws.onmessage = (e) => { try { applyState(JSON.parse(e.data)); } catch(err){} };
}

function applyState(d) {
  if (d.reset) { showToast('\u21ba Red\u00e9marrage\u2026'); return; }

  const ntpOk = d.ntp_ok === true;
  document.getElementById('ntpWarn').style.display = ntpOk ? 'none' : 'block';
  document.getElementById('btnSched').disabled = !ntpOk;

  document.getElementById('clock').textContent = d.time || '--:--:--';
  if (d.date && d.time)
    document.getElementById('dateTime').textContent =
      d.date + '\u00a0\u00a0\u00b7\u00a0\u00a0' + d.time;

  const dot = document.getElementById('relayDot');
  const txt = document.getElementById('relayTxt');
  dot.className   = 'dot ' + (d.relay ? 'on' : 'off');
  txt.textContent = d.relay ? 'ACTIF' : 'INACTIF';
  txt.style.color = d.relay ? 'var(--green)' : 'var(--red)';

  const badge = document.getElementById('modeBadge');
  badge.textContent = modeLabels[d.mode] || d.mode;
  badge.className   = 'badge ' + (modeBadgeClass[d.mode] || '');

  ['btnOn','btnSched','btnOff'].forEach(id =>
    document.getElementById(id).classList.remove('active'));
  const map = { on:'btnOn', off:'btnOff', schedule:'btnSched' };
  if (map[d.mode]) document.getElementById(map[d.mode]).classList.add('active');

  document.getElementById('schedSection')
    .classList.toggle('visible', d.mode === 'schedule');

  fillIfUnfocused('s1start', d.slot1_start);
  fillIfUnfocused('s1end',   d.slot1_end);
  fillIfUnfocused('s2start', d.slot2_start);
  fillIfUnfocused('s2end',   d.slot2_end);
}

function fillIfUnfocused(id, val) {
  const el = document.getElementById(id);
  if (el && document.activeElement !== el && val) el.value = val;
}
function setMode(mode) {
  if (ws && ws.readyState === 1)
    ws.send(JSON.stringify({ action:'set_mode', mode }));
}
function saveSchedule() {
  if (!ws || ws.readyState !== 1) return;
  ws.send(JSON.stringify({
    action:      'set_schedule',
    slot1_start:  document.getElementById('s1start').value,
    slot1_end:    document.getElementById('s1end').value,
    slot2_start:  document.getElementById('s2start').value,
    slot2_end:    document.getElementById('s2end').value
  }));
  showToast('\u2713 Horaires enregistr\u00e9s');
}
function confirmReset() {
  if (!confirm('Supprimer la configuration WiFi et relancer le portail ?')) return;
  if (ws && ws.readyState === 1)
    ws.send(JSON.stringify({ action:'reset_wifi' }));
  showToast('\u21ba Retour au portail de configuration\u2026');
}
function showToast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 3000);
}
connect();
</script>
</body>
</html>
)rawliteral";


// ─────────────────────────────────────────────────────────────
//  LittleFS — config relai
// ─────────────────────────────────────────────────────────────
void loadRelayConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) return;
  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return;
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    cfgMode    = doc["mode"] | "off";
    cfgS1Start = doc["s1s"]  | "08:00";
    cfgS1End   = doc["s1e"]  | "12:00";
    cfgS2Start = doc["s2s"]  | "14:00";
    cfgS2End   = doc["s2e"]  | "18:00";
  }
  f.close();
  Serial.printf("[FS] Config — mode:%s s1:%s-%s s2:%s-%s\n",
    cfgMode.c_str(), cfgS1Start.c_str(), cfgS1End.c_str(),
    cfgS2Start.c_str(), cfgS2End.c_str());
}

void saveRelayConfig() {
  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return;
  StaticJsonDocument<256> doc;
  doc["mode"]=cfgMode; doc["s1s"]=cfgS1Start; doc["s1e"]=cfgS1End;
  doc["s2s"]=cfgS2Start; doc["s2e"]=cfgS2End;
  serializeJson(doc, f);
  f.close();
}


// ─────────────────────────────────────────────────────────────
//  LittleFS — credentials WiFi
// ─────────────────────────────────────────────────────────────
bool loadWifiCreds(String &ssid, String &pass) {
  if (!LittleFS.exists(WIFI_FILE)) return false;
  File f = LittleFS.open(WIFI_FILE, "r");
  if (!f) return false;
  StaticJsonDocument<192> doc;
  bool ok = (deserializeJson(doc, f) == DeserializationError::Ok);
  f.close();
  if (!ok) return false;
  ssid = doc["ssid"] | "";
  pass = doc["pass"] | "";
  return ssid.length() > 0;
}

void saveWifiCreds(const String &ssid, const String &pass) {
  File f = LittleFS.open(WIFI_FILE, "w");
  if (!f) return;
  StaticJsonDocument<192> doc;
  doc["ssid"] = ssid; doc["pass"] = pass;
  serializeJson(doc, f);
  f.close();
  Serial.printf("[FS] WiFi sauvegardé — SSID: %s\n", ssid.c_str());
}

void deleteWifiCreds() {
  LittleFS.remove(WIFI_FILE);
  Serial.println("[FS] Credentials WiFi supprimés");
}


// ─────────────────────────────────────────────────────────────
//  NTP
// ─────────────────────────────────────────────────────────────
void tryNtpSync() {
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
  setenv("TZ", TZ_EUROPE_PARIS, 1);
  tzset();
  struct tm ti;
  if (getLocalTime(&ti, 200)) {
    ntpSynced = true;
    Serial.printf("[NTP] OK → %02d/%02d/%04d %02d:%02d:%02d\n",
      ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900,
      ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    Serial.println("[NTP] En attente...");
  }
}


// ─────────────────────────────────────────────────────────────
//  Logique relai
// ─────────────────────────────────────────────────────────────
int timeToMinutes(const String &t) {
  if (t.length() < 5) return 0;
  return t.substring(0,2).toInt() * 60 + t.substring(3,5).toInt();
}

bool scheduleActive(const struct tm &ti) {
  int cur = ti.tm_hour * 60 + ti.tm_min;
  int s1s = timeToMinutes(cfgS1Start), s1e = timeToMinutes(cfgS1End);
  int s2s = timeToMinutes(cfgS2Start), s2e = timeToMinutes(cfgS2End);
  return (s1s < s1e && cur >= s1s && cur < s1e) ||
         (s2s < s2e && cur >= s2s && cur < s2e);
}

void updateRelay(const struct tm *ti) {
  bool desired = false;
  if      (cfgMode == "on")                          desired = true;
  else if (cfgMode == "off")                         desired = false;
  else if (cfgMode == "schedule" && ntpSynced && ti) desired = scheduleActive(*ti);
  if (desired != relayState) {
    relayState = desired;
    digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    Serial.printf("[RELAY] %s\n", relayState ? "ON" : "OFF");
  }
}


// ─────────────────────────────────────────────────────────────
//  WebSocket — broadcast
// ─────────────────────────────────────────────────────────────
void broadcastState(const struct tm *ti) {
  StaticJsonDocument<384> doc;
  doc["relay"]  = relayState;
  doc["mode"]   = cfgMode;
  doc["ntp_ok"] = ntpSynced;
  if (ntpSynced && ti) {
    char tb[9], db[11];
    snprintf(tb, sizeof(tb), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
    snprintf(db, sizeof(db), "%02d/%02d/%04d", ti->tm_mday, ti->tm_mon+1, ti->tm_year+1900);
    doc["time"] = tb; doc["date"] = db;
  } else {
    doc["time"] = "--:--:--"; doc["date"] = "--/--/----";
  }
  doc["slot1_start"]=cfgS1Start; doc["slot1_end"]=cfgS1End;
  doc["slot2_start"]=cfgS2Start; doc["slot2_end"]=cfgS2End;
  String out;
  serializeJson(doc, out);
  ws.textAll(out);
}


// ─────────────────────────────────────────────────────────────
//  WebSocket — réception
// ─────────────────────────────────────────────────────────────
void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u depuis %s\n",
                  client->id(), client->remoteIP().toString().c_str());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u déconnecté\n", client->id());
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      char msgBuf[len + 1];
      memcpy(msgBuf, data, len);
      msgBuf[len] = '\0';
      StaticJsonDocument<300> doc;
      if (deserializeJson(doc, msgBuf) != DeserializationError::Ok) return;
      String action = doc["action"] | "";

      if (action == "set_mode") {
        String nm = doc["mode"] | "";
        if (nm == "on" || nm == "off" || nm == "schedule") {
          cfgMode = nm; saveRelayConfig();
          Serial.printf("[WS] Mode → %s\n", cfgMode.c_str());
        }
      } else if (action == "set_schedule") {
        cfgS1Start = doc["slot1_start"] | cfgS1Start;
        cfgS1End   = doc["slot1_end"]   | cfgS1End;
        cfgS2Start = doc["slot2_start"] | cfgS2Start;
        cfgS2End   = doc["slot2_end"]   | cfgS2End;
        saveRelayConfig();
        Serial.printf("[WS] Horaires → %s-%s / %s-%s\n",
          cfgS1Start.c_str(), cfgS1End.c_str(),
          cfgS2Start.c_str(), cfgS2End.c_str());
      } else if (action == "reset_wifi") {
        Serial.println("[WS] Reset WiFi");
        ws.textAll("{\"reset\":true}");
        delay(1500);
        deleteWifiCreds();
        ESP.restart();
      }
    }
  }
}


// ─────────────────────────────────────────────────────────────
//  MODE CONFIG — portail WiFi (bloquant)
// ─────────────────────────────────────────────────────────────
void runConfigPortal() {
  Serial.println("[CONFIG] Portail de configuration WiFi");
  configMode = true;

  const IPAddress AP_IP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SETUP_SSID);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255,255,255,0));
  Serial.printf("[CONFIG] AP '%s' — IP %s\n", AP_SETUP_SSID, AP_IP.toString().c_str());

  dnsServer.start(53, "*", AP_IP);

  // Page portail
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", CONFIG_PAGE);
  });

  // Scan WiFi — lancement async
  server.on("/scan/start", HTTP_GET, [](AsyncWebServerRequest *req) {
    WiFi.scanNetworks(true);
    req->send(200, "text/plain", "ok");
  });

  // Scan WiFi — résultats
  server.on("/scan/results", HTTP_GET, [](AsyncWebServerRequest *req) {
    int n = WiFi.scanComplete();
    if (n <= 0) { req->send(200, "application/json", "[]"); return; }
    std::vector<int> idx(n);
    for (int i = 0; i < n; i++) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [](int a, int b){
      return WiFi.RSSI(a) > WiFi.RSSI(b);
    });
    String json = "[";
    std::vector<String> seen;
    for (int i : idx) {
      String s = WiFi.SSID(i);
      if (s.isEmpty()) continue;
      bool dup = false;
      for (auto &v : seen) if (v == s) { dup = true; break; }
      if (dup) continue;
      seen.push_back(s);
      s.replace("\"", "\\\"");
      if (json.length() > 1) json += ",";
      json += "{\"ssid\":\"" + s + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    req->send(200, "application/json", json);
  });

  // Sauvegarde credentials
  server.on("/save", HTTP_POST,
    [](AsyncWebServerRequest *req) {},
    nullptr,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
      char buf[len + 1];
      memcpy(buf, data, len);
      buf[len] = '\0';
      StaticJsonDocument<192> doc;
      bool ok = (deserializeJson(doc, buf) == DeserializationError::Ok);
      String ssid = ok ? (doc["ssid"] | "") : "";
      String pass = ok ? (doc["pass"] | "") : "";
      if (ssid.length() > 0) {
        saveWifiCreds(ssid, pass);
        req->send(200, "application/json", "{\"ok\":true}");
        delay(3000);
        ESP.restart();
      } else {
        req->send(200, "application/json", "{\"ok\":false}");
      }
    }
  );

  server.onNotFound([](AsyncWebServerRequest *req) {
    req->redirect("http://192.168.4.1/");
  });

  server.begin();
  Serial.println("[CONFIG] Portail prêt — en attente");

  while (true) {
    dnsServer.processNextRequest();
    delay(10);
  }
}


// ─────────────────────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Relais Circulateur — démarrage ===");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  pinMode(BOOT_PIN, INPUT_PULLUP);

  // ── Bouton BOOT maintenu au démarrage → reset WiFi ────────
  if (digitalRead(BOOT_PIN) == LOW) {
    Serial.println("[BOOT] Maintenu — attendre 5 s pour reset WiFi...");
    unsigned long t = millis();
    while (digitalRead(BOOT_PIN) == LOW) {
      if (millis() - t > 5000) {
        Serial.println("[BOOT] Reset WiFi !");
        if (LittleFS.begin(false)) deleteWifiCreds();
        delay(500);
        ESP.restart();
      }
      delay(50);
    }
    Serial.println("[BOOT] Appui court — démarrage normal");
  }

  if (!LittleFS.begin(true)) {
    Serial.println("[FS] ERREUR LittleFS");
  } else {
    loadRelayConfig();
  }

  // ── Credentials WiFi ──────────────────────────────────────
  String wifiSSID, wifiPass;
  if (!loadWifiCreds(wifiSSID, wifiPass)) {
    runConfigPortal();  // bloquant
    return;
  }

  // ── Connexion STA ─────────────────────────────────────────
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  Serial.printf("[WiFi] Connexion à '%s'", wifiSSID.c_str());
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > WIFI_TIMEOUT_MS) {
      Serial.println("\n[WiFi] ÉCHEC — retour portail config");
      deleteWifiCreds();
      delay(500);
      ESP.restart();
    }
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] IP : %s\n", WiFi.localIP().toString().c_str());

  // ── NTP ───────────────────────────────────────────────────
  tryNtpSync();

  // ── WebSocket + routes HTTP ───────────────────────────────
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!req->authenticate(HTTP_USER, HTTP_PASS))
      return req->requestAuthentication("Relais Circulateur");
    req->send_P(200, "text/html", RELAY_PAGE);
  });

  server.onNotFound([](AsyncWebServerRequest *req) {
    if (!req->authenticate(HTTP_USER, HTTP_PASS))
      return req->requestAuthentication("Relais Circulateur");
    req->redirect("/");
  });

  server.begin();
  Serial.println("[HTTP] Serveur démarré — prêt");
}


// ─────────────────────────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────────────────────────
void loop() {
  // ── Bouton BOOT pendant le fonctionnement (5 s = reset) ───
  if (digitalRead(BOOT_PIN) == LOW) {
    if (!bootWasPressed) {
      bootWasPressed = true;
      bootPressStart = millis();
      Serial.println("[BOOT] Appui détecté...");
    } else if (millis() - bootPressStart > 5000) {
      Serial.println("[BOOT] 5 s — Reset WiFi !");
      ws.textAll("{\"reset\":true}");
      delay(500);
      deleteWifiCreds();
      ESP.restart();
    }
  } else {
    if (bootWasPressed) Serial.println("[BOOT] Relâché");
    bootWasPressed = false;
  }

  ws.cleanupClients();

  // ── NTP retry ─────────────────────────────────────────────
  if (!ntpSynced && millis() - lastNtpRetry >= NTP_RETRY_MS) {
    lastNtpRetry = millis();
    tryNtpSync();
  }

  // ── Relai + broadcast ─────────────────────────────────────
  struct tm ti;
  struct tm *tiPtr = nullptr;
  if (ntpSynced && getLocalTime(&ti, 50)) tiPtr = &ti;

  updateRelay(tiPtr);

  if (millis() - lastBroadcast >= BROADCAST_MS) {
    lastBroadcast = millis();
    broadcastState(tiPtr);
  }
}
