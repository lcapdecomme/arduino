// 3 Panneaux de LEDs flexible sur un ESP8266 avec serveur WEB
// Il permet d'afficher trois messages personnalisés depuis une interface WEB
// La couleur est paramétrable, le texte et le défilement sont paramétrables
// Pour cela, ce programme lance un hotspot wifi et ouvre un serveur WEB
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

// Utilisation d'une font personnelle ? 
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
#define PIN_MATRICE1 1         // Si ESP32, mettre obligatoirement la valeur 1 et brancher la data sur la PIN TX0 !!
#define PIN_MATRICE2 2         // Si ESP32, mettre obligatoirement la valeur 2 et brancher la data sur la PIN TX2 !!
#define PIN_MATRICE3 3         // Si ESP32, mettre obligatoirement la valeur 3 et brancher la data sur la PIN TX3 !!
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
 Adafruit_NeoMatrix matrix1 = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE, NOMBRE_L, NOMBRE_H, PIN_MATRICE1,
  NEO_TILE_BOTTOM   + NEO_TILE_RIGHT   + NEO_TILE_COLUMNS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

 Adafruit_NeoMatrix matrix2 = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE, NOMBRE_L, NOMBRE_H, PIN_MATRICE2,
  NEO_TILE_BOTTOM   + NEO_TILE_RIGHT   + NEO_TILE_COLUMNS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

 Adafruit_NeoMatrix matrix3 = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE, NOMBRE_L, NOMBRE_H, PIN_MATRICE3,
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
            <label>Texte 1 :</label>
            <input type="text" id="message1" value="%TEXT1%" maxlength="500">
            
            <label>Couleur :</label>
            <select id="color1">
                <option value="RED" %RED1% >Rouge</option>
                <option value="GREEN" %GREEN1% >Vert</option>
                <option value="BLUE" %BLUE1% >Bleu</option>
                <option value="WHITE" %WHITE1% >Blanc</option>
                <option value="YELLOW" %YELLOW1% >Jaune</option>
                <option value="CYAN" %CYAN1% >Cyan</option>
                <option value="MAGENTA" %MAGENTA1% >Magenta</option>
            </select>
            
            <label>Mode :</label>
            <select id="mode1">
                <option value="Fixe" %FIXE1% >Fixe</option>
                <option value="Defilant" %DEFILANT1% >Defilant</option>
            </select>
            
            <br>
            <hr>
            <br>
            
            <label>Texte 2 :</label>
            <input type="text" id="message2" value="%TEXT2%" maxlength="500">
            
            <label>Couleur :</label>
            <select id="color2">
                <option value="RED" %RED2% >Rouge</option>
                <option value="GREEN" %GREEN2% >Vert</option>
                <option value="BLUE" %BLUE2% >Bleu</option>
                <option value="WHITE" %WHITE2% >Blanc</option>
                <option value="YELLOW" %YELLOW2% >Jaune</option>
                <option value="CYAN" %CYAN2% >Cyan</option>
                <option value="MAGENTA" %MAGENTA2% >Magenta</option>
            </select>
            
            <label>Mode :</label>
            <select id="mode2">
                <option value="Fixe" %FIXE2% >Fixe</option>
                <option value="Defilant" %DEFILANT2% >Defilant</option>
            </select>

            <br>
            <hr>
            <br>

            <label>Texte 3 :</label>
            <input type="text" id="message3" value="%TEXT3%" maxlength="500">
            
            <label>Couleur :</label>
            <select id="color3">
                <option value="RED" %RED3% >Rouge</option>
                <option value="GREEN" %GREEN3% >Vert</option>
                <option value="BLUE" %BLUE3% >Bleu</option>
                <option value="WHITE" %WHITE3% >Blanc</option>
                <option value="YELLOW" %YELLOW3% >Jaune</option>
                <option value="CYAN" %CYAN3% >Cyan</option>
                <option value="MAGENTA" %MAGENTA3% >Magenta</option>
            </select>
            
            <label>Mode :</label>
            <select id="mode3">
                <option value="Fixe" %FIXE3% >Fixe</option>
                <option value="Defilant" %DEFILANT3% >Defilant</option>
            </select>

            <br>
            <hr>
            <br>
            <label>Vitesse défilement :</label>
            <input type="range" id="speed" min="1" max="5" value="%SPEED%">
            <p id="speedValue">Vitesse : %SPEED%</p>

        
        </form>
        <button onclick="sendData()">Envoyer</button>
    </div>
    <script>
        document.getElementById("color1").value = "%COLOR1%";
        document.getElementById("color2").value = "%COLOR2%";
        document.getElementById("color3").value = "%COLOR3%";
 
        document.getElementById("speed").oninput = function() {
            document.getElementById("speedValue").innerText = "Vitesse : " + this.value;
        };

        function sendData() {
            let ms1 = encodeURIComponent(document.getElementById("message1").value);
            let co1 = document.getElementById("color1").value;
            let mo1 = document.getElementById("mode1").value;
            
            let ms2 = encodeURIComponent(document.getElementById("message2").value);
            let co2 = document.getElementById("color2").value;
            let mo2 = document.getElementById("mode2").value;
            
            let ms3 = encodeURIComponent(document.getElementById("message3").value);
            let co3 = document.getElementById("color3").value;
            let mo3 = document.getElementById("mode3").value;
            
            let sp = document.getElementById("speed").value;

            let xhr = new XMLHttpRequest();
            xhr.open("GET", "/update?text1="+ms1+"&color1="+co1+"&sp="+sp+"&mode1="+mo1+"&text2="+ms2+"&color2="+co2+"&mode2="+mo2+"&text3="+ms3+"&color3="+co3+"&mode3="+mo3, true);
            xhr.send();
        }
    </script>
</body>
</html>
)rawliteral";



unsigned long lastUpdate = 0;
int scrollSpeed = 100;  // Temps en millisecondes entre chaque déplacement
int textX1 = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
int textX2 = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
int textX3 = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
// uint16_t textColor = matrix.Color(255, 0, 0);  // Rouge par défaut

int lastSpeed = 3;                // Par défaut, vitesse moyenne (1 = lent, 5 = rapide)
                                   // Dans la version actuelle, il n'y a qu'une vitesse de défilement pour tous les textes

String lastText1 = "bienvenue";    // Texte par défaut
String lastColor1 =  "white";      // Blanc par défaut
String lastMode1 =  "Defilant";    // Blanc par défaut

String lastText2 = "bienvenue";    // Texte par défaut
String lastColor2 =  "white";      // Blanc par défaut
String lastMode2 =  "Defilant";    // Blanc par défaut

String lastText3 = "bienvenue";    // Texte par défaut
String lastColor3 =  "white";      // Blanc par défaut
String lastMode3 =  "Defilant";    // Blanc par défaut

void saveData() {
    File file = SPIFFS.open("/data.txt", "w");
    if (!file) {
        Serial.println("Erreur lors de l'ouverture du fichier pour écriture !");
        return;
    }
    file.println(lastText1);
    file.println(lastColor1);
    file.println(lastMode1);
    file.println(lastText2);
    file.println(lastColor2);
    file.println(lastMode2);
    file.println(lastText3);
    file.println(lastColor3);
    file.println(lastMode3);
    file.println(lastSpeed);
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
    
    lastText1 = file.readStringUntil('\n');
    lastText1.trim();
    lastColor1 = file.readStringUntil('\n');
    lastColor1.trim();
    lastMode1 = file.readStringUntil('\n');
    lastMode1.trim();


    lastText2 = file.readStringUntil('\n');
    lastText2.trim();
    lastColor2 = file.readStringUntil('\n');
    lastColor2.trim();
    lastMode2 = file.readStringUntil('\n');
    lastMode2.trim();

    lastText3 = file.readStringUntil('\n');
    lastText3.trim();
    lastColor3 = file.readStringUntil('\n');
    lastColor3.trim();
    lastMode3 = file.readStringUntil('\n');
    lastMode3.trim();

    lastSpeed = file.readStringUntil('\n').toInt();
    if (lastSpeed < 1 || lastSpeed > 5) lastSpeed = 3; // Sécurité pour éviter des valeurs invalides

    file.close();

    Serial.println("Données chargées :");
    Serial.print("Texte1   : ");
    Serial.println(lastText1);
    Serial.print("Couleur1 : ");
    Serial.println(lastColor1);
    Serial.print("Mode1 : ");
    Serial.println(lastMode1);

    Serial.print("Texte2   : ");
    Serial.println(lastText2);
    Serial.print("Couleur2 : ");
    Serial.println(lastColor2);
    Serial.print("Mode2 : ");
    Serial.println(lastMode2);
    
    Serial.print("Texte3   : ");
    Serial.println(lastText3);
    Serial.print("Couleur3 : ");
    Serial.println(lastColor3);
    Serial.print("Mode3 : ");
    Serial.println(lastMode3);

    Serial.print("Vitesse : ");
    Serial.println(lastSpeed);
}



void setup(){  
  // Ne fait plus clignoter la led de la carte
  pinMode(LED_BUILTIN, OUTPUT);  // Définir la LED en sortie
  digitalWrite(LED_BUILTIN, HIGH);  // Éteindre la LED (car active en LOW)

  Serial.begin(57600);
  Serial.println("Démarrage ESP8266....");
  
  SPIFFS.begin();  // Initialiser SPIFFS
  loadData();      // Charger les valeurs enregistrées
  
  delay(500);
  
  Serial.println("Initialisation des matrices");
  matrix1.begin();
  matrix1.setTextWrap(false);
  matrix1.setBrightness(LUMINOSITE);
  matrix1.setTextSize(TAILLE_TEXTE);
  
  matrix2.begin();
  matrix2.setTextWrap(false);
  matrix2.setBrightness(LUMINOSITE);
  matrix2.setTextSize(TAILLE_TEXTE);
  
  matrix3.begin();
  matrix3.setTextWrap(false);
  matrix3.setBrightness(LUMINOSITE);
  matrix3.setTextSize(TAILLE_TEXTE);
  
  //matrix.setTextColor(textColor); // Couleur par defaut
  //matrix.setFont(&FreeSans8pt7b);  // Utilisation de la police avec accents
  //matrix.setFont(&CustomFont8);
  
  // Definir la taille des caractères 
  Serial.println("Démarrage des matrices....");
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

  // Matrice 1, 2 et 3
  scrollSpeed = map(lastSpeed, 1, 5, 500, 20); // Ajuste la vitesse (1 = lent, 5 = rapide)
  if (millis() - lastUpdate > scrollSpeed) {
      lastUpdate = millis();
      matrix1.fillScreen(0);
      matrix2.fillScreen(0);
      matrix3.fillScreen(0);
      if (lastMode1 == "Defilant") 
      { 
        //Scrolling text startes here
        matrix1.setCursor(textX1, 0);
        matrix1.setTextColor(getColor1(lastColor1));
        matrix1.print(lastText1);
        textX1--;
        if (textX1 < -((int)lastText1.length() * 6)) {
            textX1 = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
        }
        matrix1.show();
      }

      if (lastMode1 == "Fixe") 
      {  
        matrix1.setCursor(0, 0);
        matrix1.setTextColor(getColor1(lastColor1));
        matrix1.print(lastText1);
        matrix1.show();
      }

      // Matrice 2
      if (lastMode2 == "Defilant") 
      { 
        //Scrolling text startes here
        matrix2.setCursor(textX2, 0);
        matrix2.setTextColor(getColor2(lastColor2));
        matrix2.print(lastText2);
        textX2--;
        if (textX2 < -((int)lastText2.length() * 6)) {
            textX2 = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
        }
        matrix2.show();
      }

      if (lastMode2 == "Fixe") 
      {  
        matrix2.setCursor(0, 0);
        matrix2.setTextColor(getColor2(lastColor2));
        matrix2.print(lastText2);
        matrix2.show();
      }

      // Matrice 3
      if (lastMode3 == "Defilant") 
      { 
        //Scrolling text startes here
        matrix3.setCursor(textX3, 0);
        matrix3.setTextColor(getColor3(lastColor3));
        matrix3.print(lastText3);
        textX3--;
        if (textX3 < -((int)lastText3.length() * 6)) {
            textX3 = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
        }
        matrix3.show();
      }

      if (lastMode3 == "Fixe") 
      {  
        matrix3.setCursor(0, 0);
        matrix3.setTextColor(getColor3(lastColor3));
        matrix3.print(lastText3);
        matrix3.show();
      }
  }

  
}


// Evenements du WS et de la page WEB
void handleRoot() {
    String html = String(MAIN_page);
    
    html.replace("%TEXT1%", lastText1);
    html.replace("%RED1%", lastColor1 == "RED" ? "selected" : "");
    html.replace("%GREEN1%", lastColor1 == "GREEN" ? "selected" : "");
    html.replace("%BLUE1%", lastColor1 == "BLUE" ? "selected" : "");
    html.replace("%YELLOW1%", lastColor1 == "YELLOW" ? "selected" : "");
    html.replace("%WHITE1%", lastColor1 == "WHITE" ? "selected" : "");
    html.replace("%CYAN1%", lastColor1 == "CYAN" ? "selected" : "");
    html.replace("%MAGENTA1%", lastColor1 == "MAGENTA" ? "selected" : "");
    html.replace("%COLOR1%", lastColor1);
    html.replace("%FIXE1%", lastMode1 == "Fixe" ? "selected" : "");
    html.replace("%DEFILANT1%", lastMode1 == "Defilant" ? "selected" : "");
    
    html.replace("%TEXT2%", lastText2);
    html.replace("%RED2%", lastColor2 == "RED" ? "selected" : "");
    html.replace("%GREEN2%", lastColor2 == "GREEN" ? "selected" : "");
    html.replace("%BLUE2%", lastColor2 == "BLUE" ? "selected" : "");
    html.replace("%YELLOW2%", lastColor2 == "YELLOW" ? "selected" : "");
    html.replace("%WHITE2%", lastColor2 == "WHITE" ? "selected" : "");
    html.replace("%CYAN2%", lastColor2 == "CYAN" ? "selected" : "");
    html.replace("%MAGENTA2%", lastColor2 == "MAGENTA" ? "selected" : "");
    html.replace("%COLOR2%", lastColor2);
    html.replace("%FIXE2%", lastMode2 == "Fixe" ? "selected" : "");
    html.replace("%DEFILANT%", lastMode2 == "Defilant" ? "selected" : "");
    
    html.replace("%TEXT3%", lastText3);
    html.replace("%RED3%", lastColor3 == "RED" ? "selected" : "");
    html.replace("%GREEN3%", lastColor3 == "GREEN" ? "selected" : "");
    html.replace("%BLUE3%", lastColor3 == "BLUE" ? "selected" : "");
    html.replace("%YELLOW3%", lastColor3 == "YELLOW" ? "selected" : "");
    html.replace("%WHITE3%", lastColor3 == "WHITE" ? "selected" : "");
    html.replace("%CYAN3%", lastColor3 == "CYAN" ? "selected" : "");
    html.replace("%MAGENTA3%", lastColor3 == "MAGENTA" ? "selected" : "");
    html.replace("%COLOR3%", lastColor3);
    html.replace("%FIXE3%", lastMode3 == "Fixe" ? "selected" : "");
    html.replace("%DEFILANT3%", lastMode3 == "Defilant" ? "selected" : "");

    html.replace("%SPEED%", String(lastSpeed));
    
    server.send(200, "text/html", html);
}

// MAJ des données depuis le navigateur
// Exemple reception : 
void handleUpdate() {
        lastText1 = normalizeText(urlDecode(server.arg("text1")));  // Conversion avant affichage avec suppression des accents
        Serial.print("Nouveau texte   : ");
        Serial.println(lastText1);
        lastColor1 = server.arg("color1");
        Serial.print("Couleur choisie : ");
        Serial.println(lastColor1);
        lastMode1 = server.arg("mode1");
        Serial.print("Mode choisi : ");
        Serial.println(lastMode1);
        textX1 = LARGEUR_MATRICE;
        
        lastText2 = normalizeText(urlDecode(server.arg("text2")));  // Conversion avant affichage avec suppression des accents
        Serial.print("Nouveau texte   : ");
        Serial.println(lastText2);
        lastColor2 = server.arg("color2");
        Serial.print("Couleur choisie : ");
        Serial.println(lastColor2);
        lastMode2 = server.arg("mode2");
        Serial.print("Mode choisi : ");
        Serial.println(lastMode2);
        textX2 = LARGEUR_MATRICE;
        
        lastText3 = normalizeText(urlDecode(server.arg("text3")));  // Conversion avant affichage avec suppression des accents
        Serial.print("Nouveau texte   : ");
        Serial.println(lastText3);
        lastColor3 = server.arg("color3");
        Serial.print("Couleur choisie : ");
        Serial.println(lastColor3);
        lastMode3 = server.arg("mode3");
        Serial.print("Mode choisi : ");
        Serial.println(lastMode3);
        textX3 = LARGEUR_MATRICE;

        lastSpeed = server.arg("sp").toInt();
        Serial.print("Vitesse choisie : ");
        Serial.println(lastSpeed);

        saveData(); // Sauvegarde SPIFFS
        server.send(200, "text/plain", "OK");
}

// Plus utilisé
void handleGetData() {
    //String json = "{\"text\":\"" + lastText + "\", \"color\":\"" + lastColor + "\", \"speed\":\"" + lastSpeed + "\", \"mode\":\"" + lastMode + "\"}";
    //server.send(200, "application/json", json);
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
uint16_t getColor1(String color) {
    if (color == "RED") return matrix1.Color(255, 0, 0);
    if (color == "GREEN") return matrix1.Color(0, 255, 0);
    if (color == "BLUE") return matrix1.Color(0, 0, 255);
    if (color == "YELLOW") return matrix1.Color(255, 255, 0);
    if (color == "CYAN") return matrix1.Color(0, 255, 255);
    if (color == "MAGENTA") return matrix1.Color(255, 0, 255);
    if (color == "WHITE") return matrix1.Color(255, 255, 255);
    return matrix1.Color(255, 255, 255); // Blanc par défaut
}
uint16_t getColor2(String color) {
    if (color == "RED") return matrix2.Color(255, 0, 0);
    if (color == "GREEN") return matrix2.Color(0, 255, 0);
    if (color == "BLUE") return matrix2.Color(0, 0, 255);
    if (color == "YELLOW") return matrix2.Color(255, 255, 0);
    if (color == "CYAN") return matrix2.Color(0, 255, 255);
    if (color == "MAGENTA") return matrix2.Color(255, 0, 255);
    if (color == "WHITE") return matrix2.Color(255, 255, 255);
    return matrix2.Color(255, 255, 255); // Blanc par défaut
}
uint16_t getColor3(String color) {
    if (color == "RED") return matrix3.Color(255, 0, 0);
    if (color == "GREEN") return matrix3.Color(0, 255, 0);
    if (color == "BLUE") return matrix3.Color(0, 0, 255);
    if (color == "YELLOW") return matrix3.Color(255, 255, 0);
    if (color == "CYAN") return matrix3.Color(0, 255, 255);
    if (color == "MAGENTA") return matrix3.Color(255, 0, 255);
    if (color == "WHITE") return matrix3.Color(255, 255, 255);
    return matrix3.Color(255, 255, 255); // Blanc par défaut
}
