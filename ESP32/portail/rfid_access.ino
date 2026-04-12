/**
 * PORTAIL - Gestion RFID et contrôle d'accès
 * 
 * Ce fichier gère:
 * - Lecture des tags RFID
 * - Vérification des autorisations (catégories)
 * - Gestion de l'historique
 * - Commande de la gâche/LED
 * - Mode marche forcée
 */

// ===== LECTURE RFID =====
// Tableau vide pour comparaison
const uint8_t blankUid[12] = {0};

void checkRFID() {
  // La librairie R200 stocke l'UID dans rfid.uid[12] (tableau de 12 bytes)
  // Quand aucun tag n'est présent, uid est remis à zéro
  
  // Vérifier si un UID valide est présent (non nul)
  if (memcmp(rfid.uid, blankUid, 12) != 0) {
    // Convertir en string hex
    String uid = "";
    for (int i = 0; i < 12; i++) {
      if (rfid.uid[i] < 0x10) uid += "0";
      uid += String(rfid.uid[i], HEX);
    }
    uid.toUpperCase();
    
    // Éviter les lectures multiples du même badge
    if (uid != dernierUidLu || millis() - derniereLecture > 3000) {
      dernierUidLu = uid;
      derniereLecture = millis();
      Serial.printf("[RFID] Tag detecte: %s\n", uid.c_str());
      processTag(uid);
    }
  }
}

// ===== TRAITEMENT D'UN TAG =====
void processTag(String uid) {
  // Vérifier d'abord si mode marche forcée actif
  if (isMarcheForceeActive()) {
    Serial.println("[ACCES] Marche forcee active - Acces libre");
    // Chercher la personne pour l'historique (optionnel)
    int index = findPersonneByUID(uid);
    if (index >= 0) {
      Personne* p = &personnes[index];
      ajouterHistorique(uid, p->nom, p->prenom, STATUT_MARCHE_FORCEE);
    } else {
      ajouterHistorique(uid, "", "", STATUT_MARCHE_FORCEE);
    }
    activerGache();
    return;
  }
  
  // Chercher la personne
  int index = findPersonneByUID(uid);
  
  if (index < 0) {
    // Badge inconnu
    Serial.printf("[ACCES] Badge inconnu: %s\n", uid.c_str());
    ajouterHistorique(uid, "", "", STATUT_INCONNU);
    return;
  }
  
  Personne* p = &personnes[index];
  String nomComplet = String(p->prenom) + " " + String(p->nom);
  
  // Vérifier si agent bloqué
  if (p->bloque) {
    Serial.printf("[ACCES] Agent bloque: %s\n", nomComplet.c_str());
    ajouterHistorique(uid, p->nom, p->prenom, STATUT_BLOQUE);
    return;
  }
  
  // Vérifier si jour de congé
  if (isJourConge()) {
    Serial.printf("[ACCES] Refuse (jour ferie): %s\n", nomComplet.c_str());
    ajouterHistorique(uid, p->nom, p->prenom, STATUT_JOUR_FERIE);
    return;
  }
  
  // Vérifier les autorisations horaires (via catégorie)
  if (!isPersonneAutorisee(index)) {
    Serial.printf("[ACCES] Refuse (hors horaire): %s\n", nomComplet.c_str());
    ajouterHistorique(uid, p->nom, p->prenom, STATUT_HORS_HORAIRE);
    return;
  }
  
  // Accès autorisé !
  Serial.printf("[ACCES] Autorise: %s\n", nomComplet.c_str());
  ajouterHistorique(uid, p->nom, p->prenom, STATUT_OK);
  activerGache();
}

// ===== VÉRIFICATION MARCHE FORCÉE =====
bool isMarcheForceeActive() {
  if (!config.marcheForcee) return false;
  
  time_t now;
  time(&now);
  struct tm* tm = localtime(&now);
  
  int minutesActuelles = tm->tm_hour * 60 + tm->tm_min;
  int minutesDebut = config.forceeHeureDebut * 60 + config.forceeMinuteDebut;
  int minutesFin = config.forceeHeureFin * 60 + config.forceeMinuteFin;
  
  return (minutesActuelles >= minutesDebut && minutesActuelles <= minutesFin);
}

// ===== VÉRIFICATION AUTORISATION (VIA CATÉGORIE) =====
bool isPersonneAutorisee(int index) {
  if (index < 0 || index >= nbPersonnes) return false;
  if (!personnes[index].actif) return false;
  if (personnes[index].bloque) return false;
  
  // Récupérer la catégorie de la personne
  uint8_t catId = personnes[index].categorieId;
  if (catId >= nbCategories) return false;
  if (!categories[catId].actif) return false;
  
  int jourSemaine = getJourSemaine();
  PlageHoraire* plage = &categories[catId].plages[jourSemaine];
  
  if (!plage->actif) return false;
  
  return isDansPlageHoraire(plage);
}

bool isDansPlageHoraire(PlageHoraire* plage) {
  time_t now;
  time(&now);
  struct tm* tm = localtime(&now);
  
  int minutesActuelles = tm->tm_hour * 60 + tm->tm_min;
  int minutesDebut = plage->heureDebut * 60 + plage->minuteDebut;
  int minutesFin = plage->heureFin * 60 + plage->minuteFin;
  
  return (minutesActuelles >= minutesDebut && minutesActuelles <= minutesFin);
}

bool isJourConge() {
  time_t now;
  time(&now);
  struct tm* tm = localtime(&now);
  
  uint8_t jour = tm->tm_mday;
  uint8_t mois = tm->tm_mon + 1;
  uint16_t annee = tm->tm_year + 1900;
  
  for (int i = 0; i < nbConges; i++) {
    if (conges[i].jour == jour && conges[i].mois == mois) {
      // Vérifier l'année (0 = tous les ans)
      if (conges[i].annee == 0 || conges[i].annee == annee) {
        return true;
      }
    }
  }
  return false;
}

// ===== RECHERCHE PERSONNE =====
int findPersonneByUID(String uid) {
  for (int i = 0; i < nbPersonnes; i++) {
    if (personnes[i].actif && uid.equalsIgnoreCase(personnes[i].uid)) {
      return i;
    }
  }
  return -1;
}

// ===== HISTORIQUE =====
void ajouterHistorique(String uid, String nom, String prenom, uint8_t statut) {
  time_t now;
  time(&now);
  
  EntreeHistorique* e;
  
  if (nbHistorique < MAX_HISTORIQUE) {
    e = &historique[nbHistorique];
    nbHistorique++;
  } else {
    // Buffer circulaire
    e = &historique[indexHistorique];
    indexHistorique = (indexHistorique + 1) % MAX_HISTORIQUE;
  }
  
  e->timestamp = (uint32_t)now;
  strlcpy(e->uid, uid.c_str(), sizeof(e->uid));
  strlcpy(e->nom, nom.c_str(), sizeof(e->nom));
  strlcpy(e->prenom, prenom.c_str(), sizeof(e->prenom));
  e->statut = statut;
  
  // Mettre à jour le timestamp pour SSE
  dernierAccesTimestamp = e->timestamp;
  
  // Sauvegarder périodiquement (toutes les 10 entrées)
  static int compteurSave = 0;
  compteurSave++;
  if (compteurSave >= 10) {
    saveHistorique();
    compteurSave = 0;
  }
}

// ===== COMMANDE GÂCHE/LED =====
void activerGache() {
  digitalWrite(PIN_LED, HIGH);
  ledActive = true;
  ledOffTime = millis() + LED_DURATION_MS;
  Serial.printf("[GACHE] Activee pour %d ms\n", LED_DURATION_MS);
}

void desactiverGache() {
  digitalWrite(PIN_LED, LOW);
  ledActive = false;
  Serial.println("[GACHE] Desactivee");
}
