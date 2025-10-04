/* Mosquito Esclave
 * Projet pour esp8266 avec un mosfet IRLZ24N

Fonctionnement du programme esclave

  Esclave ESP8266:
   - Se connecte au réseau WiFi "Mosquito" (softAP du maître)
   - S'enregistre auprès du maître (/register)
   - Interroge périodiquement /poll pour recevoir nouvelles commandes
   - Applique l'état (power/speed) sur une sortie PWM
   - Indique l'état d'appairage via une LED


ATTENTION : Installer la librairie ArduinoJson (par Benoît Blanchon).

Principe de branchement : 

IRLZ24N
=====
|   |
|   |
=====
| | |
G D S

Avec 
G = Gate
D = Drain
S = Source



+ alim 12V ------> fil rouge ( + ) ventilateur
fil noir ( - ) ventilateur --> DRAIN du IRF3708
SOURCE du IRF3708 ---------> GND(-) alim 12V + GND Arduino Nano/ESP32/ESP8266 (masse commune)
GATE du IRF3708 ---[220Ω]--> Pin PWM Nano (ex : D3, D5, D6, D9, D10 ou D11 pour ESP32 MAIS  TX pour ESP8266)
                    |
                    +--[10kΩ]--> GND (pull-down)


| Broche (NodeMCU) | GPIO n° (à utiliser) |
| ---------------- | -------------------- |
| D0               | GPIO16               |
| D1               | GPIO5                |
| D2               | GPIO4                |
| D3               | GPIO0                |
| D4               | GPIO2                |
| D5               | GPIO14               |
| D6               | GPIO12               |
| D7               | GPIO13               |
| D8               | GPIO15               |

   
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

const char* MASTER_SSID = "Mosquito";
// Mot de passe sur l'AP ?  mets-le ici ; sinon laisse vide.
const char* MASTER_PASS = "";

const char* MASTER_IP = "192.168.4.1"; // IP par défaut du softAP ESP8266
const int MASTER_PORT = 80;

const int FAN_PIN = 1; // PWM pin vers MOSFET
const int LED_PIN = LED_BUILTIN; // indicateur d'appairage (builtin active-low)

unsigned long POLL_INTERVAL = 3000; // ms entre polls
unsigned long REGISTER_RETRY_MS = 5000;

String chipIdStr() {
  uint32_t id = ESP.getChipId();
  char buf[12];
  sprintf(buf, "%08X", id);
  return String(buf);
}

bool paired = false;
unsigned long lastPoll = 0;
unsigned long lastRegisterAttempt = 0;
unsigned long lastCommandId = 0;
bool powerState = true;
int speed = 3;

void setLedPairing() {
  // blinking to show pairing attempt
  digitalWrite(LED_PIN, millis() % 500 < 250 ? LOW : HIGH); // builtin often active-low
}
void setLedPaired() {
  digitalWrite(LED_PIN, LOW); // on (active low)
}
void setLedUnpaired() {
  digitalWrite(LED_PIN, HIGH); // off
}

void applyFanState() {
  int duty = 0;
  if (!powerState) {
    duty = 0;
  } else {
    switch (speed) {
      case 1: duty = 80; break; // vitesse basse
      case 2: duty = 150; break; // vitesse moyenne
      case 3: duty = 250; break; // vitesse haute
      default: duty = 150; break;
    }
  }
  Serial.print("Vitesse : ");
  Serial.println(duty);
  analogWrite(FAN_PIN, duty);
}

bool registerToMaster() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://") + MASTER_IP + "/register";
  WiFiClient client;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<128> doc;
  doc["id"] = chipIdStr();
  String body;
  serializeJson(doc, body);
  int code = http.POST(body);
  if (code == 200) {
    String resp = http.getString();
    StaticJsonDocument<256> resDoc;
    if (deserializeJson(resDoc, resp) == DeserializationError::Ok) {
      paired = resDoc["paired"] | true;
      lastCommandId = resDoc["commandId"] | lastCommandId;
      powerState = resDoc["power"] | powerState;
      speed = resDoc["speed"] | speed;
      http.end();
      return true;
    }
  }
  http.end();
  return false;
}

bool pollMaster() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String url = String("http://") + MASTER_IP + "/poll?id=" + chipIdStr() + "&last=" + String(lastCommandId);
  WiFiClient client;
  http.begin(client, url);
  int code = http.GET();
  if (code == 200) {
    String resp = http.getString();
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      unsigned long cmd = doc["commandId"] | lastCommandId;
      if (cmd > lastCommandId) {
        lastCommandId = cmd;
        powerState = doc["power"] | powerState;
        speed = doc["speed"] | speed;
        applyFanState();
      }
    }
    http.end();
    return true;
  } else if (code == 204) {
    // no new command
    http.end();
    return true;
  } else {
    http.end();
    return false;
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  setLedUnpaired();

  WiFi.mode(WIFI_STA);
  WiFi.begin(MASTER_SSID, MASTER_PASS);
  Serial.print("Connecting to ");
  Serial.println(MASTER_SSID);
  unsigned long start = millis();
  // wait a few seconds for WiFi
  while (WiFi.status() != WL_CONNECTED && millis() - start < 7000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nImpossible de se connecter, retentative bientot");
  }
  // initial off
  applyFanState();
}

void loop() {
  unsigned long now = millis();

  // Si non connecté au wifi du maitre, on reessaye régulièrement
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    paired=false;
    setLedUnpaired();
    WiFi.begin(MASTER_SSID, MASTER_PASS);
    delay(500);
    setLedUnpaired();
    // skip rest until connected
    return;
  }

  // Si pas appairé, essaye régulierement et affiche la led apparaiement
  if (!paired) {
    if (now - lastRegisterAttempt > REGISTER_RETRY_MS) {
      lastRegisterAttempt = now;
      setLedPairing();
      if (registerToMaster()) {
        paired = true;
        setLedPaired();
        applyFanState();
      } else {
        paired = false;
        setLedUnpaired();
      }
    } else {
      setLedPairing();
    }
    return;
  }

  // Paired -> poll master periodically
  if (now - lastPoll >= POLL_INTERVAL) {
    lastPoll = now;
    bool ok = pollMaster();
    if (!ok) {
      // lost connection or master unreachable -> mark unpaired and try to re-register
      paired = false;
      WiFi.disconnect();
      setLedUnpaired();
    } else {
      setLedPaired();
    }
  }
}
