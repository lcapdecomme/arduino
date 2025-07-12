
/* Ok sur esp32 Nano Pin D5 avec un mosfet IRLZ24N


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
GATE du IRF3708 ---[220Ω]--> Pin PWM Nano (ex : D3, D5, D6, D9, D10 ou D11)
                    |
                    +--[10kΩ]--> GND (pull-down)


url de carte supplémentaire : 
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
http://arduino.esp8266.com/stable/package_esp8266com_index.json

*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>


#define FAN_PIN 5
#define PWM_CHANNEL 0
#define PWM_FREQ 25000
#define PWM_RESOLUTION 8

WebServer server(80);
bool fanOn = false;
int fanSpeed = 5;
Preferences prefs;


const char* ssid = "mosquito";
const char* password = "";

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Ventilateur ESP32</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 2em; }
    label {
      font-size: 1.2em;
      margin: 0.5em; 
    }
    .btn {
      padding: 1em 2em;
      font-size: 1em;
      margin: 0.5em 0.5em 1.5em 0.5em; 
      border: none;
      border-radius: 10px;
      color: white;
    }
    .on  { background-color: green; }
    .off { background-color: red; }
.slider {
  -webkit-appearance: none;
  width: 80%;
  max-width: 400px;
  height: 30px;
  background: #ddd;
  border-radius: 10px;
  outline: none;
  transition: background 0.3s;
  margin-top: 1em;
}
.slider:hover {
  background: #ccc;
}
.slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 30px;
  height: 30px;
  background: #007bff;
  border-radius: 50%;
  cursor: pointer;
  box-shadow: 0 0 5px rgba(0,0,0,0.3);
  transition: background 0.3s;
}
.slider::-webkit-slider-thumb:hover {
  background: #0056b3;
}
.slider::-moz-range-thumb {
  width: 30px;
  height: 30px;
  background: #007bff;
  border-radius: 50%;
  cursor: pointer;
  box-shadow: 0 0 5px rgba(0,0,0,0.3);
  transition: background 0.3s;
}
    #status { 
      font-size: 1.4em;
  margin: 0.5em; 
  }
  img {
    margin-top: 2em;
  }
  </style>
</head>
<body>
  <h1>Contrôle du Ventilateur</h1>
  <div id="status">État : <span id="etat">...</span></div>
  <button id="btn" class="btn off" onclick="toggleFan()">ON / OFF</button><br>

  <label for="speed">Vitesse : <span id="val">5</span></label><br>
  <input class="slider" type="range" min="1" max="10" value="5" id="speed"
         oninput="updateSpeed(this.value)" onchange="sendSpeed(this.value)">

<img src="https://i.imgur.com/YpwMGMZ.png" alt="logo">

  <script>
    function toggleFan() {
      fetch('/toggle').then(updateState);
    }

    function updateSpeed(val) {
      document.getElementById('val').innerText = val;
    }

    function sendSpeed(val) {
      fetch('/speed?val=' + val).then(updateState);
    }

    function updateState() {
      fetch('/state')
        .then(r => r.text())
        .then(state => {
          let btn = document.getElementById('btn');
          let etat = document.getElementById('etat');
          if (state === "on") {
            btn.className = "btn on";
            etat.innerText = "Activé";
          } else {
            btn.className = "btn off";
            etat.innerText = "Désactivé";
          }
        });
    }

    // Mise à jour initiale
    updateState();
  </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}


void saveState() {
  prefs.begin("fanstate", false);  // écriture
  prefs.putBool("on", fanOn);
  prefs.putInt("speed", fanSpeed);
  prefs.end();
}


void setFanSpeed() {
  if (fanOn) {
    int pwm = map(fanSpeed, 1, 10, 25, 255);  // vitesse douce à max
    analogWrite(FAN_PIN, pwm);
  } else {
    analogWrite(FAN_PIN, 0);
  }
}


void handleToggle() {
  fanOn = !fanOn;
  setFanSpeed();
  saveState();
  server.send(200, "text/plain", "OK");
}


void handleSpeed() {
  if (server.hasArg("val")) {
    fanSpeed = server.arg("val").toInt();
    setFanSpeed();
    saveState();
  }
  server.send(200, "text/plain", "Speed OK");
}


void handleState() {
  server.send(200, "text/plain", fanOn ? "on" : "off");
}

void setup() {
  Serial.begin(115200);
  pinMode(FAN_PIN, OUTPUT);
  analogWrite(FAN_PIN, 0);  // Ventilo OFF au démarrage


  WiFi.softAP(ssid, password);
  Serial.print("WiFi créé : ");
  Serial.println(ssid);
  Serial.print("IP : ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/speed", handleSpeed);
  server.on("/state", handleState);
  server.begin();
  
  prefs.begin("fanstate", true);  // en lecture seule
  fanOn = prefs.getBool("on", false);          // false par défaut
  fanSpeed = prefs.getInt("speed", 5);         // 5 par défaut
  prefs.end();

  
}

void loop() {
  server.handleClient();
}
