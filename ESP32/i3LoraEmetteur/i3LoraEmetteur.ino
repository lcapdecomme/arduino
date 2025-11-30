/* Lora Emetteur i3 - envoi d'un message au click d'un bouton
 *  Version ESP32
 *  
 *  La librairie à installer : LoRa_E32 by Renzo Mischianti
 *  
 *  BRANCHEMENT BOUTON POUSSOIR :
 *  - Une patte du bouton -> GPIO 32
 *  - Autre patte du bouton -> GND
 *  (La résistance pull-up interne est activée dans le code)
 */
#include "LoRa_E32.h"

// Configuration des broches
#define SW_PIN  32  // Bouton poussoir (ou bouton du joystick pour les tests)
#define LED_PIN 2   // LED locale pour feedback visuel

// Clé de sécurité (doit être identique sur émetteur et récepteur)
String securityKey = "SECURE123";

// Broches ESP32 vers module LoRa UART
#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4

HardwareSerial loraSerial(1);
LoRa_E32 e32ttl100(&loraSerial, LORA_AUX);

// Variables pour anti-rebond
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200; // 200ms d'anti-rebond

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialisation du module LoRa
  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e32ttl100.begin();

  Serial.println("=================================");
  Serial.println("  Émetteur i3 - Prêt !");
  Serial.println("=================================");
  Serial.println("Appuyez sur le bouton pour envoyer un signal");
}

void loop() {
  // Lecture de l'état du bouton (LOW = appuyé car INPUT_PULLUP)
  bool pressed = (digitalRead(SW_PIN) == LOW);

  // Si bouton appuyé et délai d'anti-rebond respecté
  if (pressed && (millis() - lastButtonPress > debounceDelay)) {
    lastButtonPress = millis();
    
    // Feedback visuel : allume la LED locale
    digitalWrite(LED_PIN, HIGH);
    
    // Construction et envoi du message
    String message = buildMessage();
    sendSecureMessage(message);
    
    Serial.println("✓ Message envoyé : " + message);
    
    // LED reste allumée 200ms pour feedback
    delay(200);
    digitalWrite(LED_PIN, LOW);
    
    // Attente que le bouton soit relâché
    while(digitalRead(SW_PIN) == LOW) {
      delay(10);
    }
  }
}

// Construction du message avec timestamp pour éviter les replays
String buildMessage() {
  unsigned long nowSec = millis() / 1000;
  String timestamp = String(nowSec);
  return securityKey + ":" + timestamp + ":TRIGGER";
}

// Envoi du message chiffré
void sendSecureMessage(String message) {
  String encoded = xorEncrypt(message, 'K');
  e32ttl100.sendMessage(encoded);
}

// Chiffrement XOR simple
String xorEncrypt(String data, char key) {
  String result = "";
  for (int i = 0; i < data.length(); i++) {
    result += (char)(data[i] ^ key);
  }
  return result;
}
