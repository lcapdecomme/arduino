/* Mosquito Maitre
 * Projet pour esp32 avec un mosfet IRLZ24N

Relivraison :
Sur ESP32, LittleFS n’est pas actif par défaut (c’est SPIFFS ou LittleFS.h spécifique ESP32 qui doit être inclus).
1) Go to the releases page and click the latest esp32fs.zip file to download.
2) Unzip the downloaded file. You should have a folder called esp32fs with a file called esp32fs.jar inside.
3) Find your Sketchbook location. In your Arduino IDE, go to File > Preferences and check your Sketchbook location. 
In my case, it’s in the following path: /home/Dell/Arduino.
4) Go to the sketchbook location, and create a tools folder if you don’t have it already 
(make sure that the Arduino IDE application is closed).
5) Inside the tools folder, create another folder called ESP32FS if you haven’t already.
6) Inside the ESP32FS folder, create a folder called tool.
7) Copy the esp32fs.jar file to the tool folder
8) Now, you can open Arduino IDE.
To check if the plugin was successfully installed, open your Arduino IDE and select your ESP32 board. 
In the Tools menu, check that you have the option “ESP32 Sketch Data Upload“. 
Click on that option. A window will pop up for you to choose the filesystem you want to use.


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


ATTENTION 1 : Installer la librairie ArduinoJson (par Benoît Blanchon).




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

#include <WiFi.h>             // ✅ Remplacé par la librairie standard ESP32
#include <WebServer.h>            // ✅ Remplacé par la librairie WebServer de l’ESP32
#include <EEPROM.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>   // Sur ESP8266, LittleFS est spécifique


#define AP_SSID "Mosquito"
#define AP_PWD ""
#define AP_CHANNEL 1

// Pins (ajuste selon ton câblage)
const int FAN_PIN = 1; // PWM pin vers gate du MOSFET

//✅ ESP32 : définition manuelle de la LED intégrée si absente
#ifndef LED_BUILTIN
#define LED_BUILTIN 2   // la plupart des cartes ESP32 utilisent le GPIO 2 pour la LED intégrée
#endif
const int LED_PIN = LED_BUILTIN; // indicateur local (ESP intégré), active low on many boards
//const int LED_PIN = 2; // indicateur local (ESP intégré), active low on many boards

// ESP8266WebServer server(80);   // Obsolète sur ESP32
WebServer server(80);        // ✅ Remplacé par WebServer pour ESP32
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Etat du maitre
bool powerState = false;  // Off au lancement
int speed = 1; // 1..3

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
unsigned long SLAVE_TIMEOUT = 30UL * 1000UL; // 30s -> Considere hors ligne 

String erreurLog;

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
      case 1: duty = 160; break; // vitesse basse
      case 2: duty = 200; break; // vitesse moyenne
      case 3: duty = 255; break; // vitesse haute
      default: duty = 150; break;
    }
  }
  analogWrite(FAN_PIN, duty);
}

// ----- Web handlers -----

// Serve main page
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Ventilateur ESP8266</title>
  <style>
  body { font-family: sans-serif; text-align: center; margin-top: 2em; }
  .btn { padding: 1em 2em; font-size: 1em; margin: 0.5em; border: none; border-radius: 10px; color: white;}
  .on  { background-color: darkgreen; }
  .off { background-color: brown; }
  .slider { -webkit-appearance: none; width: 80%; max-width: 400px; height: 30px; border-radius: 10px; outline: none; margin-top: 1em; }
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
    <h3><span id="slaveCount"></span></h3>
    <div id="slaveList"></div>
  </div>

    <!-- ✅ Image dynamique -->
  <img id='fanImage' src='/image1.jpg' alt=''>

  <script>
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

        // Change le texte du bouton
        document.getElementById('power').innerText = j.power ? 'Allumé' : 'Eteint';
        // Change la couleur du bouton
        const toggleButton = document.getElementById('toggle');
        const sel = document.getElementById('speed');
        sel.value = j.speed;
        if (j.power) {
          toggleButton.classList.remove('off');
          toggleButton.classList.add('on');
          sel.classList.remove('off');
          sel.classList.add('on');
        } else {
          toggleButton.classList.remove('on');
          toggleButton.classList.add('off');
          sel.classList.remove('on');
          sel.classList.add('off');
        }
        const valspeed = document.getElementById('valspeed');
        valspeed.innerHTML = j.speed;
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


        // ✅ Image dynamique selon la vitesse et si l'appareil est allumé ou non
        const fanImage = document.getElementById('fanImage');
        await new Promise(r => setTimeout(r, 100));
        if (j.power) {
          switch (parseInt(j.speed)) {
            case 1:
              fanImage.src = '/mosquito1.png?t=' + Date.now();
              break;
            case 2:
              fanImage.src = '/mosquito2.png?t=' + Date.now();
              break;
            case 3:
              fanImage.src = '/mosquito3.png?t=' + Date.now();
              break;
          }
        } else {
              fanImage.src = '/mosquito0.png?t=' + Date.now();
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
    applyFanState();
    saveSettings();
    server.send(200, "application/json", "{\"result\":\"ok\"}");
    return;
  } else if (act == "speed") {
    int v = doc["value"] | speed;
    if (v < 1) v = 1;
    if (v > 3) v = 3;
    speed = v;
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
  doc["nbwificnx"] = WiFi.softAPgetStationNum();
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
// Un esclave s'enregistre
void handleRegister() {
  // Ajouter un en-tête Connection: close à la réponse 
  server.sendHeader("Connection", "close");
  erreurLog="";
  if (!server.hasArg("plain")) {
      erreurLog="Mauvais format";
      server.send(400, "application/json", "{\"error\":\"no body\"}");
      server.client().stop();
      return;
  }
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, body)) {
    erreurLog="Json incorrect";
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    server.client().stop();
    return;
  }
  const char* id = doc["id"];
  if (!id) {
    erreurLog="id incorrect";
    server.send(400, "application/json", "{\"error\":\"no id\"}");
    server.client().stop();
    return;
  }
  IPAddress remote = server.client().remoteIP();
  registerOrUpdateSlave(String(id), remote);
  // Respond with pairing success and current commandId + current state
  StaticJsonDocument<256> res;
  res["paired"] = true;
  res["power"] = powerState;
  res["speed"] = speed;
  String out;
  serializeJson(res, out);
  erreurLog="Connexion acceptee";
  server.send(200, "application/json", out);
  // Force la fermeture du client
  server.client().stop();

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
  // dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  server.onNotFound(handleNotFound);

}

void setup() {
  Serial.begin(9600);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off by default for builtin active-low


  // --- LittleFS init (ESP32) ---
  LittleFS.begin();

  // --- Start WiFi AP ---
  Serial.println("Starting AP...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PWD, 1, false);
  // ne marche pas sur esp32 WiFi.softAPsetMaxConnections(8);

  delay(200);


  // Setup DNS to capture all domains (captive portal)
  dnsServer.start(53, "*", WiFi.softAPIP());

  
  // Charge les valeurs sauvegardés
  loadSettings();
  applyFanState();
  
  server.on("/", handleRoot);
  server.on("/command", HTTP_POST, handleCommand);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/register", HTTP_POST, handleRegister);


  // ✅ Permet de servir automatiquement tous les fichiers du dossier /data
  server.serveStatic("/", LittleFS, "/");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");


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
}
