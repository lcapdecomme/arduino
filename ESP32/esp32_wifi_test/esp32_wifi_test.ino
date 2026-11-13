#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <soc/soc_caps.h>

const char* AP_SSID = "test_wifi";
const uint32_t TIMEOUT_MS = 15000;
const uint32_t MAC_TIMEOUT_MS = 10000;
const uint32_t LINK_HOLD_MS = 1000;
const uint32_t PAUSE_MS = 300;
const uint32_t SET_MAC_MAX_MS = 3000;

enum State { S_IDLE, S_CONNECTING, S_MAC_SET, S_MAC_TEST, S_MAC_RESTORE, S_RECONNECT, S_OK, S_FAIL };

WebServer server(80);
DNSServer dns;

State state = S_IDLE;
volatile bool evUp = false;
volatile uint32_t evUpAt = 0;
volatile bool evFail = false;
volatile uint8_t evReason = 0;

uint8_t lastReason = 0;
uint8_t tries = 0;
bool waiting = false;
uint32_t tWait = 0;
uint32_t tStep = 0;
uint32_t t0 = 0;

String targetSsid;
String targetPass;
String targetEnc;
bool pskOrOpen = false;
int targetRssi = 0;

uint8_t realMac[6];
bool realMacKnown = false;
uint8_t apBssid[6];
uint8_t apChannel = 0;

String infoRows;
String macVerdict;
bool macTested = false;
bool macAccepted = false;
uint8_t macReason = 0;

const char PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Test Wi-Fi sur ESP32</title>
<style>
body{font-family:sans-serif;max-width:420px;margin:20px auto;padding:0 14px}
input,button{width:100%;padding:12px;margin:6px 0;font-size:16px;box-sizing:border-box}
button{background:#0a7;color:#fff;border:0;border-radius:6px}
button:disabled{background:#999}
#res{margin-top:16px}
.ok{color:#080}.ko{color:#c00}
table{width:100%;border-collapse:collapse}
td{padding:6px;border-bottom:1px solid #ddd}
td:first-child{color:#666}
</style></head><body>
<h2>Test de connexion Wi-Fi</h2>
<form id="f">
<input name="ssid" list="nets" placeholder="SSID" required autocapitalize="off" autocorrect="off">
<datalist id="nets">%LISTE%</datalist>
<input name="pass" type="password" placeholder="Mot de passe">
<button>Tester</button>
</form>
<div id="res"></div>
<script>
const f=document.getElementById('f'),res=document.getElementById('res'),btn=f.querySelector('button');
const esc=s=>{const d=document.createElement('div');d.textContent=s;return d.innerHTML};
f.onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams(new FormData(f));
  btn.disabled=true;
  res.textContent='Connexion en cours...';
  try{await fetch('/connect',{method:'POST',body});}catch(x){}
  poll();
};
async function poll(){
  try{
    const j=await(await fetch('/status')).json();
    if(j.state==='connecting'){res.textContent=j.msg;setTimeout(poll,1000);return;}
    btn.disabled=false;
    if(j.state==='ok'){
      let h='<h3 class="ok">&#10004; Connexion réussie</h3><table>';
      for(const [k,v] of j.info)h+='<tr><td>'+esc(k)+'</td><td>'+esc(v)+'</td></tr>';
      res.innerHTML=h+'</table>';
    }else if(j.state==='fail'){
      res.innerHTML='<h3 class="ko">&#10008; Échec</h3>'+esc(j.msg);
    }else{
      res.textContent='';
    }
  }catch(x){setTimeout(poll,1000);}
}
</script></body></html>)rawliteral";

String htmlEsc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

String jsonEsc(const String& s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

String row(const char* k, const String& v) {
  return "[\"" + jsonEsc(k) + "\",\"" + jsonEsc(v) + "\"]";
}

String macStr(const uint8_t* m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

String encName(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN: return "Ouvert";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "Autre";
  }
}

String bandName(int channel) {
  String s = channel <= 14 ? "2,4 GHz" : "5 GHz";
#if !SOC_WIFI_SUPPORT_5G
  s += " (cet ESP32 ne voit pas le 5 GHz)";
#endif
  return s;
}

bool busy() {
  return state != S_IDLE && state != S_OK && state != S_FAIL;
}

bool wrongPassword(uint8_t r) {
  return r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT
      || (r == WIFI_REASON_AUTH_FAIL && !pskOrOpen);
}

// Refus explicite à l'authentification ou à l'association : le mot de passe n'a pas encore servi
bool rejectedEarly(uint8_t r) {
  return r == WIFI_REASON_ASSOC_TOOMANY || r == WIFI_REASON_ASSOC_FAIL
      || (r == WIFI_REASON_AUTH_FAIL && pskOrOpen);
}

// Pas de réponse du point d'accès, ou coupure sans motif précis
bool noAnswer(uint8_t r) {
  return r == WIFI_REASON_AUTH_EXPIRE
      || r == 4;  // délai d'association (nom différent selon la version du SDK)
}

String failMsg() {
  if (lastReason == 0) return "Délai dépassé (réseau injoignable ?)";
  if (lastReason == WIFI_REASON_NO_AP_FOUND) return "Réseau introuvable (SSID incorrect ou hors de portée)";
  if (wrongPassword(lastReason)) return "Authentification refusée (mot de passe incorrect ?)";
  if (rejectedEarly(lastReason)) {
    return "Le point d'accès a refusé l'ESP32 avant l'étape du mot de passe (code " + String(lastReason)
         + "). Cause probable : filtrage MAC (ou point d'accès saturé). MAC de l'ESP32 : " + WiFi.macAddress();
  }
  if (noAnswer(lastReason)) {
    String s = "Le point d'accès n'a pas donné suite à la demande de l'ESP32 (code " + String(lastReason);
    s += "). Causes possibles : filtrage MAC, mot de passe incorrect, signal trop faible";
    if (targetRssi) s += " (" + String(targetRssi) + " dBm)";
    return s + ". MAC de l'ESP32 : " + WiFi.macAddress();
  }
  return "Échec de connexion (code " + String(lastReason) + ")";
}

void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    evUpAt = millis();
    evUp = true;
  } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    uint8_t r = info.wifi_sta_disconnected.reason;
    if (r != WIFI_REASON_ASSOC_LEAVE && !evFail) {
      evReason = r;
      evFail = true;
    }
  }
}

// Coupe la station ; l'étape suivante part après PAUSE_MS
void restartSta() {
  WiFi.disconnect();
  waiting = true;
  tWait = millis();
}

void beginAttempt(bool sameAp) {
  evUp = false;
  evFail = false;
  evReason = 0;
  const char* pass = targetPass.length() ? targetPass.c_str() : nullptr;
  if (sameAp) WiFi.begin(targetSsid.c_str(), pass, apChannel, apBssid);
  else WiFi.begin(targetSsid.c_str(), pass);
  waiting = false;
  t0 = millis();
}

bool staMacIs(const uint8_t* mac) {
  uint8_t cur[6];
  return esp_wifi_get_mac(WIFI_IF_STA, cur) == ESP_OK && memcmp(cur, mac, 6) == 0;
}

esp_err_t setStaMac(const uint8_t* mac) {
  esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, mac);
  if (err == ESP_OK && !staMacIs(mac)) err = ESP_FAIL;
  return err;
}

void onConnected() {
  int channel = WiFi.channel();
  infoRows = row("SSID", WiFi.SSID()) + ",";
  infoRows += row("BSSID", WiFi.BSSIDstr()) + ",";
  infoRows += row("Canal", String(channel)) + ",";
  infoRows += row("Bande", bandName(channel)) + ",";
  infoRows += row("Signal", String(WiFi.RSSI()) + " dBm") + ",";
  infoRows += row("Sécurité", targetEnc) + ",";
  infoRows += row("IP de l'ESP32", WiFi.localIP().toString()) + ",";
  infoRows += row("Passerelle", WiFi.gatewayIP().toString()) + ",";
  infoRows += row("Masque", WiFi.subnetMask().toString()) + ",";
  infoRows += row("DNS", WiFi.dnsIP().toString()) + ",";
  infoRows += row("MAC de l'ESP32", WiFi.macAddress()) + ",";

  macTested = false;
  wifi_ap_record_t ap;
  if (!realMacKnown || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
    macVerdict = "test impossible";
    state = S_OK;
    return;
  }
  memcpy(apBssid, ap.bssid, 6);
  apChannel = ap.primary;
  state = S_MAC_SET;
  tStep = millis();
  restartSta();
}

void stepConnect() {
  if (waiting) { beginAttempt(false); return; }
  if (evUp && WiFi.status() == WL_CONNECTED) { onConnected(); return; }
  bool timeout = millis() - t0 > TIMEOUT_MS;
  if (!evFail && !timeout) return;

  uint8_t r = evFail ? evReason : 0;
  Serial.printf("Connexion, tentative %u : echec code %u\n", tries, r);
  if (tries == 1 && (rejectedEarly(r) || noAnswer(r))) {
    lastReason = r;
    tries = 2;
    restartSta();
    return;
  }
  if (tries == 1 || wrongPassword(r)) lastReason = r;
  state = S_FAIL;
  WiFi.disconnect();
}

// Test du filtrage : la station se reconnecte au même point d'accès avec une adresse MAC aléatoire
void stepMacSet() {
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = random(256);
  mac[0] = (mac[0] & 0xFE) | 0x02;
  esp_err_t err = setStaMac(mac);
  Serial.printf("MAC de test %s : err 0x%x\n", macStr(mac).c_str(), (unsigned)err);
  if (err == ESP_OK) {
    tries = 1;
    state = S_MAC_TEST;
    beginAttempt(true);
    return;
  }
  tWait = millis();
  if (millis() - tStep < SET_MAC_MAX_MS) return;
  macVerdict = "test impossible (erreur 0x" + String((unsigned long)err, HEX) + ")";
  state = S_MAC_RESTORE;
  tStep = millis();
}

void stepMacTest() {
  if (waiting) { beginAttempt(true); return; }
  // Acceptée = encore connectée LINK_HOLD_MS après l'association
  bool linked = evUp && !evFail && millis() - evUpAt >= LINK_HOLD_MS;
  bool timeout = !evUp && millis() - t0 > MAC_TIMEOUT_MS;
  if (!linked && !evFail && !timeout) return;

  uint8_t r = evFail ? evReason : 0;
  Serial.printf("MAC de test, tentative %u : %s (code %u)\n", tries, linked ? "acceptee" : "refusee", r);
  if (!linked && tries == 1) {
    tries = 2;
    restartSta();
    return;
  }
  macTested = true;
  macAccepted = linked;
  macReason = r;
  state = S_MAC_RESTORE;
  tStep = millis();
  restartSta();
}

void stepMacRestore() {
  if (!staMacIs(realMac)) {
    esp_err_t err = setStaMac(realMac);
    Serial.printf("MAC d'origine remise : err 0x%x\n", (unsigned)err);
    if (err != ESP_OK) {
      if (millis() - tStep > SET_MAC_MAX_MS) ESP.restart();
      tWait = millis();
      return;
    }
  }
  tries = 1;
  state = S_RECONNECT;
  beginAttempt(true);
}

void stepReconnect() {
  if (waiting) { beginAttempt(true); return; }
  bool up = evUp && WiFi.status() == WL_CONNECTED;
  bool timeout = millis() - t0 > TIMEOUT_MS;
  if (!up && !evFail && !timeout) return;

  Serial.printf("Reconnexion, tentative %u : %s\n", tries, up ? "ok" : "echec");
  if (!up && tries == 1) {
    tries = 2;
    restartSta();
    return;
  }
  if (!up) WiFi.disconnect();

  if (macTested) {
    if (macAccepted) macVerdict = "non détecté (une adresse MAC inconnue a pu se connecter)";
    else if (up) macVerdict = "détecté (adresse MAC inconnue refusée, code " + String(macReason) + " ; celle de l'ESP32 est autorisée)";
    else macVerdict = "indéterminé (adresse MAC inconnue refusée, mais la reconnexion de l'ESP32 a échoué)";
  }
  state = S_OK;
}

void handleRoot() {
  String opts;
  if (!busy()) {
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) opts += "<option value=\"" + htmlEsc(WiFi.SSID(i)) + "\">";
  }
  String page = FPSTR(PAGE);
  page.replace("%LISTE%", opts);
  server.send(200, "text/html; charset=utf-8", page);
}

void handleConnect() {
  if (busy()) {
    server.send(200, "text/plain", "busy");
    return;
  }
  targetSsid = server.arg("ssid");
  targetPass = server.arg("pass");
  targetEnc = "inconnue";
  pskOrOpen = false;
  targetRssi = 0;
  int n = WiFi.scanComplete();
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == targetSsid) {
      wifi_auth_mode_t m = WiFi.encryptionType(i);
      targetEnc = encName(m);
      pskOrOpen = m == WIFI_AUTH_OPEN || m == WIFI_AUTH_WPA_PSK || m == WIFI_AUTH_WPA2_PSK || m == WIFI_AUTH_WPA_WPA2_PSK;
      targetRssi = WiFi.RSSI(i);
      break;
    }
  }

  lastReason = 0;
  tries = 1;
  state = S_CONNECTING;
  restartSta();
  server.send(200, "text/plain", "ok");
}

void handleStatus() {
  String j;
  if (state == S_CONNECTING) {
    j = "{\"state\":\"connecting\",\"msg\":\"Connexion en cours...\"}";
  } else if (busy()) {
    j = "{\"state\":\"connecting\",\"msg\":\"Connexion réussie. Test du filtrage MAC...\"}";
  } else if (state == S_OK) {
    j = "{\"state\":\"ok\",\"info\":[" + infoRows + row("Filtrage MAC", macVerdict) + "]}";
  } else if (state == S_FAIL) {
    j = "{\"state\":\"fail\",\"msg\":\"" + jsonEsc(failMsg()) + "\"}";
  } else {
    j = "{\"state\":\"idle\"}";
  }
  server.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  realMacKnown = esp_wifi_get_mac(WIFI_IF_STA, realMac) == ESP_OK;

  IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);

  server.on("/", handleRoot);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/status", handleStatus);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.print("AP \"");
  Serial.print(AP_SSID);
  Serial.print("\" : http://");
  Serial.println(ip);
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  if (waiting && millis() - tWait < PAUSE_MS) return;
  switch (state) {
    case S_CONNECTING: stepConnect(); break;
    case S_MAC_SET: stepMacSet(); break;
    case S_MAC_TEST: stepMacTest(); break;
    case S_MAC_RESTORE: stepMacRestore(); break;
    case S_RECONNECT: stepReconnect(); break;
    default: break;
  }
}
