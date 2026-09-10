/*
 * Telerelais — Firmware ESP32 — version 0.3.0
 *
 * Compatible avec les sites de type 'schedule' (planning) et 'pulse' (impulsion).
 *
 * Cycle de vie :
 *  1. Au boot, lecture de la config en NVS (Preferences).
 *  2. Si NVS vide → mode AP + portail captif → enrôlement dynamique.
 *  3. Sinon → connexion WiFi + heartbeat vers le serveur toutes les 5s.
 *  4. Appui long (> 5s) sur le bouton BOOT → wipe NVS + redémarrage.
 *  5. Réception d'une impulsion (champs pulse_id et pulse_ms dans la réponse) :
 *     l'ESP32 met le GPIO à HIGH pendant pulse_ms ms, puis remet à LOW.
 *     last_pulse_id est persisté en NVS pour éviter les doublons après reboot.
 *
 * WiFi de configuration :
 *   SSID : Telerelais
 *   Password : mosquito
 *   URL portail : http://192.168.4.1/
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// =========================================================================
// CONFIG
// =========================================================================

const char* SERVER_HOST    = "relais.pythonanywhere.com";
const char* ENROLL_URL     = "https://relais.pythonanywhere.com/api/enroll";
const char* HEARTBEAT_URL  = "https://relais.pythonanywhere.com/api/heartbeat";

const int PIN_RELAY        = 4;
const int PIN_STATUS_LED   = 2;
const int PIN_BOOT_BUTTON  = 0;

const unsigned long HEARTBEAT_INTERVAL_MS   = 5000;
const unsigned long BOOT_LONG_PRESS_MS      = 5000;
const unsigned long AP_CAPTIVE_DNS_PORT     = 53;

const char* AP_SSID        = "Telerelais";
const char* AP_PASSWORD    = "mosquito";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const char* FW_VERSION = "0.3.0";

// =========================================================================
// État runtime
// =========================================================================

enum DeviceState {
  STATE_BOOT,
  STATE_AP_MODE,
  STATE_WIFI_CONNECTING,
  STATE_ONLINE_OK,
  STATE_ONLINE_ERROR
};

DeviceState   currentState    = STATE_BOOT;
bool          relayState      = false;
unsigned long lastHeartbeat   = 0;
unsigned long bootPressStart  = 0;

Preferences   prefs;
WebServer     apServer(80);
DNSServer     dnsServer;

String   configSSID;
String   configPassword;
String   configDeviceToken;
String   configSiteName;
int32_t  lastPulseId = 0;


// =========================================================================
// LED d'état
//   BOOT             : LED éteinte
//   AP_MODE          : double flash court (2 blinks toutes les secondes)
//   WIFI_CONNECTING  : blink rapide (200 ms)
//   ONLINE_OK        : LED allumée fixe
//   ONLINE_ERROR     : blink lent (1 s)
// =========================================================================

void updateStatusLed() {
  static unsigned long lastBlink = 0;
  static bool          ledOn     = false;
  unsigned long now = millis();

  switch (currentState) {
    case STATE_BOOT:
      digitalWrite(PIN_STATUS_LED, LOW);
      break;
    case STATE_AP_MODE: {
      unsigned long cycle = now % 1000;
      bool on = (cycle < 100) || (cycle >= 200 && cycle < 300);
      digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
      break;
    }
    case STATE_WIFI_CONNECTING:
      if (now - lastBlink > 200) {
        ledOn = !ledOn;
        digitalWrite(PIN_STATUS_LED, ledOn ? HIGH : LOW);
        lastBlink = now;
      }
      break;
    case STATE_ONLINE_OK:
      digitalWrite(PIN_STATUS_LED, HIGH);
      break;
    case STATE_ONLINE_ERROR:
      if (now - lastBlink > 1000) {
        ledOn = !ledOn;
        digitalWrite(PIN_STATUS_LED, ledOn ? HIGH : LOW);
        lastBlink = now;
      }
      break;
  }
}


// =========================================================================
// Relais
// =========================================================================

void applyRelayState(bool on) {
  if (on != relayState) {
    Serial.printf("[RELAY] %s -> %s\n", relayState ? "ON" : "OFF", on ? "ON" : "OFF");
    relayState = on;
  }
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}


// =========================================================================
// NVS (Preferences)
// =========================================================================

void loadConfig() {
  prefs.begin("telerelais", true);
  configSSID        = prefs.getString("ssid", "");
  configPassword    = prefs.getString("password", "");
  configDeviceToken = prefs.getString("token", "");
  configSiteName    = prefs.getString("site", "");
  lastPulseId       = prefs.getInt("pulse_id", 0);
  prefs.end();
  Serial.printf("[NVS] ssid='%s' token=%s site='%s' last_pulse_id=%d\n",
                configSSID.c_str(),
                configDeviceToken.length() ? "[present]" : "[vide]",
                configSiteName.c_str(),
                lastPulseId);
}

void saveConfig(const String& ssid, const String& password,
                const String& token, const String& siteName) {
  prefs.begin("telerelais", false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.putString("token", token);
  prefs.putString("site", siteName);
  prefs.putInt("pulse_id", 0);
  prefs.end();
  lastPulseId = 0;
  Serial.println("[NVS] Config sauvegardée");
}

void savePulseId(int32_t newId) {
  prefs.begin("telerelais", false);
  prefs.putInt("pulse_id", newId);
  prefs.end();
  lastPulseId = newId;
  Serial.printf("[NVS] last_pulse_id = %d\n", newId);
}

void wipeConfig() {
  prefs.begin("telerelais", false);
  prefs.clear();
  prefs.end();
  lastPulseId = 0;
  Serial.println("[NVS] Config effacée");
}


// =========================================================================
// Enrôlement HTTPS
// =========================================================================

bool enrollWithCode(const String& code, String& outToken, String& outSiteName, String& outError) {
  if (WiFi.status() != WL_CONNECTED) {
    outError = "Pas de connexion WiFi";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);

  if (!http.begin(client, ENROLL_URL)) {
    outError = "http.begin failed";
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<128> reqDoc;
  reqDoc["code"] = code;
  String reqBody;
  serializeJson(reqDoc, reqBody);

  int status = http.POST(reqBody);
  String respBody = http.getString();
  http.end();

  StaticJsonDocument<384> respDoc;
  DeserializationError err = deserializeJson(respDoc, respBody);
  if (err) {
    outError = String("Réponse invalide: ") + err.c_str();
    return false;
  }

  if (status != 200) {
    const char* msg = respDoc["error"];
    outError = msg ? String(msg) : String("HTTP ") + status;
    return false;
  }

  const char* token = respDoc["device_token"];
  const char* site  = respDoc["site_name"];
  if (!token) {
    outError = "Pas de device_token dans la réponse";
    return false;
  }

  outToken = String(token);
  outSiteName = site ? String(site) : String("");
  return true;
}


// =========================================================================
// Mode AP + portail captif
// =========================================================================

const char CAPTIVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Telerelais — Configuration</title>
<style>
body { font-family: -apple-system, system-ui, sans-serif; margin: 0; padding: 20px; background: #f9fafb; color: #111827; }
.card { max-width: 480px; margin: 0 auto; background: white; padding: 24px; border-radius: 12px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
h1 { font-size: 20px; margin: 0 0 8px; }
p.sub { color: #6b7280; font-size: 14px; margin: 0 0 20px; }
label { display: block; font-size: 13px; font-weight: 500; color: #374151; margin: 12px 0 4px; }
input, select { width: 100%; box-sizing: border-box; padding: 10px; border: 1px solid #d1d5db; border-radius: 6px; font-size: 15px; font-family: inherit; }
input:focus, select:focus { outline: none; border-color: #2563eb; box-shadow: 0 0 0 2px rgba(37,99,235,0.2); }
button { width: 100%; margin-top: 18px; padding: 12px; background: #2563eb; color: white; border: 0; border-radius: 6px; font-size: 16px; font-weight: 500; cursor: pointer; }
button:hover { background: #1d4ed8; }
button:disabled { background: #9ca3af; cursor: not-allowed; }
.err { margin-top: 16px; padding: 12px; background: #fee2e2; color: #991b1b; border-radius: 6px; font-size: 14px; }
.ok { margin-top: 16px; padding: 12px; background: #d1fae5; color: #065f46; border-radius: 6px; font-size: 14px; }
.hint { font-size: 12px; color: #6b7280; margin-top: 4px; }
</style>
</head>
<body>
<div class="card">
<h1>Configuration Telerelais</h1>
<p class="sub">Renseignez les informations pour mettre en service cet appareil.</p>
<form id="f" method="POST" action="/save">
<label>Réseau WiFi</label>
<select name="ssid" id="ssid" required></select>
<div class="hint">Si votre réseau n'apparaît pas, rechargez la page.</div>
<label>Mot de passe WiFi</label>
<input type="password" name="password" required>
<label>Code d'enrôlement</label>
<input type="text" name="code" required placeholder="123456" inputmode="numeric" pattern="[0-9]*" maxlength="6">
<div class="hint">Code à 6 chiffres obtenu depuis le portail Telerelais.</div>
<button type="submit" id="btn">Valider</button>
</form>
<div id="msg"></div>
</div>
<script>
fetch('/scan').then(r => r.json()).then(nets => {
  var sel = document.getElementById('ssid');
  nets.forEach(function(n) {
    var opt = document.createElement('option');
    opt.value = n.ssid;
    opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)';
    sel.appendChild(opt);
  });
});
document.getElementById('f').onsubmit = function(e) {
  e.preventDefault();
  var btn = document.getElementById('btn');
  btn.disabled = true;
  btn.textContent = 'Connexion en cours...';
  var fd = new FormData(e.target);
  fetch('/save', { method: 'POST', body: fd }).then(r => r.json()).then(resp => {
    var msg = document.getElementById('msg');
    if (resp.ok) {
      msg.className = 'ok';
      msg.textContent = 'Configuration réussie pour « ' + resp.site + ' ». L\'appareil va redémarrer.';
    } else {
      msg.className = 'err';
      msg.textContent = 'Erreur : ' + resp.error;
      btn.disabled = false;
      btn.textContent = 'Valider';
    }
  }).catch(err => {
    document.getElementById('msg').className = 'err';
    document.getElementById('msg').textContent = 'Erreur réseau.';
    btn.disabled = false;
    btn.textContent = 'Valider';
  });
};
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  apServer.send_P(200, "text/html", CAPTIVE_HTML);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = WiFi.SSID(i);
    o["rssi"] = WiFi.RSSI(i);
  }
  String out;
  serializeJson(doc, out);
  apServer.send(200, "application/json", out);
  WiFi.scanDelete();
}

void handleSave() {
  String ssid     = apServer.arg("ssid");
  String password = apServer.arg("password");
  String code     = apServer.arg("code");
  code.trim();

  Serial.printf("[AP] Tentative : ssid='%s' code='%s'\n", ssid.c_str(), code.c_str());

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(200);
  }

  if (WiFi.status() != WL_CONNECTED) {
    StaticJsonDocument<128> resp;
    resp["ok"] = false;
    resp["error"] = "Connexion WiFi impossible (SSID ou mot de passe ?)";
    String out; serializeJson(resp, out);
    apServer.send(200, "application/json", out);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    return;
  }

  String token, siteName, errMsg;
  if (!enrollWithCode(code, token, siteName, errMsg)) {
    StaticJsonDocument<256> resp;
    resp["ok"] = false;
    resp["error"] = errMsg;
    String out; serializeJson(resp, out);
    apServer.send(200, "application/json", out);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    return;
  }

  saveConfig(ssid, password, token, siteName);

  StaticJsonDocument<256> resp;
  resp["ok"] = true;
  resp["site"] = siteName;
  String out; serializeJson(resp, out);
  apServer.send(200, "application/json", out);

  delay(1500);
  ESP.restart();
}

void handleCaptivePortal() {
  apServer.sendHeader("Location", "http://192.168.4.1/", true);
  apServer.send(302, "text/plain", "");
}

void startApMode() {
  currentState = STATE_AP_MODE;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.printf("[AP] SSID='%s' password='%s' IP=%s\n",
                AP_SSID, AP_PASSWORD, WiFi.softAPIP().toString().c_str());

  dnsServer.start(AP_CAPTIVE_DNS_PORT, "*", AP_IP);

  apServer.on("/", handleRoot);
  apServer.on("/scan", handleScan);
  apServer.on("/save", HTTP_POST, handleSave);
  apServer.onNotFound(handleCaptivePortal);
  apServer.begin();
}

void loopApMode() {
  dnsServer.processNextRequest();
  apServer.handleClient();
}


// =========================================================================
// Connexion WiFi (mode normal)
// =========================================================================

void connectWiFi() {
  currentState = STATE_WIFI_CONNECTING;
  Serial.printf("[WIFI] Connexion à '%s'...\n", configSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(configSSID.c_str(), configPassword.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    updateStatusLed();
    checkBootButton();
    delay(100);
    if (millis() - start > 30000) {
      Serial.println("[WIFI] Timeout, retry");
      WiFi.disconnect();
      delay(500);
      WiFi.begin(configSSID.c_str(), configPassword.c_str());
      start = millis();
    }
  }
  Serial.print("[WIFI] Connecté, IP = ");
  Serial.println(WiFi.localIP());
}


// =========================================================================
// Heartbeat HTTPS — gère schedule ET pulse
// =========================================================================

bool sendHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(10000);

  if (!http.begin(client, HEARTBEAT_URL)) return false;

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + configDeviceToken);

  StaticJsonDocument<192> reqDoc;
  reqDoc["state"] = relayState ? "on" : "off";
  reqDoc["fw"]    = FW_VERSION;
  reqDoc["last_pulse_id"] = lastPulseId;
  String reqBody;
  serializeJson(reqDoc, reqBody);

  int status = http.POST(reqBody);
  if (status == 401) {
    Serial.println("[HB] Token rejeté (401), effacement de la config.");
    http.end();
    wipeConfig();
    delay(1000);
    ESP.restart();
    return false;
  }
  if (status != 200) {
    const char* msg = "";
    switch (status) {
      case -1:  msg = "(connexion refusée)"; break;
      case -11: msg = "(timeout)"; break;
      case -2:  msg = "(envoi échoué)"; break;
      case -3:  msg = "(connexion perdue)"; break;
    }
    Serial.printf("[HB] HTTP %d %s\n", status, msg);
    http.end();
    return false;
  }

  String respBody = http.getString();
  http.end();

  StaticJsonDocument<384> respDoc;
  if (deserializeJson(respDoc, respBody)) {
    Serial.println("[HB] JSON invalide");
    return false;
  }

  // 1. État désiré (toujours présent ; pour sites pulse, c'est "off")
  const char* desired = respDoc["desired"];
  if (desired) {
    applyRelayState(strcmp(desired, "on") == 0);
  }

  // 2. Impulsion ? (présent uniquement pour sites pulse avec demande en attente)
  int32_t pulseId = respDoc["pulse_id"] | 0;
  int32_t pulseMs = respDoc["pulse_ms"] | 0;

  if (pulseId > 0 && pulseMs > 0 && pulseId > lastPulseId) {
    Serial.printf("[PULSE] id=%d duration=%dms\n", pulseId, pulseMs);
    digitalWrite(PIN_RELAY, HIGH);
    delay(pulseMs);
    digitalWrite(PIN_RELAY, LOW);
    savePulseId(pulseId);
    Serial.println("[PULSE] terminée");
  } else {
    Serial.printf("[HB] OK desired=%s\n", desired ? desired : "?");
  }

  return true;
}


// =========================================================================
// Bouton BOOT (reset config si appui long)
// =========================================================================

void checkBootButton() {
  if (digitalRead(PIN_BOOT_BUTTON) == LOW) {
    if (bootPressStart == 0) {
      bootPressStart = millis();
    } else if (millis() - bootPressStart > BOOT_LONG_PRESS_MS) {
      Serial.println("[BOOT] Appui long détecté, wipe config + restart");
      wipeConfig();
      delay(500);
      ESP.restart();
    }
  } else {
    bootPressStart = 0;
  }
}


// =========================================================================
// setup / loop
// =========================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n\n=== Telerelais firmware %s ===\n", FW_VERSION);

  pinMode(PIN_RELAY,       OUTPUT);
  pinMode(PIN_STATUS_LED,  OUTPUT);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  digitalWrite(PIN_RELAY,      LOW);
  digitalWrite(PIN_STATUS_LED, LOW);

  loadConfig();

  if (configSSID.length() == 0 || configDeviceToken.length() == 0) {
    Serial.println("[BOOT] Config manquante, démarrage en mode AP");
    startApMode();
  } else {
    connectWiFi();
    currentState = STATE_ONLINE_OK;
  }
}

void loop() {
  updateStatusLed();
  checkBootButton();

  if (currentState == STATE_AP_MODE) {
    loopApMode();
    delay(10);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = millis();
    bool ok = sendHeartbeat();
    currentState = ok ? STATE_ONLINE_OK : STATE_ONLINE_ERROR;
  }

  delay(50);
}
