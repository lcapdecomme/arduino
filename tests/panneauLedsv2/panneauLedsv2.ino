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

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32,8,2,1,2,
  NEO_TILE_TOP   + NEO_TILE_LEFT   + NEO_TILE_ROWS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

const uint16_t colors[] = {
  matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(0, 0, 255) };

void setup() {
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(40);
  matrix.setTextColor(colors[0]);
}

int x    = matrix.width();
int pass = 0;

void loop() {
  matrix.fillScreen(0);
  matrix.setCursor(x, 0);
  matrix.print(F("Howdy"));
  if(--x < -36) {
    x = matrix.width();
    if(++pass >= 3) pass = 0;
    matrix.setTextColor(colors[pass]);
  }
  matrix.show();
  delay(100);
}
