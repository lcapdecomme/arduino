// Panneau de LEDs flexible sur un ESP8266 avec serveur WEB (Testé ok)
// Il permet d'afficher un message personnalisé depuis une interface WEB
// La couleur est paramétrable.
// L'url sur le navigateur est : 192.168.4.1 

// Branchement sur un ESP8266
// Fil rouge Matrice => 3.3v en bas à droite
// Fil Vert Data Matrice => Attention ! on utilise TXD1 (c'est-à-dire D4) car TXD0 bug 
// Fil Blanc masse Matrice => GND en bas à droite

// Branchement sur un ESP32 
// Fil rouge Matrice => vim en haut à droite
// Fil Vert Data Matrice => TX0 en bas à gauche 
// Fil Blanc masse Matrice => GND en haut à droite

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>

// Utilisaiton d'une font personnelle ? 
// Penser à revoir les coordonnées du texte avec matrix.setCursor 
// #include "CustomFont8.h"

// Liste des polices ici : https://github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts
// #include <Fonts/FreeSans9pt7b.h>
// #include <Fonts/FreeSans8pt7b.h>

// Possible de se faire sa propre police : 
//    Il suffit de lancer cette application en ligne : https://rop.nl/truetype2gfx/
//    choisir la police à convertir
//    choisir sa dimension
//    lancer sa conversion GFX
//    copier ce fichier dans le dossier fonts de Adafruit_GXF library

// ----------------------------------------------------------------------------
// Les uniques parametres à changer :
// Si 2 matrices horizontales mettre NOMBRE_L = 2 et NOMBRE_H=1
// Si 4 matrices horizontales mettre NOMBRE_L = 4 et NOMBRE_H=1
// Si 2 matrices horizontales et 2 verticales mettre NOMBRE_L = 2 et NOMBRE_H=2
#define NOMBRE_H 1
#define NOMBRE_L 2
// ----------------------------------------------------------------------------




// 5 variables a ne pas modifier 
#define PIN_MATRICE 2         // Si ESP32, mettre obligatoirement la valeur 1 et brancher la data sur la PIN TX0 !!
                              // Si ESP8266, mettre obligatoirement la valeur 2 et brancher la data sur la PIN TXD1 soit GPI02 (D4) !!
#define LUMINOSITE 50         // Luminosité de l'affichage sachant que cela varie de 0 à 255
// Ces valeurs doivent correspondre précisement à la taille des matrices 
#define HAUTEUR_MATRICE 8     // Nombre de pixel en hauteur. On laisse 8 sur ce modèle
#define LARGEUR_MATRICE 32   // Nombre de pixel en largeur. On laisse 64 sur ce modèle (2 matrices de 32 pixels)
                              // Mais il faudrait laisser 32 



// Ne pas toucher : la taille se calcule automatiquement en fonction du nombre de matrice verticale
#define TAILLE_TEXTE NOMBRE_H


// Identifiant pour le HotSpot Wifi
const char* ssid = "BelEcran_ESP";
const char* password = "belecran$";

// Création du serveur sur le port 80
ESP8266WebServer server(80);


// ****** Début du programme ***********

// Initialisation de la matrice
// Largeur : LARGEUR_MATRICE
// hauteur en pixels : HAUTEUR_MATRICE
// Matrice sur la pin num. :  PIN_MATRICE
// NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  : Première led en bas à droite 
// NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG : branchement en colonne de type zigzag
// NEO_GRB + NEO_KHZ800 : type de led
//Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE,PIN_MATRICE,
//  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  +
//  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
//  NEO_GRB + NEO_KHZ800);

// Matrice 2x32 verticale 
// C'est la solution lorsqu'on a plus d'une matrice
// Exemple et documentation ici : https://github.com/adafruit/Adafruit_NeoMatrix/blob/master/examples/tiletest/tiletest.ino
 Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE, NOMBRE_L, NOMBRE_H, PIN_MATRICE,
  NEO_TILE_BOTTOM   + NEO_TILE_RIGHT   + NEO_TILE_COLUMNS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

const char MAIN_page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP8266 LED Matrix</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 20px;
      background-color: #f4f4f4;
    }
    .container {
      max-width: 400px;
      margin: auto;
      padding: 20px;
      background: white;
      box-shadow: 0px 0px 10px rgba(0,0,0,0.1);
      border-radius: 10px;
    }
    label {
      display: block;
      margin-top: 10px;
      font-weight: bold;
    }
    input[type="text"] {
      width: 100%;
      padding: 12px;
      font-size: 18px;
      border: 1px solid #ccc;
      border-radius: 5px;
      box-sizing: border-box;
    }
    select {
      width: 100%;
      padding: 12px;
      font-size: 18px;
      border: 1px solid #ccc;
      border-radius: 5px;
      background: white;
    }
    input[type="range"] {
      width: 100%;
      height: 40px;
    }
    button {
      width: 100%;
      padding: 15px;
      font-size: 20px;
      background: #28a745;
      color: white;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      margin-top: 15px;
    }
    button:hover {
      background: #218838;
    }
  </style>
  </head>
<body>
    
    <div class="container">
        
        <h2>Contrôle de la Matrice LED</h2>
        <form action="/update" method="POST">
            <label>Texte :</label>
            <input type="text" id="message" value="%TEXT%" maxlength="500">
            
            <label>Couleur :</label>
            <select id="color">
                <option value="RED" %RED% >Rouge</option>
                <option value="GREEN" %GREEN% >Vert</option>
                <option value="BLUE" %BLUE% >Bleu</option>
                <option value="WHITE" %WHITE% >Blanc</option>
                <option value="YELLOW" %YELLOW% >Jaune</option>
                <option value="CYAN" %CYAN% >Cyan</option>
                <option value="MAGENTA" %MAGENTA% >Magenta</option>
            </select>
            
            <label>Vitesse :</label>
            <input type="range" id="speed" min="1" max="5" value="%SPEED%">
            <p id="speedValue">Vitesse : %SPEED%</p>
            
            <label>Mode :</label>
            <select id="mode">
                <option value="Fixe" %FIXE% >Fixe</option>
                <option value="Defilant" %DEFILANT% >Defilant</option>
            </select>
        </form>
        <button onclick="sendData()">Envoyer</button>
    </div>
    <script>
        document.getElementById("color").value = "%COLOR%";
        document.getElementById("speed").oninput = function() {
            document.getElementById("speedValue").innerText = "Vitesse : " + this.value;
        };

        function sendData() {
            let msg = document.getElementById("message").value;
            let color = document.getElementById("color").value;
            let speed = document.getElementById("speed").value;
            let modet = document.getElementById("mode").value;
            
            let xhr = new XMLHttpRequest();
            xhr.open("GET", "/update?text=" + encodeURIComponent(msg) + "&color=" + color + "&speed=" + speed + "&mode=" + modet, true);
            xhr.send();
        }
    </script>
</body>
</html>
)rawliteral";



unsigned long lastUpdate = 0;
int scrollSpeed = 100;  // Temps en millisecondes entre chaque déplacement
int textX = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
// uint16_t textColor = matrix.Color(255, 0, 0);  // Rouge par défaut

String lastText = "bienvenue";    // Texte par défaut
String lastColor =  "white";      // Blanc par défaut
int lastSpeed = 3;                // Par défaut, vitesse moyenne (1 = lent, 5 = rapide)
String lastMode =  "defilant";    // Blanc par défaut

void saveData() {
    File file = SPIFFS.open("/data.txt", "w");
    if (!file) {
        Serial.println("Erreur lors de l'ouverture du fichier pour écriture !");
        return;
    }
    file.println(lastText);
    file.println(lastColor);
    file.println(lastSpeed);
    file.println(lastMode);
    file.close();
    Serial.println("Données sauvegardées !");
}


void loadData() {
    if (!SPIFFS.exists("/data.txt")) {
        Serial.println("Aucune donnée sauvegardée, valeurs par défaut utilisées.");
        return;
    }

    File file = SPIFFS.open("/data.txt", "r");
    if (!file) {
        Serial.println("Erreur lors de l'ouverture du fichier pour lecture !");
        return;
    }

    lastText = file.readStringUntil('\n');
    lastText.trim();
    lastColor = file.readStringUntil('\n');
    lastColor.trim();
    lastSpeed = file.readStringUntil('\n').toInt();
    if (lastSpeed < 1 || lastSpeed > 5) lastSpeed = 3; // Sécurité pour éviter des valeurs invalides
    lastMode = file.readStringUntil('\n');
    lastMode.trim();

    file.close();

    Serial.println("Données chargées :");
    Serial.print("Texte   : ");
    Serial.println(lastText);
    Serial.print("Couleur : ");
    Serial.println(lastColor);
    Serial.print("Mode : ");
    Serial.println(lastMode);
}



void setup(){  
  // Ne fait plus clignoter la led de la carte
  pinMode(LED_BUILTIN, OUTPUT);  // Définir la LED en sortie
  digitalWrite(LED_BUILTIN, HIGH);  // Éteindre la LED (car active en LOW)

  Serial.begin(9600);
  Serial.println("Démarrage ESP8266....");
  
  SPIFFS.begin();  // Initialiser SPIFFS
  loadData();      // Charger les valeurs enregistrées
  
  delay(500);
  matrix.begin();
  Serial.println("Initialisation de la matrice");
  matrix.setTextWrap(false);
  matrix.setBrightness(LUMINOSITE);
  //matrix.setTextColor(textColor); // Couleur par defaut
  //matrix.setFont(&FreeSans8pt7b);  // Utilisation de la police avec accents
  //matrix.setFont(&CustomFont8);
  // Definir la taille des caractères 
  matrix.setTextSize(TAILLE_TEXTE);
  Serial.println("Démarrage de la matrice....");
  delay(1000);

  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Servir la page HTML principale et les WS
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/getData", HTTP_GET, handleGetData);
  
  // Start server
  server.begin();
  Serial.println("Serveur web démarré !");

  delay(1000);
}

void loop() {
  server.handleClient();
  scrollSpeed = map(lastSpeed, 1, 5, 200, 50); // Ajuste la vitesse (1 = lent, 5 = rapide)
  if (millis() - lastUpdate > scrollSpeed) {
      lastUpdate = millis();
      if (lastMode == "Defilant") 
      { 
        //Scrolling text startes here
        matrix.fillScreen(0);
        //matrix.setCursor(textX, HAUTEUR_MATRICE-1);
        matrix.setCursor(textX, 0);
        matrix.setTextColor(getColor(lastColor));
        matrix.print(lastText);
           
        textX--;
        if (textX < -((int)lastText.length() * 6)) {
            textX = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
        }
        matrix.show();
      }

      if (lastMode == "Fixe") 
      {  
        matrix.fillScreen(0);
        //matrix.setCursor(textX, HAUTEUR_MATRICE-1);
        matrix.setCursor(0, 0);
        matrix.setTextColor(getColor(lastColor));
        matrix.print(lastText);
        matrix.show();
      }
  }
}


// Evenements du WS et de la page WEB
void handleRoot() {
    String html = String(MAIN_page);
    html.replace("%TEXT%", lastText);
    html.replace("%RED%", lastColor == "RED" ? "selected" : "");
    html.replace("%GREEN%", lastColor == "GREEN" ? "selected" : "");
    html.replace("%BLUE%", lastColor == "BLUE" ? "selected" : "");
    html.replace("%YELLOW%", lastColor == "YELLOW" ? "selected" : "");
    html.replace("%WHITE%", lastColor == "WHITE" ? "selected" : "");
    html.replace("%CYAN%", lastColor == "CYAN" ? "selected" : "");
    html.replace("%MAGENTA%", lastColor == "MAGENTA" ? "selected" : "");
    html.replace("%COLOR%", lastColor);
    html.replace("%SPEED%", String(lastSpeed));
    html.replace("%FIXE%", lastMode == "Fixe" ? "selected" : "");
    html.replace("%DEFILANT%", lastColor == "Defilant" ? "selected" : "");
    server.send(200, "text/html", html);
}

void handleUpdate() {
    if (server.hasArg("text") && server.hasArg("color") && server.hasArg("mode")) {
        lastText = normalizeText(urlDecode(server.arg("text")));  // Conversion avant affichage avec suppression des accents
        Serial.print("Nouveau texte   : ");
        Serial.println(lastText);
        lastColor = server.arg("color");
        Serial.print("Couleur choisie : ");
        Serial.println(lastColor);
        lastSpeed = server.arg("speed").toInt();
        Serial.print("Vitesse choisie : ");
        Serial.println(lastSpeed);
        lastMode = server.arg("mode");
        Serial.print("Mode choisi : ");
        Serial.println(lastMode);
        textX = LARGEUR_MATRICE;
        saveData(); // Sauvegarde SPIFFS
        server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "KO");
    }
}

// Plus utilisé
void handleGetData() {
    String json = "{\"text\":\"" + lastText + "\", \"color\":\"" + lastColor + "\", \"speed\":\"" + lastSpeed + "\", \"mode\":\"" + lastMode + "\"}";
    server.send(200, "application/json", json);
}


// Decode le texte de la page web
String urlDecode(String input) {
    String decoded = "";
    char temp[] = "00";
    unsigned int len = input.length();

    for (unsigned int i = 0; i < len; i++) {
        if (input[i] == '%') {  // Détection d'un caractère encodé (%XX)
            if (i + 2 < len) {
                temp[0] = input[i + 1];
                temp[1] = input[i + 2];
                decoded += (char)strtol(temp, NULL, 16);
                i += 2;
            }
        } else if (input[i] == '+') {  // Les espaces sont souvent envoyés sous forme de "+"
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}


String normalizeText(String input) {
    input.replace("é", "e");
    input.replace("è", "e");
    input.replace("ê", "e");
    input.replace("ë", "e");
    input.replace("à", "a");
    input.replace("ç", "c");
    input.replace("ù", "u");
    input.replace("î", "i");
    input.replace("ô", "o");
    input.replace("œ", "oe");
    input.replace("É", "E");
    input.replace("È", "E");
    input.replace("Ê", "E");
    input.replace("À", "A");
    input.replace("Ç", "C");
    input.replace("Ù", "U");
    input.replace("Î", "I");
    input.replace("Ô", "O");
    input.replace("€", "Euros");
    input.replace(" ", " ");
    input.replace("’", "'");
    input.replace("«", "\"");
    input.replace("»", "\"");
    input.replace("Œ", "OE");
    return input;
}





// Fonction pour convertir le nom de couleur en RGB
uint16_t getColor(String color) {
    if (color == "RED") return matrix.Color(255, 0, 0);
    if (color == "GREEN") return matrix.Color(0, 255, 0);
    if (color == "BLUE") return matrix.Color(0, 0, 255);
    if (color == "YELLOW") return matrix.Color(255, 255, 0);
    if (color == "CYAN") return matrix.Color(0, 255, 255);
    if (color == "MAGENTA") return matrix.Color(255, 0, 255);
    if (color == "WHITE") return matrix.Color(255, 255, 255);
    return matrix.Color(255, 255, 255); // Blanc par défaut
}
