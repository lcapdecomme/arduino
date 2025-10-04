// Panneau de LEDs flexible sur un ESP8266 avec serveur WEB
// Il permet d'afficher un message personnalisé depuis une interface WEB
// La couleur est paramétrable.
// Pour cela, ce programme lance un hotspot wifi et ouvre un serveur WEB
// L'url sur le navigateur est : 192.168.4.1 

// Branchement sur un ESP8266
// Fil rouge Matrice => 3.3v en bas à droite
// Fil Vert Data Matrice => Attention ! on utilise TXD1 (c'est-à-dire D4) car TXD0 bug 
// Fil Blanc masse Matrice => GND en bas à droite
// A la fin du branchement, les trois fils sont donc à cotés les uns des autre


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// === CONFIG WIFI ===
const char* ssid = "ESP8266-Eyes";
const char* password = ""; // mot de passe


// === MATRICE ===
#define PIN 1

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(
  32, 8, PIN,
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

// === WEBSERVER ===
ESP8266WebServer server(80);

// === VARIABLES ===
unsigned long lastUpdate = 0;
uint16_t eyeColor = matrix.Color(255, 0, 0);    // Rouge par défaut
uint16_t pupilColor = matrix.Color(0, 0, 0);    // Noir par défaut
int mode = 0;
bool autoMode = false;
int autoDelay = 3000; // ms
unsigned long lastSwitch = 0;
int frame = 0;

// === TABLEAUX D'YEUX ===
byte eye_open[8] = {
  B00111100,
  B01000010,
  B10100101,
  B10011001,
  B10100101,
  B10011001,
  B01000010,
  B00111100
};

byte eye_closed[8] = {
  B00000000,
  B00000000,
  B11111111,
  B11111111,
  B11111111,
  B00000000,
  B00000000,
  B00000000
};

// === DESSINER UN ŒIL (8x8) ===
void drawEye(int xOffset, byte eyePattern[8]) {
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (bitRead(eyePattern[y], 7 - x)) {
        // PUPILLE au centre (3,3) à (4,4)
        if ((x == 3 || x == 4) && (y == 3 || y == 4)) {
          matrix.drawPixel(x + xOffset, y, pupilColor);
        } else {
          matrix.drawPixel(x + xOffset, y, eyeColor);
        }
      } else {
        matrix.drawPixel(x + xOffset, y, 0);
      }
    }
  }
}

// === PAGE WEB ===
void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>button{padding:10px;margin:5px;font-size:18px;border-radius:8px;} input{margin:5px;}</style></head><body>";
  html += "<h2>👁 Contrôle des yeux</h2>";
  
  // Couleur des yeux
  html += "<form action='/setcolor' method='GET'>Yeux: <input type='color' name='eye' value='#ff0000'>";
  html += " Pupille: <input type='color' name='pupil' value='#000000'>";
  html += "<input type='submit' value='OK'></form>";

  // Modes
  html += "<h3>Mode :</h3>";
  html += "<a href='/mode?m=0'><button>Fixe</button></a>";
  html += "<a href='/mode?m=1'><button>Clignement</button></a>";
  html += "<a href='/mode?m=2'><button>Regard</button></a>";
  html += "<a href='/mode?m=3'><button>Démoniaque</button></a>";

  // Auto alternance
  html += "<h3>Alternance auto</h3>";
  html += "<form action='/auto' method='GET'>Toutes les <input type='number' name='t' value='3'> sec ";
  html += "<input type='submit' value='Activer'></form>";
  html += "<a href='/stop'><button>Stop Auto</button></a>";

  html += "</body></html>";
  server.send(200, "text/html", html);
}

// === TRAITEMENT DES REQUÊTES ===
void handleColor() {
  if (server.hasArg("eye")) {
    long rgb = strtol(server.arg("eye").substring(1).c_str(), NULL, 16);
    eyeColor = matrix.Color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  }
  if (server.hasArg("pupil")) {
    long rgb = strtol(server.arg("pupil").substring(1).c_str(), NULL, 16);
    pupilColor = matrix.Color((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
  }
  handleRoot();
}

void handleMode() {
  if (server.hasArg("m")) {
    mode = server.arg("m").toInt();
    autoMode = false;
  }
  handleRoot();
}

void handleAuto() {
  if (server.hasArg("t")) {
    autoDelay = server.arg("t").toInt() * 1000;
    autoMode = true;
  }
  handleRoot();
}

void handleStop() {
  autoMode = false;
  handleRoot();
}

// === SETUP ===
void setup() {
  Serial.begin(9600);
  WiFi.softAP(ssid, password);
  Serial.println("AP lancé : " + WiFi.softAPIP().toString());

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(40);

  // Routes Web
  server.on("/", handleRoot);
  server.on("/setcolor", handleColor);
  server.on("/mode", handleMode);
  server.on("/auto", handleAuto);
  server.on("/stop", handleStop);
  server.begin();
  Serial.println("Serveur web démarré !");

  delay(1000);
}

// === ANIMATION ===
void loop() {
  server.handleClient();
  if (millis() - lastUpdate > 200) {
    lastUpdate = millis();
      unsigned long now = millis();
      if (autoMode && now - lastSwitch > autoDelay) {
        lastSwitch = now;
        mode = (mode + 1) % 4;
      }
    
      if (now % 200 < 50) { // vitesse approx
        frame++;
      }
    
      matrix.fillScreen(0);
    
      switch (mode) {
        case 0: // Fixe
          drawEye(2, eye_open);
          drawEye(22, eye_open);
          break;
        case 1: // Clignement
          if ((frame / 5) % 2 == 0) {
            drawEye(2, eye_open);
            drawEye(22, eye_open);
          } else {
            drawEye(2, eye_closed);
            drawEye(22, eye_closed);
          }
          break;
        case 2: // Regard gauche/droite
          drawEye(2 + (frame % 3), eye_open);
          drawEye(22 + (frame % 3), eye_open);
          break;
        case 3: // Démoniaque
          if (frame % 2 == 0) eyeColor = matrix.Color(255, 0, 0);
          else eyeColor = matrix.Color(100, 0, 0);
          drawEye(2, eye_open);
          drawEye(22, eye_open);
          break;
      }
    
      matrix.show();
  }
}
