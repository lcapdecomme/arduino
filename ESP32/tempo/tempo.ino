/*
 * TEMPO - Temporisateur à déclenchement conditionnel
 * ESP32 avec LED et détection de contact
 *  
 * Branchement :
 * 
 * Entrée : Les deux fils sont sur la masse et le GPIO4
 * Sortie : La led (tige longue) sur gpio 2, 
 *          La led (tige courte) sur masse
 * 
 * Fonctionnement :
 * - Interface web captive pour configuration
 * - Paramètre 1 : Mode "Ouvert" ou "Fermé" (condition de déclenchement)
 * - Paramètre 2 : Départ différé (immédiat, secondes, minutes)
 * - Paramètre 3 : Durée d'allumage de la LED
 * - Sauvegarde des paramètres en NVS
 * 
 * Logique de fonctionnement
 * Le système surveille l'état du contact (fils qui se touchent = circuit fermé, fils séparés = circuit ouvert).
 * Si l'utilisateur choisit "Fermé" → le système se déclenche quand les fils se touchent
 * Si l'utilisateur choisit "Ouvert" → le système se déclenche quand les fils se séparent
 * Une fois la condition détectée :

 * Attendre la temporisation (départ différé)
 * Allumer la LED pendant la durée programmée
 * Éteindre la LED → fin du cycle
 * 
 * Remarque : 
 * Pendant STATE_DELAY, le système vérifie en continu si l'état du contact est revenu à la normale
 * Si oui → annulation et retour en STATE_WAITING
 * Le message de status indique maintenant "(annulable)" pendant la temporisation
 * 
 * Exemple concret : 
 * si l'utilisateur a choisi "L'ouverture" avec un délai de 30 secondes, et que la porte s'ouvre 
 * puis se referme dans les 30 secondes, le traitement est annulé et la LED ne s'allumera pas.
 */


#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

// ----- Configuration WiFi -----
#define AP_SSID     "TEMPO"
#define AP_PASSWORD "12345678"

// ----- Configuration GPIO -----
#define PIN_LED     2      // LED intégrée ou relai externe
#define PIN_CONTACT 4      // GPIO pour détection de contact (avec pull-up interne)
#define PIN_BUTTON  5      // GPIO pour bouton activation/désactivation de l'alarme (avec pull-up interne)
#define PIN_BUZZER  18     // GPIO pour buzzer actif


// ----- Objets globaux -----
DNSServer dnsServer;
WebServer server(80);
Preferences preferences;

const byte DNS_PORT = 53;

// ----- Paramètres de configuration -----
// Mode : true = déclenche sur "Ouvert" (fils séparés), false = déclenche sur "Fermé" (fils touchés)
bool triggerOnOpen = true;

// Délai avant démarrage (en millisecondes)
unsigned long delayMs = 0;

// Durée d'allumage LED (en millisecondes)
unsigned long durationMs = 5000;

// ----- État du système -----
enum SystemState {
  STATE_WAITING,      // Attend la condition de déclenchement
  STATE_DELAY,        // Temporisation avant allumage
  STATE_ACTIVE,       // LED allumée
  STATE_FINISHED      // Cycle terminé
};

SystemState currentState = STATE_WAITING;
unsigned long stateStartTime = 0;
bool lastContactState = false;


// ----- État de l'alarme : activation/désactivation -----
bool appEnabled = true;              // Application activée par défaut
bool lastButtonState = true;         // État précédent du bouton (HIGH avec pull-up)
unsigned long lastButtonPress = 0;   // Anti-rebond
const unsigned long DEBOUNCE_MS = 50;


// ----- Prototypes -----
void handleRoot();
void handleSave();
void handleStatus();
void handleCaptivePortal();
void handleNotFound();
void loadSettings();
void saveSettings();
bool readContact();
void checkButton();
void beepDisabled();
void beepEnabled();


void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Configuration des GPIO
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  pinMode(PIN_CONTACT, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  
  // Chargement des paramètres sauvegardés
  loadSettings();
  
  // Lecture état initial du contact
  lastContactState = readContact();
  
  Serial.println("\n=== TEMPO ===");
  Serial.printf("Mode: Déclenche sur %s\n", triggerOnOpen ? "OUVERT" : "FERME");
  Serial.printf("Délai: %lu ms\n", delayMs);
  Serial.printf("Durée: %lu ms\n", durationMs);
  Serial.printf("Contact initial: %s\n", lastContactState ? "FERME" : "OUVERT");
  Serial.println("Application: ACTIVEE");
  
  // Démarrage WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);
  Serial.printf("\nWiFi: %s @ %s\n\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  
  // Démarrage DNS captif
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
  // Headers par défaut pour toutes les réponses
  server.enableCORS(true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  
  // Configuration des routes web
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);

  
  // Routes pour détection de portail captif (Android, Windows, etc.)
  server.on("/generate_204", HTTP_GET, handleCaptivePortal);
  server.on("/gen_204", HTTP_GET, handleCaptivePortal);
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortal);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);  // iOS/macOS
  server.on("/canonical.html", HTTP_GET, handleCaptivePortal);
  server.on("/success.txt", HTTP_GET, handleCaptivePortal);
  server.on("/ncsi.txt", HTTP_GET, handleCaptivePortal);  // Windows
  server.on("/fwlink", HTTP_GET, handleCaptivePortal);    // Microsoft
  // Samsung et autres Android
  server.on("/mobile/status.php", HTTP_GET, handleCaptivePortal);
  server.on("/kindle-wifi/wifistub.html", HTTP_GET, handleCaptivePortal);
  server.on("/check_network_status.txt", HTTP_GET, handleCaptivePortal);
  server.on("/library/test/success.html", HTTP_GET, handleCaptivePortal);
  server.on("/wifi/v1/portal", HTTP_GET, handleCaptivePortal);
  
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Serveur web démarré");
  
  // Test buzzer
  digitalWrite(PIN_BUZZER, HIGH);
  delay(1000);
  digitalWrite(PIN_BUZZER, LOW);
  
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
    
  // Vérifier le bouton d'activation/désactivation
  checkButton();
  
  // Si l'application est suspendue, ne pas traiter la machine à états
  if (!appEnabled) {
    delay(10);
    return;
  }
  
  // Machine à états
  switch (currentState) {
    case STATE_WAITING: {
      bool currentContact = readContact();
      
      // Détection de changement d'état
      if (currentContact != lastContactState) {
        lastContactState = currentContact;
        
        // Vérifier si la condition de déclenchement est remplie
        // triggerOnOpen = true : déclenche quand contact passe à OUVERT (false)
        // triggerOnOpen = false : déclenche quand contact passe à FERMÉ (true)
        bool shouldTrigger = (triggerOnOpen && !currentContact) || (!triggerOnOpen && currentContact);
        
        if (shouldTrigger) {
          Serial.println(">>> Condition de déclenchement détectée !");
          stateStartTime = millis();
          
          if (delayMs == 0) {
            // Pas de délai, allumage immédiat
            currentState = STATE_ACTIVE;
            digitalWrite(PIN_LED, HIGH);
            Serial.println(">>> LED allumée (immédiat)");
          } else {
            currentState = STATE_DELAY;
            Serial.printf(">>> Temporisation de %lu ms...\n", delayMs);
          }
        }
      }
      break;
    }
    
    case STATE_DELAY: {
      // Vérifier si l'état est revenu à la normale (annulation)
      bool currentContact = readContact();
      bool conditionStillMet = (triggerOnOpen && !currentContact) || (!triggerOnOpen && currentContact);
      
      if (!conditionStillMet) {
        // L'état est revenu à la normale, annuler le traitement
        currentState = STATE_WAITING;
        lastContactState = currentContact;
        Serial.println(">>> Annulation : état revenu à la normale");
        break;
      }
      
      if (millis() - stateStartTime >= delayMs) {
        currentState = STATE_ACTIVE;
        stateStartTime = millis();
        digitalWrite(PIN_LED, HIGH);
        Serial.println(">>> LED allumée (après délai)");
      }
      break;
    }
    
    case STATE_ACTIVE: {
      if (millis() - stateStartTime >= durationMs) {
        currentState = STATE_FINISHED;
        digitalWrite(PIN_LED, LOW);
        Serial.println(">>> LED éteinte - Cycle terminé");
      }
      break;
    }
    
    case STATE_FINISHED:
      // Retour en attente pour un nouveau cycle
      currentState = STATE_WAITING;
      lastContactState = readContact();  // Réinitialise l'état du contact
      Serial.println(">>> Prêt pour un nouveau cycle");
      break;
  }
  
  delay(10);
}

// ----- Lecture du contact -----
// Retourne true si fermé (fils touchés), false si ouvert (fils séparés)
bool readContact() {
  // Avec INPUT_PULLUP : LOW = fils touchés (fermé), HIGH = fils séparés (ouvert)
  return (digitalRead(PIN_CONTACT) == LOW);
}


// ----- Vérification du bouton -----
void checkButton() {
  bool currentButtonState = digitalRead(PIN_BUTTON);
  
  // Détection front descendant (appui) avec anti-rebond
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    if (millis() - lastButtonPress > DEBOUNCE_MS) {
      lastButtonPress = millis();
      
      if (appEnabled) {
        // Désactiver l'application
        appEnabled = false;
        currentState = STATE_WAITING;
        digitalWrite(PIN_LED, LOW);
        Serial.println(">>> Application SUSPENDUE");
        beepDisabled();
      } else {
        // Réactiver l'application
        appEnabled = true;
        lastContactState = readContact();  // Réinitialiser l'état du contact
        Serial.println(">>> Application ACTIVEE");
        beepEnabled();
      }
    }
  }
  
  lastButtonState = currentButtonState;
}

// ----- 3 bips courts (désactivation) -----
void beepDisabled() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(100);
  }
}

// ----- 1 bip long de 3 secondes (activation) -----
void beepEnabled() {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(3000);
  digitalWrite(PIN_BUZZER, LOW);
}

// ----- Chargement des paramètres -----
void loadSettings() {
  preferences.begin("tempo", true); // Lecture seule
  triggerOnOpen = preferences.getBool("triggerOpen", true);
  delayMs = preferences.getULong("delayMs", 0);
  durationMs = preferences.getULong("durationMs", 5000);
  preferences.end();
}

// ----- Sauvegarde des paramètres -----
void saveSettings() {
  preferences.begin("tempo", false); // Lecture/écriture
  preferences.putBool("triggerOpen", triggerOnOpen);
  preferences.putULong("delayMs", delayMs);
  preferences.putULong("durationMs", durationMs);
  preferences.end();
}

// ----- Gestionnaire page principale -----
void handleRoot() {
  String html = "<!DOCTYPE html>\n";
  html += "<html lang='fr'>\n";
  html += "<head>\n";
  html += "  <meta charset='UTF-8'>\n";
  html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n";
  html += "  <title>TEMPO</title>\n";
  html += "  <style>\n";
  html += "    * { box-sizing: border-box; margin: 0; padding: 0; }\n";
  html += "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; ";
  html += "           background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%); ";
  html += "           min-height: 100vh; padding: 20px; color: #fff; }\n";
  html += "    .container { max-width: 400px; margin: 0 auto; }\n";
  html += "    h1 { text-align: center; margin-bottom: 30px; font-size: 2.5em; ";
  html += "        background: linear-gradient(90deg, #00d2ff, #3a7bd5); ";
  html += "        -webkit-background-clip: text; -webkit-text-fill-color: transparent; }\n";
  html += "    .card { background: rgba(255,255,255,0.1); border-radius: 15px; ";
  html += "           padding: 20px; margin-bottom: 20px; backdrop-filter: blur(10px); }\n";
  html += "    .card h2 { font-size: 1.1em; margin-bottom: 15px; color: #00d2ff; }\n";
  html += "    .option-group { display: flex; gap: 10px; flex-wrap: wrap; }\n";
  html += "    .option { flex: 1; min-width: 120px; }\n";
  html += "    .option input[type='radio'] { display: none; }\n";
  html += "    .option label { display: block; padding: 15px; text-align: center; ";
  html += "                   background: rgba(255,255,255,0.1); border-radius: 10px; ";
  html += "                   cursor: pointer; transition: all 0.3s; border: 2px solid transparent; }\n";
  html += "    .option input[type='radio']:checked + label { ";
  html += "      background: rgba(0,210,255,0.3); border-color: #00d2ff; }\n";
  html += "    .option label:hover { background: rgba(255,255,255,0.2); }\n";
  html += "    .input-row { display: flex; gap: 10px; align-items: center; margin-top: 15px; }\n";
  html += "    .input-row input[type='number'] { flex: 1; padding: 12px; border: none; ";
  html += "                                     border-radius: 8px; background: rgba(255,255,255,0.2); ";
  html += "                                     color: #fff; font-size: 1em; }\n";
  html += "    .input-row select { padding: 12px; border: none; border-radius: 8px; ";
  html += "                       background: rgba(255,255,255,0.2); color: #fff; font-size: 1em; }\n";
  html += "    .input-row input[type='number']:focus, .input-row select:focus { ";
  html += "      outline: 2px solid #00d2ff; }\n";
  html += "    button { width: 100%; padding: 18px; border: none; border-radius: 12px; ";
  html += "            background: linear-gradient(90deg, #00d2ff, #3a7bd5); ";
  html += "            color: #fff; font-size: 1.2em; font-weight: bold; ";
  html += "            cursor: pointer; transition: transform 0.2s, box-shadow 0.2s; }\n";
  html += "    button:hover { transform: translateY(-2px); ";
  html += "                  box-shadow: 0 5px 20px rgba(0,210,255,0.4); }\n";
  html += "    button:active { transform: translateY(0); }\n";
  html += "    .status { text-align: center; padding: 15px; border-radius: 10px; ";
  html += "             background: rgba(255,255,255,0.1); margin-top: 20px; }\n";
  html += "    .status.waiting { border-left: 4px solid #ffc107; }\n";
  html += "    .status.delay { border-left: 4px solid #17a2b8; }\n";
  html += "    .status.active { border-left: 4px solid #28a745; }\n";
  html += "    .status.finished { border-left: 4px solid #6c757d; }\n";
  html += "    .status.disabled { border-left: 4px solid #dc3545; background: rgba(220,53,69,0.2); }\n";
  html += "    #statusText { font-weight: bold; }\n";
  html += "    .app-state { text-align: center; padding: 10px; margin-bottom: 20px; ";
  html += "                border-radius: 10px; font-weight: bold; }\n";
  html += "    .app-state.enabled { background: rgba(40,167,69,0.3); color: #28a745; }\n";
  html += "    .app-state.disabled { background: rgba(220,53,69,0.3); color: #dc3545; }\n";
  html += "  </style>\n";
  html += "</head>\n";
  html += "<body>\n";
  html += "  <div class='container'>\n";
  html += "    <h1>⏱ TEMPO</h1>\n";
  
  // Affichage état activation
  html += "    <div class='app-state";
  html += appEnabled ? " enabled'>✅ APPLICATION ACTIVÉE" : " disabled'>⛔ APPLICATION SUSPENDUE";
  html += "</div>\n";
  
  html += "    <form action='/save' method='POST'>\n";
  
  // Option 1 : Mode de déclenchement
  html += "      <div class='card'>\n";
  html += "        <h2>1. Déclenchement sur</h2>\n";
  html += "        <div class='option-group'>\n";
  html += "          <div class='option'>\n";
  html += "            <input type='radio' name='mode' id='modeOpen' value='open'";
  if (triggerOnOpen) html += " checked";
  html += ">\n";
  html += "            <label for='modeOpen'>🔓 L'ouverture</label>\n";
  html += "          </div>\n";
  html += "          <div class='option'>\n";
  html += "            <input type='radio' name='mode' id='modeClosed' value='closed'";
  if (!triggerOnOpen) html += " checked";
  html += ">\n";
  html += "            <label for='modeClosed'>🔒 La fermeture</label>\n";
  html += "          </div>\n";
  html += "        </div>\n";
  html += "      </div>\n";
  
  // Option 2 : Départ différé
  html += "      <div class='card'>\n";
  html += "        <h2>2. Départ différé</h2>\n";
  html += "        <div class='option-group'>\n";
  html += "          <div class='option'>\n";
  html += "            <input type='radio' name='delayType' id='delayImm' value='immediate'";
  if (delayMs == 0) html += " checked";
  html += " onchange='toggleDelayInput()'>\n";
  html += "            <label for='delayImm'>⚡ Immédiat</label>\n";
  html += "          </div>\n";
  html += "          <div class='option'>\n";
  html += "            <input type='radio' name='delayType' id='delayCustom' value='custom'";
  if (delayMs > 0) html += " checked";
  html += " onchange='toggleDelayInput()'>\n";
  html += "            <label for='delayCustom'>⏳ Différé</label>\n";
  html += "          </div>\n";
  html += "        </div>\n";
  html += "        <div class='input-row' id='delayInputRow'";
  if (delayMs == 0) html += " style='display:none'";
  html += ">\n";
  
  // Calcul valeur actuelle
  unsigned long delayValue = delayMs / 1000;
  String delayUnit = "sec";
  if (delayMs > 0 && delayMs % 60000 == 0) {
    delayValue = delayMs / 60000;
    delayUnit = "min";
  }
  
  html += "          <input type='number' name='delayValue' id='delayValue' min='1' max='999' value='";
  html += String(delayValue > 0 ? delayValue : 10);
  html += "'>\n";
  html += "          <select name='delayUnit' id='delayUnit'>\n";
  html += "            <option value='sec'";
  if (delayUnit == "sec") html += " selected";
  html += ">Secondes</option>\n";
  html += "            <option value='min'";
  if (delayUnit == "min") html += " selected";
  html += ">Minutes</option>\n";
  html += "          </select>\n";
  html += "        </div>\n";
  html += "      </div>\n";
  
  // Option 3 : Durée
  html += "      <div class='card'>\n";
  html += "        <h2>3. Durée d'activation</h2>\n";
  html += "        <div class='input-row'>\n";
  
  // Calcul valeur durée
  unsigned long durationValue = durationMs / 1000;
  String durationUnit = "sec";
  if (durationMs % 60000 == 0 && durationMs >= 60000) {
    durationValue = durationMs / 60000;
    durationUnit = "min";
  }
  
  html += "          <input type='number' name='durationValue' id='durationValue' min='1' max='999' value='";
  html += String(durationValue);
  html += "'>\n";
  html += "          <select name='durationUnit' id='durationUnit'>\n";
  html += "            <option value='sec'";
  if (durationUnit == "sec") html += " selected";
  html += ">Secondes</option>\n";
  html += "            <option value='min'";
  if (durationUnit == "min") html += " selected";
  html += ">Minutes</option>\n";
  html += "          </select>\n";
  html += "        </div>\n";
  html += "      </div>\n";
  
  html += "      <button type='submit'>💾 Enregistrer</button>\n";
  html += "    </form>\n";
  
  // Status
  html += "    <div class='status' id='statusBox'>\n";
  html += "      <span id='statusText'>Chargement...</span>\n";
  html += "    </div>\n";
  
  html += "  </div>\n";
  
  // JavaScript
  html += "  <script>\n";
  html += "    function toggleDelayInput() {\n";
  html += "      var row = document.getElementById('delayInputRow');\n";
  html += "      var isCustom = document.getElementById('delayCustom').checked;\n";
  html += "      row.style.display = isCustom ? 'flex' : 'none';\n";
  html += "    }\n";
  html += "    function updateStatus() {\n";
  html += "      fetch('/status').then(r => r.json()).then(data => {\n";
  html += "        var box = document.getElementById('statusBox');\n";
  html += "        var text = document.getElementById('statusText');\n";
  html += "        var appState = document.querySelector('.app-state');\n";
  html += "        box.className = 'status ' + data.state;\n";
  html += "        text.textContent = data.message;\n";
  html += "        if (data.enabled) {\n";
  html += "          appState.className = 'app-state enabled';\n";
  html += "          appState.textContent = '✅ APPLICATION ACTIVÉE';\n";
  html += "        } else {\n";
  html += "          appState.className = 'app-state disabled';\n";
  html += "          appState.textContent = '⛔ APPLICATION SUSPENDUE';\n";
  html += "        }\n";
  html += "      }).catch(e => {});\n";
  html += "    }\n";
  html += "    updateStatus();\n";
  html += "    setInterval(updateStatus, 1000);\n";
  html += "  </script>\n";
  html += "</body>\n";
  html += "</html>\n";
  
  server.send(200, "text/html", html);
}


// ----- Gestionnaire sauvegarde -----
void handleSave() {
  // Mode
  if (server.hasArg("mode")) {
    triggerOnOpen = (server.arg("mode") == "open");
  }
  
  // Délai
  if (server.hasArg("delayType")) {
    if (server.arg("delayType") == "immediate") {
      delayMs = 0;
    } else {
      unsigned long val = server.arg("delayValue").toInt();
      String unit = server.arg("delayUnit");
      if (unit == "min") {
        delayMs = val * 60000;
      } else {
        delayMs = val * 1000;
      }
    }
  }
  
  // Durée
  if (server.hasArg("durationValue")) {
    unsigned long val = server.arg("durationValue").toInt();
    String unit = server.arg("durationUnit");
    if (unit == "min") {
      durationMs = val * 60000;
    } else {
      durationMs = val * 1000;
    }
  }
  
  // Sauvegarde en NVS
  saveSettings();
  
  // Réinitialisation de l'état
  currentState = STATE_WAITING;
  lastContactState = readContact();
  digitalWrite(PIN_LED, LOW);
  
  Serial.println("\n=== Paramètres mis à jour ===");
  Serial.printf("Mode: Déclenche sur %s\n", triggerOnOpen ? "OUVERT" : "FERME");
  Serial.printf("Délai: %lu ms\n", delayMs);
  Serial.printf("Durée: %lu ms\n", durationMs);
  
  // Redirection vers la page principale
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}


// ----- Gestionnaire status JSON -----
void handleStatus() {
  String json = "{";
  
  // État activation
  json += "\"enabled\":";
  json += appEnabled ? "true" : "false";
  json += ",";
  
  if (!appEnabled) {
    json += "\"state\":\"disabled\",";
    json += "\"message\":\"⛔ Application suspendue\"";
  } else {
    switch (currentState) {
      case STATE_WAITING:
        json += "\"state\":\"waiting\",";
        json += "\"message\":\"⏸ En attente de déclenchement\"";
        break;
      case STATE_DELAY:
        {
          unsigned long remaining = delayMs - (millis() - stateStartTime);
          json += "\"state\":\"delay\",";
          json += "\"message\":\"⏳ Temporisation: " + String(remaining / 1000) + "s (annulable)\"";
        }
        break;
      case STATE_ACTIVE:
        {
          unsigned long remaining = durationMs - (millis() - stateStartTime);
          json += "\"state\":\"active\",";
          json += "\"message\":\"💡 LED active: " + String(remaining / 1000) + "s\"";
        }
        break;
      case STATE_FINISHED:
        json += "\"state\":\"finished\",";
        json += "\"message\":\"✅ Cycle terminé\"";
        break;
    }
  }
  
  json += "}";
  server.send(200, "application/json", json);
}

// ----- Gestionnaire portail captif (Android/Windows) -----
void handleCaptivePortal() {
  // Rediriger vers la page principale pour déclencher l'affichage du portail
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
  server.send(302, "text/plain", "");
}

// ----- Gestionnaire portail captif (Android/Windows) A TESTER Si problème -----
void handleCaptivePortalATESTER() {
  // Pour déclencher le popup sur Android, il faut répondre avec un code 200
  // et du contenu HTML (pas un 204 ni une redirection)
  // Android compare la réponse attendue et détecte le portail
  
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='0; url=http://";
  html += WiFi.softAPIP().toString();
  html += "/'></head><body>";
  html += "<a href='http://" + WiFi.softAPIP().toString() + "/'>Cliquez ici</a>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}


// ----- Gestionnaire 404 (redirection captive) -----
void handleNotFound() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString());
  server.send(302, "text/plain", "");
}
