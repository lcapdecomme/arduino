/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  RÉCEPTEUR RF DUAL-BAND 434MHz + 868MHz POUR ESP32
 *  AVEC INTERFACE WEB TEMPS RÉEL + PORTAIL CAPTIF
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * MATÉRIEL REQUIS :
 * ─────────────────
 *   • ESP32 DevKit V1 (ou compatible)
 *   • Module récepteur Mipot 434 MHz
 *   • Module récepteur RX480E-868 FSK de QIACHIP
 * 
 * SCHÉMA DE BRANCHEMENT :
 * ───────────────────────
 * 
 *     ESP32                         Module Mipot 434MHz
 *     ─────                         ────────────────────
 *     3.3V  ────────────────────►  [Pin 12] +Vcc
 *     GND   ────────────────────►  [Pin 11] GND
 *     GPIO27 ───────────────────►  [Pin 14] DATA OUT
 *                                   Antenne 17cm sur Pin 3
 * 
 *     ESP32                         Module RX480E-868 (QIACHIP)
 *     ─────                         ───────────────────────────
 *     5V (VIN) ─────────────────►  [+V]   Alimentation (5-24V)
 *     GND   ────────────────────►  [GND]  Masse
 *     GPIO32 ───────────────────►  [D0]   Canal 0
 *     GPIO33 ───────────────────►  [D1]   Canal 1
 *     GPIO25 ───────────────────►  [D2]   Canal 2
 *     GPIO26 ───────────────────►  [D3]   Canal 3
 *                                   Antenne 8.6cm sur pad ANT
 * 
 * ⚠️  NOTE : Le RX480E-868 a un décodeur intégré. Il détecte les
 *    activations des canaux D0-D3 pour les télécommandes appairées.
 * 
 * BIBLIOTHÈQUES : rc-switch, ESPAsyncWebServer, AsyncTCP, ArduinoJson
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <RCSwitch.h>
#include <DNSServer.h>

// Configuration
#define AP_SSID     "RF_DualBand_Sniffer"
#define AP_PASSWORD "12345678"

#define RF_434_DATA_PIN   27
#define RF_868_D0_PIN     32
#define RF_868_D1_PIN     33
#define RF_868_D2_PIN     25
#define RF_868_D3_PIN     26

#define MAX_HISTORY   20
const byte DNS_PORT = 53;

enum RFBand { BAND_434MHZ, BAND_868MHZ };

struct RFFrame {
  unsigned long code;
  unsigned int bitLength;
  unsigned int protocol;
  unsigned int pulseLength;
  String binaryCode;
  String timestamp;
  unsigned long rawTimestamp;
  RFBand band;
  int channel;
};

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
RCSwitch rfReceiver434 = RCSwitch();

RFFrame frameHistory[MAX_HISTORY];
int historyIndex = 0;
int historyCount = 0;
unsigned long totalFrames434 = 0;
unsigned long totalFrames868 = 0;

unsigned long lastReceivedCode434 = 0;
unsigned long lastReceivedTime434 = 0;
#define DEBOUNCE_TIME_434 300


bool lastState868[4] = {false, false, false, false};
unsigned long lastTriggerTime868[4] = {0, 0, 0, 0};
#define DEBOUNCE_TIME_868 500

String getCurrentTime() {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;
  unsigned long hr = min / 60;
  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hr % 24, min % 60, sec % 60);
  return String(buffer);
}

String toBinaryString(unsigned long code, unsigned int bitLength) {
  String result = "";
  for (int i = bitLength - 1; i >= 0; i--) {
    result += ((code >> i) & 1) ? "1" : "0";
  }
  return result;
}

String bandToString(RFBand band) {
  return (band == BAND_434MHZ) ? "434" : "868";
}

// Page HTML incluse via PROGMEM pour économiser la RAM
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang='fr'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>Sniffer RF Dual-Band</title>
<style>
:root{--bg:#0a0e14;--bg2:#121820;--bg3:#1a2230;--grn:#00ff9d;--blu:#00d4ff;--org:#ff9500;--red:#ff4757;--pur:#a855f7;--txt:#e8eaed;--txt2:#8b949e;--brd:#2d3748}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',sans-serif;background:var(--bg);color:var(--txt);min-height:100vh}
.c{max-width:1200px;margin:0 auto;padding:1rem}
header{text-align:center;margin-bottom:2rem}
h1{font-size:2rem;background:linear-gradient(135deg,var(--org),var(--pur));-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{color:var(--txt2);margin-top:.5rem}
.badges{display:flex;justify-content:center;gap:1rem;margin-top:1rem}
.badge{padding:.3rem .8rem;border-radius:20px;font-size:.85rem;font-weight:600}
.b434{background:rgba(255,149,0,.2);color:var(--org);border:1px solid var(--org)}
.b868{background:rgba(168,85,247,.2);color:var(--pur);border:1px solid var(--pur)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:1rem;margin-bottom:1.5rem}
.card{background:var(--bg2);border:1px solid var(--brd);border-radius:12px;padding:1rem}
.card.org{border-top:3px solid var(--org)}.card.pur{border-top:3px solid var(--pur)}.card.blu{border-top:3px solid var(--blu)}.card.grn{border-top:3px solid var(--grn)}
.lbl{font-size:.7rem;color:var(--txt2);text-transform:uppercase;letter-spacing:.1em}
.val{font-family:monospace;font-size:1.4rem;font-weight:bold;margin-top:.25rem}
.val.org{color:var(--org)}.val.pur{color:var(--pur)}.val.blu{color:var(--blu)}.val.grn{color:var(--grn)}
.dot{width:10px;height:10px;border-radius:50%;background:var(--grn);animation:blink 1.5s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.4}}
.fbar{display:flex;gap:.5rem;margin-bottom:1rem;flex-wrap:wrap}
.fbtn{padding:.5rem 1rem;border:1px solid var(--brd);border-radius:20px;background:var(--bg2);color:var(--txt2);cursor:pointer;font-size:.85rem}
.fbtn:hover{border-color:var(--txt2)}.fbtn.act{background:var(--blu);color:#fff;border-color:var(--blu)}
.fbtn.act.f434{background:var(--org);border-color:var(--org)}.fbtn.act.f868{background:var(--pur);border-color:var(--pur)}
.ctrls{display:flex;gap:1rem;margin-bottom:1.5rem;flex-wrap:wrap}
.btn{font-size:.9rem;font-weight:600;padding:.7rem 1.2rem;border:none;border-radius:8px;cursor:pointer}
.btn-d{background:transparent;border:2px solid var(--red);color:var(--red)}.btn-d:hover{background:var(--red);color:var(--bg)}
.btn-p{background:linear-gradient(135deg,var(--org),var(--pur));color:#fff}
.sec{background:var(--bg2);border:1px solid var(--brd);border-radius:16px;overflow:hidden}
.shdr{display:flex;justify-content:space-between;align-items:center;padding:1rem;border-bottom:1px solid var(--brd);background:var(--bg3)}
.sttl{font-size:1.1rem;font-weight:600}
.fcnt{font-family:monospace;background:var(--bg);padding:.3rem .8rem;border-radius:20px;font-size:.8rem;color:var(--txt2)}
.flist{padding:1rem;max-height:600px;overflow-y:auto}
.frame{background:var(--bg3);border:1px solid var(--brd);border-radius:10px;padding:1rem;margin-bottom:.75rem;animation:slide .3s;border-left:4px solid var(--org)}
.frame.b868{border-left-color:var(--pur)}
@keyframes slide{from{opacity:0;transform:translateX(-20px)}to{opacity:1;transform:translateX(0)}}
.frame.new{box-shadow:0 0 20px rgba(0,255,157,.3)}
.fhdr{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:.75rem;flex-wrap:wrap;gap:.5rem}
.ftime{font-family:monospace;font-size:.85rem;color:var(--blu)}
.fmeta{display:flex;gap:.5rem;flex-wrap:wrap}
.mbdg{font-family:monospace;font-size:.7rem;padding:.2rem .5rem;border-radius:4px;background:var(--bg)}
.mbdg.m434{color:var(--org);border:1px solid var(--org)}.mbdg.m868{color:var(--pur);border:1px solid var(--pur)}
.mbdg.proto{color:var(--blu);border:1px solid var(--blu)}.mbdg.bits{color:var(--grn);border:1px solid var(--grn)}
.mbdg.pulse{color:#ffd700;border:1px solid #ffd700}.mbdg.ch{color:var(--red);border:1px solid var(--red)}
.fcode{margin-bottom:.75rem}
.clbl{font-size:.7rem;color:#5a6370;text-transform:uppercase;letter-spacing:.1em;margin-bottom:.2rem}
.cval{font-family:monospace;font-size:1.3rem;font-weight:bold;color:var(--org);word-break:break-all}
.cval.pur{color:var(--pur)}.cval.sm{font-size:.85rem;color:var(--txt2)}
.fbin{font-family:monospace;font-size:.8rem;background:var(--bg);padding:.75rem;border-radius:6px;word-break:break-all;letter-spacing:.15em}
.b1{color:var(--grn)}.b0{color:#5a6370}
.empty{text-align:center;padding:3rem 1rem;color:#5a6370}
.eico{font-size:3rem;margin-bottom:1rem;opacity:.3}
.info{background:rgba(168,85,247,.1);border:1px solid var(--pur);border-radius:8px;padding:.75rem 1rem;margin-bottom:1rem;font-size:.85rem;color:var(--txt2)}
.info strong{color:var(--pur)}
.conn{position:fixed;bottom:1rem;right:1rem;background:var(--bg2);border:1px solid var(--brd);border-radius:10px;padding:.75rem 1rem;display:flex;align-items:center;gap:.5rem;box-shadow:0 5px 20px rgba(0,0,0,.5);z-index:1000}
.wss{width:10px;height:10px;border-radius:50%;background:var(--red)}
.wss.on{background:var(--grn);box-shadow:0 0 10px var(--grn)}
</style>
</head>
<body>
<div class='c'>
<header>
<h1>RF Sniffer Dual-Band</h1>
<p class='sub'>Mipot 434MHz + RX480E-868 FSK</p>
<div class='badges'><span class='badge b434'>434 MHz</span><span class='badge b868'>868 MHz</span></div>
</header>
<div class='info'><strong>Note 868MHz:</strong> Le RX480E-868 detecte les activations des canaux D0-D3.</div>
<div class='grid'>
<div class='card org'><div class='lbl'>Trames 434MHz</div><div class='val org' id='t434'>0</div></div>
<div class='card pur'><div class='lbl'>Trames 868MHz</div><div class='val pur' id='t868'>0</div></div>
<div class='card blu'><div class='lbl'>Dernier code</div><div class='val blu' id='lcode'>--</div></div>
<div class='card grn'><div class='lbl'>Recepteurs</div><div style='display:flex;align-items:center;gap:.5rem;margin-top:.5rem'><div class='dot' id='rf'></div><span>Actifs</span></div></div>
</div>
<div class='fbar'>
<button class='fbtn act' onclick='flt("all")' id='fa'>Tous</button>
<button class='fbtn f434' onclick='flt("434")' id='f4'>434 MHz</button>
<button class='fbtn f868' onclick='flt("868")' id='f8'>868 MHz</button>
</div>
<div class='ctrls'>
<button class='btn btn-d' onclick='clr()'>Effacer</button>
<button class='btn btn-p' onclick='exp()'>Exporter JSON</button>
</div>
<div class='sec'>
<div class='shdr'><span class='sttl'>Historique</span><span class='fcnt' id='hcnt'>0/20</span></div>
<div class='flist' id='fl'><div class='empty'><div class='eico'>📻</div><p>En attente de signaux RF...</p></div></div>
</div>
</div>
<div class='conn'><div class='wss' id='ws'></div><span id='wst'>Connexion...</span></div>
<script>
var ws,fr=[],cf='all',st={t434:0,t868:0,lc:null};
function conn(){ws=new WebSocket('ws://192.168.4.1/ws');ws.onopen=function(){document.getElementById('ws').classList.add('on');document.getElementById('wst').textContent='Connecte'};ws.onclose=function(){document.getElementById('ws').classList.remove('on');document.getElementById('wst').textContent='Deconnecte';setTimeout(conn,3000)};ws.onmessage=function(e){try{hMsg(JSON.parse(e.data))}catch(x){}};}
function hMsg(d){if(d.type==='init'){st.t434=d.stats.total434;st.t868=d.stats.total868;fr=d.frames||[];uSt();uFl();}else if(d.type==='frame'){fr.unshift(d.frame);if(fr.length>20)fr.pop();st.t434=d.stats.total434;st.t868=d.stats.total868;st.lc=d.frame.code;uSt();aFr(d.frame,true);}else if(d.type==='clear'){fr=[];st={t434:0,t868:0,lc:null};uSt();uFl();}}
function flt(f){cf=f;document.querySelectorAll('.fbtn').forEach(b=>b.classList.remove('act','f434','f868'));if(f==='all')document.getElementById('fa').classList.add('act');else if(f==='434'){document.getElementById('f4').classList.add('act','f434');}else{document.getElementById('f8').classList.add('act','f868');}uFl();}
function uSt(){document.getElementById('t434').textContent=st.t434;document.getElementById('t868').textContent=st.t868;document.getElementById('lcode').textContent=st.lc||'--';document.getElementById('hcnt').textContent=gFr().length+'/20';}
function gFr(){return cf==='all'?fr:fr.filter(f=>f.band===cf);}
function fBin(b){if(!b)return'';var r='';for(var i=0;i<b.length;i++)r+=b[i]==='1'?'<span class="b1">1</span>':'<span class="b0">0</span>';return r;}
function uFl(){var c=document.getElementById('fl'),f=gFr();if(f.length===0){c.innerHTML='<div class="empty"><div class="eico">📻</div><p>En attente...</p></div>';return;}var h='';for(var i=0;i<f.length;i++)h+=cFr(f[i],false);c.innerHTML=h;}
function aFr(f,n){if(cf!=='all'&&f.band!==cf)return;var c=document.getElementById('fl');if(c.querySelector('.empty'))c.innerHTML='';c.insertAdjacentHTML('afterbegin',cFr(f,n));var g=gFr();while(c.children.length>g.length)c.removeChild(c.lastChild);document.getElementById('hcnt').textContent=g.length+'/20';if(n)setTimeout(function(){var e=c.firstElementChild;if(e)e.classList.remove('new');},2000);}
function cFr(f,n){var nc=n?' new':'',bc=f.band==='868'?' b868':'',cc=f.band==='868'?' pur':'',bb=f.band==='868'?'m868':'m434';var h='<div class="frame'+nc+bc+'">';h+='<div class="fhdr"><span class="ftime">'+f.time+'</span><div class="fmeta">';h+='<span class="mbdg '+bb+'">'+f.band+' MHz</span>';if(f.protocol>=0)h+='<span class="mbdg proto">P'+f.protocol+'</span>';if(f.bits>0)h+='<span class="mbdg bits">'+f.bits+'b</span>';if(f.pulse>0)h+='<span class="mbdg pulse">'+f.pulse+'us</span>';if(f.channel>=0)h+='<span class="mbdg ch">CH'+f.channel+'</span>';h+='</div></div>';h+='<div class="fcode"><div class="clbl">Code</div><div class="cval'+cc+'">'+f.code+'</div></div>';if(f.binary&&f.binary.length>0){var hx=parseInt(f.code).toString(16).toUpperCase();h+='<div class="fcode"><div class="clbl">Hex</div><div class="cval sm">0x'+hx+'</div></div>';h+='<div class="clbl">Binaire</div><div class="fbin">'+fBin(f.binary)+'</div>';}h+='</div>';return h;}
function clr(){if(confirm('Effacer?'))fetch('/clear',{method:'POST'}).then(r=>{if(r.ok){fr=[];st={t434:0,t868:0,lc:null};uSt();uFl();}});}
function exp(){var o={date:new Date().toISOString(),stats:st,frames:fr};var b=new Blob([JSON.stringify(o,null,2)],{type:'application/json'});var u=URL.createObjectURL(b);var a=document.createElement('a');a.href=u;a.download='rf_export.json';a.click();URL.revokeObjectURL(u);}
conn();
</script>
</body>
</html>
)rawliteral";

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connecte\n", client->id());
    
    JsonDocument doc;
    doc["type"] = "init";
    
    JsonObject stats = doc["stats"].to<JsonObject>();
    stats["total434"] = totalFrames434;
    stats["total868"] = totalFrames868;
    
    JsonArray framesArr = doc["frames"].to<JsonArray>();
    for (int i = 0; i < historyCount; i++) {
      int idx = (historyIndex - 1 - i + MAX_HISTORY) % MAX_HISTORY;
      RFFrame& f = frameHistory[idx];
      
      JsonObject frame = framesArr.add<JsonObject>();
      frame["time"] = f.timestamp;
      frame["code"] = String(f.code);
      frame["bits"] = f.bitLength;
      frame["protocol"] = f.protocol;
      frame["pulse"] = f.pulseLength;
      frame["binary"] = f.binaryCode;
      frame["band"] = bandToString(f.band);
      frame["channel"] = f.channel;
    }
    
    String output;
    serializeJson(doc, output);
    client->text(output);
    
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u deconnecte\n", client->id());
  }
}

void broadcastFrame(RFFrame& frame) {
  if (ws.count() == 0) return;
  
  JsonDocument doc;
  doc["type"] = "frame";
  
  JsonObject f = doc["frame"].to<JsonObject>();
  f["time"] = frame.timestamp;
  f["code"] = String(frame.code);
  f["bits"] = frame.bitLength;
  f["protocol"] = frame.protocol;
  f["pulse"] = frame.pulseLength;
  f["binary"] = frame.binaryCode;
  f["band"] = bandToString(frame.band);
  f["channel"] = frame.channel;
  
  JsonObject stats = doc["stats"].to<JsonObject>();
  stats["total434"] = totalFrames434;
  stats["total868"] = totalFrames868;
  
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

void storeFrame(RFFrame& frame) {
  frameHistory[historyIndex] = frame;
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
  if (historyCount < MAX_HISTORY) historyCount++;
}

void processFrame434() {
  unsigned long code = rfReceiver434.getReceivedValue();
  unsigned int bitLength = rfReceiver434.getReceivedBitlength();
  unsigned int protocol = rfReceiver434.getReceivedProtocol();
  unsigned int pulseLength = rfReceiver434.getReceivedDelay();
  
  unsigned long now = millis();
  if (code == lastReceivedCode434 && (now - lastReceivedTime434) < DEBOUNCE_TIME_434) {
    rfReceiver434.resetAvailable();
    return;
  }
  lastReceivedCode434 = code;
  lastReceivedTime434 = now;
  
  totalFrames434++;
  
  RFFrame frame;
  frame.code = code;
  frame.bitLength = bitLength;
  frame.protocol = protocol;
  frame.pulseLength = pulseLength;
  frame.binaryCode = toBinaryString(code, bitLength);
  frame.timestamp = getCurrentTime();
  frame.rawTimestamp = now;
  frame.band = BAND_434MHZ;
  frame.channel = -1;
  
  storeFrame(frame);
  
  Serial.println("\n═══════════════════════════════════");
  Serial.printf("📻 434 MHz [%s]\n", getCurrentTime().c_str());
  Serial.printf("   Code: %lu (0x%lX)\n", code, code);
  Serial.printf("   %u bits, Proto %u, %u us\n", bitLength, protocol, pulseLength);
  Serial.println("═══════════════════════════════════");
  
  broadcastFrame(frame);
  rfReceiver434.resetAvailable();
}

void processSignals868() {
  int pins[4] = {RF_868_D0_PIN, RF_868_D1_PIN, RF_868_D2_PIN, RF_868_D3_PIN};
  unsigned long now = millis();
  
  for (int ch = 0; ch < 4; ch++) {
    bool currentState = digitalRead(pins[ch]) == HIGH;
    
    if (currentState && !lastState868[ch]) {
      if ((now - lastTriggerTime868[ch]) > DEBOUNCE_TIME_868) {
        lastTriggerTime868[ch] = now;
        totalFrames868++;
        
        RFFrame frame;
        frame.code = ch;
        frame.bitLength = 0;
        frame.protocol = -1;
        frame.pulseLength = 0;
        frame.binaryCode = "";
        frame.timestamp = getCurrentTime();
        frame.rawTimestamp = now;
        frame.band = BAND_868MHZ;
        frame.channel = ch;
        
        storeFrame(frame);
        
        Serial.println("\n═══════════════════════════════════");
        Serial.printf("📡 868 MHz [%s]\n", getCurrentTime().c_str());
        Serial.printf("   Canal D%d active\n", ch);
        Serial.println("═══════════════════════════════════");
        
        broadcastFrame(frame);
      }
    }
    lastState868[ch] = currentState;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n═══════════════════════════════════════════════");
  Serial.println("   SNIFFER RF DUAL-BAND 434MHz + 868MHz");
  Serial.println("═══════════════════════════════════════════════");
  
  // Configuration 868MHz
  pinMode(RF_868_D0_PIN, INPUT);
  pinMode(RF_868_D1_PIN, INPUT);
  pinMode(RF_868_D2_PIN, INPUT);
  pinMode(RF_868_D3_PIN, INPUT);
  Serial.printf("[868MHz] D0=%d, D1=%d, D2=%d, D3=%d\n", 
                RF_868_D0_PIN, RF_868_D1_PIN, RF_868_D2_PIN, RF_868_D3_PIN);
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(1000);
  Serial.printf("[WiFi] SSID: %s, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  
  // DNS captive portal
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // 434MHz receiver
  rfReceiver434.enableReceive(digitalPinToInterrupt(RF_434_DATA_PIN));
  rfReceiver434.setReceiveTolerance(90);  // Défaut = 60, on augmente à 90
 //pinMode(RF_434_DATA_PIN, INPUT);
 // attachInterrupt(digitalPinToInterrupt(RF_434_DATA_PIN), handleRFInterrupt, CHANGE);
  Serial.printf("[434MHz] GPIO%d\n", RF_434_DATA_PIN);
  
  // Web server
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", HTML_PAGE);
  });
  
  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request) {
    historyIndex = 0;
    historyCount = 0;
    totalFrames434 = 0;
    totalFrames868 = 0;
    lastReceivedCode434 = 0;
    lastReceivedTime434 = 0;
    for (int i = 0; i < 4; i++) {
      lastState868[i] = false;
      lastTriggerTime868[i] = 0;
    }
    broadcastClear();
    request->send(200, "text/plain", "OK");
  });
  
  // Captive portal routes
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *r) { r->send(200, "text/plain", "success"); });
  server.onNotFound([](AsyncWebServerRequest *r) { r->redirect("http://192.168.4.1/"); });
  
  server.begin();
  
  Serial.println("\n═══════════════════════════════════════════════");
  Serial.println("   SYSTEME PRET");
  Serial.printf("   http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("═══════════════════════════════════════════════\n");
}

void loop() {
  dnsServer.processNextRequest();
  
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    Serial.printf("[DEBUG] GPIO%d = %d | Available: %d\n", 
                  RF_434_DATA_PIN, 
                  digitalRead(RF_434_DATA_PIN),
                  rfReceiver434.available());
    lastDebug = millis();
  }
  
  if (rfReceiver434.available()) {
    Serial.println(">>> SIGNAL DECODE !");
    processFrame434();
  }
  
  processSignals868();
  
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 1000) {
    ws.cleanupClients();
    lastCleanup = millis();
  }
  
  delay(1);
}
