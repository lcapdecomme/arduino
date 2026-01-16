/*
 * Test mélodie Indiana Jones pour buzzer passif
 * Brancher le buzzer sur GPIO 18 et GND
 */

#define PIN_BUZZER 18

// Notes (fréquences en Hz)
// c = 262; d = 294; e = 330; f = 349; g = 392; a = 440; b = 494; 
// c2 = 523; d2 = 587; e2 = 659; f2 = 698;

const int notes[] = {
  330, 349,                                      // Takt 1
  392, 523, 294, 330,                            // Takt 2
  349, 392, 440,                                 // Takt 3
  494, 698, 440, 494,                            // Takt 4
  523, 587, 659, 330, 349,                       // Takt 5
  392, 523, 587, 659,                            // Takt 6
  698, 392, 392,                                 // Takt 7
  659, 587, 392, 659, 587, 392,                  // Takt 8
  659, 587, 392, 698, 659, 587,                  // Takt 9
  523, 330, 392,                                 // Takt 10
  349, 294, 349,                                 // Takt 11
  330, 392, 523, 330, 392,                       // Takt 12
  349, 294, 349,                                 // Takt 13
  330, 294, 262, 330, 392,                       // Takt 14
  349, 294, 349,                                 // Takt 15
  330, 392, 523, 392, 392,                       // Takt 16
  659, 587, 392, 659, 587, 392,                  // Takt 17
  659, 587, 392, 698, 659, 587,                  // Takt 18
  523                                            // Takt 19
};

// Durées en millisecondes
const int durations[] = {
  300, 100,                                      // Takt 1
  400, 800, 300, 100,                            // Takt 2
  1200, 300, 100,                                // Takt 3
  400, 800, 300, 100,                            // Takt 4
  400, 400, 400, 300, 100,                       // Takt 5
  400, 800, 300, 100,                            // Takt 6
  1200, 300, 100,                                // Takt 7
  400, 300, 100, 400, 300, 100,                  // Takt 8
  400, 300, 100, 400, 300, 100,                  // Takt 9
  1200, 300, 100,                                // Takt 10
  1200, 300, 100,                                // Takt 11
  100, 100, 1000, 300, 100,                      // Takt 12
  1200, 300, 100,                                // Takt 13
  100, 100, 1000, 300, 100,                      // Takt 14
  1200, 300, 100,                                // Takt 15
  100, 100, 1000, 300, 100,                      // Takt 16
  400, 300, 100, 400, 300, 100,                  // Takt 17
  400, 300, 100, 400, 300, 100,                  // Takt 18
  800                                            // Takt 19
};

void playIndianaJones() {
  int numberOfNotes = sizeof(notes) / sizeof(notes[0]);

  for (int i = 0; i < numberOfNotes; i++) {
    tone(PIN_BUZZER, notes[i], durations[i]);
    delay(durations[i]);
    noTone(PIN_BUZZER);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER, OUTPUT);
  
  Serial.println("Indiana Jones Theme!");
  playIndianaJones();
  Serial.println("Fin!");
}

void loop() {
  // Appuyer sur reset pour rejouer
}
