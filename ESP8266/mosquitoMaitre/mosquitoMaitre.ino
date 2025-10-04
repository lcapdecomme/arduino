/* Mosquito Maitre
 * Projet pour esp8266 avec un mosfet IRLZ24N

Fonctionnalité
 * Démarre un réseau Wifi Mosquito
 * Propose en automatique une page de paramétrage de l'appareil 
 * Discute avec un ou plusieurs esclaves

Fonctionnement du programme maitre

- Crée un AP "MosquitoMaster"
  - Fournit une interface Web (page unique) qui :
      * contrôle marche/arrêt
      * règle vitesse 1..3
      * affiche le nombre d'esclaves connectés et leur état
  - Gère l'enregistrement des esclaves (/register) et le polling (/poll)


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

url de carte supplémentaire : 
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
http://arduino.esp8266.com/stable/package_esp8266com_index.json

*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <ArduinoJson.h>


#define AP_SSID "Mosquito"
#define AP_CHANNEL 1

// Pins (ajuste selon ton câblage)
const int FAN_PIN = 1; // PWM pin vers gate du MOSFET
const int LED_PIN = LED_BUILTIN; // indicateur local (ESP intégré), active low on many boards

ESP8266WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Etat du maitre
bool powerState = false;  // Off au lancement
int speed = 1; // 1..3
unsigned long commandId = 0; // increment on change

// Slave structure
struct SlaveInfo {
  String id;
  IPAddress ip;
  unsigned long lastSeen; // millis()
  bool paired;
  unsigned long lastCommandSeen; // Derniere commande connu de l'esclave
};

#define MAX_SLAVES 20
SlaveInfo slaves[MAX_SLAVES];
int slaveCount = 0;

const unsigned long slaveTimeout = 30000; // 30 secondes avant suppression


unsigned long SLAVE_TIMEOUT = 30UL * 1000UL; // 30s -> Considere hors ligne 



void saveSettings() {
  EEPROM.write(0, powerState ? 1 : 0);
  EEPROM.write(1, speed);
  EEPROM.commit();
  Serial.print("Sauve vitesse : ");
  Serial.println(speed);
}

void loadSettings() {
  powerState = EEPROM.read(0) == 1;
  speed = EEPROM.read(1);
  if (speed < 1 || speed > 3) speed = 3;
  Serial.print("Charge vitesse : ");
  Serial.println(speed);
}

// Retrouve un esclave à partir de son id
int findSlaveIndex(const String &id) {
  for (int i = 0; i < slaveCount; ++i) {
    if (slaves[i].id == id) return i;
  }
  return -1;
}

// Ajoute ou met à jour un esclave
void registerOrUpdateSlave(const String &id, IPAddress ip) {
  int idx = findSlaveIndex(id);
  if (idx >= 0) {
    slaves[idx].ip = ip;
    slaves[idx].lastSeen = millis();
    slaves[idx].paired = true;
  } else if (slaveCount < MAX_SLAVES) {
    slaves[slaveCount].id = id;
    slaves[slaveCount].ip = ip;
    slaves[slaveCount].lastSeen = millis();
    slaves[slaveCount].paired = true;
    slaves[slaveCount].lastCommandSeen = 0;
    slaveCount++;
  } // else ignore (table pleine)
}

// Remove timed-out slaves (or mark them unpaired)
void purgeStaleSlaves() {
  unsigned long now = millis();
  for (int i = 0; i < slaveCount; ++i) {
    if (now - slaves[i].lastSeen > SLAVE_TIMEOUT) {
      // mark unpaired but keep entry for info
      slaves[i].paired = false;
    }
  }
}

void applyFanState() {
  // Map speed to PWM duty (0..1023)
  int duty = 0;
  if (!powerState) {
    duty = 0;
  } else {
    switch (speed) {
      case 1: duty = 100; break; // vitesse basse
      case 2: duty = 150; break; // vitesse moyenne
      case 3: duty = 250; break; // vitesse haute
      default: duty = 150; break;
    }
  }
  analogWrite(FAN_PIN, duty);
}

// ----- Web handlers -----

// Serve main page
void handleRoot() {

 
  purgeStaleSlaves();

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Ventilateur ESP8266</title>
  <style>
  body { font-family: sans-serif; text-align: center; margin-top: 2em; }
  .btn { padding: 1em 2em; font-size: 1em; margin: 0.5em; border: none; border-radius: 10px; color: white; background-color: brown;}
  .on  { background-color: green; }
  .off { background-color: red; }
  .slider { -webkit-appearance: none; width: 80%; max-width: 400px; height: 30px; background-color: brown; border-radius: 10px; outline: none; margin-top: 1em; }
  .slider::-webkit-slider-thumb { width: 30px; height: 30px; background: #007bff; border-radius: 50%; cursor: pointer; }
  #status { font-size: 1.4em; margin: 0.5em; }
  img { margin-top: 2em; }
  </style>
  </head>
  <body>

  <h1>Contrôle Ventilation</h1>
  <div id='status'>État : <span id='power'></span></div>
  <button id='toggle' class='btn'>ON / OFF</button><br>
  <label for='speed'>Vitesse : <span id='valspeed'></span></label><br>
  <input class='slider' type='range' min='1' max='3' id='speed'>
  <br>
  <div class="status">
    <div style='display:none;'>CommandId: <span id="cmdId">0</span></div>
    <h3><span id="slaveCount"></span></h3>
    <div id="slaveList"></div>
  </div>
  <script>
    let lastStatus = 0;
    document.getElementById('toggle').addEventListener('click', async () => {
      await fetch('/command', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({action:'toggle'})});
      await refresh();
    });
    document.getElementById('speed').addEventListener('change', async (e) => {
      await fetch('/command', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({action:'speed', value: parseInt(e.target.value)})});
      await refresh();
    });
    
    async function refresh() {
      try {
        const res = await fetch('/status');
        const j = await res.json();
        document.getElementById('power').innerText = j.power ? 'Allumé' : 'Eteint';
        document.getElementById('cmdId').innerText = j.commandId;
        const sel = document.getElementById('speed');
        sel.value = j.speed;
        
        const valspeed = document.getElementById('valspeed');
        valspeed.innerHtml = j.speed;
        const list = document.getElementById('slaveList');
        list.innerHTML = '';
        var nbApp = 0;
        j.slaves.forEach(s => {
          const div = document.createElement('div');
          div.className = 'slave';
          div.innerText = s.id + ' - ' + s.ip + ' - ' + (s.paired ? 'connecté' : 'déconnecté') + ' - vu il y a ' + s.lastSeenSec;
          list.appendChild(div);
          if (s.paired) {
            nbApp=nbApp+1;
          }
        });
        
        if (nbApp ==0) {
            document.getElementById('slaveCount').innerText = "Aucun appareil connecté";
        }
        if (nbApp == 1) {
            document.getElementById('slaveCount').innerText = "1 appareil connecté";
        }
        if (nbApp > 1) {
            document.getElementById('slaveCount').innerText = nbApp+" appareils connectés";
        }
      } catch (e) {
        console.error(e);
      }
    }
    
    setInterval(refresh, 2000);
    refresh();
  </script>
  </body>
  </html>
  )rawliteral";
 
  server.send(200, "text/html", html);
}

// POST /command  { action: "toggle" }  OR { action:"speed", value:2 }
void handleCommand() {
  if (server.hasArg("plain") == false) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  const char* action = doc["action"];
  if (!action) {
    server.send(400, "application/json", "{\"error\":\"no action\"}");
    return;
  }
  String act(action);
  if (act == "toggle") {
    powerState = !powerState;
    commandId++;
    applyFanState();
    saveSettings();
    server.send(200, "application/json", "{\"result\":\"ok\"}");
    return;
  } else if (act == "speed") {
    int v = doc["value"] | speed;
    if (v < 1) v = 1;
    if (v > 3) v = 3;
    speed = v;
    commandId++;
    applyFanState();
    saveSettings();
    server.send(200, "application/json", "{\"result\":\"ok\"}");
    return;
  }
  server.send(400, "application/json", "{\"error\":\"unknown action\"}");
}

// Formatte les secondes en jours, heures, minutes et secondes
String formatDuration(unsigned long totalSeconds) {
  unsigned long days = totalSeconds / 86400; // 24*60*60
  totalSeconds %= 86400;
  unsigned long hours = totalSeconds / 3600;
  totalSeconds %= 3600;
  unsigned long minutes = totalSeconds / 60;
  unsigned long seconds = totalSeconds % 60;

  String result = "";
  if (days > 0) result += String(days) + "j ";
  if (hours > 0 || days > 0) result += String(hours) + "h ";
  if (minutes > 0 || hours > 0 || days > 0) result += String(minutes) + "m ";
  result += String(seconds) + "s";

  return result;
}


// GET /status  -> returns current master state + list of slaves
void handleStatus() {
  purgeStaleSlaves();
  StaticJsonDocument<1500> doc;
  doc["power"] = powerState;
  doc["speed"] = speed;
  doc["commandId"] = commandId;
  JsonArray arr = doc.createNestedArray("slaves");
  unsigned long now = millis();
  for (int i = 0; i < slaveCount; ++i) {
    JsonObject s = arr.createNestedObject();
    s["id"] = slaves[i].id;
    s["ip"] = slaves[i].ip.toString();
    s["paired"] = slaves[i].paired;
    s["lastSeenSec"] = formatDuration((now - slaves[i].lastSeen) / 1000);
    s["lastCmdSeen"] = slaves[i].lastCommandSeen;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST /register  body: { "id":"espXXXX" }
// Called by slave to register (pair)
void handleRegister() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"no body\"}");
    return;
  }
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }
  const char* id = doc["id"];
  if (!id) {
    server.send(400, "application/json", "{\"error\":\"no id\"}");
    return;
  }
  IPAddress remote = server.client().remoteIP();
  registerOrUpdateSlave(String(id), remote);
  // Respond with pairing success and current commandId + current state
  StaticJsonDocument<256> res;
  res["paired"] = true;
  res["commandId"] = commandId;
  res["power"] = powerState;
  res["speed"] = speed;
  String out;
  serializeJson(res, out);
  server.send(200, "application/json", out);
}

// GET /poll?id=espXXXX&last=123
// Slave asks whether there's a newer command than 'last'
void handlePoll() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json", "{\"error\":\"no id\"}");
    return;
  }
  String id = server.arg("id");
  unsigned long last = 0;
  if (server.hasArg("last")) last = server.arg("last").toInt();
  int idx = findSlaveIndex(id);
  if (idx >= 0) {
    // update lastSeen even if no new command
    slaves[idx].lastSeen = millis();
    slaves[idx].paired = true;
  }
  // If there's a new command since 'last', send it
  if (commandId > last) {
    StaticJsonDocument<256> res;
    res["commandId"] = commandId;
    res["power"] = powerState;
    res["speed"] = speed;
    String out;
    serializeJson(res, out);
    server.send(200, "application/json", out);
  } else {
    server.send(204, "application/json", ""); // no content
  }
}

void handleNotFound() {
  // Redirige toutes les requêtes vers la page d'accueil
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}


void setupAP() {
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID);
  delay(500);

  // Démarrage du DNS captif
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.onNotFound(handleNotFound);

}

void setup() {
  Serial.begin(115200);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off by default for builtin active-low


  // Charge les valeurs sauvegardés
  loadSettings();
  applyFanState();

  setupAP();
  
  server.on("/", handleRoot);
  server.on("/command", HTTP_POST, handleCommand);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/register", HTTP_POST, handleRegister);
  server.on("/poll", HTTP_GET, handlePoll);

  server.begin();
  Serial.println();
  Serial.print("AP started. IP: ");
  Serial.println(WiFi.softAPIP());
  delay(500);
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  purgeStaleSlaves();
  // no heavy work here
}
