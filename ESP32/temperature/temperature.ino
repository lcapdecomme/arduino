#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <time.h>

// Configuration des broches
#define ONE_WIRE_BUS 4    // GPIO4 pour le DS18B20
#define LED_ROUGE 2       // GPIO2 pour LED rouge
#define LED_VERTE 15      // GPIO15 pour LED verte

// Configuration WiFi (Point d'accès)
const char* ssid = "ESP32-Temperature";
const char* password = "";  // Pas de mot de passe

// Serveur DNS pour le portail captif
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Objets
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WebServer server(80);
Preferences preferences;

// Variables globales
float temperatureActuelle = 0.0;
float limiteMax = 30.0;  // Limite maximale par défaut
float limiteMin = 10.0;  // Limite minimale par défaut

// Structure pour l'historique
struct Depassement {
  String dateHeure;
  float temperature;
  String type;  // "MAX" ou "MIN"
};

Depassement historique[5];
int indexHistorique = 0;
int nombreDepassements = 0;

bool dernierEtatMax = false;
bool dernierEtatMin = false;

// Configuration du serveur NTP pour l'heure
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;  // GMT+1 (France)
const int daylightOffset_sec = 3600;  // Heure d'été

void setup() {
  Serial.begin(115200);
  
  // Configuration des LEDs
  pinMode(LED_ROUGE, OUTPUT);
  pinMode(LED_VERTE, OUTPUT);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_VERTE, LOW);
  
  // Démarrage du capteur de température
  sensors.begin();
  
  // Chargement des préférences
  preferences.begin("temp-config", false);
  limiteMax = preferences.getFloat("limiteMax", 30.0);
  limiteMin = preferences.getFloat("limiteMin", 10.0);
  chargerHistorique();
  
  // Configuration du WiFi en mode Point d'accès
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Point d'accès démarré. IP: ");
  Serial.println(IP);
  
  // Démarrage du serveur DNS pour le portail captif
  // Redirige toutes les requêtes DNS vers l'IP de l'ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("Serveur DNS captif démarré");
  
  // Configuration du temps (NTP) - tentera de se synchroniser si possible
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Configuration des routes du serveur web
  server.on("/", handleRoot);
  server.on("/temperature", handleTemperature);
  server.on("/limites", HTTP_GET, handleLimites);   // GET pour récupérer
  server.on("/limites", HTTP_POST, handleLimites);  // POST pour enregistrer
  server.on("/historique", handleHistorique);
  server.on("/effacer-historique", HTTP_POST, handleEffacerHistorique);
  
  // Routes pour le portail captif (détection automatique sur différents OS)
  server.on("/generate_204", handleRoot);  // Android
  server.on("/fwlink", handleRoot);        // Microsoft
  server.on("/hotspot-detect.html", handleRoot);  // Apple
  server.on("/canonical.html", handleRoot);       // Firefox
  server.on("/success.txt", handleRoot);          // Firefox
  server.on("/ncsi.txt", handleRoot);             // Windows
  
  // Redirection de toutes les autres requêtes vers la page principale
  server.onNotFound(handleRoot);
  
  server.begin();
  Serial.println("Serveur HTTP démarré");
}

void loop() {
  dnsServer.processNextRequest();  // Traitement des requêtes DNS pour le portail captif
  server.handleClient();
  
  // Lecture de la température toutes les 2 secondes
  static unsigned long derniereLecture = 0;
  if (millis() - derniereLecture > 2000) {
    derniereLecture = millis();
    lireTemperature();
    verifierLimites();
  }
}

void lireTemperature() {
  sensors.requestTemperatures();
  temperatureActuelle = sensors.getTempCByIndex(0);
  
  if (temperatureActuelle == DEVICE_DISCONNECTED_C) {
    Serial.println("Erreur: Capteur déconnecté!");
    temperatureActuelle = 0.0;
  }
}

void verifierLimites() {
  bool etatMax = temperatureActuelle > limiteMax;
  bool etatMin = temperatureActuelle < limiteMin;
  
  // Gestion LED rouge (dépassement max)
  if (etatMax) {
    digitalWrite(LED_ROUGE, HIGH);
    if (!dernierEtatMax) {
      ajouterDepassement("MAX");
      dernierEtatMax = true;
    }
  } else {
    digitalWrite(LED_ROUGE, LOW);
    dernierEtatMax = false;
  }
  
  // Gestion LED verte (dépassement min)
  if (etatMin) {
    digitalWrite(LED_VERTE, HIGH);
    if (!dernierEtatMin) {
      ajouterDepassement("MIN");
      dernierEtatMin = true;
    }
  } else {
    digitalWrite(LED_VERTE, LOW);
    dernierEtatMin = false;
  }
}

void ajouterDepassement(String type) {
  struct tm timeinfo;
  String dateHeure;
  
  if (getLocalTime(&timeinfo)) {
    char buffer[30];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    dateHeure = String(buffer);
  } else {
    dateHeure = "Heure non disponible";
  }
  
  historique[indexHistorique].dateHeure = dateHeure;
  historique[indexHistorique].temperature = temperatureActuelle;
  historique[indexHistorique].type = type;
  
  indexHistorique = (indexHistorique + 1) % 5;
  if (nombreDepassements < 5) nombreDepassements++;
  
  sauvegarderHistorique();
  
  Serial.println("Dépassement " + type + " détecté: " + String(temperatureActuelle) + "°C à " + dateHeure);
}

void chargerHistorique() {
  nombreDepassements = preferences.getInt("nbDepass", 0);
  indexHistorique = preferences.getInt("indexHist", 0);
  
  for (int i = 0; i < 5; i++) {
    String key = "hist_" + String(i);
    String data = preferences.getString(key.c_str(), "");
    if (data != "") {
      int pos1 = data.indexOf('|');
      int pos2 = data.indexOf('|', pos1 + 1);
      historique[i].dateHeure = data.substring(0, pos1);
      historique[i].temperature = data.substring(pos1 + 1, pos2).toFloat();
      historique[i].type = data.substring(pos2 + 1);
    }
  }
}

void sauvegarderHistorique() {
  preferences.putInt("nbDepass", nombreDepassements);
  preferences.putInt("indexHist", indexHistorique);
  
  for (int i = 0; i < 5; i++) {
    String key = "hist_" + String(i);
    String data = historique[i].dateHeure + "|" + 
                  String(historique[i].temperature, 2) + "|" + 
                  historique[i].type;
    preferences.putString(key.c_str(), data);
  }
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Moniteur de Température ESP32</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        
        .card {
            background: white;
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 30px;
            font-size: 2em;
        }
        
        .temp-display {
            text-align: center;
            padding: 40px 20px;
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
            border-radius: 15px;
            color: white;
            margin-bottom: 20px;
        }
        
        .temp-value {
            font-size: 4em;
            font-weight: bold;
            margin: 10px 0;
        }
        
        .temp-label {
            font-size: 1.2em;
            opacity: 0.9;
        }
        
        .limites-section {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .limite-input {
            display: flex;
            flex-direction: column;
        }
        
        label {
            color: #555;
            font-weight: 600;
            margin-bottom: 8px;
            font-size: 0.95em;
        }
        
        input[type="number"] {
            padding: 12px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 1.1em;
            transition: border-color 0.3s;
        }
        
        input[type="number"]:focus {
            outline: none;
            border-color: #667eea;
        }
        
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 15px 30px;
            border-radius: 8px;
            font-size: 1.1em;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
            width: 100%;
        }
        
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        
        button:active {
            transform: translateY(0);
        }
        
        .status-indicators {
            display: flex;
            justify-content: center;
            gap: 30px;
            margin: 20px 0;
        }
        
        .indicator {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .led {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            box-shadow: 0 0 10px rgba(0,0,0,0.2);
        }
        
        .led.rouge {
            background-color: #ff4444;
        }
        
        .led.verte {
            background-color: #44ff44;
        }
        
        .led.off {
            background-color: #ddd;
        }
        
        .historique-section {
            margin-top: 20px;
        }
        
        .historique-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
        }
        
        .historique-header h2 {
            color: #333;
            font-size: 1.5em;
        }
        
        .btn-effacer {
            background: #ff4444;
            padding: 8px 15px;
            font-size: 0.9em;
            width: auto;
        }
        
        .historique-liste {
            list-style: none;
        }
        
        .historique-item {
            padding: 15px;
            border-left: 4px solid #667eea;
            background: #f8f9fa;
            margin-bottom: 10px;
            border-radius: 5px;
        }
        
        .historique-item.max {
            border-left-color: #ff4444;
        }
        
        .historique-item.min {
            border-left-color: #44ff44;
        }
        
        .historique-date {
            font-size: 0.9em;
            color: #666;
        }
        
        .historique-temp {
            font-weight: bold;
            font-size: 1.2em;
            color: #333;
        }
        
        .historique-type {
            display: inline-block;
            padding: 3px 10px;
            border-radius: 12px;
            font-size: 0.85em;
            font-weight: 600;
            margin-left: 10px;
        }
        
        .historique-type.max {
            background: #ffe0e0;
            color: #ff4444;
        }
        
        .historique-type.min {
            background: #e0ffe0;
            color: #44aa44;
        }
        
        .aucun-depassement {
            text-align: center;
            padding: 20px;
            color: #999;
            font-style: italic;
        }
        
        @media (max-width: 600px) {
            .temp-value {
                font-size: 3em;
            }
            
            .limites-section {
                grid-template-columns: 1fr;
            }
            
            .status-indicators {
                flex-direction: column;
                gap: 15px;
            }
            
            h1 {
                font-size: 1.5em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="card">
            <h1>🌡️ Moniteur de Température</h1>
            
            <div class="temp-display">
                <div class="temp-label">Température Actuelle</div>
                <div class="temp-value" id="temperature">--°C</div>
            </div>
            
            <div class="status-indicators">
                <div class="indicator">
                    <div class="led rouge off" id="led-rouge"></div>
                    <span>Dépassement Max</span>
                </div>
                <div class="indicator">
                    <div class="led verte off" id="led-verte"></div>
                    <span>Dépassement Min</span>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h2 style="margin-bottom: 20px; color: #333;">⚙️ Configuration des Limites</h2>
            <form id="limitesForm">
                <div class="limites-section">
                    <div class="limite-input">
                        <label for="limiteMax">🔴 Limite Maximale (°C)</label>
                        <input type="number" id="limiteMax" step="0.1" required>
                    </div>
                    <div class="limite-input">
                        <label for="limiteMin">🟢 Limite Minimale (°C)</label>
                        <input type="number" id="limiteMin" step="0.1" required>
                    </div>
                </div>
                <button type="submit">Enregistrer les Limites</button>
            </form>
        </div>
        
        <div class="card historique-section">
            <div class="historique-header">
                <h2>📋 Historique des Dépassements</h2>
                <button class="btn-effacer" onclick="effacerHistorique()">Effacer</button>
            </div>
            <ul class="historique-liste" id="historique">
                <li class="aucun-depassement">Aucun dépassement enregistré</li>
            </ul>
        </div>
    </div>
    
    <script>
        // Charger les limites actuelles
        fetch('/limites')
            .then(response => response.json())
            .then(data => {
                document.getElementById('limiteMax').value = data.max;
                document.getElementById('limiteMin').value = data.min;
            });
        
        // Charger l'historique
        function chargerHistorique() {
            fetch('/historique')
                .then(response => response.json())
                .then(data => {
                    const liste = document.getElementById('historique');
                    if (data.depassements.length === 0) {
                        liste.innerHTML = '<li class="aucun-depassement">Aucun dépassement enregistré</li>';
                    } else {
                        liste.innerHTML = data.depassements.map(d => `
                            <li class="historique-item ${d.type.toLowerCase()}">
                                <div class="historique-date">${d.dateHeure}</div>
                                <div>
                                    <span class="historique-temp">${d.temperature}°C</span>
                                    <span class="historique-type ${d.type.toLowerCase()}">${d.type === 'MAX' ? 'Dépassement Max' : 'Dépassement Min'}</span>
                                </div>
                            </li>
                        `).join('');
                    }
                });
        }
        
        chargerHistorique();
        
        // Actualiser la température
        function updateTemperature() {
            fetch('/temperature')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temperature').textContent = data.temperature.toFixed(1) + '°C';
                    
                    // Mise à jour des LEDs
                    const ledRouge = document.getElementById('led-rouge');
                    const ledVerte = document.getElementById('led-verte');
                    
                    if (data.depassementMax) {
                        ledRouge.classList.remove('off');
                    } else {
                        ledRouge.classList.add('off');
                    }
                    
                    if (data.depassementMin) {
                        ledVerte.classList.remove('off');
                    } else {
                        ledVerte.classList.add('off');
                    }
                });
        }
        
        // Enregistrer les limites
        document.getElementById('limitesForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const max = document.getElementById('limiteMax').value;
            const min = document.getElementById('limiteMin').value;
            
            fetch('/limites', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: `max=${max}&min=${min}`
            })
            .then(response => response.json())
            .then(data => {
                alert('✅ Limites enregistrées avec succès!');
            });
        });
        
        // Effacer l'historique
        function effacerHistorique() {
            if (confirm('Êtes-vous sûr de vouloir effacer l\'historique ?')) {
                fetch('/effacer-historique', {method: 'POST'})
                    .then(() => {
                        chargerHistorique();
                        alert('✅ Historique effacé');
                    });
            }
        }
        
        // Actualiser toutes les 2 secondes
        updateTemperature();
        setInterval(updateTemperature, 2000);
        setInterval(chargerHistorique, 5000);
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleTemperature() {
  bool depassementMax = temperatureActuelle > limiteMax;
  bool depassementMin = temperatureActuelle < limiteMin;
  
  String json = "{";
  json += "\"temperature\":" + String(temperatureActuelle, 2) + ",";
  json += "\"depassementMax\":" + String(depassementMax ? "true" : "false") + ",";
  json += "\"depassementMin\":" + String(depassementMin ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleLimites() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("max") && server.hasArg("min")) {
      limiteMax = server.arg("max").toFloat();
      limiteMin = server.arg("min").toFloat();
      
      preferences.putFloat("limiteMax", limiteMax);
      preferences.putFloat("limiteMin", limiteMin);
      
      String json = "{";
      json += "\"status\":\"ok\",";
      json += "\"max\":" + String(limiteMax, 2) + ",";
      json += "\"min\":" + String(limiteMin, 2);
      json += "}";
      server.send(200, "application/json", json);
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Paramètres manquants\"}");
    }
  } else {
    // GET - Récupération des limites actuelles
    String json = "{";
    json += "\"max\":" + String(limiteMax, 2) + ",";
    json += "\"min\":" + String(limiteMin, 2);
    json += "}";
    server.send(200, "application/json", json);
  }
}

void handleHistorique() {
  String json = "{\"depassements\":[";
  
  int debut = (indexHistorique + 5 - nombreDepassements) % 5;
  for (int i = 0; i < nombreDepassements; i++) {
    int idx = (debut + i) % 5;
    if (i > 0) json += ",";
    json += "{";
    json += "\"dateHeure\":\"" + historique[idx].dateHeure + "\",";
    json += "\"temperature\":" + String(historique[idx].temperature, 2) + ",";
    json += "\"type\":\"" + historique[idx].type + "\"";
    json += "}";
  }
  
  json += "]}";
  server.send(200, "application/json", json);
}

void handleEffacerHistorique() {
  nombreDepassements = 0;
  indexHistorique = 0;
  
  for (int i = 0; i < 5; i++) {
    historique[i].dateHeure = "";
    historique[i].temperature = 0.0;
    historique[i].type = "";
  }
  
  sauvegarderHistorique();
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}
