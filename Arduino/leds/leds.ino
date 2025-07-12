// Projet barre de LEDs 
// V1 : 2/1/2024 : Dans cette version, les LEDS s'allument petit à petit en augmentant de brillance. 
// Une fois au bout de la rangée, elles s'éteignent petit à petit en diminuant la brillance

#include <FastLED.h>

// 5 variables pour modifier le comportement du programme
#define NUM_LEDS 28           // Combien de leds 
#define DATA_PIN 8           // Port sur l'arduino 
#define MAXLUMINOSITE 50     // Luminosité maximumum sachant que cela varie de 0 à 255
#define DELAI1  500           // Attente en milliseconde entre deux leds
#define DELAI2  2000          // Attente en milliseconde avant changement de sens 


// Definition du tableau de LED
CRGB leds[NUM_LEDS];
 
void setup() { 
    FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
    delay(DELAI2);    // Bonne pratique pour protéger les LES au démarrage de l'arduino
    // Serial.begin(9600);
}



// Programme final
void loop() {
    int bright_current = 0;  
    for( int i = 0; i < NUM_LEDS; i++) {
        for( int j = 0; j < i; j++) {
          leds[j] = CRGB::White;
        }
        FastLED.setBrightness(bright_current);
        FastLED.show();
        delay(DELAI1);
        bright_current =  bright_current+(MAXLUMINOSITE/NUM_LEDS);
        if (bright_current>MAXLUMINOSITE-1) {
          bright_current=MAXLUMINOSITE;
        }
    }
    delay(DELAI2);
    for( int i = NUM_LEDS-1; i >= 0; i--) {
        for( int j = 0; j < i; j++) {
          leds[j] = CRGB::White;
        }
        for( int j = i; j < NUM_LEDS; j++) {
          leds[j] = CRGB::Black;
        }
        FastLED.setBrightness(bright_current);
        FastLED.show();
        delay(DELAI1);
        bright_current =  bright_current-(MAXLUMINOSITE/NUM_LEDS);
        if (bright_current<0) {
          bright_current=0;
        }
    }
    delay(DELAI2);
 }
