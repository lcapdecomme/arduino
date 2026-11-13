/*
 * Diagnostic Wi-Fi ESP32
 * Se connecter au réseau "test", ouvrir http://192.168.4.1, saisir le SSID et le mot de passe.
 * Le rapport s'affiche dans la page et sur le port série (115200).
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>

const char* AP_SSID   = "diag_wifi";
const char* TEST_HOST = "relais.pythonanywhere.com";
const char* TEST_URL  = "https://relais.pythonanywhere.com/login";
const char* HTTP_204  = "http://connectivitycheck.gstatic.com/generate_204";

const uint32_t ATTEMPT_TIMEOUT_MS = 20000;
const uint32_t QUIET_MS           = 5000;
const uint32_t PAUSE_MS           = 1500;
const int MAX_NETS      = 30;
const int MAX_CAND      = 3;
const int BASE_ATTEMPTS = 3;

enum Phase { P_IDLE, P_START, P_PAUSE, P_WAIT, P_NET, P_DONE };

struct Net {
  String  ssid;
  uint8_t bssid[6];
  int32_t ch;
  int32_t rssi;
  wifi_auth_mode_t auth;
};

WebServer server(80);
DNSServer dns;

Phase    phase = P_IDLE;
String   report;
String   targetSsid;
String   targetPass;
Net      nets[MAX_NETS];
int      nNets = 0;
int      cand[MAX_CAND];
int      nCand = 0;
int      attempt = 0;
int      okAttempt = -1;
uint32_t tPhase = 0;
uint8_t  seen[16];
int      nSeen = 0;
bool     dhcpStuck = false;
wifi_country_t origCountry;

volatile uint8_t  reasons[8];
volatile uint8_t  nReasons = 0;
volatile uint32_t lastEvt = 0;
volatile bool     assoc = false;

const char PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Diagnostic Wi-Fi ESP32</title>
<style>
body{font-family:sans-serif;max-width:560px;margin:20px auto;padding:0 14px}
input[type=text],input[type=password],button{width:100%;padding:12px;margin:6px 0;font-size:16px;box-sizing:border-box}
button{background:#0a7;color:#fff;border:0;border-radius:6px}
button:disabled{background:#999}
#copy{background:#555}
label{font-size:14px}
textarea{width:100%;height:60vh;font:12px monospace;box-sizing:border-box}
</style></head><body>
<h2>Diagnostic Wi-Fi</h2>
<form id="f">
<input type="text" name="ssid" placeholder="SSID (majuscules et minuscules exactes)" required autocapitalize="off" autocorrect="off" autocomplete="off" spellcheck="false">
<input type="password" name="pass" id="pass" placeholder="Mot de passe" autocapitalize="off" autocorrect="off" autocomplete="off">
<label><input type="checkbox" id="show"> Afficher le mot de passe</label>
<button id="go">Lancer le diagnostic</button>
</form>
<p id="st"></p>
<textarea id="out" readonly wrap="off"></textarea>
<button id="copy" type="button">Copier le rapport</button>
<script>
const f=document.getElementById('f'),out=document.getElementById('out'),st=document.getElementById('st'),go=document.getElementById('go');
const RUN="Diagnostic en cours, jusqu'à 2 minutes. Si le téléphone quitte le réseau test, s'y reconnecter et recharger la page.";
document.getElementById('show').onchange=e=>{document.getElementById('pass').type=e.target.checked?'text':'password'};
document.getElementById('copy').onclick=()=>{out.select();document.execCommand('copy')};
f.onsubmit=async e=>{
  e.preventDefault();
  go.disabled=true;
  st.textContent=RUN;
  try{await fetch('/run',{method:'POST',body:new URLSearchParams(new FormData(f))});}catch(x){}
  setTimeout(poll,1500);
};
async function poll(){
  try{
    const t=await(await fetch('/status')).text();
    const i=t.indexOf('\n');
    const s=t.slice(0,i);
    out.value=t.slice(i+1);
    if(s==='running'){go.disabled=true;st.textContent=RUN;setTimeout(poll,1500);return;}
    go.disabled=false;
    st.textContent=s==='done'?'Diagnostic terminé.':'';
  }catch(x){setTimeout(poll,1500);}
}
poll();
</script></body></html>)rawliteral";


void add(const String& s) {
  report += s;
  report += '\n';
  Serial.println(s);
}

String macStr(const uint8_t* m) {
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

String authName(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:            return "Ouvert";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-Entreprise";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/WPA3";
    default:                        return "Autre (" + String((int)m) + ")";
  }
}

String reasonText(uint8_t r) {
  switch (r) {
    case 2:   return "authentification sans réponse de la borne (filtrage MAC ou signal faible ?)";
    case 4:   return "association expirée";
    case 5:   return "borne saturée : trop d'appareils connectés";
    case 15:
    case 204: return "échange de clés interrompu (mot de passe incorrect le plus souvent)";
    case 23:  return "authentification 802.1X refusée (réseau Entreprise)";
    case 200: return "balises de la borne perdues (signal instable)";
    case 201: return "réseau introuvable (SSID inexact, 5 GHz seulement, canal non interrogé ou hors de portée)";
    case 202: return "authentification refusée par la borne (filtrage MAC possible)";
    case 203: return "association refusée par la borne (limite d'appareils ou filtrage possible)";
    case 205: return "échec de connexion";
    case 210: return "réseau trouvé mais sécurité incompatible";
    case 211: return "réseau trouvé mais sécurité inférieure au minimum exigé par l'ESP32";
    case 212: return "réseau trouvé mais signal trop faible";
    default:  return "voir la liste wifi_err_reason_t d'Espressif";
  }
}

String countryStr() {
  wifi_country_t c;
  if (esp_wifi_get_country(&c) != ESP_OK) return "inconnu";
  char cc[3] = { c.cc[0], c.cc[1], 0 };
  String s = String(cc) + ", canaux " + String(c.schan) + " à " + String(c.schan + c.nchan - 1);
  s += (c.policy == WIFI_COUNTRY_POLICY_AUTO) ? ", mode auto" : ", mode manuel";
  return s;
}

bool setCountryFR() {
  wifi_country_t c = origCountry;
  c.cc[0] = 'F';
  c.cc[1] = 'R';
  c.cc[2] = 0;
  c.schan = 1;
  c.nchan = 13;
  c.policy = WIFI_COUNTRY_POLICY_MANUAL;
  return esp_wifi_set_country(&c) == ESP_OK;
}

String boardInfo() {
  String s;
  s += "Carte : " + String(ESP.getChipModel()) + " rév. " + String(ESP.getChipRevision()) + "\n";
  s += "Core Arduino : " + String(ESP_ARDUINO_VERSION_MAJOR) + "." + String(ESP_ARDUINO_VERSION_MINOR) + "." + String(ESP_ARDUINO_VERSION_PATCH);
  s += ", IDF : " + String(ESP.getSdkVersion()) + "\n";
  s += "MAC de l'ESP32 vue par la borne : " + WiFi.macAddress() + "\n";
  s += "MAC du point d'accès \"" + String(AP_SSID) + "\" : " + WiFi.softAPmacAddress() + "\n";
  s += "Réglage pays Wi-Fi : " + countryStr();
  return s;
}

void onStaDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  uint8_t r = info.wifi_sta_disconnected.reason;
  assoc = false;
  if (phase != P_WAIT || r == WIFI_REASON_ASSOC_LEAVE) return;
  uint8_t n = nReasons;
  if (n < sizeof(reasons)) {
    reasons[n] = r;
    nReasons = n + 1;
  }
  lastEvt = millis();
}

void onStaConnected(arduino_event_id_t, arduino_event_info_t) {
  assoc = true;
}

void doScan() {
  add("--- Réseaux captés en 2,4 GHz (écoute passive, masqués inclus) ---");
  int n = WiFi.scanNetworks(false, true, true, 300);
  nNets = 0;
  nCand = 0;
  if (n < 0) {
    add("Échec du balayage (code " + String(n) + ")");
    n = 0;
  }
  for (int i = 0; i < n && nNets < MAX_NETS; i++) {
    Net& e = nets[nNets++];
    e.ssid = WiFi.SSID(i);
    memcpy(e.bssid, WiFi.BSSID(i), 6);
    e.ch = WiFi.channel(i);
    e.rssi = WiFi.RSSI(i);
    e.auth = WiFi.encryptionType(i);
  }
  WiFi.scanDelete();

  int hidden = 0;
  bool visible = false;
  for (int i = 0; i < nNets; i++) {
    if (nets[i].ssid.length() == 0) hidden++;
    if (nets[i].ssid == targetSsid) visible = true;
  }
  for (int i = 0; i < nNets && nCand < MAX_CAND; i++) {
    bool match = visible ? (nets[i].ssid == targetSsid) : (nets[i].ssid.length() == 0);
    if (match) cand[nCand++] = i;
  }

  for (int i = 0; i < nNets; i++) {
    bool isCand = false;
    for (int k = 0; k < nCand; k++) if (cand[k] == i) isCand = true;
    char b[96];
    snprintf(b, sizeof(b), "canal %2d  %4d dBm  %s  ", (int)nets[i].ch, (int)nets[i].rssi, macStr(nets[i].bssid).c_str());
    String line = String(b) + authName(nets[i].auth) + "  ";
    line += nets[i].ssid.length() ? nets[i].ssid : String("(masqué)");
    if (isCand) line += "  <- sera ciblé";
    add(line);
  }
  if (nNets == 0) add("Aucun réseau capté.");
  add("Réseaux masqués captés : " + String(hidden));
  add("\"" + targetSsid + "\" diffusé en clair : " + String(visible ? "oui" : "non"));
}

void beginRun() {
  report = "";
  nSeen = 0;
  dhcpStuck = false;
  okAttempt = -1;
  attempt = 0;
  WiFi.disconnect();
  esp_wifi_set_country(&origCountry);
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
  WiFi.setScanMethod(WIFI_FAST_SCAN);

  add(boardInfo());
  add("Réseau demandé : \"" + targetSsid + "\" (" + String(targetSsid.length()) + " caractères)");
  add("Mot de passe : " + String(targetPass.length()) + " caractères");
  add("");
  doScan();
  add("");
  add("--- Essais de connexion (réglages cumulés d'un essai au suivant) ---");
  tPhase = millis();
  phase = P_PAUSE;
}

void conclude() {
  esp_wifi_set_country(&origCountry);
  add("");
  add("--- Lecture ---");
  if (okAttempt == 0) {
    add("Connexion réussie avec les réglages du firmware : ce réseau accepte cette carte.");
    add("Refaire ce test sur la carte à relais, dont l'adresse MAC est différente.");
  } else if (okAttempt == 1) {
    add("Connexion réussie seulement avec les canaux 1 à 13 actifs.");
    add("Correctif firmware à prévoir : régler le pays Wi-Fi.");
  } else if (okAttempt == 2) {
    add("Connexion réussie seulement avec la sécurité minimale abaissée et le balayage complet.");
    add("Correctif firmware à prévoir sur ces deux réglages.");
  } else if (okAttempt >= BASE_ATTEMPTS) {
    add("Connexion réussie seulement en ciblant la borne par son adresse.");
    add("Correctif firmware à prévoir : connexion par BSSID et canal.");
  } else {
    add("Aucun essai n'a abouti.");
    if (nCand == 0) {
      add("Aucun réseau masqué ni \"" + targetSsid + "\" capté en 2,4 GHz :");
      add("réseau diffusé en 5 GHz seulement, radio 2,4 GHz coupée, ou borne hors de portée.");
    }
    if (dhcpStuck) add("La borne a accepté l'ESP32 mais aucune adresse IP n'a été attribuée : voir le DHCP.");
    for (int i = 0; i < nSeen; i++) add("Code " + String(seen[i]) + " : " + reasonText(seen[i]));
  }
}

void startAttempt() {
  if (attempt >= BASE_ATTEMPTS + nCand) {
    conclude();
    phase = P_DONE;
    return;
  }

  String label;
  const uint8_t* bssid = nullptr;
  int32_t ch = 0;

  if (attempt == 0) {
    label = "réglages d'origine, identiques au firmware";
  } else if (attempt == 1) {
    bool ok = setCountryFR();
    label = String("pays FR, canaux 1 à 13 actifs") + (ok ? "" : " (réglage refusé)");
  } else if (attempt == 2) {
    WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    label = "sécurité minimale abaissée à WPA, balayage de tous les canaux";
  } else {
    Net& c = nets[cand[attempt - BASE_ATTEMPTS]];
    bssid = c.bssid;
    ch = c.ch;
    label = "borne ciblée " + macStr(c.bssid) + ", canal " + String(c.ch);
  }

  add("Essai " + String(attempt + 1) + " : " + label);
  nReasons = 0;
  lastEvt = 0;
  assoc = false;
  phase = P_WAIT;
  tPhase = millis();
  WiFi.begin(targetSsid.c_str(), targetPass.length() ? targetPass.c_str() : nullptr, ch, bssid);
}

void checkAttempt() {
  uint32_t now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    add("  CONNECTÉ en " + String((now - tPhase) / 1000.0, 1) + " s");
    okAttempt = attempt;
    phase = P_NET;
    return;
  }

  bool quiet = nReasons > 0 && !assoc && (now - lastEvt > QUIET_MS);
  if (!quiet && now - tPhase <= ATTEMPT_TIMEOUT_MS) return;

  bool wasAssoc = assoc;
  uint8_t n = nReasons;
  phase = P_PAUSE;
  WiFi.disconnect();

  add("  ÉCHEC après " + String((now - tPhase) / 1000.0, 1) + " s");
  if (n == 0 && wasAssoc) {
    add("  accepté par la borne, mais aucune adresse IP reçue (DHCP)");
    dhcpStuck = true;
  } else if (n == 0) {
    add("  aucune réponse dans le délai");
  }
  for (uint8_t i = 0; i < n; i++) {
    uint8_t r = reasons[i];
    bool dup = false;
    for (uint8_t k = 0; k < i; k++) if (reasons[k] == r) dup = true;
    if (dup) continue;
    add("  code " + String(r) + " : " + reasonText(r));
    bool known = false;
    for (int k = 0; k < nSeen; k++) if (seen[k] == r) known = true;
    if (!known && nSeen < (int)sizeof(seen)) seen[nSeen++] = r;
  }

  attempt++;
  tPhase = millis();
}

void netTests() {
  add("");
  add("--- Connexion établie ---");
  add("SSID : " + WiFi.SSID());
  add("BSSID : " + WiFi.BSSIDstr());
  add("Canal : " + String(WiFi.channel()));
  add("Signal : " + String(WiFi.RSSI()) + " dBm");
  wifi_ap_record_t ap;
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) add("Sécurité : " + authName(ap.authmode));
  add("IP de l'ESP32 : " + WiFi.localIP().toString());
  add("Passerelle : " + WiFi.gatewayIP().toString());
  add("Masque : " + WiFi.subnetMask().toString());
  add("DNS : " + WiFi.dnsIP().toString());

  add("");
  add("--- Accès Internet ---");
  IPAddress ip;
  if (WiFi.hostByName(TEST_HOST, ip)) add("Résolution DNS : OK, " + String(TEST_HOST) + " = " + ip.toString());
  else add("Résolution DNS : ÉCHEC pour " + String(TEST_HOST));

  {
    WiFiClient c;
    HTTPClient h;
    h.setConnectTimeout(8000);
    h.setTimeout(8000);
    int code = h.begin(c, HTTP_204) ? h.GET() : -1;
    h.end();
    if (code == 204) add("HTTP : OK, pas de portail captif");
    else if (code > 0) add("HTTP : réponse " + String(code) + " au lieu de 204 (portail captif ou proxy ?)");
    else add("HTTP : ÉCHEC, " + HTTPClient::errorToString(code));
  }
  {
    WiFiClientSecure c;
    c.setInsecure();
    HTTPClient h;
    h.setConnectTimeout(10000);
    h.setTimeout(10000);
    int code = h.begin(c, TEST_URL) ? h.GET() : -1;
    h.end();
    if (code == 200) add("HTTPS vers le serveur Télérelais : OK");
    else if (code > 0) add("HTTPS vers le serveur Télérelais : réponse " + String(code) + " au lieu de 200");
    else add("HTTPS vers le serveur Télérelais : ÉCHEC, " + HTTPClient::errorToString(code));
  }

  conclude();
  WiFi.disconnect();
  add("");
  add("ESP32 déconnecté du réseau en fin de test.");
  phase = P_DONE;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handleRun() {
  if (phase != P_IDLE && phase != P_DONE) {
    server.send(409, "text/plain", "busy");
    return;
  }
  targetSsid = server.arg("ssid");
  targetPass = server.arg("pass");
  targetSsid.trim();
  server.send(200, "text/plain", "ok");
  phase = P_START;
}

void handleStatus() {
  String s;
  if (phase == P_IDLE) s = "idle\n" + boardInfo();
  else if (phase == P_DONE) s = "done\n" + report;
  else s = "running\n" + report;
  server.send(200, "text/plain; charset=utf-8", s);
}

void setup() {
  Serial.begin(115200);
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(onStaDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onStaConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  esp_wifi_get_country(&origCountry);

  IPAddress ip = WiFi.softAPIP();
  dns.start(53, "*", ip);

  server.on("/", handleRoot);
  server.on("/run", HTTP_POST, handleRun);
  server.on("/status", handleStatus);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.println();
  Serial.println(boardInfo());
  Serial.print("Point d'accès \"");
  Serial.print(AP_SSID);
  Serial.print("\" : http://");
  Serial.println(ip);
}

void loop() {
  dns.processNextRequest();
  server.handleClient();

  switch (phase) {
    case P_START: beginRun(); break;
    case P_PAUSE: if (millis() - tPhase >= PAUSE_MS) startAttempt(); break;
    case P_WAIT:  checkAttempt(); break;
    case P_NET:   netTests(); break;
    default: break;
  }
}
