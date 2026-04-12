/**
 * PORTAIL - Gestionnaires Web API
 * 
 * Ce fichier contient tous les handlers pour l'API REST
 */

// ===== PAGES PRINCIPALES =====
void handleRoot() {
  server.send(200, "text/html", getPageHTML());
}

void handleCaptivePortal() {
  // Répondre avec une page de redirection pour le portail captif
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='0;url=http://" + WiFi.softAPIP().toString() + "/'>";
  html += "<title>Redirecting...</title>";
  html += "</head><body>";
  html += "<h1>Redirection en cours...</h1>";
  html += "<p><a href='http://" + WiFi.softAPIP().toString() + "/'>Cliquez ici</a> si vous n'etes pas redirige.</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  // Toujours rediriger vers la page principale pour le captive portal
  String host = server.hostHeader();
  
  // Si c'est une requête vers notre IP, on sert la page
  if (host == WiFi.softAPIP().toString() || host == "192.168.4.1") {
    handleRoot();
    return;
  }
  
  // Sinon, rediriger
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "Redirecting to captive portal");
}

void handleFavicon() {
  server.send(204, "image/x-icon", "");
}

// ===== API REFRESH (REMPLACE SSE) =====
void handleRefresh() {
  // API légère pour vérifier s'il y a eu des mises à jour
  JsonDocument doc;
  doc["ts"] = dernierAccesTimestamp;
  doc["nb"] = nbHistorique;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ===== API PERSONNES =====
void handleGetPersonnes() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbPersonnes; i++) {
    if (!personnes[i].actif) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = i;
    obj["uid"] = personnes[i].uid;
    obj["nom"] = personnes[i].nom;
    obj["prenom"] = personnes[i].prenom;
    obj["plaque"] = personnes[i].plaque;
    obj["photo"] = personnes[i].photoFile;
    obj["bloque"] = personnes[i].bloque;
    obj["categorie"] = personnes[i].categorieId;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAddPersonne() {
  if (nbPersonnes >= MAX_PERSONNES) {
    server.send(400, "application/json", "{\"error\":\"Limite atteinte\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  String uid = doc["uid"] | "";
  if (findPersonneByUID(uid) >= 0) {
    server.send(400, "application/json", "{\"error\":\"Badge deja enregistre\"}");
    return;
  }
  
  Personne* p = &personnes[nbPersonnes];
  p->actif = true;
  p->bloque = false;
  strlcpy(p->uid, uid.c_str(), sizeof(p->uid));
  strlcpy(p->nom, doc["nom"] | "", sizeof(p->nom));
  strlcpy(p->prenom, doc["prenom"] | "", sizeof(p->prenom));
  strlcpy(p->plaque, doc["plaque"] | "", sizeof(p->plaque));
  p->photoFile[0] = '\0';
  p->categorieId = doc["categorie"] | 0;
  
  nbPersonnes++;
  savePersonnes();
  server.send(200, "application/json", "{\"success\":true,\"id\":" + String(nbPersonnes-1) + "}");
}

void handleUpdatePersonne() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  int id = doc["id"] | -1;
  if (id < 0 || id >= nbPersonnes || !personnes[id].actif) {
    server.send(404, "application/json", "{\"error\":\"Personne non trouvee\"}");
    return;
  }
  
  Personne* p = &personnes[id];
  if (doc.containsKey("uid")) strlcpy(p->uid, doc["uid"] | "", sizeof(p->uid));
  if (doc.containsKey("nom")) strlcpy(p->nom, doc["nom"] | "", sizeof(p->nom));
  if (doc.containsKey("prenom")) strlcpy(p->prenom, doc["prenom"] | "", sizeof(p->prenom));
  if (doc.containsKey("plaque")) strlcpy(p->plaque, doc["plaque"] | "", sizeof(p->plaque));
  if (doc.containsKey("categorie")) p->categorieId = doc["categorie"];
  if (doc.containsKey("bloque")) p->bloque = doc["bloque"];
  
  savePersonnes();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDeletePersonne() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  int id = doc["id"] | -1;
  if (id < 0 || id >= nbPersonnes) {
    server.send(404, "application/json", "{\"error\":\"Personne non trouvee\"}");
    return;
  }
  
  if (personnes[id].photoFile[0] != '\0') {
    String photoPath = "/photos/" + String(personnes[id].photoFile);
    LittleFS.remove(photoPath);
  }
  
  personnes[id].actif = false;
  savePersonnes();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleToggleBloque() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  int id = doc["id"] | -1;
  if (id < 0 || id >= nbPersonnes || !personnes[id].actif) {
    server.send(404, "application/json", "{\"error\":\"Personne non trouvee\"}");
    return;
  }
  
  personnes[id].bloque = !personnes[id].bloque;
  savePersonnes();
  
  server.send(200, "application/json", "{\"success\":true,\"bloque\":" + String(personnes[id].bloque ? "true" : "false") + "}");
}

// ===== API PHOTOS =====
void handleUploadPhoto() {
  String id = server.arg("id");
  if (id.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"ID manquant\"}");
    return;
  }
  
  int idx = id.toInt();
  if (idx < 0 || idx >= nbPersonnes || !personnes[idx].actif) {
    server.send(404, "application/json", "{\"error\":\"Personne non trouvee\"}");
    return;
  }
  
  String filename = String(personnes[idx].uid) + ".jpg";
  strlcpy(personnes[idx].photoFile, filename.c_str(), sizeof(personnes[idx].photoFile));
  savePersonnes();
  server.send(200, "application/json", "{\"success\":true,\"filename\":\"" + filename + "\"}");
}

void handleGetPhoto() {
  String filename = server.arg("file");
  String path = "/photos/" + filename;
  
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "Photo non trouvee");
    return;
  }
  
  File f = LittleFS.open(path, "r");
  server.streamFile(f, "image/jpeg");
  f.close();
}

// ===== API HISTORIQUE =====
void handleGetHistorique() {
  int limit = server.arg("limit").toInt();
  if (limit <= 0 || limit > MAX_HISTORIQUE) limit = 50;
  
  // Filtre par statut (optionnel)
  int filtreStatut = -1;
  if (server.hasArg("statut")) {
    filtreStatut = server.arg("statut").toInt();
  }
  
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  int count = 0;
  for (int i = nbHistorique - 1; i >= 0 && count < limit; i--) {
    // Appliquer filtre si défini
    if (filtreStatut >= 0 && historique[i].statut != filtreStatut) {
      continue;
    }
    
    JsonObject obj = arr.add<JsonObject>();
    obj["ts"] = historique[i].timestamp;
    obj["date"] = getTimestampStr(historique[i].timestamp);
    obj["uid"] = historique[i].uid;
    obj["nom"] = historique[i].nom;
    obj["prenom"] = historique[i].prenom;
    obj["statut"] = historique[i].statut;
    obj["statutStr"] = getStatutStr(historique[i].statut);
    count++;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleClearHistorique() {
  // Effacer tout l'historique
  nbHistorique = 0;
  indexHistorique = 0;
  dernierAccesTimestamp = 0;
  
  // Effacer le fichier
  saveHistorique();
  
  server.send(200, "application/json", "{\"success\":true}");
}

void handleGetStats() {
  time_t now;
  time(&now);
  uint32_t aujourdhuiDebut = now - (now % 86400);
  uint32_t semaineDebut = aujourdhuiDebut - (getJourSemaine() * 86400);
  
  int totalAujourdhui = 0, okAujourdhui = 0;
  int totalSemaine = 0, okSemaine = 0;
  int inconnu = 0, bloque = 0, horsHoraire = 0;
  int nbBloques = 0;
  
  // Compter les agents bloqués
  for (int i = 0; i < nbPersonnes; i++) {
    if (personnes[i].actif && personnes[i].bloque) {
      nbBloques++;
    }
  }
  
  for (int i = 0; i < nbHistorique; i++) {
    if (historique[i].timestamp >= aujourdhuiDebut) {
      totalAujourdhui++;
      if (historique[i].statut == STATUT_OK || historique[i].statut == STATUT_MARCHE_FORCEE) okAujourdhui++;
      if (historique[i].statut == STATUT_INCONNU) inconnu++;
      if (historique[i].statut == STATUT_BLOQUE) bloque++;
      if (historique[i].statut == STATUT_HORS_HORAIRE) horsHoraire++;
    }
    if (historique[i].timestamp >= semaineDebut) {
      totalSemaine++;
      if (historique[i].statut == STATUT_OK || historique[i].statut == STATUT_MARCHE_FORCEE) okSemaine++;
    }
  }
  
  JsonDocument doc;
  doc["personnes"] = nbPersonnes;
  doc["agentsBloques"] = nbBloques;
  doc["marcheForcee"] = isMarcheForceeActive();
  doc["aujourdhui"]["total"] = totalAujourdhui;
  doc["aujourdhui"]["ok"] = okAujourdhui;
  doc["aujourdhui"]["refuse"] = totalAujourdhui - okAujourdhui;
  doc["aujourdhui"]["inconnu"] = inconnu;
  doc["aujourdhui"]["bloque"] = bloque;
  doc["aujourdhui"]["horsHoraire"] = horsHoraire;
  doc["semaine"]["total"] = totalSemaine;
  doc["semaine"]["ok"] = okSemaine;
  doc["semaine"]["refuse"] = totalSemaine - okSemaine;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// ===== API CONGÉS =====
void handleGetConges() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbConges; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = i;
    obj["jour"] = conges[i].jour;
    obj["mois"] = conges[i].mois;
    obj["annee"] = conges[i].annee;
    obj["desc"] = conges[i].description;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAddConge() {
  if (nbConges >= MAX_CONGES) {
    server.send(400, "application/json", "{\"error\":\"Limite atteinte\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  JourConge* c = &conges[nbConges];
  c->jour = doc["jour"] | 1;
  c->mois = doc["mois"] | 1;
  c->annee = doc["annee"] | 0;
  strlcpy(c->description, doc["desc"] | "", sizeof(c->description));
  
  nbConges++;
  saveConges();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDeleteConge() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  int id = doc["id"] | -1;
  if (id < 0 || id >= nbConges) {
    server.send(404, "application/json", "{\"error\":\"Conge non trouve\"}");
    return;
  }
  
  for (int i = id; i < nbConges - 1; i++) {
    conges[i] = conges[i + 1];
  }
  nbConges--;
  saveConges();
  server.send(200, "application/json", "{\"success\":true}");
}

// ===== API CATÉGORIES =====
void handleGetCategories() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (int i = 0; i < nbCategories; i++) {
    if (!categories[i].actif) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = i;
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
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAddCategorie() {
  if (nbCategories >= MAX_CATEGORIES) {
    server.send(400, "application/json", "{\"error\":\"Limite atteinte\"}");
    return;
  }
  
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  Categorie* c = &categories[nbCategories];
  c->actif = true;
  strlcpy(c->nom, doc["nom"] | "Nouvelle", sizeof(c->nom));
  
  // Plages par défaut : Lun-Ven 8h-18h
  for (int j = 0; j < 7; j++) {
    if (doc["plages"].is<JsonArray>() && j < doc["plages"].size()) {
      JsonObject pl = doc["plages"][j];
      c->plages[j].actif = pl["actif"] | (j < 5);
      c->plages[j].heureDebut = pl["hd"] | 8;
      c->plages[j].minuteDebut = pl["md"] | 0;
      c->plages[j].heureFin = pl["hf"] | 18;
      c->plages[j].minuteFin = pl["mf"] | 0;
    } else {
      c->plages[j].actif = (j < 5);
      c->plages[j].heureDebut = 8;
      c->plages[j].minuteDebut = 0;
      c->plages[j].heureFin = 18;
      c->plages[j].minuteFin = 0;
    }
  }
  
  nbCategories++;
  saveCategories();
  server.send(200, "application/json", "{\"success\":true,\"id\":" + String(nbCategories-1) + "}");
}

void handleUpdateCategorie() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  int id = doc["id"] | -1;
  if (id < 0 || id >= nbCategories || !categories[id].actif) {
    server.send(404, "application/json", "{\"error\":\"Categorie non trouvee\"}");
    return;
  }
  
  Categorie* c = &categories[id];
  if (doc.containsKey("nom")) strlcpy(c->nom, doc["nom"] | "", sizeof(c->nom));
  
  if (doc["plages"].is<JsonArray>()) {
    for (int j = 0; j < 7 && j < doc["plages"].size(); j++) {
      JsonObject pl = doc["plages"][j];
      c->plages[j].actif = pl["actif"] | c->plages[j].actif;
      c->plages[j].heureDebut = pl["hd"] | c->plages[j].heureDebut;
      c->plages[j].minuteDebut = pl["md"] | c->plages[j].minuteDebut;
      c->plages[j].heureFin = pl["hf"] | c->plages[j].heureFin;
      c->plages[j].minuteFin = pl["mf"] | c->plages[j].minuteFin;
    }
  }
  
  saveCategories();
  server.send(200, "application/json", "{\"success\":true}");
}

void handleDeleteCategorie() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  int id = doc["id"] | -1;
  if (id < 0 || id >= nbCategories) {
    server.send(404, "application/json", "{\"error\":\"Categorie non trouvee\"}");
    return;
  }
  
  // Vérifier qu'aucune personne n'utilise cette catégorie
  for (int i = 0; i < nbPersonnes; i++) {
    if (personnes[i].actif && personnes[i].categorieId == id) {
      server.send(400, "application/json", "{\"error\":\"Categorie utilisee\"}");
      return;
    }
  }
  
  categories[id].actif = false;
  saveCategories();
  server.send(200, "application/json", "{\"success\":true}");
}

// ===== API CONFIGURATION =====
void handleGetConfig() {
  JsonDocument doc;
  doc["titre"] = config.titre;
  doc["marcheForcee"] = config.marcheForcee;
  doc["forceeHD"] = config.forceeHeureDebut;
  doc["forceeMD"] = config.forceeMinuteDebut;
  doc["forceeHF"] = config.forceeHeureFin;
  doc["forceeMF"] = config.forceeMinuteFin;
  doc["marcheForceeActive"] = isMarcheForceeActive();
  doc["ssid"] = AP_SSID;
  doc["nbPersonnes"] = nbPersonnes;
  doc["nbHistorique"] = nbHistorique;
  doc["nbCategories"] = nbCategories;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["totalHeap"] = ESP.getHeapSize();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetConfig() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  if (doc.containsKey("titre")) {
    strlcpy(config.titre, doc["titre"] | "PORTAIL", sizeof(config.titre));
  }
  if (doc.containsKey("marcheForcee")) {
    config.marcheForcee = doc["marcheForcee"];
  }
  if (doc.containsKey("forceeHD")) {
    config.forceeHeureDebut = doc["forceeHD"];
  }
  if (doc.containsKey("forceeMD")) {
    config.forceeMinuteDebut = doc["forceeMD"];
  }
  if (doc.containsKey("forceeHF")) {
    config.forceeHeureFin = doc["forceeHF"];
  }
  if (doc.containsKey("forceeMF")) {
    config.forceeMinuteFin = doc["forceeMF"];
  }
  
  saveConfig();
  server.send(200, "application/json", "{\"success\":true}");
}

// ===== API EXPORT/IMPORT =====
void handleExportData() {
  JsonDocument doc;
  
  // Exporter personnees
  JsonArray pArr = doc["personnes"].to<JsonArray>();
  for (int i = 0; i < nbPersonnes; i++) {
    if (!personnes[i].actif) continue;
    JsonObject obj = pArr.add<JsonObject>();
    obj["uid"] = personnes[i].uid;
    obj["nom"] = personnes[i].nom;
    obj["prenom"] = personnes[i].prenom;
    obj["plaque"] = personnes[i].plaque;
    obj["bloque"] = personnes[i].bloque;
    obj["categorie"] = personnes[i].categorieId;
  }
  
  // Exporter catégories
  JsonArray catArr = doc["categories"].to<JsonArray>();
  for (int i = 0; i < nbCategories; i++) {
    if (!categories[i].actif) continue;
    JsonObject obj = catArr.add<JsonObject>();
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
  
  // Exporter congés
  JsonArray cArr = doc["conges"].to<JsonArray>();
  for (int i = 0; i < nbConges; i++) {
    JsonObject obj = cArr.add<JsonObject>();
    obj["jour"] = conges[i].jour;
    obj["mois"] = conges[i].mois;
    obj["annee"] = conges[i].annee;
    obj["desc"] = conges[i].description;
  }
  
  // Exporter config
  doc["config"]["titre"] = config.titre;
  doc["config"]["marcheForcee"] = config.marcheForcee;
  doc["config"]["forceeHD"] = config.forceeHeureDebut;
  doc["config"]["forceeMD"] = config.forceeMinuteDebut;
  doc["config"]["forceeHF"] = config.forceeHeureFin;
  doc["config"]["forceeMF"] = config.forceeMinuteFin;
  
  String response;
  serializeJsonPretty(doc, response);
  server.sendHeader("Content-Disposition", "attachment; filename=portail_backup.json");
  server.send(200, "application/json", response);
}

void handleImportData() {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"error\":\"JSON invalide\"}");
    return;
  }
  
  // Importer catégories
  if (doc["categories"].is<JsonArray>()) {
    nbCategories = 0;
    JsonArray arr = doc["categories"];
    for (JsonObject obj : arr) {
      if (nbCategories >= MAX_CATEGORIES) break;
      Categorie* c = &categories[nbCategories];
      c->actif = true;
      strlcpy(c->nom, obj["nom"] | "", sizeof(c->nom));
      JsonArray plagesArr = obj["plages"];
      for (int j = 0; j < 7 && j < plagesArr.size(); j++) {
        JsonObject pl = plagesArr[j];
        c->plages[j].actif = pl["actif"] | false;
        c->plages[j].heureDebut = pl["hd"] | 8;
        c->plages[j].minuteDebut = pl["md"] | 0;
        c->plages[j].heureFin = pl["hf"] | 18;
        c->plages[j].minuteFin = pl["mf"] | 0;
      }
      nbCategories++;
    }
    saveCategories();
  }
  
  // Importer personnes
  if (doc["personnes"].is<JsonArray>()) {
    nbPersonnes = 0;
    JsonArray arr = doc["personnes"];
    for (JsonObject obj : arr) {
      if (nbPersonnes >= MAX_PERSONNES) break;
      Personne* p = &personnes[nbPersonnes];
      p->actif = true;
      p->bloque = obj["bloque"] | false;
      strlcpy(p->uid, obj["uid"] | "", sizeof(p->uid));
      strlcpy(p->nom, obj["nom"] | "", sizeof(p->nom));
      strlcpy(p->prenom, obj["prenom"] | "", sizeof(p->prenom));
      strlcpy(p->plaque, obj["plaque"] | "", sizeof(p->plaque));
      p->photoFile[0] = '\0';
      p->categorieId = obj["categorie"] | 0;
      nbPersonnes++;
    }
    savePersonnes();
  }
  
  // Importer congés
  if (doc["conges"].is<JsonArray>()) {
    nbConges = 0;
    JsonArray arr = doc["conges"];
    for (JsonObject obj : arr) {
      if (nbConges >= MAX_CONGES) break;
      JourConge* c = &conges[nbConges];
      c->jour = obj["jour"] | 1;
      c->mois = obj["mois"] | 1;
      c->annee = obj["annee"] | 0;
      strlcpy(c->description, obj["desc"] | "", sizeof(c->description));
      nbConges++;
    }
    saveConges();
  }
  
  // Importer config
  if (doc["config"].is<JsonObject>()) {
    strlcpy(config.titre, doc["config"]["titre"] | "PORTAIL", sizeof(config.titre));
    config.marcheForcee = doc["config"]["marcheForcee"] | false;
    config.forceeHeureDebut = doc["config"]["forceeHD"] | 8;
    config.forceeMinuteDebut = doc["config"]["forceeMD"] | 0;
    config.forceeHeureFin = doc["config"]["forceeHF"] | 18;
    config.forceeMinuteFin = doc["config"]["forceeMF"] | 0;
    saveConfig();
  }
  
  server.send(200, "application/json", "{\"success\":true}");
}

// ===== API DERNIER BADGE =====
void handleGetDernierBadge() {
  JsonDocument doc;
  doc["uid"] = dernierUidLu;
  doc["timestamp"] = derniereLecture;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}
