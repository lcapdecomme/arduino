/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  RÉCEPTEUR RF 434MHz MULTI-PROTOCOLES AVEC INTERFACE WEB
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * TÉLÉCOMMANDES SUPPORTÉES :
 * ──────────────────────────
 *   • Type A (Manchester) : ID 16 bits (ex: 0x5FAA) - 4 boutons
 *   • Type B (Manchester) : ID 4 bits (ex: 0x01) - 4 boutons  
 *   • Type C (Proto 6)    : ID 8 bits (ex: 0xD5) - 4 boutons
 * 
 * MATÉRIEL :
 * ──────────
 *   • ESP32 DevKit V1
 *   • Module récepteur Mipot 434 MHz sur GPIO27
 * 
 * BIBLIOTHÈQUES : ESPAsyncWebServer, AsyncTCP, ArduinoJson, DNSServer
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>

// ═══════════════════════════════════════════════════════════════════════════
// Configuration
// ═══════════════════════════════════════════════════════════════════════════
#define AP_SSID     "RF_Telecommandes"
#define AP_PASSWORD "12345678"
#define RF_DATA_PIN 27
#define MAX_HISTORY 30

const byte DNS_PORT = 53;

// ═══════════════════════════════════════════════════════════════════════════
// Types de télécommandes
// ═══════════════════════════════════════════════════════════════════════════
enum RemoteType { 
  TYPE_A_MANCHESTER,  // ID 16 bits (0x5FAA)
  TYPE_B_MANCHESTER,  // ID 4 bits (0x01)
  TYPE_C_PROTO6       // ID 8 bits (0xD5)
};

struct RFFrame {
  uint32_t remoteID;
  uint8_t button;
  String buttonName;
  RemoteType type;
  String typeName;
  uint64_t rawCode;
  String binaryCode;
  String timestamp;
  unsigned long rawTimestamp;
};

// ═══════════════════════════════════════════════════════════════════════════
// Variables globales
// ═══════════════════════════════════════════════════════════════════════════
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

RFFrame frameHistory[MAX_HISTORY];
int historyIndex = 0;
int historyCount = 0;
unsigned long totalFrames = 0;
unsigned long totalTypeA = 0;
unsigned long totalTypeB = 0;
unsigned long totalTypeC = 0;

// ═══════════════════════════════════════════════════════════════════════════
// Décodage RF - Variables
// ═══════════════════════════════════════════════════════════════════════════
#define SYNC_GAP_MIN  2500
#define RAW_BUFFER_SIZE 200

volatile unsigned int rawTimings[RAW_BUFFER_SIZE];
volatile int rawIndex = 0;
volatile unsigned long lastMicros = 0;
volatile bool frameReady = false;
volatile int frameLength = 0;

unsigned int processBuffer[RAW_BUFFER_SIZE];
int processLength = 0;
unsigned long lastDecodeTime = 0;
uint64_t lastManchesterCode = 0;
uint32_t lastProto6Code = 0;

// ═══════════════════════════════════════════════════════════════════════════
// Interruption RF
// ═══════════════════════════════════════════════════════════════════════════
void IRAM_ATTR handleRFInterrupt() {
  unsigned long now = micros();
  unsigned int duration = now - lastMicros;
  lastMicros = now;
  
  if (duration < 50) return;
  
  if (duration > SYNC_GAP_MIN) {
    if (rawIndex >= 20 && rawIndex <= 150 && !frameReady) {
      frameReady = true;
      frameLength = rawIndex;
    }
    rawIndex = 0;
    return;
  }
  
  if (rawIndex < RAW_BUFFER_SIZE && !frameReady) {
    rawTimings[rawIndex++] = duration;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Fonctions utilitaires
// ═══════════════════════════════════════════════════════════════════════════
String getCurrentTime() {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;
  unsigned long hr = min / 60;
  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hr % 24, min % 60, sec % 60);
  return String(buffer);
}

String toBinaryString(uint64_t code, int bitLength) {
  String result = "";
  for (int i = bitLength - 1; i >= 0; i--) {
    result += ((code >> i) & 1) ? "1" : "0";
  }
  return result;
}

const char* getManchesterButtonName(uint8_t btn) {
  switch(btn) {
    case 0: return "HAUT";
    case 1: return "GAUCHE";
    case 2: return "DROITE";
    case 3: return "BAS";
    default: return "?";
  }
}

const char* getProto6ButtonName(uint8_t btn) {
  switch(btn) {
    case 0xC: return "A";
    case 0xD: return "B";
    case 0xE: return "C";
    case 0xF: return "D";
    default: return "?";
  }
}

String typeToString(RemoteType type) {
  switch(type) {
    case TYPE_A_MANCHESTER: return "TypeA";
    case TYPE_B_MANCHESTER: return "TypeB";
    case TYPE_C_PROTO6: return "TypeC";
    default: return "?";
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Décodage Protocole 6 (12 bits)
// ═══════════════════════════════════════════════════════════════════════════
bool decodeProtocol6(uint32_t* code, uint8_t* bitLength) {
  if (processLength < 23 || processLength > 26) return false;
  
  int shortCount = 0, longCount = 0;
  for (int i = 0; i < processLength; i++) {
    unsigned int t = processBuffer[i];
    if (t >= 250 && t <= 450) shortCount++;
    else if (t >= 550 && t <= 750) longCount++;
  }
  
  if (shortCount < 5 || longCount < 5) return false;
  
  *code = 0;
  *bitLength = 0;
  
  for (int i = 0; i < processLength - 1 && *bitLength < 12; i += 2) {
    unsigned int t1 = processBuffer[i];
    unsigned int t2 = processBuffer[i + 1];
    
    if (t1 >= 550 && t1 <= 750 && t2 >= 250 && t2 <= 450) {
      *code = (*code << 1) | 1;
      (*bitLength)++;
    }
    else if (t1 >= 250 && t1 <= 450 && t2 >= 550 && t2 <= 750) {
      *code = (*code << 1);
      (*bitLength)++;
    }
    else if (t1 >= 250 && t1 <= 450 && t2 >= 250 && t2 <= 450) {
      *code = (*code << 1);
      (*bitLength)++;
    }
  }
  
  return (*bitLength >= 10);
}

// ═══════════════════════════════════════════════════════════════════════════
// Décodage Manchester (40 bits)
// ═══════════════════════════════════════════════════════════════════════════
bool decodeManchester(uint64_t* code, uint8_t* bitLength) {
  if (processLength < 78 || processLength > 86) return false;
  
  int validCount = 0;
  for (int i = 0; i < processLength; i++) {
    unsigned int t = processBuffer[i];
    if ((t >= 300 && t <= 520) || (t >= 650 && t <= 870)) {
      validCount++;
    }
  }
  if ((float)validCount / processLength < 0.90) return false;
  
  uint8_t rawBits[100];
  int rawBitCount = 0;
  
  for (int i = 0; i < processLength && rawBitCount < 100; i++) {
    rawBits[rawBitCount++] = (processBuffer[i] < 575) ? 1 : 0;
  }
  
  *code = 0;
  *bitLength = 0;
  
  for (int i = 1; i < rawBitCount - 1 && *bitLength < 40; i += 2) {
    if (rawBits[i] == 0 && rawBits[i+1] == 1) {
      *code = (*code << 1);
      (*bitLength)++;
    } else if (rawBits[i] == 1 && rawBits[i+1] == 0) {
      *code = (*code << 1) | 1;
      (*bitLength)++;
    }
  }
  
  return (*bitLength >= 38);
}

// ═══════════════════════════════════════════════════════════════════════════
// Stockage et broadcast
// ═══════════════════════════════════════════════════════════════════════════
void storeFrame(RFFrame& frame) {
  frameHistory[historyIndex] = frame;
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
  if (historyCount < MAX_HISTORY) historyCount++;
}

void broadcastFrame(RFFrame& frame) {
  if (ws.count() == 0) return;
  
  JsonDocument doc;
  doc["type"] = "frame";
  
  JsonObject f = doc["frame"].to<JsonObject>();
  f["time"] = frame.timestamp;
  f["remoteID"] = frame.remoteID;
  f["button"] = frame.button;
  f["buttonName"] = frame.buttonName;
  f["remoteType"] = typeToString(frame.type);
  f["typeName"] = frame.typeName;
  f["binary"] = frame.binaryCode;
  
  JsonObject stats = doc["stats"].to<JsonObject>();
  stats["total"] = totalFrames;
  stats["typeA"] = totalTypeA;
  stats["typeB"] = totalTypeB;
  stats["typeC"] = totalTypeC;
  
  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

void broadcastClear() {
  if (ws.count() == 0) return;
  JsonDocument doc;
  doc["type"] = "clear";
  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

// ═══════════════════════════════════════════════════════════════════════════
// Traitement des trames RF
// ═══════════════════════════════════════════════════════════════════════════
void processRFFrame() {
  unsigned long now = millis();
  if ((now - lastDecodeTime) < 150) return;
  
  // Essayer Protocole 6 (Type C)
  uint32_t proto6Code;
  uint8_t proto6Bits;
  
  if (decodeProtocol6(&proto6Code, &proto6Bits)) {
    if (proto6Code == lastProto6Code) return;
    lastProto6Code = proto6Code;
    lastDecodeTime = now;
    
    uint8_t remoteID = (proto6Code >> 4) & 0xFF;
    uint8_t button = proto6Code & 0x0F;
    
    totalFrames++;
    totalTypeC++;
    
    RFFrame frame;
    frame.remoteID = remoteID;
    frame.button = button;
    frame.buttonName = getProto6ButtonName(button);
    frame.type = TYPE_C_PROTO6;
    frame.typeName = "Proto6";
    frame.rawCode = proto6Code;
    frame.binaryCode = toBinaryString(proto6Code, proto6Bits);
    frame.timestamp = getCurrentTime();
    frame.rawTimestamp = now;
    
    storeFrame(frame);
    
    Serial.printf("\n📻 [%s] Type C (0x%02X) - Bouton %s\n", 
                  frame.timestamp.c_str(), remoteID, frame.buttonName.c_str());
    
    broadcastFrame(frame);
    return;
  }
  
  // Essayer Manchester (Type A et B)
  uint64_t manchesterCode;
  uint8_t manchesterBits;
  
  if (decodeManchester(&manchesterCode, &manchesterBits)) {
    uint8_t byte0 = (manchesterCode >> 32) & 0xFF;
    uint8_t byte1 = (manchesterCode >> 24) & 0xFF;
    uint8_t byte2 = (manchesterCode >> 16) & 0xFF;
    uint8_t byte4 = manchesterCode & 0xFF;
    
    uint8_t button = byte4 & 0x0F;
    
    if (button > 3) return;
    
    uint16_t prefix = ((uint16_t)byte0 << 8) | byte1;
    uint32_t remoteID;
    RemoteType type;
    String typeName;
    
    if (prefix != 0x0000) {
      remoteID = prefix;
      type = TYPE_A_MANCHESTER;
      typeName = "Manchester A";
    } else {
      remoteID = byte2 & 0x0F;
      type = TYPE_B_MANCHESTER;
      typeName = "Manchester B";
    }
    
    uint64_t signature = ((uint64_t)remoteID << 8) | button;
    static uint64_t lastSig = 0;
    if (signature == lastSig) return;
    lastSig = signature;
    lastDecodeTime = now;
    
    totalFrames++;
    if (type == TYPE_A_MANCHESTER) totalTypeA++;
    else totalTypeB++;
    
    RFFrame frame;
    frame.remoteID = remoteID;
    frame.button = button;
    frame.buttonName = getManchesterButtonName(button);
    frame.type = type;
    frame.typeName = typeName;
    frame.rawCode = manchesterCode;
    frame.binaryCode = toBinaryString(manchesterCode, 40);
    frame.timestamp = getCurrentTime();
    frame.rawTimestamp = now;
    
    storeFrame(frame);
    
    Serial.printf("\n🎯 [%s] %s (0x%04X) - Bouton %s\n", 
                  frame.timestamp.c_str(), typeName.c_str(), remoteID, frame.buttonName.c_str());
    
    broadcastFrame(frame);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Page HTML
// ═══════════════════════════════════════════════════════════════════════════
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='fr'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1.0'>
<title>RF Telecommandes</title>
<style>
:root{--bg:#0d1117;--bg2:#161b22;--bg3:#21262d;--grn:#3fb950;--blu:#58a6ff;--org:#f0883e;--red:#f85149;--pur:#a371f7;--yel:#d29922;--txt:#c9d1d9;--txt2:#8b949e;--brd:#30363d}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Helvetica,Arial,sans-serif;background:var(--bg);color:var(--txt);min-height:100vh}
.c{max-width:900px;margin:0 auto;padding:1rem}
header{text-align:center;padding:1.5rem 0;border-bottom:1px solid var(--brd);margin-bottom:1.5rem}
h1{font-size:1.8rem;font-weight:600;background:linear-gradient(135deg,var(--blu),var(--pur));-webkit-background-clip:text;-webkit-text-fill-color:transparent;margin-bottom:.5rem}
.sub{color:var(--txt2);font-size:.9rem}
.badges{display:flex;justify-content:center;gap:.5rem;margin-top:1rem;flex-wrap:wrap}
.badge{padding:.25rem .75rem;border-radius:2rem;font-size:.75rem;font-weight:600}
.bA{background:rgba(63,185,80,.15);color:var(--grn);border:1px solid var(--grn)}
.bB{background:rgba(88,166,255,.15);color:var(--blu);border:1px solid var(--blu)}
.bC{background:rgba(240,136,62,.15);color:var(--org);border:1px solid var(--org)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:.75rem;margin-bottom:1.5rem}
.card{background:var(--bg2);border:1px solid var(--brd);border-radius:10px;padding:1rem;text-align:center}
.card .lbl{font-size:.7rem;color:var(--txt2);text-transform:uppercase;letter-spacing:.05em}
.card .val{font-family:monospace;font-size:1.5rem;font-weight:700;margin-top:.25rem}
.card.grn .val{color:var(--grn)}.card.blu .val{color:var(--blu)}.card.org .val{color:var(--org)}.card.pur .val{color:var(--pur)}
.fbar{display:flex;gap:.5rem;margin-bottom:1rem;flex-wrap:wrap}
.fbtn{padding:.5rem 1rem;border:1px solid var(--brd);border-radius:2rem;background:var(--bg2);color:var(--txt2);cursor:pointer;font-size:.8rem;transition:all .2s}
.fbtn:hover{border-color:var(--txt2)}.fbtn.act{background:var(--blu);color:#fff;border-color:var(--blu)}
.ctrls{display:flex;gap:.75rem;margin-bottom:1rem;flex-wrap:wrap}
.btn{font-size:.85rem;font-weight:600;padding:.6rem 1.2rem;border:none;border-radius:8px;cursor:pointer;transition:all .2s}
.btn-d{background:transparent;border:1px solid var(--red);color:var(--red)}.btn-d:hover{background:var(--red);color:#fff}
.btn-p{background:var(--pur);color:#fff}.btn-p:hover{opacity:.9}
.sec{background:var(--bg2);border:1px solid var(--brd);border-radius:12px;overflow:hidden}
.shdr{display:flex;justify-content:space-between;align-items:center;padding:.75rem 1rem;background:var(--bg3);border-bottom:1px solid var(--brd)}
.sttl{font-weight:600;font-size:.95rem}
.fcnt{font-family:monospace;font-size:.75rem;color:var(--txt2);background:var(--bg);padding:.2rem .6rem;border-radius:1rem}
.flist{max-height:500px;overflow-y:auto;padding:.75rem}
.frame{background:var(--bg3);border:1px solid var(--brd);border-radius:8px;padding:.75rem;margin-bottom:.5rem;border-left:3px solid var(--grn);animation:slideIn .3s ease}
.frame.tA{border-left-color:var(--grn)}.frame.tB{border-left-color:var(--blu)}.frame.tC{border-left-color:var(--org)}
@keyframes slideIn{from{opacity:0;transform:translateY(-10px)}to{opacity:1;transform:translateY(0)}}
.frame.new{box-shadow:0 0 15px rgba(63,185,80,.3)}
.fhdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:.5rem;flex-wrap:wrap;gap:.5rem}
.ftime{font-family:monospace;font-size:.8rem;color:var(--blu)}
.fmeta{display:flex;gap:.4rem;flex-wrap:wrap}
.mtag{font-family:monospace;font-size:.65rem;padding:.15rem .4rem;border-radius:4px;background:var(--bg)}
.mtag.tA{color:var(--grn);border:1px solid var(--grn)}.mtag.tB{color:var(--blu);border:1px solid var(--blu)}.mtag.tC{color:var(--org);border:1px solid var(--org)}
.mtag.id{color:var(--pur);border:1px solid var(--pur)}.mtag.btn{color:var(--yel);border:1px solid var(--yel)}
.fbody{display:flex;align-items:center;gap:1rem;flex-wrap:wrap}
.fbtn-name{font-size:1.5rem;font-weight:700;min-width:80px}
.fbtn-name.tA{color:var(--grn)}.fbtn-name.tB{color:var(--blu)}.fbtn-name.tC{color:var(--org)}
.fbin{font-family:monospace;font-size:.7rem;color:var(--txt2);word-break:break-all;flex:1}
.empty{text-align:center;padding:3rem 1rem;color:var(--txt2)}
.eico{font-size:2.5rem;margin-bottom:.75rem;opacity:.5}
.conn{position:fixed;bottom:1rem;right:1rem;background:var(--bg2);border:1px solid var(--brd);border-radius:8px;padding:.5rem .75rem;display:flex;align-items:center;gap:.5rem;font-size:.8rem;box-shadow:0 4px 12px rgba(0,0,0,.3)}
.wsd{width:8px;height:8px;border-radius:50%;background:var(--red)}.wsd.on{background:var(--grn);box-shadow:0 0 8px var(--grn)}
</style>
</head>
<body>
<div class='c'>
<header>
<h1>📻 RF Telecommandes</h1>
<p class='sub'>Recepteur Multi-Protocoles 434MHz</p>
<div class='badges'>
<span class='badge bA'>Type A (0x5FAA)</span>
<span class='badge bB'>Type B (0x01)</span>
<span class='badge bC'>Type C (0xD5)</span>
</div>
</header>
<div class='grid'>
<div class='card grn'><div class='lbl'>Total</div><div class='val' id='st'>0</div></div>
<div class='card grn'><div class='lbl'>Type A</div><div class='val' id='sA'>0</div></div>
<div class='card blu'><div class='lbl'>Type B</div><div class='val' id='sB'>0</div></div>
<div class='card org'><div class='lbl'>Type C</div><div class='val' id='sC'>0</div></div>
</div>
<div class='fbar'>
<button class='fbtn act' onclick='flt("all")' id='fa'>Tous</button>
<button class='fbtn' onclick='flt("TypeA")' id='fA'>Type A</button>
<button class='fbtn' onclick='flt("TypeB")' id='fB'>Type B</button>
<button class='fbtn' onclick='flt("TypeC")' id='fC'>Type C</button>
</div>
<div class='ctrls'>
<button class='btn btn-d' onclick='clr()'>Effacer</button>
<button class='btn btn-p' onclick='exp()'>Exporter</button>
</div>
<div class='sec'>
<div class='shdr'><span class='sttl'>Historique</span><span class='fcnt' id='hc'>0/30</span></div>
<div class='flist' id='fl'><div class='empty'><div class='eico'>📻</div><p>En attente de signaux...</p></div></div>
</div>
</div>
<div class='conn'><div class='wsd' id='ws'></div><span id='wst'>Connexion...</span></div>
<script>
var ws,fr=[],cf='all',st={t:0,a:0,b:0,c:0};
function conn(){ws=new WebSocket('ws://'+location.host+'/ws');ws.onopen=function(){document.getElementById('ws').classList.add('on');document.getElementById('wst').textContent='Connecte'};ws.onclose=function(){document.getElementById('ws').classList.remove('on');document.getElementById('wst').textContent='Deconnecte';setTimeout(conn,3000)};ws.onmessage=function(e){try{hMsg(JSON.parse(e.data))}catch(x){}};}
function hMsg(d){if(d.type==='init'){st.t=d.stats.total;st.a=d.stats.typeA;st.b=d.stats.typeB;st.c=d.stats.typeC;fr=d.frames||[];uSt();uFl();}else if(d.type==='frame'){fr.unshift(d.frame);if(fr.length>30)fr.pop();st.t=d.stats.total;st.a=d.stats.typeA;st.b=d.stats.typeB;st.c=d.stats.typeC;uSt();aFr(d.frame,true);}else if(d.type==='clear'){fr=[];st={t:0,a:0,b:0,c:0};uSt();uFl();}}
function flt(f){cf=f;document.querySelectorAll('.fbtn').forEach(b=>b.classList.remove('act'));document.getElementById(f==='all'?'fa':f==='TypeA'?'fA':f==='TypeB'?'fB':'fC').classList.add('act');uFl();}
function uSt(){document.getElementById('st').textContent=st.t;document.getElementById('sA').textContent=st.a;document.getElementById('sB').textContent=st.b;document.getElementById('sC').textContent=st.c;document.getElementById('hc').textContent=gFr().length+'/30';}
function gFr(){return cf==='all'?fr:fr.filter(f=>f.remoteType===cf);}
function uFl(){var c=document.getElementById('fl'),f=gFr();if(!f.length){c.innerHTML='<div class="empty"><div class="eico">📻</div><p>En attente...</p></div>';return;}var h='';for(var i=0;i<f.length;i++)h+=cFr(f[i],false);c.innerHTML=h;}
function aFr(f,n){if(cf!=='all'&&f.remoteType!==cf)return;var c=document.getElementById('fl');if(c.querySelector('.empty'))c.innerHTML='';c.insertAdjacentHTML('afterbegin',cFr(f,n));while(c.children.length>30)c.removeChild(c.lastChild);document.getElementById('hc').textContent=gFr().length+'/30';if(n)setTimeout(function(){var e=c.firstElementChild;if(e)e.classList.remove('new');},1500);}
function cFr(f,n){var tc=f.remoteType==='TypeA'?'tA':f.remoteType==='TypeB'?'tB':'tC';var nc=n?' new':'';var id=f.remoteType==='TypeA'?'0x'+f.remoteID.toString(16).toUpperCase():f.remoteType==='TypeB'?'0x0'+f.remoteID.toString(16).toUpperCase():'0x'+f.remoteID.toString(16).toUpperCase();var h='<div class="frame '+tc+nc+'">';h+='<div class="fhdr"><span class="ftime">'+f.time+'</span><div class="fmeta">';h+='<span class="mtag '+tc+'">'+f.typeName+'</span>';h+='<span class="mtag id">ID:'+id+'</span>';h+='<span class="mtag btn">BTN:'+f.button+'</span></div></div>';h+='<div class="fbody"><div class="fbtn-name '+tc+'">'+f.buttonName+'</div>';h+='<div class="fbin">'+f.binary+'</div></div></div>';return h;}
function clr(){if(confirm('Effacer historique?'))fetch('/clear',{method:'POST'}).then(r=>{if(r.ok){fr=[];st={t:0,a:0,b:0,c:0};uSt();uFl();}});}
function exp(){var o={date:new Date().toISOString(),stats:st,frames:fr};var b=new Blob([JSON.stringify(o,null,2)],{type:'application/json'});var a=document.createElement('a');a.href=URL.createObjectURL(b);a.download='rf_export_'+Date.now()+'.json';a.click();}
conn();
</script>
</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════════════════════
// WebSocket Events
// ═══════════════════════════════════════════════════════════════════════════
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connecte\n", client->id());
    
    JsonDocument doc;
    doc["type"] = "init";
    
    JsonObject stats = doc["stats"].to<JsonObject>();
    stats["total"] = totalFrames;
    stats["typeA"] = totalTypeA;
    stats["typeB"] = totalTypeB;
    stats["typeC"] = totalTypeC;
    
    JsonArray framesArr = doc["frames"].to<JsonArray>();
    for (int i = 0; i < historyCount; i++) {
      int idx = (historyIndex - 1 - i + MAX_HISTORY) % MAX_HISTORY;
      RFFrame& f = frameHistory[idx];
      
      JsonObject frame = framesArr.add<JsonObject>();
      frame["time"] = f.timestamp;
      frame["remoteID"] = f.remoteID;
      frame["button"] = f.button;
      frame["buttonName"] = f.buttonName;
      frame["remoteType"] = typeToString(f.type);
      frame["typeName"] = f.typeName;
      frame["binary"] = f.binaryCode;
    }
    
    String output;
    serializeJson(doc, output);
    client->text(output);
    
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u deconnecte\n", client->id());
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// Setup
// ═══════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n═══════════════════════════════════════════════════════════");
  Serial.println("   RF TELECOMMANDES - Multi-Protocoles 434MHz");
  Serial.println("═══════════════════════════════════════════════════════════");
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);
  Serial.printf("[WiFi] SSID: %s\n", AP_SSID);
  Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
  
  // DNS captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("[DNS] Portail captif actif");
  
  // RF receiver
  pinMode(RF_DATA_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RF_DATA_PIN), handleRFInterrupt, CHANGE);
  Serial.printf("[RF] GPIO%d\n", RF_DATA_PIN);
  
  // Web server
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", HTML_PAGE);
  });
  
  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
    historyIndex = 0;
    historyCount = 0;
    totalFrames = 0;
    totalTypeA = 0;
    totalTypeB = 0;
    totalTypeC = 0;
    lastManchesterCode = 0;
    lastProto6Code = 0;
    broadcastClear();
    request->send(200, "text/plain", "OK");
  });
  
  // Captive portal routes
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *r) { r->send(200, "text/plain", "success"); });
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.onNotFound([](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  
  server.begin();
  
  Serial.println("\n═══════════════════════════════════════════════════════════");
  Serial.println("   SYSTEME PRET");
  Serial.printf("   Connectez-vous au WiFi '%s'\n", AP_SSID);
  Serial.printf("   Puis ouvrez http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("═══════════════════════════════════════════════════════════\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Loop
// ═══════════════════════════════════════════════════════════════════════════
void loop() {
  dnsServer.processNextRequest();
  
  if (frameReady) {
    noInterrupts();
    processLength = frameLength;
    memcpy(processBuffer, (void*)rawTimings, processLength * sizeof(unsigned int));
    frameReady = false;
    interrupts();
    
    processRFFrame();
  }
  
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 1000) {
    ws.cleanupClients();
    lastCleanup = millis();
  }
  
  delay(1);
}
