/* Mosquito Esclave
 * Projet pour esp8266 avec un mosfet IRLZ24N

Fonctionnement du programme esclave

  Esclave ESP8266:
   - Se connecte au réseau WiFi "Mosquito" (softAP du maître)
   - S'enregistre auprès du maître (/register) pour recevoir nouvelles commandes
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
//const int LED_PIN = LED_BUILTIN; // indicateur d'appairage (builtin active-low)
const int LED_PIN = 4; // LED sur D2 (GPIO04) !!!!! 
 
const unsigned long CNX_INTERVAL = 15000UL; // intervalle de polling (ms)
const unsigned long WIFI_TIMEOUT = 8000UL;  // timeout pour WiFi.begin (ms)


String chipIdStr() {
  uint32_t id = ESP.getChipId();
  char buf[12];
  sprintf(buf, "%08X", id);
  return String(buf);
}

unsigned long lastCnx = 0;
unsigned long lastCommandId = 0;
bool powerState = true;
int speed = 3;
bool paired = false;

void setLedPairing() {
  // blinking to show pairing attempt
  digitalWrite(LED_PIN, millis() % 500 < 250 ? LOW : HIGH); // builtin often active-low
}
void setLedPaired() {
  digitalWrite(LED_PIN, HIGH); // on (active low)
}
void setLedUnpaired() {
  digitalWrite(LED_PIN, LOW); // off Attention c'est l'inverse sur LED_BUILTIN !!
}

void applyFanState() {
  int duty = 0;
  if (!powerState) {
    duty = 0;
  } else {
    switch (speed) {
      case 1: duty = 160; break; // vitesse basse
      case 2: duty = 200; break; // vitesse moyenne
      case 3: duty = 255; break; // vitesse haute
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

  // commence la connexion
  if (!http.begin(client, url)) {
    http.end();
    client.stop(); // assure la libération du socket
    return false;
  }
  
  // forcer la fermeture côté serveur pour éviter sockets KEEP-ALIVE
  http.addHeader("Connection", "close");
  // facultatif : user-agent ou autre header
  http.setTimeout(5000); // ms
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
      client.stop(); // assure la libération du socket
      return true;
    }
  }
  http.end();
  client.stop(); // assure la libération du socket

  return false;
}

void setup() {
  Serial.begin(38400);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  
  // Led éteinte
  setLedUnpaired();
  // Ventilateur eteint
  applyFanState();
  delay(1000);
}


bool connexionMaitre() {
  // Connexion WiFi (STA) au softAP du maître
  WiFi.mode(WIFI_STA);
  WiFi.begin(MASTER_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT) {
    delay(50);
    // tu peux ajouter un petit yield/WDOG reset si nécessaire
  }
  if (WiFi.status() != WL_CONNECTED) {
    // échec connexion
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return false;
  }
  return true;
}

void loop() {

    // jitter aléatoire pour éviter que tous les esclaves se connectent en même temps
    delay(random(0, 2000));

    if (connexionMaitre())
    {
        setLedPairing();
        // L'esclave essaye ensuite un enregistrement et demande l'état du ventilateur
        registerToMaster();
        applyFanState();   
        
        // bien fermer la connexion WiFi proprement
        WiFi.disconnect(true);     // true = supprime les informations de connexion
        WiFi.mode(WIFI_OFF);       // coupe interface pour libérer ressources
        delay(100);                // petit délai pour laisser le hardware nettoyer
    }
    
    setLedUnpaired();
    delay(CNX_INTERVAL);
}
