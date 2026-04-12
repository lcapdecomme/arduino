/**
 * PORTAIL - Contrôle d'accès RFID UHF - v2
 * Fichier principal : Setup, loop, WiFi captif, DNS, RFID, routes
 *
 * Matériel : ESP32 + Lecteur R200 UHF + LED GPIO2
 * Stockage : LittleFS (JSON)
 * Interface : Web responsive (WiFi AP captif)
 *
 * Architecture :
 *   portail.ino      -> Setup, loop, WiFi, DNS, routes, logique accès
 *   web_pages.ino    -> Pages HTML + handlers API
 *   data_manager.ino -> LittleFS, CRUD, historique, config
 *   R200.h / R200.cpp -> Driver RFID UHF (existant)
 */

// =====================================================
// INCLUDES
// =====================================================
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <time.h>
#include "R200.h"

// =====================================================
// CONFIGURATION MATERIELLE
// =====================================================
#define AP_SSID       "PORTAIL"
#define AP_PASSWORD   "12345678"
#define DNS_PORT      53
#define LED_PIN       2
#define LED_DURATION  3000
#define RFID_POLL_MS  800
#define RFID_RX_PIN   16
#define RFID_TX_PIN   17
#define RFID_BAUD     115200
#define TAG_LENGTH    24

// =====================================================
// LIMITES MEMOIRE
// =====================================================
#define MAX_AGENTS      200
#define MAX_HISTORY     1000
#define MAX_HOLIDAYS    30
#define MAX_CATEGORIES  10

// =====================================================
// STATUTS D'ACCES
// =====================================================
#define STATUS_OK         "OK"
#define STATUS_INCONNU    "Inconnu"
#define STATUS_BLOQUE     "Bloque"
#define STATUS_HORAIRE    "Horaire"
#define STATUS_FERIE      "Ferie"
#define STATUS_FORCE_OK   "ForceOK"

// =====================================================
// STRUCTURES (char[] fixes pour économiser le heap)
// =====================================================
struct Category {
  char nom[20];
  char joursAutorise[16];  // "1,2,3,4,5,6,7"
  char heureDebut[6];      // "08:00"
  char heureFin[6];        // "18:00"
};

struct Agent {
  char tag[25];            // 24 hex + \0
  char nom[30];
  char prenom[30];
  char plaque[12];
  int  categorieIdx;
  bool bloque;
};

struct HistoryEntry {
  char dateTime[20];       // "2025-02-10 14:30:25"
  char tag[25];
  char nom[30];
  char prenom[30];
  char statut[10];
};

struct Holiday {
  char date[12];           // "2025-12-25"
  char label[20];
};

struct Config {
  char titre[30];
  bool marcheForce;
  char forceHeureDebut[6];
  char forceHeureFin[6];
};

// =====================================================
// PROTOTYPES - web_pages.ino
// =====================================================
void handleRoot();
void handleEnrollPage();
void handleAuthPage();
void handleHistoryPage();
void handleConfigPage();
void handleApiAgents();
void handleApiAgentAdd();
void handleApiAgentDelete();
void handleApiAgentUpdate();
void handleApiAgentBlock();
void handleApiHistory();
void handleApiStats();
void handleApiCategories();
void handleApiCategoryAdd();
void handleApiCategoryUpdate();
void handleApiCategoryDelete();
void handleApiHolidays();
void handleApiHolidayAdd();
void handleApiHolidayDelete();
void handleApiConfig();
void handleApiConfigUpdate();
void handleApiExport();
void handleApiImport();
void handleApiLastTag();
void handleNotFound();
void handleCaptiveDetect();

// =====================================================
// PROTOTYPES - data_manager.ino
// =====================================================
void    initStorage();
bool    loadAgents();
bool    saveAgents();
bool    loadHistory();
bool    saveHistory();
bool    loadHolidays();
bool    saveHolidays();
bool    loadCategories();
bool    saveCategories();
bool    loadConfig();
bool    saveConfig();
void    initDefaultCategories();
int     findAgentByTag(const String &tag);
int     addAgent(const String &tag, const String &nom, const String &prenom, const String &plaque, int catIdx);
bool    deleteAgent(int index);
bool    updateAgent(int index, const String &nom, const String &prenom, const String &plaque, int catIdx);
bool    setAgentBlocked(int index, bool bloque);
void    addHistoryEntry(const String &tag, const String &nom, const String &prenom, const String &statut);
String  getStatsJson();
String  getAgentsJson();
String  getHistoryJson();
String  getHolidaysJson();
String  getCategoriesJson();
String  getConfigJson();
String  getAllDataJson();
bool    importAllData(const String &json);
bool    isTodayHoliday();
bool    isInForceMode();
String  checkAccess(int agentIndex);
String  getCurrentTimeStr();
String  getCurrentDateStr();
String  getCurrentDateTimeStr();
int     getCurrentHour();
int     getCurrentMinute();
int     getCurrentDayOfWeek();

// =====================================================
// VARIABLES GLOBALES
// =====================================================
DNSServer dnsServer;
WebServer server(80);
R200 rfid;

Agent        agents[MAX_AGENTS];
int          agentCount = 0;
HistoryEntry history[MAX_HISTORY];
int          historyCount = 0;
Holiday      holidays[MAX_HOLIDAYS];
int          holidayCount = 0;
Category     categories[MAX_CATEGORIES];
int          categoryCount = 0;
Config       config;

// RFID
unsigned long lastPollTime = 0;
String lastDetectedTag = "";
unsigned long lastDetectedTime = 0;

// LED
bool ledActive = false;
unsigned long ledStartTime = 0;

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== PORTAIL v2 ===\n");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  configTime(3600, 3600, "pool.ntp.org");

  initStorage();
  loadConfig();
  loadCategories();
  loadAgents();
  loadHistory();
  loadHolidays();

  Serial.printf("Titre: %s | Cat: %d | Agents: %d | Hist: %d | Fer: %d\n",
    config.titre.c_str(), categoryCount, agentCount, historyCount, holidayCount);

  rfid.begin(&Serial2, RFID_BAUD, RFID_RX_PIN, RFID_TX_PIN);
  rfid.dumpModuleInfo();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(500);
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("WiFi: %s @ %s\n", AP_SSID, apIP.toString().c_str());

  dnsServer.start(DNS_PORT, "*", apIP);

  // Captive portal detection
  server.on("/generate_204",        HTTP_GET, handleCaptiveDetect);
  server.on("/gen_204",             HTTP_GET, handleCaptiveDetect);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveDetect);
  server.on("/canonical.html",      HTTP_GET, handleCaptiveDetect);
  server.on("/connecttest.txt",     HTTP_GET, handleCaptiveDetect);
  server.on("/redirect",            HTTP_GET, handleCaptiveDetect);
  server.on("/ncsi.txt",            HTTP_GET, handleCaptiveDetect);
  server.on("/fwlink",              HTTP_GET, handleCaptiveDetect);

  // Pages
  server.on("/",         HTTP_GET, handleRoot);
  server.on("/enroll",   HTTP_GET, handleEnrollPage);
  server.on("/auth",     HTTP_GET, handleAuthPage);
  server.on("/history",  HTTP_GET, handleHistoryPage);
  server.on("/config",   HTTP_GET, handleConfigPage);

  // API
  server.on("/api/agents",          HTTP_GET,  handleApiAgents);
  server.on("/api/agent/add",       HTTP_POST, handleApiAgentAdd);
  server.on("/api/agent/delete",    HTTP_POST, handleApiAgentDelete);
  server.on("/api/agent/update",    HTTP_POST, handleApiAgentUpdate);
  server.on("/api/agent/block",     HTTP_POST, handleApiAgentBlock);
  server.on("/api/categories",      HTTP_GET,  handleApiCategories);
  server.on("/api/category/add",    HTTP_POST, handleApiCategoryAdd);
  server.on("/api/category/update", HTTP_POST, handleApiCategoryUpdate);
  server.on("/api/category/delete", HTTP_POST, handleApiCategoryDelete);
  server.on("/api/history",         HTTP_GET,  handleApiHistory);
  server.on("/api/stats",           HTTP_GET,  handleApiStats);
  server.on("/api/holidays",        HTTP_GET,  handleApiHolidays);
  server.on("/api/holiday/add",     HTTP_POST, handleApiHolidayAdd);
  server.on("/api/holiday/delete",  HTTP_POST, handleApiHolidayDelete);
  server.on("/api/config",          HTTP_GET,  handleApiConfig);
  server.on("/api/config/update",   HTTP_POST, handleApiConfigUpdate);
  server.on("/api/export",          HTTP_GET,  handleApiExport);
  server.on("/api/import",          HTTP_POST, handleApiImport);
  server.on("/api/last-tag",        HTTP_GET,  handleApiLastTag);

  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Pret.\n");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (millis() - lastPollTime > RFID_POLL_MS) {
    rfid.loop();
    rfid.poll();
    String currentTag = readTagFromR200();
    if (currentTag.length() == TAG_LENGTH && currentTag != "000000000000000000000000") {
      if (currentTag != lastDetectedTag || (millis() - lastDetectedTime > 10000)) {
        lastDetectedTag = currentTag;
        lastDetectedTime = millis();
        Serial.printf("Tag: %s\n", currentTag.c_str());
        processAccess(currentTag);
      }
    }
    lastPollTime = millis();
  }

  if (ledActive && (millis() - ledStartTime > LED_DURATION)) {
    digitalWrite(LED_PIN, LOW);
    ledActive = false;
  }
  delay(10);
}

// =====================================================
// TRAITEMENT ACCES
// =====================================================
void processAccess(const String &tag) {
  // Marche forcée
  if (isInForceMode()) {
    int idx = findAgentByTag(tag);
    if (idx >= 0)
      addHistoryEntry(tag, agents[idx].nom, agents[idx].prenom, STATUS_FORCE_OK);
    else
      addHistoryEntry(tag, "", "", STATUS_FORCE_OK);
    activateLed();
    return;
  }

  int idx = findAgentByTag(tag);

  if (idx < 0) {
    addHistoryEntry(tag, "", "", STATUS_INCONNU);
    return;
  }
  if (agents[idx].bloque) {
    addHistoryEntry(tag, agents[idx].nom, agents[idx].prenom, STATUS_BLOQUE);
    return;
  }

  String statut = checkAccess(idx);
  addHistoryEntry(tag, agents[idx].nom, agents[idx].prenom, statut);

  if (statut == STATUS_OK) {
    activateLed();
  }
}

void activateLed() {
  digitalWrite(LED_PIN, HIGH);
  ledActive = true;
  ledStartTime = millis();
}

// =====================================================
// LECTURE TAG R200
// =====================================================
String readTagFromR200() {
  String tag = "";
  for (int i = 0; i < 12; i++) {
    if (rfid.uid[i] < 0x10) tag += "0";
    tag += String(rfid.uid[i], HEX);
  }
  tag.toUpperCase();
  return tag;
}
