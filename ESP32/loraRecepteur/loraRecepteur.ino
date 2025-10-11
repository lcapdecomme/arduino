/* Lora Recepteur 
 *  Version ESP32
 *  
 *  La librairie à installer : LoRa_E32 by Renzo Mischianti
 *  
 */
 
 #include "LoRa_E32.h"

#define LED_PIN 25
#define AUX_PIN 4

LoRa_E32 e32ttl100(&Serial2, AUX_PIN);
String securityKey = "SECURE123"; // même clé que l’émetteur

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  e32ttl100.begin();

  Serial.println("Récepteur LoRa DX-LR02 prêt !");
}

void loop() {
  if (e32ttl100.available() > 1) {
    ResponseContainer rc = e32ttl100.receiveMessage();
    String encoded = rc.data;
    String message = xorDecrypt(encoded, 'K');

    Serial.println("Message reçu (déchiffré) : " + message);

    int firstSep = message.indexOf(':');
    int secondSep = message.indexOf(':', firstSep + 1);

    if (firstSep > 0 && secondSep > 0) {
      String key = message.substring(0, firstSep);
      String timestamp = message.substring(firstSep + 1, secondSep);
      String countStr = message.substring(secondSep + 1);

      if (key == securityKey) {
        int count = countStr.toInt();
        Serial.println("Clé valide. Clignotement x" + String(count));
        blinkLED(count);
      } else {
        Serial.println("⚠️ Clé invalide !");
      }
    }
  }
}

void blinkLED(int n) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);
    delay(300);
  }
}

String xorDecrypt(String data, char key) {
  String result = "";
  for (int i = 0; i < data.length(); i++) {
    result += (char)(data[i] ^ key);
  }
  return result;
}
