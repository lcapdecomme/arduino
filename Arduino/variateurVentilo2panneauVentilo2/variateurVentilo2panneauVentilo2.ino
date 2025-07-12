
// Ok sur Arduino Nano Pin D5 avec un mosfet IRLZ24N

#define FAN_PIN 5  // Exemple : D5, broche PWM

void setup() {
  pinMode(FAN_PIN, OUTPUT);
}

void loop() {
  analogWrite(FAN_PIN, 200); // ~50% PWM (valeur 0-255 sur Nano)
  delay(5000);
  
  analogWrite(FAN_PIN, 10);   // OFF
  delay(5000);
}
