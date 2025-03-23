// Projet d'une matrice avec une barre défilante
// 

#include "Adafruit_NeoPixel.h"


// 5 variables pour modifier le comportement de la barre de LEDS
#define NUM_LEDS 32           // Combien de leds 
#define PIN_BARRE_LEDS 8      // Port sur l'arduino de la barre défilante
#define LUMINOSITE1  20       // Luminosité maximumum sachant que cela varie de 0 à 255
#define DELAI1  100           // Attente en milliseconde entre deux leds
#define DELAI2  1000          // Attente en milliseconde avant changement de sens 

// 4 variables pour modifier le comportement de la matrice de LEDS
#define NOMBRE_TOTAL_LEDS 512 // Port sur l'arduino de la barre défilante
#define PIN_MATRICE_LEDS 6    // Port sur l'arduino de la barre défilante
#define LUMINOSITE2   50      // Luminosité maximumum sachant que cela varie de 0 à 255
#define DEPART_LEDS   0      // Leds de départ du dessin du demi-cercle sachant que cela doit être un multiple de 16

// Nombre de led : 256, Pin 8, Pilotage des leds NEO_KHZ800
Adafruit_NeoPixel led(NOMBRE_TOTAL_LEDS, PIN_MATRICE_LEDS, NEO_GRB + NEO_KHZ800);
// 2 variables pour modifier les couleurs sur la matrice de leds
uint32_t COULEUR_HAUT =  led.Color(255, 255, 255);
uint32_t COULEUR_BAS =  led.Color(80,80,80);


// Tableau contenant les indices des LEDs à allumer
const uint8_t positions[] = {0, 15, 16, 31, 32, 47, 48, 63, 64, 79, 80, 95, 96, 111, 112, 127, 128, 143, 144, 159, 160, 175, 176, 191, 192, 207, 208, 223, 224, 239, 240, 255};

// Initialisation du programme
void setup() {

  // Initialisation des deux demi cercles 
  led.begin();

  // Partie 1 : La matrice 
  // On Efface la matrice, on règle la luminosité et on dessine les demi cercles
  led.clear();
  led.setBrightness(LUMINOSITE2);
  
  for (int i=2; i<14; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }

  for (int i=18; i<30; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT);
  }

  for (int i=34; i<46; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }
  
  for (int i=50; i<62; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }
  
  for (int i=66; i<78; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=82; i<94; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }
  
  for (int i=98; i<110; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=114; i<126; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }
  
  for (int i=130; i<142; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=146; i<158; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=162; i<174; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=178; i<190; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }
  
  for (int i=194; i<206; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=210; i<222; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); 
  }
  
  for (int i=226; i<238; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  for (int i=242; i<254; i++) { 
    led.setPixelColor(DEPART_LEDS+i, COULEUR_HAUT); //col5
  }
  
  led.show();
}


// Animation de la barre centrale
void loop() {
    
    for( int i = 0; i < NUM_LEDS; i++) {
        //for( int j = 0; j < i; j++) {
          led.setPixelColor(DEPART_LEDS+positions[i], COULEUR_HAUT);
        //}
        led.show();
        delay(DELAI1);
    }
    delay(DELAI2);
    for( int i = NUM_LEDS-1; i >= 0; i--) {
        for( int j = 0; j < i; j++) {
          led.setPixelColor(DEPART_LEDS+positions[j], COULEUR_HAUT);
        }
        for( int j = i; j < NUM_LEDS; j++) {
          led.setPixelColor(DEPART_LEDS+positions[j], COULEUR_BAS);
        }
        led.show();
        delay(DELAI1);
    }
    delay(DELAI2);
 }
