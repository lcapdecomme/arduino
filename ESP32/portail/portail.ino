/**
 * PORTAIL - Système de contrôle d'accès RFID UHF
 * 
 * Fichier principal - Configuration et initialisation
 * 
 * Matériel requis:
 * - ESP32
 * - Module RFID UHF IN-R200
 * - LED (optionnel, pour simulation gâche)
 * 
 * Câblage IN-R200:
 * - VCC -> 5V
 * - GND -> GND  
 * - TX  -> GPIO16 (RX2)
 * - RX  -> GPIO17 (TX2)
 */

// ===== INCLUDES =====
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include "R200.h"


// ===== CONFIGURATION =====
// WiFi Access Point
#define AP_SSID         "PORTAIL"
#define AP_PASSWORD     "12345678"

// GPIO
#define PIN_LED         4       // LED simulation gâche
#define PIN_RFID_RX     16      // RX2 - connecté au TX du module
#define PIN_RFID_TX     17      // TX2 - connecté au RX du module

// Paramètres système
#define RFID_BAUDRATE   115200
#define LED_DURATION_MS 3000    // Durée d'activation LED par défaut (ms)
#define MAX_PERSONNES   50      // Nombre max de personnes
#define MAX_CONGES      20      // Nombre max de jours de congés
#define HISTORIQUE_JOURS 30     // Jours d'historique à conserver
#define MAX_HISTORIQUE  100     // Entrées max dans l'historique
#define MAX_CATEGORIES  10      // Nombre max de catégories

// ===== STRUCTURES DE DONNÉES =====

// Plage horaire
struct PlageHoraire {
  bool actif;
  uint8_t heureDebut;    // 0-23
  uint8_t minuteDebut;   // 0-59
  uint8_t heureFin;      // 0-23
  uint8_t minuteFin;     // 0-59
};

// Catégorie de personnel
struct Categorie {
  bool actif;
  char nom[20];           // Nom de la catégorie
  PlageHoraire plages[7]; // Lundi=0 ... Dimanche=6
};

// Personne enregistrée - NOUVELLE STRUCTURE
struct Personne {
  bool actif;
  bool bloque;            // NOUVEAU: agent bloqué
  char uid[25];           // EPC 24 chars + null
  char nom[20];
  char prenom[20];
  char plaque[12];
  char photoFile[20];
  uint8_t categorieId;    // NOUVEAU: ID de la catégorie (remplace plages)
};

// Entrée historique - TAILLE RÉDUITE
struct EntreeHistorique {
  uint32_t timestamp;
  char uid[25];
  char nom[20];
  char prenom[20];
  uint8_t statut;         // NOUVEAU: 0=OK, 1=Inconnu, 2=Bloqué, 3=Hors horaire, 4=Jour férié
};

// Jour de congé/férié
struct JourConge {
  uint8_t jour;
  uint8_t mois;
  uint16_t annee;         // 0 = tous les ans
  char description[20];
};

// Configuration système
struct ConfigSysteme {
  char titre[30];              // Titre personnalisé
  bool marcheForcee;           // Mode marche forcée actif
  uint8_t forceeHeureDebut;    // Heure début marche forcée
  uint8_t forceeMinuteDebut;
  uint8_t forceeHeureFin;      // Heure fin marche forcée
  uint8_t forceeMinuteFin;
};

// Statuts historique
#define STATUT_OK           0
#define STATUT_INCONNU      1
#define STATUT_BLOQUE       2
#define STATUT_HORS_HORAIRE 3
#define STATUT_JOUR_FERIE   4
#define STATUT_MARCHE_FORCEE 5

// ===== OBJETS GLOBAUX =====
DNSServer dnsServer;
WebServer server(80);
R200 rfid;
RTC_DS3231 rtc;

// Données en mémoire
Personne personnes[MAX_PERSONNES];
EntreeHistorique historique[MAX_HISTORIQUE];
JourConge conges[MAX_CONGES];
Categorie categories[MAX_CATEGORIES];
ConfigSysteme config;

int nbPersonnes = 0;
int nbHistorique = 0;
int nbConges = 0;
int nbCategories = 0;
int indexHistorique = 0;  // Index circulaire

// État système
unsigned long ledOffTime = 0;
bool ledActive = false;
unsigned long lastRfidPoll = 0;
String dernierUidLu = "";
unsigned long derniereLecture = 0;
uint32_t dernierAccesTimestamp = 0;  // Pour SSE

// DNS
const byte DNS_PORT = 53;

// ===== PROTOTYPES - Fichier web_handlers.ino =====
void handleRoot();
void handleGetPersonnes();
void handleAddPersonne();
void handleDeletePersonne();
void handleUpdatePersonne();
void handleToggleBloque();
void handleUploadPhoto();
void handleGetPhoto();
void handleGetHistorique();
void handleClearHistorique();
void handleGetStats();
void handleGetConges();
void handleAddConge();
void handleDeleteConge();
void handleGetCategories();
void handleAddCategorie();
void handleUpdateCategorie();
void handleDeleteCategorie();
void handleGetConfig();
void handleSetConfig();
void handleExportData();
void handleImportData();
void handleGetDernierBadge();
void handleRefresh();
void handleFavicon();
void handleNotFound();
void handleCaptivePortal();

// ===== PROTOTYPES - Fichier rfid_access.ino =====
void checkRFID();
void processTag(String uid);
bool isPersonneAutorisee(int index);
bool isJourConge();
bool isDansPlageHoraire(PlageHoraire* plage);
bool isMarcheForceeActive();
int findPersonneByUID(String uid);
void ajouterHistorique(String uid, String nom, String prenom, uint8_t statut);
void activerGache();
void desactiverGache();

// ===== PROTOTYPES - Fonctions utilitaires =====
void loadData();
void savePersonnes();
void saveHistorique();
void saveConges();
void saveCategories();
void saveConfig();
void initDefaultCategories();
void nettoyerHistorique();
String getTimestampStr(uint32_t ts);
String getStatutStr(uint8_t statut);
int getJourSemaine();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("   PORTAIL - Contrôle d'accès RFID");
  Serial.println("========================================\n");
  
  // GPIO
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  Serial.println("[OK] GPIO configurés");
  
  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[ERREUR] Impossible de monter LittleFS");
  } else {
    Serial.println("[OK] LittleFS monté");
  }
  
  // Initialiser config par défaut
  strcpy(config.titre, "PORTAIL");
  config.marcheForcee = false;
  config.forceeHeureDebut = 8;
  config.forceeMinuteDebut = 0;
  config.forceeHeureFin = 18;
  config.forceeMinuteFin = 0;
  
  // Charger les données
  loadData();
  
  // Créer catégories par défaut si aucune
  if (nbCategories == 0) {
    initDefaultCategories();
  }
  
  // RFID
  rfid.begin(&Serial2, RFID_BAUDRATE, PIN_RFID_RX, PIN_RFID_TX);
  Serial.println("[OK] Module RFID initialisé");
  rfid.dumpModuleInfo();
  
  // WiFi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);
  Serial.printf("[OK] WiFi AP: %s @ %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  
  // DNS captif
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  Serial.println("[OK] DNS captif démarré");
  
  // Configuration heure (sans NTP, heure locale fictive)
  /*configTime(0, 0, "");
  struct tm timeinfo;
  timeinfo.tm_year = 2025 - 1900;
  timeinfo.tm_mon = 0;
  timeinfo.tm_mday = 1;
  timeinfo.tm_hour = 12;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  time_t t = mktime(&timeinfo);
  struct timeval tv = { .tv_sec = t };
  settimeofday(&tv, NULL); */
  // Initialisation RTC DS3231
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("[ERREUR] RTC DS3231 non trouvé!");
  } else {
    Serial.println("[OK] RTC DS3231 initialisé");
    // Si RTC a perdu l'alimentation, mettre à l'heure de compilation
    if (rtc.lostPower()) {
      Serial.println("[INFO] RTC reset - mise à l'heure de compilation");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    // Synchroniser l'heure système avec le RTC
    DateTime now = rtc.now();
    struct timeval tv = { .tv_sec = now.unixtime() };
    settimeofday(&tv, NULL);
    Serial.printf("[OK] Heure: %02d/%02d/%04d %02d:%02d:%02d\n", 
      now.day(), now.month(), now.year(), 
      now.hour(), now.minute(), now.second());
  }
  
  // Routes Web
  setupWebServer();
  
  server.begin();
  Serial.println("[OK] Serveur web démarré");
  Serial.println("\n>>> Système prêt <<<\n");
}

// ===== CONFIGURATION SERVEUR WEB =====
void setupWebServer() {
  // Pages principales
  server.on("/", HTTP_GET, handleRoot);
  
  // ===== CAPTIVE PORTAL - TOUTES PLATEFORMES =====
  // Android
  server.on("/generate_204", HTTP_GET, handleCaptivePortal);
  server.on("/gen_204", HTTP_GET, handleCaptivePortal);
  server.on("/mobile/status.php", HTTP_GET, handleCaptivePortal);
  server.on("/success.txt", HTTP_GET, handleCaptivePortal);
  server.on("/wifi/connectivitycheck.gstatic.com", HTTP_GET, handleCaptivePortal);
  server.on("/connectivitycheck.gstatic.com", HTTP_GET, handleCaptivePortal);
  
  // Apple iOS / macOS
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
  server.on("/library/test/success.html", HTTP_GET, handleCaptivePortal);
  server.on("/captive.apple.com", HTTP_GET, handleCaptivePortal);
  server.on("/captive.apple.com/hotspot-detect.html", HTTP_GET, handleCaptivePortal);
  
  // Windows
  server.on("/fwlink", HTTP_GET, handleCaptivePortal);
  server.on("/connecttest.txt", HTTP_GET, handleCaptivePortal);
  server.on("/ncsi.txt", HTTP_GET, handleCaptivePortal);
  server.on("/redirect", HTTP_GET, handleCaptivePortal);
  server.on("/msftconnecttest/redirect", HTTP_GET, handleCaptivePortal);
  server.on("/msftncsi.com/ncsi.txt", HTTP_GET, handleCaptivePortal);
  
  // Linux / NetworkManager / systemd
  server.on("/nm", HTTP_GET, handleCaptivePortal);
  server.on("/check_network_status.txt", HTTP_GET, handleCaptivePortal);
  server.on("/connectivity-check.html", HTTP_GET, handleCaptivePortal);
  server.on("/canonical.html", HTTP_GET, handleCaptivePortal);
  server.on("/ubuntu/connectivity-check.html", HTTP_GET, handleCaptivePortal);
  server.on("/nmcheck.gnome.org", HTTP_GET, handleCaptivePortal);
  
  // Firefox
  server.on("/success.txt", HTTP_GET, handleCaptivePortal);
  server.on("/detectportal.firefox.com", HTTP_GET, handleCaptivePortal);
  server.on("/detectportal.firefox.com/success.txt", HTTP_GET, handleCaptivePortal);
  
  // Chrome
  server.on("/chrome/test", HTTP_GET, handleCaptivePortal);
  server.on("/clients3.google.com", HTTP_GET, handleCaptivePortal);
  
  // Générique
  server.on("/kindle-wifi/wifistub.html", HTTP_GET, handleCaptivePortal);
  server.on("/favicon.ico", HTTP_GET, handleFavicon);
  
  // API pour rafraîchissement (remplace SSE)
  server.on("/api/refresh", HTTP_GET, handleRefresh);
  
  // API REST - Personnes
  server.on("/api/personnes", HTTP_GET, handleGetPersonnes);
  server.on("/api/personnes", HTTP_POST, handleAddPersonne);
  server.on("/api/personnes/delete", HTTP_POST, handleDeletePersonne);
  server.on("/api/personnes/update", HTTP_POST, handleUpdatePersonne);
  server.on("/api/personnes/toggle-bloque", HTTP_POST, handleToggleBloque);
  
  // API REST - Photos
  server.on("/api/photo/upload", HTTP_POST, [](){
    handleUploadPhoto();
  }, handlePhotoUpload);
  server.on("/api/photo", HTTP_GET, handleGetPhoto);
  
  // API REST - Historique
  server.on("/api/historique", HTTP_GET, handleGetHistorique);
  server.on("/api/historique/clear", HTTP_POST, handleClearHistorique);
  server.on("/api/stats", HTTP_GET, handleGetStats);
  
  // API REST - Congés
  server.on("/api/conges", HTTP_GET, handleGetConges);
  server.on("/api/conges", HTTP_POST, handleAddConge);
  server.on("/api/conges/delete", HTTP_POST, handleDeleteConge);
  
  // API REST - Catégories
  server.on("/api/categories", HTTP_GET, handleGetCategories);
  server.on("/api/categories", HTTP_POST, handleAddCategorie);
  server.on("/api/categories/update", HTTP_POST, handleUpdateCategorie);
  server.on("/api/categories/delete", HTTP_POST, handleDeleteCategorie);
  
  // API REST - Configuration
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handleSetConfig);
  
  // API REST - Export/Import
  server.on("/api/export", HTTP_GET, handleExportData);
  server.on("/api/import", HTTP_POST, handleImportData);
  
  // API REST - Badge
  server.on("/api/dernier-badge", HTTP_GET, handleGetDernierBadge);
  
  // Fichiers statiques
  server.serveStatic("/photos/", LittleFS, "/photos/");
  
  // 404 - Redirige vers page principale (captive portal)
  server.onNotFound(handleNotFound);
  
  // Headers CORS
  server.enableCORS(true);
}

// ===== LOOP =====
void loop() {
  // DNS captif
  dnsServer.processNextRequest();
  
  // Serveur web
  server.handleClient();
  
  // Lecture RFID
  rfid.loop();
  if (millis() - lastRfidPoll > 500) {
    rfid.poll();
    checkRFID();
    lastRfidPoll = millis();
  }
  
  // Gestion LED/Gâche
  if (ledActive && millis() >= ledOffTime) {
    desactiverGache();
  }
  
  // Nettoyage historique périodique (toutes les heures)
  static unsigned long lastCleanup = 0;
  if (millis() - lastCleanup > 3600000) {
    nettoyerHistorique();
    lastCleanup = millis();
  }
}

// ===== FONCTIONS UTILITAIRES =====

void loadData() {
  Serial.println("[INFO] Chargement des données...");
  
  // Charger catégories
  File f = LittleFS.open("/categories.json", "r");
  if (f) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (!err) {
      nbCategories = 0;
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject obj : arr) {
        if (nbCategories >= MAX_CATEGORIES) break;
        Categorie* c = &categories[nbCategories];
        c->actif = true;
        strlcpy(c->nom, obj["nom"] | "", sizeof(c->nom));
        JsonArray plagesArr = obj["plages"];
        for (int j = 0; j < 7 && j < plagesArr.size(); j++) {
          JsonObject pl = plagesArr[j];
          c->plages[j].actif = pl["actif"] | false;
          c->plages[j].heureDebut = pl["hd"] | 0;
          c->plages[j].minuteDebut = pl["md"] | 0;
          c->plages[j].heureFin = pl["hf"] | 23;
          c->plages[j].minuteFin = pl["mf"] | 59;
        }
        nbCategories++;
      }
      Serial.printf("[OK] %d catégories chargées\n", nbCategories);
    }
  }
  
  // Charger personnes
  f = LittleFS.open("/personnes.json", "r");
  if (f) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (!err) {
      nbPersonnes = 0;
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject obj : arr) {
        if (nbPersonnes >= MAX_PERSONNES) break;
        Personne* p = &personnes[nbPersonnes];
        p->actif = true;
        p->bloque = obj["bloque"] | false;
        strlcpy(p->uid, obj["uid"] | "", sizeof(p->uid));
        strlcpy(p->nom, obj["nom"] | "", sizeof(p->nom));
        strlcpy(p->prenom, obj["prenom"] | "", sizeof(p->prenom));
        strlcpy(p->plaque, obj["plaque"] | "", sizeof(p->plaque));
        strlcpy(p->photoFile, obj["photo"] | "", sizeof(p->photoFile));
        p->categorieId = obj["categorie"] | 0;
        nbPersonnes++;
      }
      Serial.printf("[OK] %d personnes chargées\n", nbPersonnes);
    }
  }
  
  // Charger historique
  f = LittleFS.open("/historique.json", "r");
  if (f) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (!err) {
      nbHistorique = 0;
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject obj : arr) {
        if (nbHistorique >= MAX_HISTORIQUE) break;
        EntreeHistorique* e = &historique[nbHistorique];
        e->timestamp = obj["ts"] | 0;
        strlcpy(e->uid, obj["uid"] | "", sizeof(e->uid));
        strlcpy(e->nom, obj["nom"] | "", sizeof(e->nom));
        strlcpy(e->prenom, obj["prenom"] | "", sizeof(e->prenom));
        e->statut = obj["statut"] | 0;
        nbHistorique++;
      }
      indexHistorique = nbHistorique % MAX_HISTORIQUE;
      Serial.printf("[OK] %d entrées historique chargées\n", nbHistorique);
    }
  }
  
  // Charger congés
  f = LittleFS.open("/conges.json", "r");
  if (f) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (!err) {
      nbConges = 0;
      JsonArray arr = doc.as<JsonArray>();
      for (JsonObject obj : arr) {
        if (nbConges >= MAX_CONGES) break;
        JourConge* c = &conges[nbConges];
        c->jour = obj["j"] | 1;
        c->mois = obj["m"] | 1;
        c->annee = obj["a"] | 0;
        strlcpy(c->description, obj["desc"] | "", sizeof(c->description));
        nbConges++;
      }
      Serial.printf("[OK] %d jours de congés chargés\n", nbConges);
    }
  }
  
  // Charger configuration
  f = LittleFS.open("/config.json", "r");
  if (f) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (!err) {
      strlcpy(config.titre, doc["titre"] | "PORTAIL", sizeof(config.titre));
      config.marcheForcee = doc["marcheForcee"] | false;
      config.forceeHeureDebut = doc["forceeHD"] | 8;
      config.forceeMinuteDebut = doc["forceeMD"] | 0;
      config.forceeHeureFin = doc["forceeHF"] | 18;
      config.forceeMinuteFin = doc["forceeMF"] | 0;
      Serial.printf("[OK] Configuration chargée (titre: %s)\n", config.titre);
    }
  }
}

void initDefaultCategories() {
  Serial.println("[INFO] Création des catégories par défaut...");
  
  // Direction : Lun-Dim 00h-23h59
  Categorie* c = &categories[0];
  c->actif = true;
  strcpy(c->nom, "Direction");
  for (int j = 0; j < 7; j++) {
    c->plages[j].actif = true;
    c->plages[j].heureDebut = 0;
    c->plages[j].minuteDebut = 0;
    c->plages[j].heureFin = 23;
    c->plages[j].minuteFin = 59;
  }
  
  // Service nettoyage : Lun-Sam 6h-22h
  c = &categories[1];
  c->actif = true;
  strcpy(c->nom, "Nettoyage");
  for (int j = 0; j < 7; j++) {
    c->plages[j].actif = (j < 6); // Lun-Sam
    c->plages[j].heureDebut = 6;
    c->plages[j].minuteDebut = 0;
    c->plages[j].heureFin = 22;
    c->plages[j].minuteFin = 0;
  }
  
  // Commercial : Lun-Ven 8h-20h
  c = &categories[2];
  c->actif = true;
  strcpy(c->nom, "Commercial");
  for (int j = 0; j < 7; j++) {
    c->plages[j].actif = (j < 5); // Lun-Ven
    c->plages[j].heureDebut = 8;
    c->plages[j].minuteDebut = 0;
    c->plages[j].heureFin = 20;
    c->plages[j].minuteFin = 0;
  }
  
  // Personnel : Lun-Ven 8h-18h
  c = &categories[3];
  c->actif = true;
  strcpy(c->nom, "Personnel");
  for (int j = 0; j < 7; j++) {
    c->plages[j].actif = (j < 5); // Lun-Ven
    c->plages[j].heureDebut = 8;
    c->plages[j].minuteDebut = 0;
    c->plages[j].heureFin = 18;
    c->plages[j].minuteFin = 0;
  }
  
  nbCategories = 4;
  saveCategories();
  Serial.println("[OK] 4 catégories créées");
}

void saveCategories() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbCategories; i++) {
    if (!categories[i].actif) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["nom"] = categories[i].nom;
    
    JsonArray plagesArr = obj["plages"].to<JsonArray>();
    for (int j = 0; j < 7; j++) {
      JsonObject pl = plagesArr.add<JsonObject>();
      pl["actif"] = categories[i].plages[j].actif;
      pl["hd"] = categories[i].plages[j].heureDebut;
      pl["md"] = categories[i].plages[j].minuteDebut;
      pl["hf"] = categories[i].plages[j].heureFin;
      pl["mf"] = categories[i].plages[j].minuteFin;
    }
  }
  
  File f = LittleFS.open("/categories.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void savePersonnes() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbPersonnes; i++) {
    if (!personnes[i].actif) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["uid"] = personnes[i].uid;
    obj["nom"] = personnes[i].nom;
    obj["prenom"] = personnes[i].prenom;
    obj["plaque"] = personnes[i].plaque;
    obj["photo"] = personnes[i].photoFile;
    obj["bloque"] = personnes[i].bloque;
    obj["categorie"] = personnes[i].categorieId;
  }
  
  File f = LittleFS.open("/personnes.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void saveHistorique() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbHistorique && i < MAX_HISTORIQUE; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["ts"] = historique[i].timestamp;
    obj["uid"] = historique[i].uid;
    obj["nom"] = historique[i].nom;
    obj["prenom"] = historique[i].prenom;
    obj["statut"] = historique[i].statut;
  }
  
  File f = LittleFS.open("/historique.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void saveConges() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbConges; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["j"] = conges[i].jour;
    obj["m"] = conges[i].mois;
    obj["a"] = conges[i].annee;
    obj["desc"] = conges[i].description;
  }
  
  File f = LittleFS.open("/conges.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void saveConfig() {
  JsonDocument doc;
  doc["titre"] = config.titre;
  doc["marcheForcee"] = config.marcheForcee;
  doc["forceeHD"] = config.forceeHeureDebut;
  doc["forceeMD"] = config.forceeMinuteDebut;
  doc["forceeHF"] = config.forceeHeureFin;
  doc["forceeMF"] = config.forceeMinuteFin;
  
  File f = LittleFS.open("/config.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

void nettoyerHistorique() {
  time_t now;
  time(&now);
  uint32_t limite = now - (HISTORIQUE_JOURS * 24 * 3600);
  
  int nouveauNb = 0;
  for (int i = 0; i < nbHistorique; i++) {
    if (historique[i].timestamp >= limite) {
      if (i != nouveauNb) {
        historique[nouveauNb] = historique[i];
      }
      nouveauNb++;
    }
  }
  
  if (nouveauNb != nbHistorique) {
    nbHistorique = nouveauNb;
    indexHistorique = nbHistorique % MAX_HISTORIQUE;
    saveHistorique();
    Serial.printf("[INFO] Historique nettoyé: %d entrées\n", nbHistorique);
  }
}

String getTimestampStr(uint32_t ts) {
  time_t t = ts;
  struct tm* tm = localtime(&t);
  char buf[32];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", tm);
  return String(buf);
}

String getStatutStr(uint8_t statut) {
  switch (statut) {
    case STATUT_OK: return "OK";
    case STATUT_INCONNU: return "Inconnu";
    case STATUT_BLOQUE: return "Bloque";
    case STATUT_HORS_HORAIRE: return "Hors horaire";
    case STATUT_JOUR_FERIE: return "Jour ferie";
    case STATUT_MARCHE_FORCEE: return "Marche forcee";
    default: return "?";
  }
}

int getJourSemaine() {
  time_t now;
  time(&now);
  struct tm* tm = localtime(&now);
  return (tm->tm_wday + 6) % 7;
}

// Placeholder pour upload photo
void handlePhotoUpload() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/photos/" + String(upload.filename);
    uploadFile = LittleFS.open(filename, "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
  }
}
