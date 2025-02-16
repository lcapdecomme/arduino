#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// Concernant le branchement : 

// modèle I2C (comme un SSD1306 0.96" ou SH1106), il a 4 broches :
//
//    VCC → 5V sur l’Arduino
//    GND → GND sur l’Arduino
//    SDA → A4 (broche I2C sur Arduino Nano)
//    SCL → A5 (broche I2C sur Arduino Nano)

// Le piezo est branché sur la pin 3


// Voici les variables que l'on peut ajuster sur ce programme 
// ----------------------------------------------------------

#define BUZZER_PIN 3  // Pin du buzzer

// Plage des fréquences (en Hz) : Cela permet de définir l'intervalle de la fréquence
#define MIN_FREQ 10000  // Fréquence minimale (on ira jamais en dessous) 
#define MAX_FREQ 20000  // Fréquence maximale (on ira jamais au dessus)

// Plage des durées (en secondes) : Cela permet de définir l'intervalle de temps d'exécution d'une fréquence
#define MIN_TIME 5      // Durée minimum d'une fréquence
#define MAX_TIME 10     // Durée maximum d'une fréquence


// Gestion de l'écran 
// On n'y touche pas
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C  // Adresse I2C de l'écran OLED

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);



void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  randomSeed(analogRead(0));  // Initialisation du générateur de nombres aléatoires

  // Initialisation de l'écran OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while (true);  // Boucle infinie si l'écran ne s'initialise pas
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
}

void loop() {
  // Génère une fréquence et une durée aléatoire
  int frequency = random(MIN_FREQ, MAX_FREQ + 1);
  int duration = random(MIN_TIME, MAX_TIME + 1);

  // Joue la fréquence
  tone(BUZZER_PIN, frequency);

  // Affiche les valeurs sur l'OLED
  for (int secondsLeft = duration; secondsLeft > 0; secondsLeft--) {
    display.clearDisplay();
    
    display.setCursor(10, 10);
    display.print("Freq: ");
    display.print(frequency);
    display.print(" Hz");
    
    display.setCursor(10, 30);
    display.print("Temps: ");
    display.print(secondsLeft);
    display.print(" sec");

    display.display();

    delay(1000);  // Attendre 1 seconde
  }

  // Arrêter le buzzer avant de recommencer
  noTone(BUZZER_PIN);
  delay(500);  // Pause avant la prochaine fréquence
}
