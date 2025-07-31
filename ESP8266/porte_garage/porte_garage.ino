/*
 * LCA : 24/07/2025 : Première version de détection de l'état de la porte du garage
 * Version ESP 8266 et pour un détecteur IM9700
 * Consignes : 
 * 1. Il faut installer les bibliotheques : 
 *    1.1 WiFiManager
 *    Aller sur : https://github.com/tzapu/WiFiManager
 *    Télécharger la dernière version et l'installer (croquis/inclure une bibliotheque/ajouter la bibliotheque .ZIP)
 *    1.2 NTPClient de Fabrice Weinberg
 *    Aller sur : https://www.arduinolibraries.info/libraries/ntp-client
 *    Télécharger la dernière version et l'installer (croquis/inclure une bibliotheque/ajouter la bibliotheque .ZIP)
 *    1.3 WiFiUdp (déjà présente normalement)
 * 2. Le détecteur doit être branché sur D5 et le GND coté ESP8266
 *    et sur la boucle d'alarme coté détecteur (ce sont les deux vis en bas les plus éloignés)
 * 
 */

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>

#include <WiFiUdp.h>
#include <NTPClient.h>

#define DETECT_PIN 14                             // D5 donc
// #define DELAI_NOTIFICATION 120000              // 2 minutes en ms
#define DELAI_NOTIFICATION 5000                  // 10 secondes en ms
#define TOPIC "porte_garage_lionel_31240"         // Topic ntfy.sh personnalisé

unsigned long ouvertureTimestamp = 0;
bool notificationEnvoyee = false;
bool lastState;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7200, 60000); // UTC+2 pour la France


void setup() {
  Serial.begin(115200);
  pinMode(DETECT_PIN, INPUT_PULLUP);  // Le capteur ferme le contact => état bas
  // pinMode(DETECT_PIN, INPUT);  // A eclaircir ! INPUT ou INPUT_PULLUP

  WiFiManager wm;
  wm.autoConnect("GarageESP");

  Serial.println("Connecté au WiFi !");

  timeClient.begin();
  
  lastState = digitalRead(DETECT_PIN);
  Serial.println("État initial : " + String(lastState == LOW ? "FERMÉ" : "OUVERT"));
}

void loop() {
    bool currentState = digitalRead(DETECT_PIN);
    // Stocke le changement d'état
    if (currentState != lastState) {
      Serial.print("L'état du détecteur a changé : ");
      Serial.println(currentState == LOW ? "Fermé" : "Ouvert");
      lastState = currentState;
    }
    String horodatage = getTimestamp();

    if (currentState == HIGH) {
        if (ouvertureTimestamp == 0) {
          Serial.println("La porte est ouverte, lancement du chronometre");
          ouvertureTimestamp = millis();  // début du chronomètre
          notificationEnvoyee = false;
        } else if ( (notificationEnvoyee==false) && (millis() - ouvertureTimestamp >= DELAI_NOTIFICATION) ) {
          Serial.println("Envoi d'une notification");
          sendNotification("🚪 Porte OUVERTE le " + String(horodatage));
          notificationEnvoyee = true;
        }
    }   
    else {
        if (notificationEnvoyee) {
          sendNotification("✅ Porte FERMÉE le " + String(horodatage));
        }
        // Réinitialisation si porte refermée
        ouvertureTimestamp = 0;
        notificationEnvoyee = false;
    }
    //Serial.println(notificationEnvoyee==false);
    //Serial.println(millis() - ouvertureTimestamp >= DELAI_NOTIFICATION );
    // Serial.println("millis(): " + String(millis()) + " ouvertureTimestamp: " + String(ouvertureTimestamp));
    delay(500);
}

void sendNotification(String message) {
  
  Serial.print("Envoi de la notification : ");
  Serial.println(message);
  if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure();  // éviter les erreurs SSL
  
      HTTPClient https;
      String url = "https://ntfy.sh/" + String(TOPIC);
      https.begin(client, url);
      https.addHeader("Content-Type", "text/plain");
  
      int httpCode = https.POST(message);
      Serial.printf("Notification envoyée (code HTTP %d)\n", httpCode);
      https.end();
  }   else {
     Serial.println("Non connecté au WiFi, impossible d'envoyer la notification.");
  }
}



String getTimestamp() {
  timeClient.update();
  time_t rawTime = timeClient.getEpochTime();

  struct tm * timeinfo = localtime(&rawTime);
  char buffer[25];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d",
          timeinfo->tm_mday,
          timeinfo->tm_mon + 1,
          timeinfo->tm_year + 1900,
          timeinfo->tm_hour,
          timeinfo->tm_min,
          timeinfo->tm_sec);
  return String(buffer);
}
