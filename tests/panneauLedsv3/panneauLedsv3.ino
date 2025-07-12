// Adafruit_NeoMatrix example for tiled NeoPixel matrices.  Scrolls
// 'Howdy' across three 10x8 NeoPixel grids that were created using
// NeoPixel 60 LEDs per meter flex strip.

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#ifndef PSTR
 #define PSTR // Make Arduino Due happy
#endif

// MATRIX DECLARATION:
// Parameter 1 = width of EACH NEOPIXEL MATRIX (not total display)
// Parameter 2 = height of each matrix
// Parameter 3 = number of matrices arranged horizontally
// Parameter 4 = number of matrices arranged vertically
// Parameter 5 = pin number (most are valid)
// Parameter 6 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the FIRST MATRIX; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs WITHIN EACH MATRIX are
//     arranged in horizontal rows or in vertical columns, respectively;
//     pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns WITHIN
//     EACH MATRIX proceed in the same order, or alternate lines reverse
//     direction; pick one.
//   NEO_TILE_TOP, NEO_TILE_BOTTOM, NEO_TILE_LEFT, NEO_TILE_RIGHT:
//     Position of the FIRST MATRIX (tile) in the OVERALL DISPLAY; pick
//     two, e.g. NEO_TILE_TOP + NEO_TILE_LEFT for the top-left corner.
//   NEO_TILE_ROWS, NEO_TILE_COLUMNS: the matrices in the OVERALL DISPLAY
//     are arranged in horizontal rows or in vertical columns, respectively;
//     pick one or the other.
//   NEO_TILE_PROGRESSIVE, NEO_TILE_ZIGZAG: the ROWS/COLUMS OF MATRICES
//     (tiles) in the OVERALL DISPLAY proceed in the same order for every
//     line, or alternate lines reverse direction; pick one.  When using
//     zig-zag order, the orientation of the matrices in alternate rows
//     will be rotated 180 degrees (this is normal -- simplifies wiring).
//   See example below for these values in action.
// Parameter 7 = pixel type flags, add together as needed:
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 pixels)
//   NEO_GRB     Pixels are wired for GRB bitstream (v2 pixels)
//   NEO_KHZ400  400 KHz bitstream (e.g. FLORA v1 pixels)
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)

// Example with three 10x8 matrices (created using NeoPixel flex strip --
// these grids are not a ready-made product).  In this application we'd
// like to arrange the three matrices side-by-side in a wide display.
// The first matrix (tile) will be at the left, and the first pixel within
// that matrix is at the top left.  The matrices use zig-zag line ordering.
// There's only one row here, so it doesn't matter if we declare it in row
// or column order.  The matrices use 800 KHz (v2) pixels that expect GRB
// color data.

#define PIN 2
#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 32
#define NUM_TILES 2  

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(MATRIX_WIDTH, MATRIX_HEIGHT, NUM_TILES, 1,    PIN,
  NEO_TILE_BOTTOM   + NEO_TILE_LEFT   + NEO_TILE_ROWS   + NEO_TILE_PROGRESSIVE +
  NEO_MATRIX_TOP + NEO_MATRIX_RIGHT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

const char text[] = "HELLO";
unsigned long previousMillis = 0;
unsigned long previousScrollMillis = 0;
int effectIndex = 0;
int scrollX = MATRIX_WIDTH;
int scrollY = -8;

// Variable pour le deffilement en travers
int x    = -8;
int y    = -8;
int pass = 0;


// Définir ici le nombre total d'effets
const int TOTAL_EFFECTS = 7;

// Prototype des fonctions
void displayStaticText();
void scrollTextHorizontally();
void scrollTextVertically();
void rainbowText();
void changeFont();
void differentLetterColors();
void testDefilement();

// Tableau de pointeurs vers les fonctions
void (*effects[])() = {
  displayStaticText,
  scrollTextHorizontally,
  scrollTextVertically,
  rainbowText,
  changeFont,
  differentLetterColors,
  testDefilement
};

void setup() {
  matrix.begin();
  matrix.setRotation(1);  // Ajout de la rotation
  matrix.setTextWrap(false);
  matrix.setBrightness(50);
}

void loop() {
  unsigned long currentMillis = millis();

  // Changer d'effet toutes les 5 secondes
  if (currentMillis - previousMillis >= 3000) {  
    previousMillis = currentMillis;
    effectIndex = (effectIndex + 1) % TOTAL_EFFECTS;  
    scrollX = MATRIX_WIDTH;  // Réinitialisation pour le scroll horizontal
    scrollY = -8;            // Réinitialisation pour le scroll vertical
  }

  matrix.fillScreen(0);

  // Appeler dynamiquement la fonction d'effet actuelle
  effects[effectIndex]();

  matrix.show();
}

// Effet 1: Affichage statique en rouge
void displayStaticText() {
  matrix.setCursor(2, 6);
  matrix.setTextColor(matrix.Color(255, 0, 0));  // Rouge
  matrix.print("HELLO");
}

// Effet 2: Défilement horizontal 
void scrollTextHorizontally() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousScrollMillis >= 100) {  // Accélération du scroll
    previousScrollMillis = currentMillis;
    scrollX--;
    if (scrollX < -30) scrollX = MATRIX_WIDTH;
  }

  matrix.setCursor(scrollX, 6);
  matrix.setTextColor(matrix.Color(0, 255, 0));  // Vert
  matrix.print("HELLO");
}

// Effet 3: Défilement vertical
void scrollTextVertically() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousScrollMillis >= 100) {  // Accélération du scroll
    previousScrollMillis = currentMillis;
    scrollY++;
    if (scrollY > MATRIX_HEIGHT * NUM_TILES) scrollY = -8;
  }

  matrix.setCursor(2, scrollY);
  matrix.setTextColor(matrix.Color(0, 0, 255));  // Bleu
  matrix.print("HELLO");
}

// Effet 4: Arc-en-ciel (chaque lettre change de couleur progressivement)
void rainbowText() {
  int colors[] = {matrix.Color(255, 0, 0), matrix.Color(255, 165, 0),
                  matrix.Color(255, 255, 0), matrix.Color(0, 255, 0),
                  matrix.Color(0, 0, 255), matrix.Color(75, 0, 130),
                  matrix.Color(148, 0, 211)};

  for (int i = 0; i < 5; i++) {
    matrix.setCursor(2 + (i * 6), 6);
    matrix.setTextColor(colors[(effectIndex + i) % 7]);
    matrix.print(text[i]);
  }
}

// Effet 5: Changement de taille
void changeFont() {
  matrix.fillScreen(0); // Nettoie tout avant d'afficher

  // Effacer proprement la zone pour éviter les pixels parasites
  matrix.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, 0);

  matrix.setTextColor(matrix.Color(0, 255, 0));  
  matrix.setTextSize(2);
  matrix.setCursor(0, 0);
  matrix.print("H");
  
  matrix.setTextColor(matrix.Color(0, 0, 255)); 
  matrix.setTextSize(1);
  matrix.setCursor(14, 6);
  matrix.print("ey!");
}

// Effet 6: Chaque lettre de "HELLO" avec une couleur différente
void differentLetterColors() {
  int letterColors[5] = {
    matrix.Color(255, 0, 0),   // Rouge
    matrix.Color(0, 255, 0),   // Vert
    matrix.Color(0, 0, 255),   // Bleu
    matrix.Color(255, 255, 0), // Jaune
    matrix.Color(255, 0, 255)  // Magenta
  };

  for (int i = 0; i < 5; i++) {
    matrix.setCursor(2 + (i * 6), 6);
    matrix.setTextColor(letterColors[i]);
    matrix.print(text[i]);
  }
}

/*--------------------------------------------------------------------------------------
 Texte défilant de haut gauche vers bas/droite de trois couleurs 
--------------------------------------------------------------------------------------*/

void testDefilement() {
    const uint16_t colors[] = {
  matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(0, 0, 255) };

  matrix.setTextColor(colors[0]);
  matrix.setCursor(x, y);
  matrix.print("Hello");
  ++x;
  ++y;
  if(y > 36) {
    y    = -8;
    x = -8;
    if(++pass >= 3) return;
    matrix.setTextColor(colors[pass]);
  }
  if (x > 36) {
    x = -8;
    y    = -8;
  }
  matrix.show();
  delay(100);
}


/* Test positionnement du curseur */
void test1() {
  matrix.begin();
  matrix.setBrightness(50);
  matrix.fillScreen(0);
  
  // Allume un pixel en haut à gauche de la première matrice
  matrix.drawPixel(0, 0, matrix.Color(255, 0, 0));  // Rouge

  
  // Allume un pixel en haut à gauche de la deuxième matrice
  matrix.drawPixel(8, 0, matrix.Color(0, 255, 0));  // Vert
  
  // Allume un pixel en haut à gauche de la deuxième matrice
  matrix.drawPixel(15, 31, matrix.Color(0, 0, 255));  // Bleu
  
  matrix.show();
}
