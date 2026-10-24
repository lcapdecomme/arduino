/*
 * Telerelais — Firmware ESP32 — version 0.4.0
 *
 * Pour la programmation, maintenir la touche I0O maintenu et appuyez brievement sur la touche EN (une impulsion) 
 * et lacher immediatement les deux boutons
 * 
 * Par contre; le réseau Telerelais ne sera visible une fois désalimenté et realimenté
 * (d'ailleurs a ce moment la led bleu se met à clignoter)
 * 
 * Une fois sur Telerelais, saisir le mot de passe mosquito
 *
 *
 * Compatible avec les sites de type 'schedule' (planning) et 'pulse' (impulsion).
 *
 * Nouveauté v0.4.0 : forçage physique local via un contact sec sur GPIO 14.
 *   - Contact fermé (2 fils reliés à GND) → relais forcé ON, priorité absolue
 *   - Contact ouvert → comportement normal (planning / forçage serveur / etc.)
 *   - L'état est remonté au serveur via le champ physical_override du heartbeat.
 *
 * Câblage du contact externe :
 *   - Un fil sur GPIO 14
 *   - Un fil sur GND
 *   - Le contact ferme quand les deux fils se touchent.
 *
 * Cycle de vie :
 *  1. Au boot, lecture de la config en NVS (Preferences).
 *  2. Si NVS vide → mode AP + portail captif → enrôlement dynamique.
 *  3. Sinon → connexion WiFi + heartbeat vers le serveur toutes les 5s.
 *  4. Appui long (> 5s) sur le bouton BOOT → wipe NVS + redémarrage.
 *  5. Réception d'une impulsion : GPIO relais HIGH pendant pulse_ms ms.
 *  6. Contact GPIO 14 fermé : relais forcé ON, priorité absolue.
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

const int PIN_RELAY          = 16;   // Relais sur cette carte AC-powered
const int PIN_STATUS_LED     = 23;   // LED bleue en haut à droite
const int PIN_BOOT_BUTTON    = 0;    // Bouton BOOT (IO0)
const int PIN_PHYSICAL_INPUT = 14;   // Contact sec externe (forçage physique)

const unsigned long HEARTBEAT_INTERVAL_MS   = 5000;
const unsigned long BOOT_LONG_PRESS_MS      = 5000;
const unsigned long PHYSICAL_POLL_MS        = 50;    // Lecture contact
const unsigned long PHYSICAL_DEBOUNCE_MS    = 100;   // Filtrage anti-rebond
const unsigned long AP_CAPTIVE_DNS_PORT     = 53;

const char* AP_SSID        = "Telerelais";
const char* AP_PASSWORD    = "mosquito";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

const char* FW_VERSION = "0.4.2";

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

// Forçage physique (contact GPIO 14)
bool          physicalOverride     = false;   // État confirmé (débouncé)
bool          physicalRawLast      = true;    // Dernière lecture brute (HIGH=ouvert)
unsigned long physicalStableSince  = 0;       // Depuis quand la lecture brute est stable
unsigned long lastPhysicalPoll     = 0;

// Consigne serveur (utilisée quand physicalOverride est off)
bool          serverDesiredOn = false;

Preferences   prefs;
WebServer     apServer(80);
DNSServer     dnsServer;

String   configSSID;
String   configPassword;
String   configDeviceToken;
String   configSiteName;
int32_t  lastPulseId = 0;


// =========================================================================
// LED d'état (inchangée)
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
// Relais et application de la consigne finale
// =========================================================================

void applyRelayState(bool on) {
  if (on != relayState) {
    Serial.printf("[RELAY] %s -> %s\n", relayState ? "ON" : "OFF", on ? "ON" : "OFF");
    relayState = on;
  }
  digitalWrite(PIN_RELAY, on ? HIGH : LOW);
}

// Calcule la consigne finale en tenant compte du forçage physique (prioritaire).
void applyEffectiveState() {
  bool effective = physicalOverride ? true : serverDesiredOn;
  applyRelayState(effective);
}


// =========================================================================
// Lecture du contact physique (GPIO 14) avec anti-rebond
// =========================================================================

void pollPhysicalInput() {
  unsigned long now = millis();
  if (now - lastPhysicalPoll < PHYSICAL_POLL_MS) return;
  lastPhysicalPoll = now;

  // INPUT_PULLUP : au repos HIGH, contact fermé LOW.
  bool rawClosed = (digitalRead(PIN_PHYSICAL_INPUT) == LOW);
  bool rawState  = !rawClosed;   // true si "ouvert" (repos)

  // Si la lecture brute change, on redémarre le compteur de stabilité.
  if (rawState != physicalRawLast) {
    physicalRawLast = rawState;
    physicalStableSince = now;
    return;
  }

  // Si la lecture brute est stable depuis assez longtemps ET différente de l'état
  // confirmé, on met à jour l'état confirmé (débouncé).
  if (now - physicalStableSince >= PHYSICAL_DEBOUNCE_MS) {
    bool newOverride = rawClosed;
    if (newOverride != physicalOverride) {
      physicalOverride = newOverride;
      Serial.printf("[PHYSICAL] contact %s -> forçage %s\n",
                    rawClosed ? "FERMÉ" : "OUVERT",
                    physicalOverride ? "ACTIF" : "inactif");
      // Applique immédiatement la nouvelle consigne effective, sans attendre
      // le prochain heartbeat pour une réactivité maximale.
      applyEffectiveState();
    }
  }
}


// =========================================================================
// NVS (Preferences) — inchangé
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
// Enrôlement HTTPS — inchangé
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
// Mode AP + portail captif — inchangé (HTML identique v0.3.0)
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
.pwd-wrap { position: relative; }
.pwd-wrap input { padding-right: 44px; }
.pwd-toggle { position: absolute; right: 8px; top: 50%; transform: translateY(-50%); background: none; border: 0; padding: 6px; margin: 0; width: auto; font-size: 18px; cursor: pointer; color: #6b7280; }
.pwd-toggle:hover { color: #111827; background: none; }
</style>
</head>
<body>
<div class="card">
<h1>Configuration Telerelais</h1>
<p class="sub">Renseignez les informations pour mettre en service cet appareil.</p>
<form id="f" method="POST" action="/save">
<label>Réseau WiFi</label>
<select name="ssid_select" id="ssid_select"></select>
<div class="hint">Si votre réseau n'apparaît pas, rechargez la page ou cochez « réseau caché ».</div>
<label style="display:flex;align-items:center;gap:8px;font-weight:normal;margin-top:12px;cursor:pointer;">
  <input type="checkbox" id="hidden_toggle" style="width:auto;margin:0;"> Réseau caché (saisie manuelle)
</label>
<div id="ssid_manual_wrap" style="display:none;">
  <label>Nom du réseau (SSID)</label>
  <input type="text" name="ssid_manual" id="ssid_manual" maxlength="32" autocomplete="off">
</div>
<label>Mot de passe WiFi</label>
<div class="pwd-wrap">
<input type="password" name="password" id="password" required>
<button type="button" class="pwd-toggle" id="pwd_toggle" aria-label="Afficher le mot de passe">👁</button>
</div>
<label>Code d'enrôlement</label>
<input type="text" name="code" required placeholder="123456" inputmode="numeric" pattern="[0-9]*" maxlength="6">
<div class="hint">Code à 6 chiffres obtenu depuis le portail Telerelais.</div>
<button type="submit" id="btn">Valider</button>
</form>
<div id="msg"></div>
</div>
<script>
fetch('/scan').then(r => r.json()).then(nets => {
  var sel = document.getElementById('ssid_select');
  nets.forEach(function(n) {
    var opt = document.createElement('option');
    opt.value = n.ssid;
    opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)';
    sel.appendChild(opt);
  });
});
document.getElementById('hidden_toggle').onchange = function(e) {
  var hidden = e.target.checked;
  document.getElementById('ssid_manual_wrap').style.display = hidden ? 'block' : 'none';
  document.getElementById('ssid_select').disabled = hidden;
  document.getElementById('ssid_manual').required = hidden;
  document.getElementById('ssid_select').required = !hidden;
};
document.getElementById('ssid_select').required = true;
document.getElementById('pwd_toggle').onclick = function() {
  var pwd = document.getElementById('password');
  var btn = document.getElementById('pwd_toggle');
  if (pwd.type === 'password') {
    pwd.type = 'text';
    btn.textContent = '🙈';
  } else {
    pwd.type = 'password';
    btn.textContent = '👁';
  }
};
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
  String ssidSelect = apServer.arg("ssid_select");
  String ssidManual = apServer.arg("ssid_manual");
  ssidSelect.trim();
  ssidManual.trim();
  String ssid = ssidManual.length() > 0 ? ssidManual : ssidSelect;
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
    pollPhysicalInput();   // Le contact physique reste actif même si WiFi down
    applyEffectiveState(); // Applique le forçage physique même hors ligne
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
// Heartbeat HTTPS — enrichi avec physical_override
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

  StaticJsonDocument<256> reqDoc;
  reqDoc["state"] = relayState ? "on" : "off";
  reqDoc["fw"]    = FW_VERSION;
  reqDoc["last_pulse_id"] = lastPulseId;
  reqDoc["physical_override"] = physicalOverride;
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

  // Récupère la consigne serveur (elle n'est appliquée que si physicalOverride est off,
  // ce que garantit applyEffectiveState).
  const char* desired = respDoc["desired"];
  if (desired) {
    serverDesiredOn = (strcmp(desired, "on") == 0);
    applyEffectiveState();
  }

  // Impulsion en attente ? (uniquement sites pulse)
  // Note : une impulsion pendant un forçage physique actif reste appliquée
  // — c'est un pulse ponctuel, il ne s'oppose pas au forçage.
  int32_t pulseId = respDoc["pulse_id"] | 0;
  int32_t pulseMs = respDoc["pulse_ms"] | 0;

  if (pulseId > 0 && pulseMs > 0 && pulseId > lastPulseId) {
    Serial.printf("[PULSE] id=%d duration=%dms\n", pulseId, pulseMs);
    digitalWrite(PIN_RELAY, HIGH);
    delay(pulseMs);
    digitalWrite(PIN_RELAY, LOW);
    savePulseId(pulseId);
    Serial.println("[PULSE] terminée");
    // Après l'impulsion, on remet l'état effectif normal
    applyEffectiveState();
  } else {
    Serial.printf("[HB] OK desired=%s override=%s\n",
                  desired ? desired : "?",
                  physicalOverride ? "1" : "0");
  }

  return true;
}


// =========================================================================
// Bouton BOOT — inchangé
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

  pinMode(PIN_RELAY,          OUTPUT);
  pinMode(PIN_STATUS_LED,     OUTPUT);
  pinMode(PIN_BOOT_BUTTON,    INPUT_PULLUP);
  pinMode(PIN_PHYSICAL_INPUT, INPUT_PULLUP);
  digitalWrite(PIN_RELAY,      LOW);
  digitalWrite(PIN_STATUS_LED, LOW);

  // Initialisation de l'état débouncé du contact physique
  physicalRawLast     = (digitalRead(PIN_PHYSICAL_INPUT) == HIGH);
  physicalOverride    = !physicalRawLast;
  physicalStableSince = millis();
  Serial.printf("[PHYSICAL] État initial : %s\n",
                physicalOverride ? "FORCÉ ON" : "normal");

  loadConfig();

  if (configSSID.length() == 0 || configDeviceToken.length() == 0) {
    Serial.println("[BOOT] Config manquante, démarrage en mode AP");
    startApMode();
  } else {
    connectWiFi();
    currentState = STATE_ONLINE_OK;
    applyEffectiveState();
  }
}

void loop() {
  updateStatusLed();
  checkBootButton();
  pollPhysicalInput();

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
