/* Lora Emetteur - envoi sur appui bouton
 *  Version ESP32
 *  
 *  La librairie à installer : LoRa_E32 by Renzo Mischianti
 *  
 *  Envoie "debut" quand le bouton est pressé
 *  Envoie "fin" quand le bouton est relâché
 */
#include "LoRa_E32.h"

#define VRX_PIN 34
#define VRY_PIN 35
#define SW_PIN  32
#define LED_PIN 2  // LED locale
#define DEBOUNCE_DELAY 50  // Anti-rebond en ms

String securityKey = "SECURE123"; // même clé que le récepteur

// Broches ESP32 vers module LoRa UART
// RX: reçoit depuis LoRa (donc TX du module)
// TX: envoie vers LoRa (donc RX du module)
#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4

HardwareSerial loraSerial(1);
LoRa_E32 e32ttl100(&loraSerial, LORA_AUX);

// Variables pour la gestion du bouton
bool lastButtonState = HIGH;      // État précédent du bouton (HIGH = non appuyé car INPUT_PULLUP)
bool currentButtonState = HIGH;   // État actuel du bouton
unsigned long lastDebounceTime = 0;
bool messageEnvoye = false;       // Pour éviter d'envoyer plusieurs fois le même message

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
  configuration.OPTION.fec = FEC_1_ON;                 // correction d'erreurs
  configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;

  e32ttl100.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
  c.close();

  Serial.println("Nouvelle configuration appliquée !");
  Serial.println("Émetteur prêt ! Appuyez sur le bouton pour envoyer 'debut'");
}

void loop() {
  // Lecture de l'état du bouton
  int reading = digitalRead(SW_PIN);
  
  // Si l'état du bouton a changé, on réinitialise le timer de debounce
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // Si suffisamment de temps s'est écoulé depuis le dernier changement
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // Si l'état du bouton a changé de manière stable
    if (reading != currentButtonState) {
      currentButtonState = reading;
      
      // Le bouton vient d'être PRESSÉ (passage de HIGH à LOW car INPUT_PULLUP)
      if (currentButtonState == LOW && !messageEnvoye) {
        Serial.println("Bouton pressé - Envoi de 'debut'");
        digitalWrite(LED_PIN, HIGH);  // Allume la LED locale
        
        String message = buildMessage("debut");
        sendSecureMessage(message);
        Serial.println("Message envoyé : debut");
        
        messageEnvoye = true;
      }
      // Le bouton vient d'être RELÂCHÉ (passage de LOW à HIGH)
      else if (currentButtonState == HIGH && messageEnvoye) {
        Serial.println("Bouton relâché - Envoi de 'fin'");
        digitalWrite(LED_PIN, LOW);  // Éteint la LED locale
        
        String message = buildMessage("fin");
        sendSecureMessage(message);
        Serial.println("Message envoyé : fin");
        
        messageEnvoye = false;
      }
    }
  }
  
  // Sauvegarde de l'état pour la prochaine itération
  lastButtonState = reading;
}

String buildMessage(String command) {
  unsigned long nowSec = millis() / 1000;
  String timestamp = String(nowSec);
  return securityKey + ":" + timestamp + ":" + command;
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
