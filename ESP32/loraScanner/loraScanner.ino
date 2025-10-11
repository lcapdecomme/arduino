/* 
  ESP32 LoRa Scanner & Channel Listen + Web UI (responsive + WebSocket)
  - Compatible E32 / DX-LR02 (ex: E32-433T22D)
  - Fonctionnalités :
    * Sniffer (analyse multicanal)
    * Écoute sur un seul canal (en continu)
    * Mises à jour WebSocket en temps réel
    * Affichage des informations sur l'appareil
    * Historique stocké dans SPIFFS (history.txt)
    * Effacer l'historique / l'historique des téléchargements
  - Librairie : https://github.com/ESP32Async/ESPAsyncWebServer
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LoRa_E32.h"
#include "SPIFFS.h"

// ------------------------- CONFIG -------------------------
const char* AP_SSID = "ESP32_LoRa_Scanner";
const char* AP_PASS = "12345678";

// LoRa UART pins
#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4

HardwareSerial loraSerial(1);
LoRa_E32 e32(&loraSerial, LORA_AUX);

// Web server + WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// History file
const char *HISTORY_PATH = "/history.txt";
const size_t MAX_HISTORY_BYTES = 200 * 1024; // 200 KB max

// Channels to scan (subset by default). Pour scan complet utiliser 0..83.
const int channelsToScan[] = {0,5,10,15,20,23,25,30,35,40,45,50,55,60,65,70,75,80};
const int NUM_CHANNELS = sizeof(channelsToScan)/sizeof(channelsToScan[0]);

const unsigned long DWELL_MS = 700;       // temps d'écoute par canal (ms)
const unsigned long APPLY_DELAY_MS = 200; // délai après changement canal (ms)

// Flags & state
volatile bool scanRunning = false;
volatile bool listenMode = false;
volatile uint8_t listenChannel = 0;
volatile bool restoreOriginalCfg = true;

// original configuration (to restore after scan)
Configuration originalCfg;
bool originalCfgLoaded = false;

// storage for last messages (memory buffer to speed UI on connect)
#define MAX_IN_MEM 500
std::deque<String> memMessages; // need #include <deque> but not available; use vector circular
// we'll use a simple circular buffer
const int MEM_BUF = 200;
String memBuf[MEM_BUF];
int memIdx = 0;
int memCount = 0;

// ------------------------- UTIL -------------------------
String uptimeStamp() {
  unsigned long s = millis()/1000;
  unsigned long h = s/3600;
  unsigned long m = (s%3600)/60;
  unsigned long ss = s%60;
  char tmp[20];
  snprintf(tmp, sizeof(tmp), "%02lu:%02lu:%02lu", h,m,ss);
  return String(tmp);
}

void pushToMem(const String &s) {
  memBuf[memIdx] = s;
  memIdx = (memIdx + 1) % MEM_BUF;
  if (memCount < MEM_BUF) memCount++;
}

String getMemLine(int i) {
  // i = 0 -> oldest, i = memCount-1 -> newest
  if (i < 0 || i >= memCount) return String();
  int newest = (memIdx - 1 + MEM_BUF) % MEM_BUF;
  int idx = (newest - (memCount - 1 - i) + MEM_BUF) % MEM_BUF;
  return memBuf[idx];
}

// append to SPIFFS history
void appendHistory(const String &line) {
  File f = SPIFFS.open(HISTORY_PATH, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
  pushToMem(line);
}

// clear history file
void clearHistoryFile() {
  SPIFFS.remove(HISTORY_PATH);
  // recreate empty
  File f = SPIFFS.open(HISTORY_PATH, FILE_WRITE);
  if (f) f.close();
  memIdx = memCount = 0;
}

// read history into a String (careful with size)
String readHistoryAll() {
  if (!SPIFFS.exists(HISTORY_PATH)) return String();
  File f = SPIFFS.open(HISTORY_PATH, FILE_READ);
  if (!f) return String();
  String out;
  while (f.available()) {
    out += f.readStringUntil('\n');
    out += "\n";
    if (out.length() > MAX_HISTORY_BYTES) {
      out += "...truncated...\n";
      break;
    }
  }
  f.close();
  return out;
}

// send device info as JSON-like string
String getDeviceInfo() {
  // LoRa config: read channel
  uint8_t chan = 0;
  String loraInfo = "unknown";
  ResponseStructContainer rc = e32.getConfiguration();
  if (rc.status.code == 1) {
    Configuration cfg = *(Configuration*) rc.data;
    chan = cfg.CHAN;
    float freq = 410.0 + cfg.CHAN; // approx
    char buf[80];
    snprintf(buf, sizeof(buf), "CH=%u (~%.1f MHz) ADD=0x%02X%02X SPED=0x%02X OPTION=0x%02X",
             cfg.CHAN, freq, cfg.ADDH, cfg.ADDL, cfg.SPED, cfg.OPTION);
    loraInfo = String(buf);
    rc.close();
  } else {
    rc.close();
  }

  // ESP info
  char buf2[200];
  snprintf(buf2, sizeof(buf2),
           "{ \"esp_chip\": %u, \"cpu_mhz\": %u, \"free_heap\": %u, \"lora\": \"%s\" }",
           ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getFreeHeap(), loraInfo.c_str());
  return String(buf2);
}

// send ws message to clients
void wsBroadcast(const String &s) {
  ws.textAll(s);
}

// ------------------------- LoRa reception helpers -------------------------
void handleLoRaIncomingOnce(uint8_t chIdForLog = 255) {
  // chIdForLog: if 255, we don't prefix channel; else prefix [CHxx]
  if (e32.available() > 0) {
    ResponseContainer rc = e32.receiveMessage();
    if (rc.status.code == 1) {
      String raw = rc.data;
      String line;
      if (chIdForLog == 255) {
        line = "[" + uptimeStamp() + "] " + raw;
      } else {
        char tmp[40];
        snprintf(tmp, sizeof(tmp), "[CH%u] %s ", chIdForLog, uptimeStamp().c_str());
        line = String(tmp) + raw;
      }
      // store and broadcast
      appendHistory(line);
      wsBroadcast(line);
      Serial.println(line);
    }
    rc.close();
  }
}

// ------------------------- Scan task -------------------------
void scanChannelsOnce() {
  // ensure original cfg loaded
  if (!originalCfgLoaded) {
    ResponseStructContainer rc = e32.getConfiguration();
    if (rc.status.code == 1) {
      originalCfg = *(Configuration*) rc.data;
      originalCfgLoaded = true;
      rc.close();
    } else {
      rc.close();
      Serial.println("Impossible de lire config originale");
      return;
    }
  }

  scanRunning = true;
  String header = String("=== Scan start ") + uptimeStamp() + " ===";
  pushToMem(header); appendHistory(header); wsBroadcast(header);

  for (int i = 0; i < NUM_CHANNELS && scanRunning; ++i) {
    uint8_t ch = channelsToScan[i];
    // read fresh config
    ResponseStructContainer rc = e32.getConfiguration();
    if (rc.status.code != 1) { rc.close(); continue; }
    Configuration cfg = *(Configuration*) rc.data;
    rc.close();

    cfg.CHAN = ch;
    // write config
    ResponseStatus rs = e32.setConfiguration(cfg);
    if (rs.code != 1) {
      String err = String("Failed set CH") + String(ch) + " code=" + String(rs.code);
      pushToMem(err); appendHistory(err); wsBroadcast(err);
      continue;
    }
    delay(APPLY_DELAY_MS);

    // listen for DWELL_MS
    unsigned long deadline = millis() + DWELL_MS;
    while (millis() < deadline && scanRunning) {
      if (e32.available() > 0) {
        ResponseContainer rc2 = e32.receiveMessage();
        if (rc2.status.code == 1) {
          String raw = rc2.data;
          String line = String("[CH") + String(ch) + "] " + uptimeStamp() + " " + raw;
          appendHistory(line);
          wsBroadcast(line);
          Serial.println(line);
        }
        rc2.close();
      }
      delay(10);
    }
  }

  // restore original config if requested
  if (restoreOriginalCfg && originalCfgLoaded) {
    ResponseStatus rs2 = e32.setConfiguration(originalCfg);
    if (rs2.status.code == 1) {
      String done = String("Scan finished, restored channel ") + String(originalCfg.CHAN);
      pushToMem(done); appendHistory(done); wsBroadcast(done);
    } else {
      String fail = String("Scan finished, failed to restore (code ") + String(rs2.status.code) + ")";
      pushToMem(fail); appendHistory(fail); wsBroadcast(fail);
    }
  }
  scanRunning = false;
}

// wrapper to run scan in separate task
void startScanTask() {
  if (scanRunning) return;
  xTaskCreatePinnedToCore([](void*){
    scanChannelsOnce();
    vTaskDelete(NULL);
  }, "scanTask", 8192, NULL, 1, NULL, 1);
}

// ------------------------- Listen mode -------------------------
TaskHandle_t listenTaskHandle = NULL;

void listenTask(void *pvParameters) {
  uint8_t ch = *((uint8_t*) pvParameters);
  free(pvParameters);

  // set channel
  ResponseStructContainer rc = e32.getConfiguration();
  if (rc.status.code == 1) {
    Configuration cfg = *(Configuration*) rc.data;
    rc.close();
    cfg.CHAN = ch;
    ResponseStatus rs = e32.setConfiguration(cfg);
    if (rs.code != 1) {
      String err = String("Failed set listen CH") + String(ch);
      pushToMem(err); appendHistory(err); wsBroadcast(err);
      listenMode = false;
      vTaskDelete(NULL);
      return;
    }
    delay(APPLY_DELAY_MS);
  } else {
    rc.close();
    listenMode = false;
    vTaskDelete(NULL);
    return;
  }

  String start = String("Listening on CH") + String(ch) + " at " + uptimeStamp();
  pushToMem(start); appendHistory(start); wsBroadcast(start);

  while (listenMode) {
    if (e32.available() > 0) {
      ResponseContainer rc2 = e32.receiveMessage();
      if (rc2.status.code == 1) {
        String raw = rc2.data;
        String line = String("[CH") + String(ch) + "] " + uptimeStamp() + " " + raw;
        appendHistory(line);
        wsBroadcast(line);
        Serial.println(line);
      }
      rc2.close();
    }
    delay(10);
  }

  // optionally restore original config
  if (restoreOriginalCfg && originalCfgLoaded) {
    ResponseStatus rs2 = e32.setConfiguration(originalCfg);
    if (rs2.status.code == 1) {
      String done = String("Stopped listen, restored CH") + String(originalCfg.CHAN);
      pushToMem(done); appendHistory(done); wsBroadcast(done);
    }
  }

  vTaskDelete(NULL);
}

void startListen(uint8_t ch) {
  if (listenMode) return;
  listenMode = true;
  uint8_t *arg = (uint8_t*)malloc(sizeof(uint8_t));
  *arg = ch;
  xTaskCreatePinnedToCore(listenTask, "listenTask", 8192, arg, 1, &listenTaskHandle, 1);
}

void stopListen() {
  if (!listenMode) return;
  listenMode = false;
  // task will self-terminate
}

// ------------------------- Web routes & websocket -------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LoRa Scanner UI</title>
<style>
body{font-family:Arial,Helvetica,sans-serif;background:#0b1220;color:#e6f9f1;margin:0;padding:0}
.header{background:#093; color:#012; padding:10px 14px; font-weight:bold; display:flex; justify-content:space-between; align-items:center}
.header .info{font-size:13px}
.controls{padding:10px; display:flex; gap:8px; flex-wrap:wrap; justify-content:center}
.btn{background:#06c;border:none;padding:10px 14px;color:#fff;border-radius:6px;cursor:pointer}
.btn.warn{background:#f39c12}
.container{padding:10px}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
.card{background:#071326;padding:10px;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.4);width:100%}
#log{height:50vh;overflow:auto;padding:6px;background:#001219;border-radius:6px}
.channel-list{display:flex;gap:6px;flex-wrap:wrap}
.chbtn{background:#0aa;padding:6px 10px;border-radius:6px;border:none;color:#022;cursor:pointer}
.small{font-size:12px;color:#bcd}
.footer{padding:8px;text-align:center;color:#89b}
@media(min-width:700px){ .card{width:48%} }
</style>
</head>
<body>
<div class="header">
  <div><b>LoRa Scanner</b></div>
  <div class="info" id="devinfo">Loading...</div>
</div>

<div class="controls">
  <button class="btn" onclick="startSniff()">Sniffer (balayage)</button>
  <button class="btn" onclick="stopSniff()">Stop scan</button>
  <input id="chInput" placeholder="Canal (ex:23)" style="padding:8px;border-radius:6px;border:1px solid #034" />
  <button class="btn" onclick="startListen()">Écouter canal</button>
  <button class="btn warn" onclick="stopListen()">Stop écouter</button>
  <button class="btn" onclick="showHistory()">Historique</button>
  <button class="btn warn" onclick="clearHistory()">Effacer historique</button>
  <button class="btn" onclick="downloadHistory()">Télécharger</button>
</div>

<div class="container">
  <div class="row">
    <div class="card" style="flex:1">
      <h4>Journal (live)</h4>
      <div id="log"></div>
    </div>
    <div class="card" style="flex:1">
      <h4>Résumé canaux</h4>
      <div id="summary" class="small">Aucun scan en cours</div>
    </div>
  </div>
</div>

<div class="footer">Connecte-toi au Wi-Fi <b>ESP32_LoRa_Scanner</b> (pwd 12345678) puis ouvre http://192.168.4.1</div>

<script>
let ws = new WebSocket('ws://' + location.host + '/ws');
ws.onopen = function(){ console.log('ws open'); };
ws.onmessage = function(evt){
  let log = document.getElementById('log');
  let d = document.createElement('div');
  d.textContent = evt.data;
  d.style.padding='6px';
  d.style.borderBottom='1px solid rgba(255,255,255,0.03)';
  log.appendChild(d);
  log.scrollTop = log.scrollHeight;
};

function fetchJSON(url, cb) {
  fetch(url).then(r=>r.json()).then(cb).catch(e=>console.log(e));
}
function startSniff(){
  fetch('/startscan').then(()=>{ document.getElementById('summary').textContent = 'Scan en cours...'; });
}
function stopSniff(){ fetch('/stopscan').then(()=>{ document.getElementById('summary').textContent = 'Scan arrêté'; }); }
function startListen(){
  let ch = document.getElementById('chInput').value;
  fetch('/listen?ch=' + encodeURIComponent(ch));
}
function stopListen(){
  fetch('/stoplisten');
}
function showHistory(){
  fetch('/history').then(r=>r.text()).then(t=>{
    document.getElementById('log').innerHTML = '';
    t.split('\n').forEach(line => {
      if (line.trim().length) {
        let d = document.createElement('div');
        d.textContent = line;
        d.style.padding='6px';
        d.style.borderBottom='1px solid rgba(255,255,255,0.03)';
        document.getElementById('log').appendChild(d);
      }
    });
  });
}
function clearHistory(){
  fetch('/clearhistory').then(()=>{ document.getElementById('log').innerHTML=''; });
}
function downloadHistory(){
  window.location = '/downloadhistory';
}
function loadDeviceInfo(){
  fetch('/deviceinfo').then(r=>r.json()).then(j=>{
    document.getElementById('devinfo').textContent = 'CH:'+j.lora_chan + ' | ' + j.lora_freq + ' MHz | freeHeap:'+j.free_heap;
  });
}
setInterval(loadDeviceInfo,2000);
loadDeviceInfo();
</script>
</body>
</html>
)rawliteral";

// handlers
void handleRoot(AsyncWebServerRequest *req) {
  req->send_P(200, "text/html", index_html);
}

void handleStartScan(AsyncWebServerRequest *req) {
  if (!scanRunning) {
    startScanTask();
    req->send(200, "text/plain", "scan started");
  } else req->send(200, "text/plain", "already scanning");
}
void handleStopScan(AsyncWebServerRequest *req) {
  scanRunning = false;
  req->send(200, "text/plain", "scan stopped");
}
void handleListen(AsyncWebServerRequest *req) {
  if (!req->hasParam("ch")) { req->send(400, "text/plain", "missing ch"); return; }
  String chs = req->getParam("ch")->value();
  int ch = chs.toInt();
  if (ch < 0 || ch > 255) { req->send(400, "text/plain", "bad ch"); return; }
  if (listenMode) { req->send(200, "text/plain", "already listening"); return; }
  startListen((uint8_t)ch);
  req->send(200, "text/plain", "listen started");
}
void handleStopListen(AsyncWebServerRequest *req) {
  stopListen();
  req->send(200, "text/plain", "listen stopped");
}
void handleHistory(AsyncWebServerRequest *req) {
  String h = readHistoryAll();
  req->send(200, "text/plain", h);
}
void handleClearHistory(AsyncWebServerRequest *req) {
  clearHistoryFile();
  req->send(200, "text/plain", "cleared");
}
void handleDownloadHistory(AsyncWebServerRequest *req) {
  if (!SPIFFS.exists(HISTORY_PATH)) { req->send(404, "text/plain", "no history"); return; }
  req->send(SPIFFS, HISTORY_PATH, "text/plain", true);
}
void handleDeviceInfo(AsyncWebServerRequest *req) {
  // return JSON
  String dev = getDeviceInfo();
  // parse to json-like and send
  // we'll craft a small JSON
  // extract CH from getDeviceInfo earlier format
  ResponseStructContainer rc = e32.getConfiguration();
  uint8_t chan = 0;
  if (rc.status.code == 1) {
    Configuration cfg = *(Configuration*) rc.data;
    chan = cfg.CHAN; rc.close();
  } else rc.close();
  float freq = 410.0 + chan;
  DynamicJsonDocument doc(256);
  doc["lora_chan"] = chan;
  doc["lora_freq"] = freq;
  doc["free_heap"] = ESP.getFreeHeap();
  String out; serializeJson(doc, out);
  req->send(200, "application/json", out);
}

// WebSocket events
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client %u connected\n", client->id());
    // send last mem messages
    for (int i = 0; i < memCount; ++i) {
      client->text(getMemLine(i));
    }
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client %u disconnected\n", client->id());
  }
}

// ------------------------- SETUP & LOOP -------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // init SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    // ensure history file exists
    if (!SPIFFS.exists(HISTORY_PATH)) {
      File f = SPIFFS.open(HISTORY_PATH, FILE_WRITE);
      if (f) f.close();
    }
  }

  // init LoRa serial and module
  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e32.begin();
  Serial.println("LoRa E32 initialized");

  // read original cfg
  ResponseStructContainer rc = e32.getConfiguration();
  if (rc.status.code == 1) {
    originalCfg = *(Configuration*) rc.data;
    originalCfgLoaded = true;
    rc.close();
    Serial.printf("Original CH=%u\n", originalCfg.CHAN);
  } else rc.close();

  // WiFi AP
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP started: "); Serial.println(AP_SSID);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());

  // webserver
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/startscan", HTTP_GET, handleStartScan);
  server.on("/stopscan", HTTP_GET, handleStopScan);
  server.on("/listen", HTTP_GET, handleListen);
  server.on("/stoplisten", HTTP_GET, handleStopListen);
  server.on("/history", HTTP_GET, handleHistory);
  server.on("/clearhistory", HTTP_GET, handleClearHistory);
  server.on("/downloadhistory", HTTP_GET, handleDownloadHistory);
  server.on("/deviceinfo", HTTP_GET, handleDeviceInfo);
  server.begin();

  Serial.println("Web server started");
}

void loop() {
  ws.cleanupClients();

  // if not scanning / listening, still check for incoming frames on current channel
  if (!scanRunning && !listenMode) {
    if (e32.available() > 0) {
      ResponseContainer rc = e32.receiveMessage();
      if (rc.status.code == 1) {
        String raw = rc.data;
        String line = String("[CH?] ") + uptimeStamp() + " " + raw; // unknown channel (current module channel)
        appendHistory(line);
        wsBroadcast(line);
        Serial.println(line);
      }
      rc.close();
    }
  }
  delay(50);
}
