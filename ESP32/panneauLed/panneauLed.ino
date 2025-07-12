// Panneau de test d'affichage d'un texte simple sur une matrice de 2 LEDs flexible avec un ESP32 

// NE FONCTIONNE PAS !!

// Branchement sur un ESP32
// Fil rouge Matrice => vim en haut à droite
// Fil Vert Data Matrice => TX0 en bas à gauche 
// Fil Blanc masse Matrice => GND en haut à droite


#include <Adafruit_GFX.h>
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
#define PIN_MATRICE 5         // Si ESP32, mettre obligatoirement la valeur 1 et brancher la data sur la PIN TX0 !!
#define LUMINOSITE 50         // Luminosité de l'affichage sachant que cela varie de 0 à 255
// Ces valeurs doivent correspondre précisement à la taille des matrices 
#define HAUTEUR_MATRICE 8     // Nombre de pixel en hauteur. On laisse 8 sur ce modèle
#define LARGEUR_MATRICE 32   // Nombre de pixel en largeur. On laisse 64 sur ce modèle (2 matrices de 32 pixels)
                              // Mais il faudrait laisser 32 



// Ne pas toucher : la taille se calcule automatiquement en fonction du nombre de matrice verticale
#define TAILLE_TEXTE NOMBRE_H


// ****** Début du programme ***********

// Initialisation de la matrice 2x32 
// C'est la solution lorsqu'on a plus d'une matrice
// Exemple et documentation ici : https://github.com/adafruit/Adafruit_NeoMatrix/blob/master/examples/tiletest/tiletest.ino
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE, NOMBRE_L, NOMBRE_H, PIN_MATRICE,
  NEO_TILE_BOTTOM   + NEO_TILE_RIGHT   + NEO_TILE_COLUMNS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

unsigned long lastUpdate = 0;
int scrollSpeed = 100;  // Temps en millisecondes entre chaque déplacement
int textX = LARGEUR_MATRICE*NOMBRE_H*NOMBRE_L/TAILLE_TEXTE;
// uint16_t textColor = matrix.Color(255, 0, 0);  // Rouge par défaut

String lastText = "bienvenue";    // Texte par défaut

void setup(){  

  Serial.begin(115200);
  Serial.println("Démarrage ESP32 ....");
  delay(1000);
  matrix.begin();
  Serial.println("Initialisation de la matrice");
  matrix.setTextWrap(false);
  matrix.setBrightness(LUMINOSITE);
  matrix.setTextColor(matrix.Color(255, 0, 0)); // Couleur par defaut
  // Definir la taille des caractères 
  matrix.setTextSize(TAILLE_TEXTE);
  Serial.println("Démarrage de la matrice....");
  delay(1000);
}

void loop() {
  
  delay(1000);

      Serial.println("Matrice  1 ...");
          matrix.fillScreen(0);
    Serial.println("Matrice  2 ...");
        matrix.setCursor(0, 0);
    Serial.println("Matrice  3 ...");
        matrix.print(lastText);
    Serial.println("Matrice  5 ...");
noInterrupts();     // désactive les interruptions
    Serial.println("Matrice  51 ...");
//matrix.show();
    Serial.println("Matrice  52 ...");

    
  delay(1000);
  }
