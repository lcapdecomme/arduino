/* Lora Emetteur - envoi d'un message au click d'un joystick
 *  Version ESP32
 *  
 *  La librairie à installer : LoRa_E32 by Renzo Mischianti
 *  
 */
#include "LoRa_E32.h"

#define VRX_PIN 34
#define VRY_PIN 35
#define SW_PIN  32
#define LED_PIN 2  // LED locale

String securityKey = "SECURE123"; // même clé que l’émetteur

// Broches ESP32 vers module LoRa UART
// RX: reçoit depuis LoRa (donc TX du module)
// TX: envoie vers LoRa (donc RX du module)
#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4

HardwareSerial loraSerial(1);
LoRa_E32 e32ttl100(&loraSerial, LORA_AUX);

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e32ttl100.begin();

  Serial.println("Émetteur prêt !");
}

void loop() {
  int x = analogRead(VRX_PIN);
  int y = analogRead(VRY_PIN);
  bool pressed = (digitalRead(SW_PIN) == LOW);

  if (pressed) {
      // Allume la LED locale si le bouton est appuyé
      digitalWrite(LED_PIN, HIGH);
    
      // Encode le message et l'envoie
      String message = buildMessage(1);
      sendSecureMessage(message);
      Serial.println("Message envoyé : " + message); 
      delay(200);    
      digitalWrite(LED_PIN, LOW);
  }
}


String buildMessage(int count) {
  unsigned long nowSec = millis() / 1000;
  String timestamp = String(nowSec);
  return securityKey + ":" + timestamp + ":" + String(count);
}

void sendSecureMessage(String message) {
  // Chiffrement simple XOR
  String encoded = xorEncrypt(message, 'K');
  e32ttl100.sendMessage(encoded);
}

String xorEncrypt(String data, char key) {
  String result = "";
  for (int i = 0; i < data.length(); i++) {
    result += (char)(data[i] ^ key);
  }
  return result;
}
