#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// Pins pour les écrans ST7789 - ESP32
#define TFT_CS1    5  // GPIO5 - Chip Select écran 1 (gauche)
#define TFT_CS2   15  // GPIO15 - Chip Select écran 2 (droit)
#define TFT_RST1   4  // GPIO4 - Reset écran 1 (gauche)
#define TFT_RST2  16  // GPIO16 - Reset écran 2 (droit) - SÉPARÉ!
#define TFT_DC     2  // GPIO2 - Data/Command (partagé)
// SPI matériel : GPIO18 = SCK, GPIO23 = MOSI (par défaut)

// Création des objets pour les deux écrans avec RST séparés
Adafruit_ST7789 tftLeft = Adafruit_ST7789(TFT_CS1, TFT_DC, TFT_RST1);
Adafruit_ST7789 tftRight = Adafruit_ST7789(TFT_CS2, TFT_DC, TFT_RST2);

// Paramètres des yeux
const int SCREEN_WIDTH = 240;
const int SCREEN_HEIGHT = 240;

// Structure pour un œil
struct Eye {
  int pupilX;
  int pupilY;
  int pupilSize;
  int irisSize;
  int blinkState; // 0=ouvert, 1-10=clignement
  uint16_t irisColor;
  uint16_t pupilColor;
};

Eye leftEye, rightEye;

unsigned long lastBlink = 0;
unsigned long lastLook = 0;
unsigned long lastAnimation = 0;
int animationState = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Initialisation des yeux Halloween...");
  
  // Initialisation écran GAUCHE
  Serial.println("Init écran gauche...");
  tftLeft.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  delay(100);
  tftLeft.setRotation(0);
  tftLeft.fillScreen(ST77XX_RED);  // Test rouge
  delay(500);
  tftLeft.fillScreen(ST77XX_BLACK);
  Serial.println("Écran gauche OK");
  
  // Initialisation écran DROIT
  Serial.println("Init écran droit...");
  tftRight.init(SCREEN_WIDTH, SCREEN_HEIGHT);
  delay(100);
  tftRight.setRotation(0);
  tftRight.fillScreen(ST77XX_GREEN);  // Test vert
  delay(500);
  tftRight.fillScreen(ST77XX_BLACK);
  Serial.println("Écran droit OK");
  
  // Couleurs inquiétantes pour Halloween
  leftEye.irisColor = ST77XX_RED;      // Rouge sang
  leftEye.pupilColor = ST77XX_BLACK;
  leftEye.irisSize = 80;
  leftEye.pupilSize = 40;
  leftEye.pupilX = SCREEN_WIDTH / 2;
  leftEye.pupilY = SCREEN_HEIGHT / 2;
  leftEye.blinkState = 0;
  
  rightEye.irisColor = ST77XX_RED;
  rightEye.pupilColor = ST77XX_BLACK;
  rightEye.irisSize = 80;
  rightEye.pupilSize = 40;
  rightEye.pupilX = SCREEN_WIDTH / 2;
  rightEye.pupilY = SCREEN_HEIGHT / 2;
  rightEye.blinkState = 0;
  
  Serial.println("Démarrage des animations...");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Animation de clignement aléatoire (toutes les 2-5 secondes)
  if (currentMillis - lastBlink > random(2000, 5000)) {
    blinkBothEyes();
    lastBlink = currentMillis;
  }
  
  // Mouvement du regard (toutes les 1-3 secondes)
  if (currentMillis - lastLook > random(1000, 3000)) {
    lookAround();
    lastLook = currentMillis;
  }
  
  // Animations spéciales Halloween (toutes les 8-15 secondes)
  if (currentMillis - lastAnimation > random(8000, 15000)) {
    playHalloweenAnimation();
    lastAnimation = currentMillis;
  }
  
  // Dessiner les yeux
  drawEye(&tftLeft, &leftEye);
  drawEye(&tftRight, &rightEye);
  
  delay(50);
}

void drawEye(Adafruit_ST7789 *tft, Eye *eye) {
  // Fond noir (blanc de l'œil en noir pour effet inquiétant)
  tft->fillScreen(ST77XX_BLACK);
  
  // Dessiner des vaisseaux sanguins (lignes rouges fines)
  drawBloodVessels(tft);
  
  // Calculer la position selon l'état de clignement
  int openAmount = 240;
  if (eye->blinkState > 0) {
    openAmount = 240 - (eye->blinkState * 24);
  }
  
  // Paupières (rectangles noirs du haut et du bas)
  if (openAmount < 240) {
    int closeAmount = (240 - openAmount) / 2;
    tft->fillRect(0, 0, 240, closeAmount, ST77XX_BLACK);
    tft->fillRect(0, 240 - closeAmount, 240, closeAmount, ST77XX_BLACK);
  }
  
  if (eye->blinkState == 0) {
    // Iris (cercle rouge sang)
    tft->fillCircle(eye->pupilX, eye->pupilY, eye->irisSize, eye->irisColor);
    
    // Anneau sombre autour de l'iris
    tft->drawCircle(eye->pupilX, eye->pupilY, eye->irisSize, ST77XX_YELLOW);
    tft->drawCircle(eye->pupilX, eye->pupilY, eye->irisSize - 1, ST77XX_YELLOW);
    
    // Pupille (noire verticale allongée, style reptilien)
    drawVerticalPupil(tft, eye->pupilX, eye->pupilY, eye->pupilSize);
    
    // Reflet inquiétant (petit cercle rouge clair)
    int reflectX = eye->pupilX - eye->pupilSize / 3;
    int reflectY = eye->pupilY - eye->pupilSize / 3;
    tft->fillCircle(reflectX, reflectY, 8, 0xF800); // Rouge vif
  }
  
  // Décrémenter l'état de clignement
  if (eye->blinkState > 0) {
    eye->blinkState--;
  }
}

void drawVerticalPupil(Adafruit_ST7789 *tft, int x, int y, int size) {
  // Pupille verticale comme un chat/reptile
  int width = size / 4;
  int height = size;
  
  for (int i = -height/2; i < height/2; i++) {
    int w = width * sqrt(1 - (float)(i*i) / (float)((height/2)*(height/2)));
    tft->drawFastHLine(x - w/2, y + i, w, ST77XX_BLACK);
  }
}

void drawBloodVessels(Adafruit_ST7789 *tft) {
  // Quelques vaisseaux sanguins rouges aléatoires
  for (int i = 0; i < 5; i++) {
    int x1 = random(0, 240);
    int y1 = random(0, 240);
    int x2 = x1 + random(-40, 40);
    int y2 = y1 + random(-40, 40);
    tft->drawLine(x1, y1, x2, y2, 0x8800); // Rouge sombre
  }
}

void blinkBothEyes() {
  leftEye.blinkState = 10;
  rightEye.blinkState = 10;
}

void lookAround() {
  // Mouvement aléatoire du regard
  int direction = random(0, 8);
  int moveAmount = random(20, 40);
  
  switch(direction) {
    case 0: // Haut
      leftEye.pupilY = max(80, leftEye.pupilY - moveAmount);
      rightEye.pupilY = max(80, rightEye.pupilY - moveAmount);
      break;
    case 1: // Bas
      leftEye.pupilY = min(160, leftEye.pupilY + moveAmount);
      rightEye.pupilY = min(160, rightEye.pupilY + moveAmount);
      break;
    case 2: // Gauche
      leftEye.pupilX = max(80, leftEye.pupilX - moveAmount);
      rightEye.pupilX = max(80, rightEye.pupilX - moveAmount);
      break;
    case 3: // Droite
      leftEye.pupilX = min(160, leftEye.pupilX + moveAmount);
      rightEye.pupilX = min(160, rightEye.pupilX + moveAmount);
      break;
    default: // Retour au centre
      leftEye.pupilX = SCREEN_WIDTH / 2;
      leftEye.pupilY = SCREEN_HEIGHT / 2;
      rightEye.pupilX = SCREEN_WIDTH / 2;
      rightEye.pupilY = SCREEN_HEIGHT / 2;
      break;
  }
}

void playHalloweenAnimation() {
  int anim = random(0, 3);
  
  switch(anim) {
    case 0: // Yeux qui roulent
      rollEyes();
      break;
    case 1: // Un seul œil cligne
      leftEye.blinkState = 10;
      delay(200);
      break;
    case 2: // Dilatation rapide des pupilles
      dilateAndContract();
      break;
  }
}

void rollEyes() {
  // Faire rouler les yeux en cercle
  for (int angle = 0; angle < 360; angle += 30) {
    float rad = angle * PI / 180.0;
    int offsetX = cos(rad) * 30;
    int offsetY = sin(rad) * 30;
    
    leftEye.pupilX = SCREEN_WIDTH/2 + offsetX;
    leftEye.pupilY = SCREEN_HEIGHT/2 + offsetY;
    rightEye.pupilX = SCREEN_WIDTH/2 + offsetX;
    rightEye.pupilY = SCREEN_HEIGHT/2 + offsetY;
    
    drawEye(&tftLeft, &leftEye);
    drawEye(&tftRight, &rightEye);
    delay(50);
  }
  
  // Retour au centre
  leftEye.pupilX = SCREEN_WIDTH / 2;
  leftEye.pupilY = SCREEN_HEIGHT / 2;
  rightEye.pupilX = SCREEN_WIDTH / 2;
  rightEye.pupilY = SCREEN_HEIGHT / 2;
}

void dilateAndContract() {
  // Dilatation/contraction rapide des pupilles
  int originalSize = leftEye.pupilSize;
  
  for (int i = 0; i < 3; i++) {
    leftEye.pupilSize = 60;
    rightEye.pupilSize = 60;
    drawEye(&tftLeft, &leftEye);
    drawEye(&tftRight, &rightEye);
    delay(100);
    
    leftEye.pupilSize = 20;
    rightEye.pupilSize = 20;
    drawEye(&tftLeft, &leftEye);
    drawEye(&tftRight, &rightEye);
    delay(100);
  }
  
  leftEye.pupilSize = originalSize;
  rightEye.pupilSize = originalSize;
}
