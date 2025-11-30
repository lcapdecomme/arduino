#include <TFT_eSPI.h>
#include <SPI.h>

// Pins pour l'écran gauche
#define TFT1_CS   5
#define TFT1_RST  4
#define TFT1_DC   2

// Pins pour l'écran droit
#define TFT2_CS   15
#define TFT2_RST  16
#define TFT2_DC   2  // Partagé

// Pins SPI partagés
#define TFT_MOSI  23
#define TFT_SCLK  18

// Créer deux instances TFT
TFT_eSPI tft1 = TFT_eSPI();
TFT_eSPI tft2 = TFT_eSPI();

// Couleurs
#define COLOR_BACKGROUND 0x0000  // Noir
#define COLOR_SCLERA     0xFFFF  // Blanc
#define COLOR_IRIS       0xF800  // Rouge sang
#define COLOR_IRIS_DARK  0xC000  // Rouge sombre
#define COLOR_PUPIL      0x0000  // Noir
#define COLOR_VEINS      0xD000  // Rouge foncé

// Paramètres des yeux
const int screenWidth = 240;
const int screenHeight = 240;
const int eyeCenterX = 120;
const int eyeCenterY = 120;

// Variables d'animation
int pupilX = 0;
int pupilY = 0;
int targetPupilX = 0;
int targetPupilY = 0;
unsigned long lastMoveTime = 0;

// Veines statiques
struct Vein {
  int x1, y1, x2, y2;
};
Vein veins[6];

void setup() {
  Serial.begin(115200);
  Serial.println("Démarrage...");
  
  // Initialiser les pins
  pinMode(TFT1_CS, OUTPUT);
  pinMode(TFT1_RST, OUTPUT);
  pinMode(TFT2_CS, OUTPUT);
  pinMode(TFT2_RST, OUTPUT);
  
  digitalWrite(TFT1_CS, HIGH);
  digitalWrite(TFT2_CS, HIGH);
  
  // Reset des écrans
  digitalWrite(TFT1_RST, LOW);
  digitalWrite(TFT2_RST, LOW);
  delay(100);
  digitalWrite(TFT1_RST, HIGH);
  digitalWrite(TFT2_RST, HIGH);
  delay(100);
  
  // Configurer SPI à vitesse maximale
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
  SPI.setFrequency(40000000);
  
  // Initialiser écran 1
  Serial.println("Init écran 1...");
  digitalWrite(TFT1_CS, LOW);
  tft1.init();
  tft1.setRotation(0);
  digitalWrite(TFT1_CS, HIGH);
  
  // Initialiser écran 2
  Serial.println("Init écran 2...");
  digitalWrite(TFT2_CS, LOW);
  tft2.init();
  tft2.setRotation(0);
  digitalWrite(TFT2_CS, HIGH);
  
  // Générer les veines une seule fois
  randomSeed(analogRead(0));
  veins[0] = {50, 80, 90, 100};
  veins[1] = {190, 80, 150, 100};
  veins[2] = {80, 50, 100, 90};
  veins[3] = {160, 50, 140, 90};
  veins[4] = {70, 170, 100, 150};
  veins[5] = {170, 170, 140, 150};
  
  // Dessiner les yeux initiaux
  drawStaticEye(tft1, TFT1_CS);
  drawStaticEye(tft2, TFT2_CS);
  
  delay(100);
  Serial.println("Yeux initialisés!");
}

void drawStaticEye(TFT_eSPI &tft, int csPin) {
  digitalWrite(csPin, LOW);
  
  // Fond noir
  tft.fillScreen(COLOR_BACKGROUND);
  
  // Dessiner le blanc de l'œil (une seule fois)
  tft.fillCircle(eyeCenterX, eyeCenterY, 100, COLOR_SCLERA);
  
  // Dessiner les veines (une seule fois)
  for (int i = 0; i < 6; i++) {
    tft.drawLine(veins[i].x1, veins[i].y1, veins[i].x2, veins[i].y2, COLOR_VEINS);
  }
  
  digitalWrite(csPin, HIGH);
}

void drawIrisOnly(TFT_eSPI &tft, int csPin, int x, int y, bool erase) {
  digitalWrite(csPin, LOW);
  
  if (erase) {
    // Effacer en dessinant un cercle blanc
    tft.fillCircle(x, y, 48, COLOR_SCLERA);
    
    // Redessiner les veines dans cette zone
    for (int i = 0; i < 6; i++) {
      if ((abs(veins[i].x1 - x) < 60 && abs(veins[i].y1 - y) < 60) ||
          (abs(veins[i].x2 - x) < 60 && abs(veins[i].y2 - y) < 60)) {
        tft.drawLine(veins[i].x1, veins[i].y1, veins[i].x2, veins[i].y2, COLOR_VEINS);
      }
    }
  } else {
    // Dessiner l'iris
    tft.fillCircle(x, y, 45, COLOR_IRIS);
    tft.fillCircle(x - 3, y - 3, 40, COLOR_IRIS_DARK);
    
    // Dessiner la pupille
    tft.fillCircle(x, y, 25, COLOR_PUPIL);
    
    // Reflet lumineux parfaitement centré
    tft.fillCircle(x, y, 8, TFT_WHITE);
  }
  
  digitalWrite(csPin, HIGH);
}

void updateEyePosition(TFT_eSPI &tft, int csPin, int newX, int newY, int oldX, int oldY) {
  // Limiter le mouvement
  if (newX < eyeCenterX - 30) newX = eyeCenterX - 30;
  if (newX > eyeCenterX + 30) newX = eyeCenterX + 30;
  if (newY < eyeCenterY - 30) newY = eyeCenterY - 30;
  if (newY > eyeCenterY + 30) newY = eyeCenterY + 30;
  
  // Si le mouvement est trop petit, ne rien faire
  if (abs(newX - oldX) < 3 && abs(newY - oldY) < 3) {
    return;
  }
  
  // Effacer l'ancienne position
  drawIrisOnly(tft, csPin, oldX, oldY, true);
  
  // Dessiner à la nouvelle position
  drawIrisOnly(tft, csPin, newX, newY, false);
}

void loop() {
  unsigned long currentTime = millis();
  
  // Mouvement des yeux toutes les 2-4 secondes
  if (currentTime - lastMoveTime > random(3000, 5²000)) {
    int newTargetX = random(-25, 25);
    int newTargetY = random(-25, 25);
    
    // Calculer les positions réelles
    int oldIrisX = eyeCenterX + pupilX;
    int oldIrisY = eyeCenterY + pupilY;
    
    // Mettre à jour directement (pas d'interpolation = pas de scintillement)
    pupilX = newTargetX;
    pupilY = newTargetY;
    targetPupilX = newTargetX;
    targetPupilY = newTargetY;
    
    int newIrisX = eyeCenterX + pupilX;
    int newIrisY = eyeCenterY + pupilY;
    
    // Mise à jour des deux écrans
    updateEyePosition(tft1, TFT1_CS, newIrisX, newIrisY, oldIrisX, oldIrisY);
    updateEyePosition(tft2, TFT2_CS, newIrisX, newIrisY, oldIrisX, oldIrisY);
    
    lastMoveTime = currentTime;
  }
  
  delay(50);
}
