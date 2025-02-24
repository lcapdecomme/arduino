// Projet panneau de LEDs 
// V1 : 11/1/2024 : PRogramme de démonstration de changement de couleur
// url démo : https://www.youtube.com/watch?v=8UhAqc5VQxE
// Code source : https://lasdi25.blogspot.com/search?q=arduino
// Url du projet : https://lasdi25.blogspot.com/2021/09/46-arduino-comment-commander-un-panneau.html
// Les programmes sont : https://www.lasdi.com/arduino5.html

// Librairies à utiliser : 
// Adafruit_neopixel : https://github.com/adafruit/Adafruit_NeoPixel/releases



#include "Adafruit_NeoPixel.h"

// Nombre de led : 256, Pin 8, Pilotage des leds NEO_KHZ800
Adafruit_NeoPixel led(512,2, NEO_GRB + NEO_KHZ800);


void setup() {
  led.begin();
  led.setBrightness(5);
  led.show();
  delay(1000);
}


// Affiche une animation 
void loop() {
  couleurs(10);
}

void couleurs(int temps) {
  for (long PremiereLedCouleur=0; PremiereLedCouleur<65536; PremiereLedCouleur+=256) {
    for (int i=0; i<256; i++) { 
      int ledTeinte = PremiereLedCouleur + (i * 65536 / 256); 
      led.setPixelColor (i, led.gamma32(led.ColorHSV(ledTeinte)));
    }
    led.show();
    delay (temps);
  }
}


// Affiche une animation 
void loop6() {
  couleurspas(100);
}

void couleurspas(int temps) {
  int PremiereLedCouleur = 0; 
  for (int a=0; a<30; a++) {
    for (int b=0; b<3; b++) { 
      led.clear();
      for (int c=b; c<256; c += 9) {    
          int teinte = PremiereLedCouleur + c * 65536 / led.numPixels (); 
          uint32_t couleur = led.gamma32(led.ColorHSV(teinte)); 
          led.setPixelColor (c, couleur);
      }
      led.show();
      delay (temps);
      PremiereLedCouleur += 65536 / 30;
    }
  }
}


// Affiche une animation 
void loop5() {
  uint32_t rouge = led.Color (255, 0, 0);
  uint32_t magenta = led.Color (255, 0, 255); 
  uint32_t vert = led.Color (0, 255, 0);
  uint32_t bleu = led.Color (0, 0, 255);
  tourne3 (rouge, 80);
  tourne3 (magenta, 80);  
  tourne3 (vert, 80);
  tourne3 (bleu, 80);
}

void tourne3 (uint32_t couleur, uint8_t temp) {
  for (int q=0; q<8; q++) {
    for (uint16_t i=0; i <= 255; i=i+8) {
      led.setPixelColor(i+q,couleur);
    }
    led.show();
    delay(temp);
    for(uint16_t i=0;i<=255; i=i+8) {
      led.setPixelColor(i+q,0);
    }
  }
}


// Affiche le drapeau Francais : 10 col. en bleu, 10 col. en blanc, 10 col. en rouge, 
void loop4() {
  led.fill(led.Color(0,0,255),168,80);
  led.show();
  delay(500);
  led.fill(led.Color(255,255,255),88,80);
  led.show();
  delay(500);
  led.fill(led.Color(255,0,0),8,80);
  led.show();
  delay(3000);
  led.clear();
  led.show();
  delay(1000);
}



// Affiche 2 colonnes de led en bleue
void loop3() {
  // Position de la première led à colorier : 120
  // Nombre de led à colorier : 16
  led.fill(led.Color(0,0,255),120,16);
  led.show();
}


// Affiche les leds en zigzag
void loop2() {
  // Efface le panneau
  led.clear();
  for (int pixel=0;pixel<255;pixel++) {
    // Affiche les leds une par une en couleur RGB Verte en faible intensité
    led.setPixelColor(pixel, led.Color(0,5,0));
    led.show();
    delay(50); 
  }
}

// Affiche la led 0 en rouge
void loop1() {

  // Affiche la led 0 couleur RGB Rouge en faible intensité
  led.setPixelColor(0, led.Color(10,0,0));
  led.show(); 
}
