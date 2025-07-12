#define BUZZER_PIN 3  // Pin du buzzer
#define LED1_PIN 9    // Première LED
#define LED2_PIN 10   // Deuxième LED

// Fréquences des notes (en Hz)
#define NOTE_C4  262
#define NOTE_E4  330
#define NOTE_G4  392
#define NOTE_C5  523
#define NOTE_A4  440
#define NOTE_F4  349
#define NOTE_D4  294
#define NOTE_B4  494

// Mélodie dynamique
int melody[] = {
  NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, 
  NOTE_A4, NOTE_F4, NOTE_D4, NOTE_B4, 
  NOTE_C5, NOTE_G4, NOTE_E4, NOTE_C4
};

int durations[] = {
  150, 150, 150, 300, 
  150, 150, 150, 300, 
  200, 150, 150, 300
};

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
}

void loop() {
  for (int i = 0; i < 12; i++) {
    int noteDuration = durations[i];

    // Joue la note
    tone(BUZZER_PIN, melody[i], noteDuration);

    // Alterne les LEDs en rythme
    if (i % 2 == 0) {
      digitalWrite(LED1_PIN, HIGH);
      digitalWrite(LED2_PIN, LOW);
    } else {
      digitalWrite(LED1_PIN, LOW);
      digitalWrite(LED2_PIN, HIGH);
    }

    delay(noteDuration);  // Attend la durée de la note
    noTone(BUZZER_PIN);   // Stoppe le son après chaque note
    delay(50);            // Petit délai entre les notes
  }
}
