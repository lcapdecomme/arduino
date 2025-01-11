// Projet barre de LEDs 
// V2 : 4/1/2024 : Dans cette version, les LEDS s'allument en même temps et augmentent de brillance petit à petit. 
// Une fois au maximum de la brillance, elles s'éteignent toutes petit à petit en diminuant la brillance

#include <FastLED.h>

// 5 variables pour modifier le comportement du programme
#define NUM_LEDS 6            // Combien de leds 
#define DATA_PIN 8            // Port sur l'arduino 
#define MAXLUMINOSITE 10     // Luminosité maximumum sachant que cela varie de 0 à 255
#define NOMBREITERATION 5     // Nombre d'itération pour passer de la brillance 0 à MAXLUMINOSITE 
#define DELAI1  1000           // Attente en milliseconde entre changement de brillance entre deux passages
#define DELAI2  500           // Attente en milliseconde avant changement de sens 


// Definition du tableau de LED
CRGB leds[NUM_LEDS];
 
void setup() { 
    FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    delay(DELAI2);    // Bonne pratique pour protéger les LES au démarrage de l'arduino
    // On initialise toutes les leds à la couleur blanche
    for( int j = 0; j < NUM_LEDS; j++) {
      leds[j] = CRGB::White;
    }
    Serial.begin(9600);
}



// Programme final
void loop() {
      Serial.println("---");
    int bright_current = 0;  
    // Passage 1 : On augmente la luminosité de toutes les leds
    for( int i = 0; i <= NOMBREITERATION; i++) {
        Serial.println(bright_current);
        FastLED.setBrightness(bright_current);
        FastLED.show();
        delay(DELAI1);
        bright_current =  bright_current+(MAXLUMINOSITE/NOMBREITERATION);
        if (bright_current>MAXLUMINOSITE) {
          bright_current=MAXLUMINOSITE;
        }
    }
    delay(DELAI2);
    
    Serial.println("changement");
    // Passage 2 : On diminue la luminosité de toutes les leds
    for( int i = 0; i <= NOMBREITERATION; i++) {
        Serial.println(bright_current);
        FastLED.setBrightness(bright_current);
        FastLED.show();
        delay(DELAI1);
        bright_current =  bright_current-(MAXLUMINOSITE/NOMBREITERATION);
        if (bright_current<0) {
          bright_current=0;
        }
    }
    delay(DELAI2);
    Serial.println("changement");
 }
