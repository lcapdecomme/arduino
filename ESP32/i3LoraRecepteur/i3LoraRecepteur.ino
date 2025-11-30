/* Lora Récepteur i3 - avec portail captif WiFi et timer
 *  Version ESP32
 *  
 *  Librairies à installer :
 *  - LoRa_E32 by Renzo Mischianti
 *  - ArduinoJson by Benoit Blanchon
 *  - Preferences (incluse avec ESP32)
 */

#include "LoRa_E32.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Configuration des broches
#define LED_PIN 25
#define AUX_PIN 4

// Configuration WiFi Access Point
const char* AP_SSID = "i3-Portail";
const char* AP_PWD = "Alarme31*"; 

// Serveur DNS pour le portail captif
DNSServer dnsServer;
WebServer server(80);
const byte DNS_PORT = 53;

// Module LoRa
LoRa_E32 e32ttl100(&Serial2, AUX_PIN);
String securityKey = "SECURE123";

// Stockage persistant
Preferences preferences;

// Variables du timer
unsigned long timerDuration = 10 * 60 * 1000; // 10 minutes par défaut (en millisecondes)
unsigned long timerStartTime = 0;
bool timerActive = false;
String companyName = "Entreprise";

void setup() {
  Serial.begin(115200);
  delay(500);

  // Configuration de la LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Chargement de la configuration
  loadConfig();

  // Initialisation du module LoRa
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  e32ttl100.begin();

  // Configuration du WiFi en mode Access Point
  Serial.println("=================================");
  Serial.println("  Récepteur i3 - Prêt !");
  Serial.println("=================================");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(AP_SSID, AP_PWD);
  delay(200);
  
  Serial.print("Point d'accès WiFi : ");
  Serial.println(AP_SSID);
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Durée du timer : ");
  Serial.print(timerDuration / 60000);
  Serial.println(" minutes");

  // Démarrage du serveur DNS pour le portail captif
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Configuration des routes du serveur web
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Serveur web démarré - En attente de messages LoRa...");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Vérification de réception LoRa
  if (e32ttl100.available() > 1) {
    ResponseContainer rc = e32ttl100.receiveMessage();
    String encoded = rc.data;
    String message = xorDecrypt(encoded, 'K');

    Serial.println("Message LoRa reçu : " + message);

    // Vérification du message
    if (validateMessage(message)) {
      Serial.println("✓ Message valide - Timer démarré/redémarré");
      startTimer();
    } else {
      Serial.println("⚠️ Message invalide ou clé incorrecte");
    }
  }

  // Gestion du timer
  if (timerActive) {
    unsigned long elapsed = millis() - timerStartTime;
    
    if (elapsed >= timerDuration) {
      // Timer terminé
      stopTimer();
    } else {
      // Timer en cours - LED allumée
      digitalWrite(LED_PIN, HIGH);
      
      // Affichage du temps restant toutes les 10 secondes
      static unsigned long lastDisplay = 0;
      if (millis() - lastDisplay > 10000) {
        lastDisplay = millis();
        unsigned long remaining = (timerDuration - elapsed) / 1000;
        Serial.print("Temps restant : ");
        Serial.print(remaining / 60);
        Serial.print(":");
        if ((remaining % 60) < 10) Serial.print("0");
        Serial.println(remaining % 60);
      }
    }
  }
}

void startTimer() {
  timerStartTime = millis();
  timerActive = true;
  digitalWrite(LED_PIN, HIGH);
}

void stopTimer() {
  timerActive = false;
  digitalWrite(LED_PIN, LOW);
  Serial.println("✓ Timer terminé - LED éteinte");
}

bool validateMessage(String message) {
  int firstSep = message.indexOf(':');
  int secondSep = message.indexOf(':', firstSep + 1);

  if (firstSep > 0 && secondSep > 0) {
    String key = message.substring(0, firstSep);
    return (key == securityKey);
  }
  return false;
}

String xorDecrypt(String data, char key) {
  String result = "";
  for (int i = 0; i < data.length(); i++) {
    result += (char)(data[i] ^ key);
  }
  return result;
}

void loadConfig() {
  preferences.begin("i3-config", false);
  timerDuration = preferences.getULong("duration", 10) * 60 * 1000; // En millisecondes
  companyName = preferences.getString("company", "Entreprise");
  preferences.end();
}

void saveConfig(unsigned long minutes, String company) {
  preferences.begin("i3-config", false);
  preferences.putULong("duration", minutes);
  preferences.putString("company", company);
  preferences.end();
  
  timerDuration = minutes * 60 * 1000;
  companyName = company;
  
  Serial.println("Configuration sauvegardée :");
  Serial.println("- Entreprise : " + company);
  Serial.println("- Durée : " + String(minutes) + " minutes");
}

// Formatte les secondes en format lisible
String formatDuration(unsigned long totalSeconds) {
  unsigned long minutes = totalSeconds / 60;
  unsigned long seconds = totalSeconds % 60;
  
  String result = String(minutes) + "m ";
  if (seconds < 10) result += "0";
  result += String(seconds) + "s";
  
  return result;
}

// ----- Gestionnaires Web -----

void handleRoot() {
  unsigned long currentMinutes = timerDuration / 60000;
  
  String html = "<!DOCTYPE html>\n";
  html += "<html lang='fr'>\n";
  html += "<head>\n";
  html += "    <meta charset='UTF-8'>\n";
  html += "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "    <title>Configuration i3</title>\n";
  html += "    <style>\n";
  html += "        * { margin: 0; padding: 0; box-sizing: border-box; }\n";
  html += "        body {\n";
  html += "            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n";
  html += "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
  html += "            min-height: 100vh;\n";
  html += "            display: flex;\n";
  html += "            justify-content: center;\n";
  html += "            align-items: center;\n";
  html += "            padding: 20px;\n";
  html += "        }\n";
  html += "        .container {\n";
  html += "            background: white;\n";
  html += "            border-radius: 20px;\n";
  html += "            box-shadow: 0 20px 60px rgba(0,0,0,0.3);\n";
  html += "            max-width: 500px;\n";
  html += "            width: 100%;\n";
  html += "            padding: 40px;\n";
  html += "        }\n";
  html += "        h1 {\n";
  html += "            color: #667eea;\n";
  html += "            font-size: 28px;\n";
  html += "            margin-bottom: 10px;\n";
  html += "            text-align: center;\n";
  html += "        }\n";
  html += "        .subtitle {\n";
  html += "            color: #666;\n";
  html += "            text-align: center;\n";
  html += "            margin-bottom: 30px;\n";
  html += "            font-size: 14px;\n";
  html += "        }\n";
  html += "        .form-group { margin-bottom: 25px; }\n";
  html += "        label {\n";
  html += "            display: block;\n";
  html += "            color: #333;\n";
  html += "            font-weight: 600;\n";
  html += "            margin-bottom: 8px;\n";
  html += "            font-size: 14px;\n";
  html += "        }\n";
  html += "        input[type='text'], input[type='number'] {\n";
  html += "            width: 100%;\n";
  html += "            padding: 12px 15px;\n";
  html += "            border: 2px solid #e0e0e0;\n";
  html += "            border-radius: 10px;\n";
  html += "            font-size: 16px;\n";
  html += "            transition: all 0.3s;\n";
  html += "        }\n";
  html += "        input:focus {\n";
  html += "            outline: none;\n";
  html += "            border-color: #667eea;\n";
  html += "            box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);\n";
  html += "        }\n";
  html += "        .info-box {\n";
  html += "            background: #f0f4ff;\n";
  html += "            border-left: 4px solid #667eea;\n";
  html += "            padding: 15px;\n";
  html += "            border-radius: 8px;\n";
  html += "            margin-bottom: 25px;\n";
  html += "            font-size: 14px;\n";
  html += "            color: #555;\n";
  html += "        }\n";
  html += "        button {\n";
  html += "            width: 100%;\n";
  html += "            padding: 15px;\n";
  html += "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
  html += "            color: white;\n";
  html += "            border: none;\n";
  html += "            border-radius: 10px;\n";
  html += "            font-size: 16px;\n";
  html += "            font-weight: 600;\n";
  html += "            cursor: pointer;\n";
  html += "        }\n";
  html += "        .status {\n";
  html += "            margin-top: 25px;\n";
  html += "            padding: 15px;\n";
  html += "            background: #f8f9fa;\n";
  html += "            border-radius: 10px;\n";
  html += "            text-align: center;\n";
  html += "        }\n";
  html += "        .status-title {\n";
  html += "            font-weight: 600;\n";
  html += "            color: #333;\n";
  html += "            margin-bottom: 10px;\n";
  html += "        }\n";
  html += "        .status-value {\n";
  html += "            font-size: 24px;\n";
  html += "            font-weight: bold;\n";
  html += "            color: #667eea;\n";
  html += "        }\n";
  html += "        .timer-active {\n";
  html += "            background: #d4edda;\n";
  html += "            color: #155724;\n";
  html += "            padding: 10px;\n";
  html += "            border-radius: 8px;\n";
  html += "            margin-top: 15px;\n";
  html += "            font-weight: 600;\n";
  html += "            text-align: center;\n";
  html += "        }\n";
  html += "        @media (max-width: 480px) {\n";
  html += "            .container { padding: 25px; }\n";
  html += "            h1 { font-size: 24px; }\n";
  html += "        }\n";
  html += "    </style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "    <div class='container'>\n";
  html += "        <h1>⚙️ Configuration i3</h1>\n";
  html += "        <p class='subtitle'>Système de minuterie LoRa</p>\n";
  html += "        <form action='/save' method='POST'>\n";
  html += "            <div class='form-group'>\n";
  html += "                <label for='duration'>⏱️ Durée du timer (minutes)</label>\n";
  html += "                <input type='number' id='duration' name='duration' value='" + String(currentMinutes) + "' min='1' max='120' required>\n";
  html += "            </div>\n";
  html += "            <div class='info-box'>\n";
  html += "                💡 <strong>Info :</strong> Lorsque le bouton est pressé, l'alarme sera désactivé pendant la durée configurée. Si un nouveau signal est reçu pendant ce temps, le décompte redémarre à zéro.\n";
  html += "            </div>\n";
  html += "            <button type='submit'>💾 Sauvegarder</button>\n";
  html += "        </form>\n";
  html += "        <div class='status'>\n";
  html += "            <div class='status-title'>Durée actuelle</div>\n";
  html += "            <div class='status-value' id='currentDuration'>" + String(currentMinutes) + " min</div>\n";
  html += "        </div>\n";
  html += "        <div id='timerStatus'></div>\n";
  html += "    </div>\n";
  html += "    <script>\n";
  html += "        function updateStatus() {\n";
  html += "            fetch('/status')\n";
  html += "                .then(response => response.json())\n";
  html += "                .then(data => {\n";
  html += "                    if (data.timerActive) {\n";
  html += "                        const minutes = Math.floor(data.remaining / 60);\n";
  html += "                        const seconds = data.remaining % 60;\n";
  html += "                        document.getElementById('timerStatus').innerHTML = \n";
  html += "                            '<div class=\"timer-active\">🔴 Timer actif : ' + \n";
  html += "                            minutes + 'm ' + seconds + 's restantes</div>';\n";
  html += "                    } else {\n";
  html += "                        document.getElementById('timerStatus').innerHTML = '';\n";
  html += "                    }\n";
  html += "                })\n";
  html += "                .catch(e => console.error(e));\n";
  html += "        }\n";
  html += "        setInterval(updateStatus, 2000);\n";
  html += "        updateStatus();\n";
  html += "    </script>\n";
  html += "</body>\n";
  html += "</html>";
  
  server.send(200, "text/html", html);
}

void handleSave() {
  if (!server.hasArg("duration")) {
    server.send(400, "text/plain", "Paramètre manquant");
    return;
  }

  String company = server.arg("company");
  unsigned long minutes = server.arg("duration").toInt();
  
  if (minutes < 1) minutes = 1;
  if (minutes > 120) minutes = 120;

  saveConfig(minutes, company);

  // Page de succès
  String html = "<!DOCTYPE html>\n";
  html += "<html lang='fr'>\n";
  html += "<head>\n";
  html += "    <meta charset='UTF-8'>\n";
  html += "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "    <title>Configuration sauvegardée</title>\n";
  html += "    <style>\n";
  html += "        * { margin: 0; padding: 0; box-sizing: border-box; }\n";
  html += "        body {\n";
  html += "            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;\n";
  html += "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
  html += "            min-height: 100vh;\n";
  html += "            display: flex;\n";
  html += "            justify-content: center;\n";
  html += "            align-items: center;\n";
  html += "            padding: 20px;\n";
  html += "        }\n";
  html += "        .container {\n";
  html += "            background: white;\n";
  html += "            border-radius: 20px;\n";
  html += "            box-shadow: 0 20px 60px rgba(0,0,0,0.3);\n";
  html += "            max-width: 400px;\n";
  html += "            width: 100%;\n";
  html += "            padding: 40px;\n";
  html += "            text-align: center;\n";
  html += "        }\n";
  html += "        .success-icon { font-size: 64px; margin-bottom: 20px; }\n";
  html += "        h1 { color: #28a745; margin-bottom: 15px; }\n";
  html += "        p { color: #666; margin-bottom: 30px; }\n";
  html += "        button {\n";
  html += "            padding: 12px 30px;\n";
  html += "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n";
  html += "            color: white;\n";
  html += "            border: none;\n";
  html += "            border-radius: 10px;\n";
  html += "            font-size: 16px;\n";
  html += "            font-weight: 600;\n";
  html += "            cursor: pointer;\n";
  html += "        }\n";
  html += "    </style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "    <div class='container'>\n";
  html += "        <div class='success-icon'>✅</div>\n";
  html += "        <h1>Configuration sauvegardée !</h1>\n";
  html += "        <p>Les paramètres ont été enregistrés avec succès.</p>\n";
  html += "        <button onclick='location.href=\"/\"'>Retour</button>\n";
  html += "    </div>\n";
  html += "    <script>\n";
  html += "        setTimeout(function() { location.href = '/'; }, 3000);\n";
  html += "    </script>\n";
  html += "</body>\n";
  html += "</html>";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  StaticJsonDocument<256> doc;
  doc["company"] = companyName;
  doc["duration"] = timerDuration / 60000;
  doc["timerActive"] = timerActive;
  
  if (timerActive) {
    unsigned long remaining = (timerDuration - (millis() - timerStartTime)) / 1000;
    doc["remaining"] = remaining;
  }
  
  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleNotFound() {
  // Redirige toutes les requêtes vers la page d'accueil (portail captif)
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
