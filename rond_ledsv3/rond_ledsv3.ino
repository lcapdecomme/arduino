// Projet de deux matrices et une barre défilante
// 
// url démo : https://www.youtube.com/watch?v=8UhAqc5VQxE
// Code source : https://lasdi25.blogspot.com/search?q=arduino
// Url du projet : https://lasdi25.blogspot.com/2021/09/46-arduino-comment-commander-un-panneau.html
// Les programmes sont : https://www.lasdi.com/arduino5.html

// Librairies à utiliser : 
// Adafruit_neopixel : https://github.com/adafruit/Adafruit_NeoPixel/releases



#include <FastLED.h>
#include "Adafruit_NeoPixel.h"


// 5 variables pour modifier le comportement de la barre de LEDS
#define NUM_LEDS 16           // Combien de leds 
#define PIN_BARRE_LEDS 8      // Port sur l'arduino de la barre défilante
#define LUMINOSITE1  50       // Luminosité maximumum sachant que cela varie de 0 à 255
#define DELAI1  100           // Attente en milliseconde entre deux leds
#define DELAI2  1000          // Attente en milliseconde avant changement de sens 

// Definition du tableau de LED
CRGB leds[NUM_LEDS];

// 4 variables pour modifier le comportement de la matrice de LEDS
#define NOMBRE_TOTAL_LEDS 512 // Port sur l'arduino de la barre défilante
#define PIN_MATRICE_LEDS 6    // Port sur l'arduino de la barre défilante
#define LUMINOSITE2   50      // Luminosité maximumum sachant que cela varie de 0 à 255
#define DEPART_LEDS   64      // Leds de départ du dessin du demi-cercle sachant que cela doit être un multiple de 16

// Nombre de led : 256, Pin 8, Pilotage des leds NEO_KHZ800
Adafruit_NeoPixel led(NOMBRE_TOTAL_LEDS, PIN_MATRICE_LEDS, NEO_GRB + NEO_KHZ800);
// 2 variables pour modifier les couleurs sur la matrice de leds
uint32_t COULEUR_HAUT =  led.Color(255, 255, 255);
uint32_t COULEUR_BAS =  led.Color(10, 10, 10);
uint32_t COULEUR_VIDE =  led.Color(0, 0, 0);

    int DEP=64;

// Tableau contenant les indices des LEDs à allumer
const uint8_t positions[] = {0, 15, 16, 31, 32, 47, 48, 63, 64, 79, 80, 95, 96, 111, 112, 127, 128, 143, 144, 159, 160, 175};
//const uint8_t positions[] = {0, 15, 16, 31, 32, 47, 48, 63, 64, 79, 80, 95, 96, 111, 112, 127, 128, 143, 144, 159, 160, 175, 176, 191, 192, 207, 208, 223, 224, 239, 240, 255};

// Initialisation du programme
void setup() {

  // Initialisation de la barre centrale de leds 
  FastLED.addLeds<WS2812B, PIN_BARRE_LEDS, RGB>(leds, NUM_LEDS);  // GRB ordering is typical

  // Initialisation des deux demi cercles 
  led.begin();

  // Partie 1 : La matrice 
  // On Efface la matrice, on règle la luminosité et on dessine les demi cercles
  led.clear();
  led.setBrightness(LUMINOSITE2);
  
  led.setPixelColor(DEPART_LEDS+0, COULEUR_HAUT); //col1
  led.setPixelColor(DEPART_LEDS+1, COULEUR_HAUT);
  
  led.setPixelColor(DEPART_LEDS+12, COULEUR_HAUT); //col2
  led.setPixelColor(DEPART_LEDS+13, COULEUR_HAUT);
  led.setPixelColor(DEPART_LEDS+14, COULEUR_HAUT);
  led.setPixelColor(DEPART_LEDS+15, COULEUR_HAUT);

  
  for (int i=16; i<21; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col3
  }

  for (int i=26; i<32; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col4
  }

  for (int i=32; i<39; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=40; i<88; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col6 => 11
  }
  
  for (int i=89; i<96; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col12
  }
  
  for (int i=96; i<102; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col13
  }
  
  for (int i=107; i<116; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col14 & col15
  }
  led.setPixelColor(DEPART_LEDS+126, COULEUR_HAUT); //col16
  led.setPixelColor(DEPART_LEDS+127, COULEUR_HAUT);
  

  led.setPixelColor(DEPART_LEDS+ 262, COULEUR_BAS); //col1
  led.setPixelColor(DEPART_LEDS+ 263, COULEUR_BAS);
  
  led.setPixelColor(DEPART_LEDS+264, COULEUR_BAS); //col2
  led.setPixelColor(DEPART_LEDS+265, COULEUR_BAS);
  led.setPixelColor(DEPART_LEDS+266, COULEUR_BAS);
  led.setPixelColor(DEPART_LEDS+267, COULEUR_BAS);
  
  for (int i=275; i<286; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_BAS); //col3&col4
  }
  
  for (int i=289; i<296; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_BAS); //col5
  }
  
  for (int i=296; i<344; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_BAS); //col6 ==> col11
  }

  for (int i=344; i<351; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_BAS); //col12
  }
  
  for (int i=354; i<365; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_BAS); //col13 et 14
  }
  for (int i=372; i<378; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_BAS); //col15 et 16
  }


        for( int i = 0; i < NUM_LEDS; i++) {
          led.setPixelColor(DEP+positions[i], COULEUR_VIDE);
        }
        
  
    led.show();
}


// Animation de la barre centrale
void loop() {
    for( int i = 0; i < NUM_LEDS; i++) {
        //for( int j = 0; j < i; j++) {
          led.setPixelColor(DEP+positions[i], COULEUR_HAUT);
        //}
        led.show();
        delay(DELAI1);
    }
    delay(DELAI2);
    for( int i = NUM_LEDS-1; i >= 0; i--) {
        for( int j = 0; j < i; j++) {
          led.setPixelColor(DEP+positions[j], COULEUR_HAUT);
        }
        for( int j = i; j < NUM_LEDS; j++) {
          led.setPixelColor(DEP+positions[j], COULEUR_VIDE);
        }
        led.show();
        delay(DELAI1);
    }
    delay(DELAI2);
 }
