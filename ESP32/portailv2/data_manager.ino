/**
 * PORTAIL v2 - Gestionnaire de données
 * Adapté aux structures char[] fixes (économie heap)
 */

// =====================================================
// HELPER : copie sécurisée String -> char[]
// =====================================================
static void sCopy(char *dst, const String &src, size_t maxLen) {
  strncpy(dst, src.c_str(), maxLen - 1);
  dst[maxLen - 1] = '\0';
}

static void sCopy(char *dst, const char *src, size_t maxLen) {
  strncpy(dst, src, maxLen - 1);
  dst[maxLen - 1] = '\0';
}

// =====================================================
// HELPERS TEMPS
// =====================================================
static String padTwo(int val) {
  return (val < 10) ? ("0" + String(val)) : String(val);
}

static unsigned long bootTimeOffset = 0;

String getCurrentTimeStr() {
  struct tm ti;
  if (getLocalTime(&ti, 100)) {
    char buf[10]; strftime(buf, sizeof(buf), "%H:%M:%S", &ti); return String(buf);
  }
  unsigned long s = (millis() / 1000) + bootTimeOffset;
  return padTwo((s/3600)%24) + ":" + padTwo((s/60)%60) + ":" + padTwo(s%60);
}

String getCurrentDateStr() {
  struct tm ti;
  if (getLocalTime(&ti, 100)) {
    char buf[12]; strftime(buf, sizeof(buf), "%Y-%m-%d", &ti); return String(buf);
  }
  return "2025-01-01";
}

String getCurrentDateTimeStr() { return getCurrentDateStr() + " " + getCurrentTimeStr(); }

int getCurrentHour() {
  struct tm ti; if (getLocalTime(&ti, 100)) return ti.tm_hour;
  return ((millis()/1000 + bootTimeOffset) / 3600) % 24;
}

int getCurrentMinute() {
  struct tm ti; if (getLocalTime(&ti, 100)) return ti.tm_min;
  return ((millis()/1000 + bootTimeOffset) / 60) % 60;
}

int getCurrentDayOfWeek() {
  struct tm ti;
  if (getLocalTime(&ti, 100)) { return (ti.tm_wday == 0) ? 7 : ti.tm_wday; }
  return 1;
}

// =====================================================
// INIT STOCKAGE
// =====================================================
void initStorage() {
  if (!LittleFS.begin(true)) { Serial.println("ERREUR LittleFS"); return; }
  Serial.printf("LittleFS: %u/%u octets\n", LittleFS.usedBytes(), LittleFS.totalBytes());
}

// =====================================================
// CATEGORIES PAR DEFAUT
// =====================================================
void initDefaultCategories() {
  categoryCount = 4;
  sCopy(categories[0].nom, "Direction", 20);
  sCopy(categories[0].joursAutorise, "1,2,3,4,5,6,7", 16);
  sCopy(categories[0].heureDebut, "00:00", 6);
  sCopy(categories[0].heureFin, "23:59", 6);

  sCopy(categories[1].nom, "Nettoyage", 20);
  sCopy(categories[1].joursAutorise, "1,2,3,4,5,6", 16);
  sCopy(categories[1].heureDebut, "05:00", 6);
  sCopy(categories[1].heureFin, "22:00", 6);

  sCopy(categories[2].nom, "Commercial", 20);
  sCopy(categories[2].joursAutorise, "1,2,3,4,5", 16);
  sCopy(categories[2].heureDebut, "07:00", 6);
  sCopy(categories[2].heureFin, "20:00", 6);

  sCopy(categories[3].nom, "Personnel", 20);
  sCopy(categories[3].joursAutorise, "1,2,3,4,5", 16);
  sCopy(categories[3].heureDebut, "08:00", 6);
  sCopy(categories[3].heureFin, "18:00", 6);

  saveCategories();
}

// =====================================================
// LOAD / SAVE CONFIG
// =====================================================
bool loadConfig() {
  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    sCopy(config.titre, "PORTAIL", 30);
    config.marcheForce = false;
    sCopy(config.forceHeureDebut, "06:00", 6);
    sCopy(config.forceHeureFin, "22:00", 6);
    saveConfig();
    return true;
  }
  JsonDocument doc; DeserializationError e = deserializeJson(doc, f); f.close();
  if (e) return false;
  sCopy(config.titre,           doc["titre"] | "PORTAIL", 30);
  config.marcheForce =          doc["force"] | false;
  sCopy(config.forceHeureDebut, doc["fDeb"]  | "06:00", 6);
  sCopy(config.forceHeureFin,   doc["fFin"]  | "22:00", 6);
  return true;
}

bool saveConfig() {
  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;
  JsonDocument doc;
  doc["titre"] = config.titre;
  doc["force"] = config.marcheForce;
  doc["fDeb"]  = config.forceHeureDebut;
  doc["fFin"]  = config.forceHeureFin;
  serializeJson(doc, f); f.close();
  return true;
}

// =====================================================
// LOAD / SAVE CATEGORIES
// =====================================================
bool loadCategories() {
  File f = LittleFS.open("/categories.json", "r");
  if (!f || f.size() < 10) {
    if (f) f.close();
    initDefaultCategories();
    return true;
  }
  JsonDocument doc; DeserializationError e = deserializeJson(doc, f); f.close();
  if (e) { initDefaultCategories(); return false; }
  JsonArray arr = doc["cat"].as<JsonArray>();
  categoryCount = 0;
  for (JsonObject o : arr) {
    if (categoryCount >= MAX_CATEGORIES) break;
    sCopy(categories[categoryCount].nom,           o["nom"] | "", 20);
    sCopy(categories[categoryCount].joursAutorise,  o["jours"] | "1,2,3,4,5", 16);
    sCopy(categories[categoryCount].heureDebut,     o["hDeb"] | "08:00", 6);
    sCopy(categories[categoryCount].heureFin,       o["hFin"] | "18:00", 6);
    categoryCount++;
  }
  if (categoryCount == 0) initDefaultCategories();
  return true;
}

bool saveCategories() {
  File f = LittleFS.open("/categories.json", "w");
  if (!f) return false;
  JsonDocument doc; JsonArray arr = doc["cat"].to<JsonArray>();
  for (int i = 0; i < categoryCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["nom"]  = categories[i].nom;
    o["jours"]= categories[i].joursAutorise;
    o["hDeb"] = categories[i].heureDebut;
    o["hFin"] = categories[i].heureFin;
  }
  serializeJson(doc, f); f.close(); return true;
}

// =====================================================
// LOAD / SAVE AGENTS
// =====================================================
bool loadAgents() {
  File f = LittleFS.open("/agents.json", "r");
  if (!f) { agentCount = 0; return true; }
  JsonDocument doc; DeserializationError e = deserializeJson(doc, f); f.close();
  if (e) return false;
  JsonArray arr = doc["ag"].as<JsonArray>();
  agentCount = 0;
  for (JsonObject o : arr) {
    if (agentCount >= MAX_AGENTS) break;
    sCopy(agents[agentCount].tag,    o["t"] | "", 25);
    sCopy(agents[agentCount].nom,    o["n"] | "", 30);
    sCopy(agents[agentCount].prenom, o["p"] | "", 30);
    sCopy(agents[agentCount].plaque, o["pl"] | "", 12);
    agents[agentCount].categorieIdx = o["c"] | 0;
    agents[agentCount].bloque       = o["b"] | false;
    agentCount++;
  }
  return true;
}

bool saveAgents() {
  File f = LittleFS.open("/agents.json", "w");
  if (!f) return false;
  JsonDocument doc; JsonArray arr = doc["ag"].to<JsonArray>();
  for (int i = 0; i < agentCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["t"]  = agents[i].tag;
    o["n"]  = agents[i].nom;
    o["p"]  = agents[i].prenom;
    o["pl"] = agents[i].plaque;
    o["c"]  = agents[i].categorieIdx;
    o["b"]  = agents[i].bloque;
  }
  serializeJson(doc, f); f.close(); return true;
}

// =====================================================
// LOAD / SAVE HISTORIQUE
// =====================================================
bool loadHistory() {
  File f = LittleFS.open("/history.json", "r");
  if (!f) { historyCount = 0; return true; }
  JsonDocument doc; DeserializationError e = deserializeJson(doc, f); f.close();
  if (e) return false;
  JsonArray arr = doc["hi"].as<JsonArray>();
  historyCount = 0;
  for (JsonObject o : arr) {
    if (historyCount >= MAX_HISTORY) break;
    sCopy(history[historyCount].dateTime, o["d"] | "", 20);
    sCopy(history[historyCount].tag,      o["t"] | "", 25);
    sCopy(history[historyCount].nom,      o["n"] | "", 30);
    sCopy(history[historyCount].prenom,   o["p"] | "", 30);
    sCopy(history[historyCount].statut,   o["s"] | "", 10);
    historyCount++;
  }
  return true;
}

bool saveHistory() {
  File f = LittleFS.open("/history.json", "w");
  if (!f) return false;
  JsonDocument doc; JsonArray arr = doc["hi"].to<JsonArray>();
  for (int i = 0; i < historyCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["d"] = history[i].dateTime;
    o["t"] = history[i].tag;
    o["n"] = history[i].nom;
    o["p"] = history[i].prenom;
    o["s"] = history[i].statut;
  }
  serializeJson(doc, f); f.close(); return true;
}

// =====================================================
// LOAD / SAVE JOURS FERIES
// =====================================================
bool loadHolidays() {
  File f = LittleFS.open("/holidays.json", "r");
  if (!f) { holidayCount = 0; return true; }
  JsonDocument doc; DeserializationError e = deserializeJson(doc, f); f.close();
  if (e) return false;
  JsonArray arr = doc["ho"].as<JsonArray>();
  holidayCount = 0;
  for (JsonObject o : arr) {
    if (holidayCount >= MAX_HOLIDAYS) break;
    sCopy(holidays[holidayCount].date,  o["d"] | "", 12);
    sCopy(holidays[holidayCount].label, o["l"] | "", 20);
    holidayCount++;
  }
  return true;
}

bool saveHolidays() {
  File f = LittleFS.open("/holidays.json", "w");
  if (!f) return false;
  JsonDocument doc; JsonArray arr = doc["ho"].to<JsonArray>();
  for (int i = 0; i < holidayCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["d"] = holidays[i].date;
    o["l"] = holidays[i].label;
  }
  serializeJson(doc, f); f.close(); return true;
}

// =====================================================
// CRUD AGENTS
// =====================================================
int findAgentByTag(const String &tag) {
  char upper[25];
  sCopy(upper, tag, 25);
  for (int i = 0; upper[i]; i++) upper[i] = toupper(upper[i]);
  for (int i = 0; i < agentCount; i++) {
    char stored[25];
    sCopy(stored, agents[i].tag, 25);
    for (int j = 0; stored[j]; j++) stored[j] = toupper(stored[j]);
    if (strcmp(stored, upper) == 0) return i;
  }
  return -1;
}

int addAgent(const String &tag, const String &nom, const String &prenom, const String &plaque, int catIdx) {
  if (agentCount >= MAX_AGENTS) return -1;
  if (findAgentByTag(tag) >= 0) return -2;
  sCopy(agents[agentCount].tag,    tag, 25);
  // Mettre le tag en majuscules
  for (int i = 0; agents[agentCount].tag[i]; i++)
    agents[agentCount].tag[i] = toupper(agents[agentCount].tag[i]);
  sCopy(agents[agentCount].nom,    nom, 30);
  sCopy(agents[agentCount].prenom, prenom, 30);
  sCopy(agents[agentCount].plaque, plaque, 12);
  agents[agentCount].categorieIdx = catIdx;
  agents[agentCount].bloque       = false;
  agentCount++;
  saveAgents();
  return agentCount - 1;
}

bool deleteAgent(int index) {
  if (index < 0 || index >= agentCount) return false;
  for (int i = index; i < agentCount - 1; i++) agents[i] = agents[i + 1];
  agentCount--;
  saveAgents();
  return true;
}

bool updateAgent(int index, const String &nom, const String &prenom, const String &plaque, int catIdx) {
  if (index < 0 || index >= agentCount) return false;
  sCopy(agents[index].nom,    nom, 30);
  sCopy(agents[index].prenom, prenom, 30);
  sCopy(agents[index].plaque, plaque, 12);
  agents[index].categorieIdx = catIdx;
  saveAgents();
  return true;
}

bool setAgentBlocked(int index, bool bloque) {
  if (index < 0 || index >= agentCount) return false;
  agents[index].bloque = bloque;
  saveAgents();
  return true;
}

// =====================================================
// HISTORIQUE
// =====================================================
void addHistoryEntry(const String &tag, const String &nom, const String &prenom, const String &statut) {
  if (historyCount >= MAX_HISTORY) {
    memmove(&history[0], &history[1], sizeof(HistoryEntry) * (MAX_HISTORY - 1));
    historyCount = MAX_HISTORY - 1;
  }
  sCopy(history[historyCount].dateTime, getCurrentDateTimeStr(), 20);
  sCopy(history[historyCount].tag,      tag, 25);
  sCopy(history[historyCount].nom,      nom, 30);
  sCopy(history[historyCount].prenom,   prenom, 30);
  sCopy(history[historyCount].statut,   statut, 10);
  historyCount++;
  static int cnt = 0;
  if (++cnt >= 5) { saveHistory(); cnt = 0; }
}

// =====================================================
// VERIFICATION ACCES
// =====================================================
bool isTodayHoliday() {
  String today = getCurrentDateStr();
  for (int i = 0; i < holidayCount; i++) {
    if (today == holidays[i].date) return true;
  }
  return false;
}

bool isInForceMode() {
  if (!config.marcheForce) return false;
  int now = getCurrentHour() * 60 + getCurrentMinute();
  int deb = atoi(config.forceHeureDebut) * 60;
  // Parser "HH:MM"
  const char *p = strchr(config.forceHeureDebut, ':');
  if (p) deb = atoi(config.forceHeureDebut) * 60 + atoi(p + 1);
  int fin = atoi(config.forceHeureFin) * 60;
  p = strchr(config.forceHeureFin, ':');
  if (p) fin = atoi(config.forceHeureFin) * 60 + atoi(p + 1);
  return (now >= deb && now <= fin);
}

// Helper pour parser "HH:MM" en minutes
static int parseHHMM(const char *s) {
  int h = atoi(s);
  const char *p = strchr(s, ':');
  int m = p ? atoi(p + 1) : 0;
  return h * 60 + m;
}

String checkAccess(int agentIndex) {
  if (agentIndex < 0 || agentIndex >= agentCount) return STATUS_INCONNU;
  if (isTodayHoliday()) return STATUS_FERIE;

  int catIdx = agents[agentIndex].categorieIdx;
  if (catIdx < 0 || catIdx >= categoryCount) return STATUS_HORAIRE;

  Category &cat = categories[catIdx];

  // Vérifier jour
  int today = getCurrentDayOfWeek();
  bool jourOK = false;
  char jours[16];
  sCopy(jours, cat.joursAutorise, 16);
  char *tok = strtok(jours, ",");
  while (tok) {
    if (atoi(tok) == today) { jourOK = true; break; }
    tok = strtok(NULL, ",");
  }
  if (!jourOK) return STATUS_HORAIRE;

  // Vérifier heure
  int now = getCurrentHour() * 60 + getCurrentMinute();
  int deb = parseHHMM(cat.heureDebut);
  int fin = parseHHMM(cat.heureFin);
  if (now < deb || now > fin) return STATUS_HORAIRE;

  return STATUS_OK;
}

// =====================================================
// JSON POUR API
// =====================================================
String getAgentsJson() {
  JsonDocument doc; JsonArray arr = doc["agents"].to<JsonArray>();
  for (int i = 0; i < agentCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["idx"] = i; o["tag"] = agents[i].tag; o["nom"] = agents[i].nom;
    o["prenom"] = agents[i].prenom; o["plaque"] = agents[i].plaque;
    o["catIdx"] = agents[i].categorieIdx;
    o["catNom"] = (agents[i].categorieIdx >= 0 && agents[i].categorieIdx < categoryCount) ?
                  categories[agents[i].categorieIdx].nom : "?";
    o["bloque"] = agents[i].bloque;
  }
  doc["count"] = agentCount;
  String r; serializeJson(doc, r); return r;
}

String getHistoryJson() {
  JsonDocument doc; JsonArray arr = doc["history"].to<JsonArray>();
  for (int i = historyCount - 1; i >= 0; i--) {
    JsonObject o = arr.add<JsonObject>();
    o["dt"] = history[i].dateTime; o["tag"] = history[i].tag;
    o["nom"] = history[i].nom; o["pre"] = history[i].prenom;
    o["st"] = history[i].statut;
  }
  doc["count"] = historyCount;
  String r; serializeJson(doc, r); return r;
}

String getCategoriesJson() {
  JsonDocument doc; JsonArray arr = doc["cat"].to<JsonArray>();
  for (int i = 0; i < categoryCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["idx"] = i; o["nom"] = categories[i].nom;
    o["jours"] = categories[i].joursAutorise;
    o["hDeb"] = categories[i].heureDebut; o["hFin"] = categories[i].heureFin;
  }
  doc["count"] = categoryCount;
  String r; serializeJson(doc, r); return r;
}

String getHolidaysJson() {
  JsonDocument doc; JsonArray arr = doc["holidays"].to<JsonArray>();
  for (int i = 0; i < holidayCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["idx"] = i; o["date"] = holidays[i].date; o["label"] = holidays[i].label;
  }
  doc["count"] = holidayCount;
  String r; serializeJson(doc, r); return r;
}

String getConfigJson() {
  JsonDocument doc;
  doc["titre"] = config.titre;
  doc["force"] = config.marcheForce;
  doc["fDeb"]  = config.forceHeureDebut;
  doc["fFin"]  = config.forceHeureFin;
  String r; serializeJson(doc, r); return r;
}

String getStatsJson() {
  JsonDocument doc;
  doc["agentCount"]    = agentCount;
  doc["catCount"]      = categoryCount;
  doc["holidayCount"]  = holidayCount;
  doc["titre"]         = config.titre;
  doc["forceMode"]     = isInForceMode();
  doc["dateTime"]      = getCurrentDateTimeStr();

  String today = getCurrentDateStr();
  int ok=0, inconnu=0, bloque=0, horaire=0, ferie=0, forceOk=0;
  for (int i = 0; i < historyCount; i++) {
    if (strncmp(history[i].dateTime, today.c_str(), 10) != 0) continue;
    if (strcmp(history[i].statut, STATUS_OK)==0)       ok++;
    else if (strcmp(history[i].statut, STATUS_INCONNU)==0)  inconnu++;
    else if (strcmp(history[i].statut, STATUS_BLOQUE)==0)   bloque++;
    else if (strcmp(history[i].statut, STATUS_HORAIRE)==0)  horaire++;
    else if (strcmp(history[i].statut, STATUS_FERIE)==0)    ferie++;
    else if (strcmp(history[i].statut, STATUS_FORCE_OK)==0) forceOk++;
  }
  doc["ok"]=ok; doc["inconnu"]=inconnu; doc["bloque"]=bloque;
  doc["horaire"]=horaire; doc["ferie"]=ferie; doc["forceOk"]=forceOk;

  int nbBl = 0;
  for (int i = 0; i < agentCount; i++) { if (agents[i].bloque) nbBl++; }
  doc["agentsBloque"] = nbBl;

  JsonArray last = doc["last"].to<JsonArray>();
  int start = (historyCount > 8) ? historyCount - 8 : 0;
  for (int i = historyCount - 1; i >= start; i--) {
    JsonObject o = last.add<JsonObject>();
    o["dt"]=history[i].dateTime; o["tag"]=history[i].tag;
    o["nom"]=history[i].nom; o["pre"]=history[i].prenom;
    o["st"]=history[i].statut;
  }

  doc["diskTotal"] = LittleFS.totalBytes();
  doc["diskUsed"]  = LittleFS.usedBytes();
  String r; serializeJson(doc, r); return r;
}

// =====================================================
// EXPORT / IMPORT
// =====================================================
String getAllDataJson() {
  JsonDocument doc;

  JsonArray ag = doc["agents"].to<JsonArray>();
  for (int i = 0; i < agentCount; i++) {
    JsonObject o = ag.add<JsonObject>();
    o["tag"]=agents[i].tag; o["nom"]=agents[i].nom; o["prenom"]=agents[i].prenom;
    o["plaque"]=agents[i].plaque; o["cat"]=agents[i].categorieIdx; o["bloque"]=agents[i].bloque;
  }

  JsonArray ca = doc["categories"].to<JsonArray>();
  for (int i = 0; i < categoryCount; i++) {
    JsonObject o = ca.add<JsonObject>();
    o["nom"]=categories[i].nom; o["jours"]=categories[i].joursAutorise;
    o["hDeb"]=categories[i].heureDebut; o["hFin"]=categories[i].heureFin;
  }

  JsonArray hi = doc["history"].to<JsonArray>();
  for (int i = 0; i < historyCount; i++) {
    JsonObject o = hi.add<JsonObject>();
    o["dt"]=history[i].dateTime; o["tag"]=history[i].tag;
    o["nom"]=history[i].nom; o["pre"]=history[i].prenom; o["st"]=history[i].statut;
  }

  JsonArray ho = doc["holidays"].to<JsonArray>();
  for (int i = 0; i < holidayCount; i++) {
    JsonObject o = ho.add<JsonObject>();
    o["date"]=holidays[i].date; o["label"]=holidays[i].label;
  }

  doc["config"]["titre"] = config.titre;
  doc["config"]["force"] = config.marcheForce;
  doc["config"]["fDeb"]  = config.forceHeureDebut;
  doc["config"]["fFin"]  = config.forceHeureFin;

  String r; serializeJson(doc, r); return r;
}

bool importAllData(const String &json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;

  if (doc.containsKey("categories")) {
    categoryCount = 0;
    for (JsonObject o : doc["categories"].as<JsonArray>()) {
      if (categoryCount >= MAX_CATEGORIES) break;
      sCopy(categories[categoryCount].nom,          o["nom"] | "", 20);
      sCopy(categories[categoryCount].joursAutorise, o["jours"] | "1,2,3,4,5", 16);
      sCopy(categories[categoryCount].heureDebut,    o["hDeb"] | "08:00", 6);
      sCopy(categories[categoryCount].heureFin,      o["hFin"] | "18:00", 6);
      categoryCount++;
    }
    saveCategories();
  }

  if (doc.containsKey("agents")) {
    agentCount = 0;
    for (JsonObject o : doc["agents"].as<JsonArray>()) {
      if (agentCount >= MAX_AGENTS) break;
      sCopy(agents[agentCount].tag,    o["tag"] | "", 25);
      sCopy(agents[agentCount].nom,    o["nom"] | "", 30);
      sCopy(agents[agentCount].prenom, o["prenom"] | "", 30);
      sCopy(agents[agentCount].plaque, o["plaque"] | "", 12);
      agents[agentCount].categorieIdx = o["cat"] | 0;
      agents[agentCount].bloque       = o["bloque"] | false;
      agentCount++;
    }
    saveAgents();
  }

  if (doc.containsKey("history")) {
    historyCount = 0;
    for (JsonObject o : doc["history"].as<JsonArray>()) {
      if (historyCount >= MAX_HISTORY) break;
      sCopy(history[historyCount].dateTime, o["dt"] | "", 20);
      sCopy(history[historyCount].tag,      o["tag"] | "", 25);
      sCopy(history[historyCount].nom,      o["nom"] | "", 30);
      sCopy(history[historyCount].prenom,   o["pre"] | "", 30);
      sCopy(history[historyCount].statut,   o["st"] | "", 10);
      historyCount++;
    }
    saveHistory();
  }

  if (doc.containsKey("holidays")) {
    holidayCount = 0;
    for (JsonObject o : doc["holidays"].as<JsonArray>()) {
      if (holidayCount >= MAX_HOLIDAYS) break;
      sCopy(holidays[holidayCount].date,  o["date"] | "", 12);
      sCopy(holidays[holidayCount].label, o["label"] | "", 20);
      holidayCount++;
    }
    saveHolidays();
  }

  if (doc.containsKey("config")) {
    sCopy(config.titre,           doc["config"]["titre"] | "PORTAIL", 30);
    config.marcheForce =          doc["config"]["force"] | false;
    sCopy(config.forceHeureDebut, doc["config"]["fDeb"]  | "06:00", 6);
    sCopy(config.forceHeureFin,   doc["config"]["fFin"]  | "22:00", 6);
    saveConfig();
  }

  return true;
}
