/* Lora Recepteur avec Répéteur
 *  Version ESP32
 *  
 *  La librairie à installer : LoRa_E32 by Renzo Mischianti
 *  
 *  Allume la LED quand "debut" est reçu
 *  Éteint la LED quand "fin" est reçu
 *  Propage les messages aux autres récepteurs
 *  Évite les doublons en gardant l'état actuel
 */
 
 #include "LoRa_E32.h"

#define LED_PIN 25

// Broches ESP32 vers module LoRa UART
#define M0_PIN 19  // À connecter au M0 du module
#define M1_PIN 18  // À connecter au M1 du module
#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4
LoRa_E32 e32ttl100(&Serial2, LORA_AUX, M0_PIN, M1_PIN);

String securityKey = "SECURE123"; // même clé que l'émetteur

// État actuel du système
String currentState = "fin";  // État initial : "fin" (LED éteinte)

// Variables pour éviter les répétitions infinies
#define MAX_MESSAGES 10
String recentMessages[MAX_MESSAGES];
int messageIndex = 0;
unsigned long messageTimestamps[MAX_MESSAGES];
#define MESSAGE_MEMORY_TIME 5000  // Garde en mémoire les messages pendant 5 secondes

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(M0_PIN, OUTPUT);
  pinMode(M1_PIN, OUTPUT);

  Serial2.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  
  // Initialiser le tableau des messages récents
  for (int i = 0; i < MAX_MESSAGES; i++) {
    recentMessages[i] = "";
    messageTimestamps[i] = 0;
  }
  
  e32ttl100.begin();
  // --- CONFIGURATION AUTOMATIQUE UNIVERSELLE ---
  ResponseStructContainer c;
  c = e32ttl100.getConfiguration();
  Configuration configuration = *(Configuration*) c.data;

  // 1. Définir l'adresse (0 pour broadcast/défaut)
  configuration.ADDL = 0x00;
  configuration.ADDH = 0x00;
  
  // 2. Définir le Canal (0x17 = 23 décimal -> 410Mhz + 23 = 433MHz)
  configuration.CHAN = 0x17; 

  // 3. Paramètres de transmission
  configuration.SPED.airDataRate = AIR_DATA_RATE_010_24; 
  configuration.SPED.uartBaudRate = UART_BPS_9600;
  configuration.SPED.uartParity = MODE_00_8N1;

  // 4. Options et Puissance
  configuration.OPTION.fec = FEC_1_ON;
  configuration.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  configuration.OPTION.transmissionPower = POWER_20; 

  ResponseStatus rs = e32ttl100.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
  Serial.print("Statut de l'écriture : ");
  Serial.println(rs.getResponseDescription());
  
  delay(500); 

  c = e32ttl100.getConfiguration();
  
  if (c.status.code == E32_SUCCESS) {
      Configuration loadedConfiguration = *(Configuration*) c.data;
      
      Serial.println("=========================================");
      Serial.println("✅ Configuration chargée du module (Vérification) :");
      Serial.println("=========================================");
      
      Serial.print("Adresse (H/L) : 0x");
      Serial.print(loadedConfiguration.ADDH, HEX);
      Serial.print(" / 0x");
      Serial.println(loadedConfiguration.ADDL, HEX);
      
      Serial.print("Canal (CHAN) : 0x");
      Serial.print(loadedConfiguration.CHAN, HEX);
      Serial.print(" (Fréquence : 410Mhz + ");
      Serial.print(loadedConfiguration.CHAN);
      Serial.println(" = ~433MHz)");
      
      Serial.print("Vitesse Air (Air Data Rate) : ");
      Serial.println(getTransmissionPowerDescriptionByParams(loadedConfiguration.SPED.airDataRate));
      
      Serial.print("Puissance (TX Power) : ");
      Serial.println(getTransmissionPowerDescriptionByParams(loadedConfiguration.OPTION.transmissionPower));

      Serial.println("=========================================");
  } else {
      Serial.print("⚠️ Erreur lors de la lecture de la configuration : ");
      Serial.println(c.status.getResponseDescription());
  }
  
  Serial.println("Mode : Récepteur avec Répéteur");
  Serial.println("En attente de messages...");
}

void loop() {
  // Nettoyer les anciens messages de la mémoire
  cleanOldMessages();
  
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
      String command = message.substring(secondSep + 1);

      if (key == securityKey) {
        // Vérifier si ce message a déjà été traité récemment
        if (isMessageRecent(message)) {
          Serial.println("⚠️ Message déjà traité récemment, ignoré");
          return;
        }
        
        // Enregistrer ce message comme traité
        addMessageToHistory(message);
        
        Serial.println("Clé valide. Commande : " + command);
        
        // Traiter la commande seulement si l'état change
        if (command == "debut" && currentState != "debut") {
          currentState = "debut";
          digitalWrite(LED_PIN, HIGH);
          Serial.println("✅ LED allumée - Nouvel état : debut");
          
          // Propager le message aux autres récepteurs
          propagateMessage(encoded);
        } 
        else if (command == "fin" && currentState != "fin") {
          currentState = "fin";
          digitalWrite(LED_PIN, LOW);
          Serial.println("✅ LED éteinte - Nouvel état : fin");
          
          // Propager le message aux autres récepteurs
          propagateMessage(encoded);
        }
        else {
          Serial.println("ℹ️ Déjà dans l'état '" + command + "', pas de changement");
        }
      } else {
        Serial.println("⚠️ Clé invalide !");
      }
    }
  }
}

// Fonction pour propager le message aux autres récepteurs
void propagateMessage(String encodedMessage) {
  Serial.println("📡 Propagation du message aux autres récepteurs...");
  
  // Petit délai pour éviter les collisions
  delay(random(50, 150));
  
  // Renvoyer le message déjà encodé
  e32ttl100.sendMessage(encodedMessage);
  
  Serial.println("✅ Message propagé");
}

// Vérifie si un message a déjà été traité récemment
bool isMessageRecent(String message) {
  for (int i = 0; i < MAX_MESSAGES; i++) {
    if (recentMessages[i] == message && messageTimestamps[i] > 0) {
      return true;
    }
  }
  return false;
}

// Ajoute un message à l'historique
void addMessageToHistory(String message) {
  recentMessages[messageIndex] = message;
  messageTimestamps[messageIndex] = millis();
  messageIndex = (messageIndex + 1) % MAX_MESSAGES;
}

// Nettoie les messages trop anciens
void cleanOldMessages() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_MESSAGES; i++) {
    if (messageTimestamps[i] > 0 && (now - messageTimestamps[i] > MESSAGE_MEMORY_TIME)) {
      recentMessages[i] = "";
      messageTimestamps[i] = 0;
    }
  }
}

String xorDecrypt(String data, char key) {
  String result = "";
  for (int i = 0; i < data.length(); i++) {
    result += (char)(data[i] ^ key);
  }
  return result;
}
