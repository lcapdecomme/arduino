/* Lora Emetteur - envoi d'un message toutes les x secondes
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
#define INTERVAL 3000            // Intervalle entre envois (en ms)

String securityKey = "SECURE123"; // même clé que l’émetteur

// Broches ESP32 vers module LoRa UART
// RX: reçoit depuis LoRa (donc TX du module)
// TX: envoie vers LoRa (donc RX du module)
#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4

HardwareSerial loraSerial(1);
LoRa_E32 e32ttl100(&loraSerial, LORA_AUX);


unsigned long lastSendTime = 0;

void setup() {
  Serial.begin(115200);
  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  // Initialisation LoRa
  e32ttl100.begin();


  // Lecture de la configuration actuelle
  ResponseStructContainer c = e32ttl100.getConfiguration();
  Configuration configuration = *(Configuration*) c.data;
  Serial.println("Configuration actuelle :");
  Serial.print(F("Parity: "));
  Serial.println(configuration.SPED.uartParity);
  Serial.print(F("Air Data Rate: "));
  Serial.println(configuration.SPED.airDataRate);
  Serial.print(F("Transmission Power: "));
  Serial.println(configuration.OPTION.transmissionPower);


  // On peut modifier certains paramètres :
  configuration.ADDL = 0x01; // Adresse basse (optionnel)
  configuration.ADDH = 0x00; // Adresse haute (optionnel)
  configuration.CHAN = 0x17; // Canal (433 + 0x17 MHz = 433 + 23 = 456 MHz)

  configuration.SPED.airDataRate = AIR_DATA_RATE_000_03;  // plus lent = meilleure portée

  configuration.OPTION.transmissionPower = POWER_20;   // puissance max 20 dBm (100 mW)
  configuration.OPTION.fec = FEC_1_ON;                 // correction d’erreurs
  configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;

  e32ttl100.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
  c.close();

  Serial.println("Nouvelle configuration appliquée !");

  Serial.println("Émetteur prêt !");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSendTime >= INTERVAL) {
    lastSendTime = currentMillis;


      // Allume la LED locale si le bouton est appuyé
      digitalWrite(LED_PIN, HIGH);
    
      // Encode le message et l'envoie
      String message = buildMessage(1);
      sendSecureMessage(message);
      Serial.println("Message envoyé : " + message); 
      
      // Clignotement de la LED à chaque envoi
      blinkLED();

  }
}

// Fonction pour faire clignoter la LED intégrée
void blinkLED() {
  digitalWrite(LED_PIN, HIGH);
  delay(150);
  digitalWrite(LED_PIN, LOW);
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
