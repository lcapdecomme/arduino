// Projet panneau de LEDs 
// V2 : 11/1/2024 : PRogramme de démonstration pour écrire sur un panneau de leds
// url démo : https://www.youtube.com/watch?v=nmb2hDd9Ju4
// Url du projet : https://lasdi25.blogspot.com/2021/09/46-arduino-comment-commander-un-panneau.html
// Les programmes sont : https://www.lasdi.com/arduino5.html

// Librairies à utiliser : 
// Adafruit_neopixel : https://github.com/adafruit/Adafruit_NeoPixel/releases
// Adafruit_gfx : https://github.com/adafruit/Adafruit-GFX-Library/releases
// Adafruit_neomatrix : https://github.com/adafruit/Adafruit_NeoMatrix/releases


#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// Largeur : 32 pixels
// hauteur : 8 pixels
// pin num 8
// NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  : Première led en bas à droite 
// NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG : branchement en colonne de type zigzag
// NEO_GRB + NEO_KHZ800 : type de led

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(64,8,8,
  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

    
void setup(){
  // Initialise la matrice
  matrix.begin();
  //Pas de débordement e la matrice
  matrix.setTextWrap(false);
  matrix.setBrightness(10);
}

// Le texte arrive part la droite et défile vers la gauche
void loopOk() {
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(0, 0, 200));
  // Position 64 à -30
  for(int x=64; x>=-68; x-=1) { 
    matrix.setCursor(x, 1); 
    matrix.print("Salut Bernie"); 
    matrix.show();
    delay(80);
    matrix.fillScreen(0);
  }
}





// Texte en une seule fois
void loop() {
  // Effacement de la matrice
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(255, 255, 255));
  matrix.setCursor(2, 1); 
  matrix.print("Hello"); 
  matrix.show();
  delay(2000);
}



// Lettres A, puis B, puis C,,puis D, puis E
void loop2() {
  // Effacement de la matrice
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(200, 0, 0));
  matrix.setCursor(2, 1); 
  matrix.print("A"); 
  matrix.show();
  delay(1000);
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(200, 0, 0));
  matrix.setCursor(8, 1); 
  matrix.print("B"); 
  matrix.show();
  delay(1000);
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(200, 0, 0));
  matrix.setCursor(14, 1); 
  matrix.print("C"); 
  matrix.show();
  delay(1000);
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(200, 0, 0));
  matrix.setCursor(20, 1); 
  matrix.print("D"); 
  matrix.show();
  delay(1000);
  matrix.fillScreen(0);
  matrix.setTextColor(matrix.Color(200, 0, 0));
  matrix.setCursor(26, 1);
  matrix.print("E"); 
  matrix.show();
  delay(1000);
}



// Lettre A à gauche
void loop1() {
  // Effacement de la matrice
  matrix.fillScreen(0);
  // Couleur du texte
  matrix.setTextColor(matrix.Color(200, 0, 0));
  // Position du curseur en haut à gauche
  matrix.setCursor(0, 0); 
  // Ecriture d'une lettre A
  matrix.print("A"); 
  matrix.show();
  delay(2000);
}
