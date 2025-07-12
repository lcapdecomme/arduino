// Panneau de test d'affichage d'un texte avec un ESP8266 sur une matrice

// Branchement sur un ESP8266
// Fil rouge Matrice => 3.3v en bas à droite
// Fil Vert Data Matrice => Attention ! on utilise TXD1 (c'est-à-dire D4) car TXD0 bug 
// Fil Blanc masse Matrice => GND en bas à droite

#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

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
#define PIN_MATRICE 1         // Si ESP32, mettre obligatoirement la valeur 1 et brancher la data sur la PIN TX0 !!
#define LUMINOSITE 50         // Luminosité de l'affichage sachant que cela varie de 0 à 255
// Ces valeurs doivent correspondre précisement à la taille des matrices 
#define HAUTEUR_MATRICE 8     // Nombre de pixel en hauteur. On laisse 8 sur ce modèle
#define LARGEUR_MATRICE 32   // Nombre de pixel en largeur. On laisse 64 sur ce modèle (2 matrices de 32 pixels)
                              // Mais il faudrait laisser 32 



// Ne pas toucher : la taille se calcule automatiquement en fonction du nombre de matrice verticale
#define TAILLE_TEXTE NOMBRE_H


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

// Matrice 2x32 verticale 
// C'est la solution lorsqu'on a plus d'une matrice
// Exemple et documentation ici : https://github.com/adafruit/Adafruit_NeoMatrix/blob/master/examples/tiletest/tiletest.ino
//Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE, NOMBRE_L, NOMBRE_H, PIN_MATRICE,
//  NEO_TILE_BOTTOM   + NEO_TILE_RIGHT   + NEO_TILE_COLUMNS   + NEO_TILE_PROGRESSIVE +
//  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
//  NEO_GRB + NEO_KHZ800);

unsigned long lastUpdate = 0;
int scrollSpeed = 100;  // Temps en millisecondes entre chaque déplacement
int textX = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
// uint16_t textColor = matrix.Color(255, 0, 0);  // Rouge par défaut

String lastText = "bienvenue";    // Texte par défaut
String lastColor =  "WHITE";      // Blanc par défaut
int lastSpeed = 3;                // Par défaut, vitesse moyenne (1 = lent, 5 = rapide)
String lastMode =  "Defilant";    // Blanc par défaut

void setup(){  

  Serial.begin(115200);
  Serial.println("Démarrage ESP32 ....");
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
}

void loop() {
  
  delay(1000);

      Serial.println("Matrice  1 ...");
          matrix.fillScreen(0);
    Serial.println("Matrice  2 ...");
        matrix.setCursor(0, 0);
    Serial.println("Matrice  3 ...");
        matrix.setTextColor(getColor(lastColor));
    Serial.println("Matrice  4 ...");
        matrix.print(lastText);
    Serial.println("Matrice  5 ...");
noInterrupts();     // désactive les interruptions
    Serial.println("Matrice  51 ...");
//matrix.show();
    Serial.println("Matrice  52 ...");
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
