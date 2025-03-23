// Panneau de LEDs flexible sur un ESP8266 avec serveur WEB
// Il permet d'afficher un message personnalisé depuis une interface WEB
// La couleur est paramétrable.
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
// Liste des polices ici : https://github.com/adafruit/Adafruit-GFX-Library/tree/master/Fonts
// #include <Fonts/FreeSans9pt7b.h>
// #include <Fonts/FreeSans8pt7b.h>

// Possible de se faire sa propre police : 
//    Il suffit de lancer cette application en ligne : https://rop.nl/truetype2gfx/
//    choisir la police à convertir
//    choisir sa dimension
//    lancer sa conversion GFX
//    copier ce fichier dans le dossier fonts de Adafruit_GXF library



// 5 variables pour modifier le comportement du programme
#define PIN_MATRICE 2         // Si ESP32, mettre obligatoirement la valeur 1 et brancher la data sur la PIN TX0 !!
                              // Si ESP8266, mettre obligatoirement la valeur 2 et brancher la data sur la PIN TXD1 soit GPI02 (D4) !!
#define LUMINOSITE 50         // Luminosité de l'affichage sachant que cela varie de 0 à 255
#define HAUTEUR_MATRICE 8     // Nombre de pixel en hauteur. On laisse 8 sur ce modèle
#define LARGEUR_MATRICE 64    // Nombre de pixel en largeur. On laisse 64 sur ce modèle (2 matrices de 32 pixels)

// Identifiant pour le HotSpot Wifi
const char* ssid = "BelEcran";
const char* password = "12345678";

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
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE,PIN_MATRICE,
  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);


String textToDisplay = "bienvenue";   // Texte par défaut


const char index_html[] PROGMEM = R"rawliteral(
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
            padding: 0;
            background-color: #f4f4f4;
        }
        .container {
            max-width: 400px;
            margin: 50px auto;
            padding: 20px;
            background: white;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        }
        h1 {
            font-size: 1.5em;
            color: #333;
        }
        input, select {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 1px solid #ccc;
            border-radius: 5px;
            font-size: 1em;
        }
        input[type="submit"] {
            background: #007BFF;
            color: white;
            border: none;
            cursor: pointer;
        }
        input[type="submit"]:hover {
            background: #0056b3;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Bel écran</h1>
        <form action="/update" method="get">
            <input type="text" name="text" placeholder="Entrez votre message" required>
            <select name="color">
                <option value="red">Rouge</option>
                <option value="green">Vert</option>
                <option value="blue">Bleu</option>
                <option value="yellow">Jaune</option>
                <option value="cyan">Cyan</option>
                <option value="magenta">Magenta</option>
                <option value="white">Blanc</option>
            </select>
            <input type="submit" value="Envoyer">
        </form>
    </div>
</body>
</html>
)rawliteral";




unsigned long lastUpdate = 0;
const int scrollSpeed = 100;  // Temps en millisecondes entre chaque déplacement
int textX = LARGEUR_MATRICE;
uint16_t textColor = matrix.Color(255, 0, 0);  // Rouge par défaut


void setup(){  
  Serial.begin(9600);
  delay(1000);
  Serial.println("Démarrage ESP8266....");

  matrix.begin();
  Serial.println("Initialisation de la matrice");
  matrix.setTextWrap(false);
  matrix.setBrightness(LUMINOSITE);
  matrix.setTextColor(textColor); // Couleur par defaut
  //matrix.setFont(&FreeSans8pt7b);  // Utilisation de la police avec accents
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

  // Servir la page HTML principale et le WS
  server.on("/", HTTP_GET, handleRoot);
  server.on("/update", HTTP_GET, handleUpdate);

  // Start server
  server.begin();
  Serial.println("Serveur web démarré !");

  delay(1000);
}

void loop() {
  server.handleClient();

  if (millis() - lastUpdate > scrollSpeed) {
    lastUpdate = millis();
    //Scrolling text startes here
    matrix.fillScreen(0);
    //matrix.setCursor(textX, HAUTEUR_MATRICE-1);
    matrix.setCursor(textX, 0);
    matrix.setTextColor(textColor);
    matrix.print(textToDisplay);
       
    textX--;
    if (textX < -((int)textToDisplay.length() * 6)) {
        textX = LARGEUR_MATRICE;
    }

    matrix.show();
  }
}


void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleUpdate() {
    if (server.hasArg("text") && server.hasArg("color")) {
        textToDisplay = normalizeText(urlDecode(server.arg("text")));  // Conversion avant affichage avec suppression des accents
        Serial.println(textToDisplay);
        textColor = getColor(server.arg("color"));
        textX = LARGEUR_MATRICE;
        server.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='2;url=/' /></head><body><p>Texte et couleur mis à jour ! Retour à l'accueil...</p></body></html>");
    } else {
        server.send(400, "text/plain", "Erreur : paramètres manquants.");
    }
}



// Fonction pour convertir le nom de couleur en RGB
uint16_t getColor(String color) {
    if (color == "red") return matrix.Color(255, 0, 0);
    if (color == "green") return matrix.Color(0, 255, 0);
    if (color == "blue") return matrix.Color(0, 0, 255);
    if (color == "yellow") return matrix.Color(255, 255, 0);
    if (color == "cyan") return matrix.Color(0, 255, 255);
    if (color == "magenta") return matrix.Color(255, 0, 255);
    if (color == "white") return matrix.Color(255, 255, 255);
    return matrix.Color(255, 255, 255); // Blanc par défaut
}


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
    input.replace(" ", " ");
    input.replace("’", "'");
    input.replace("«", "\"");
    input.replace("»", "\"");
    input.replace("Œ", "OE");
    return input;
}
