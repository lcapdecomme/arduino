# PORTAIL - Système de contrôle d'accès RFID UHF

Système de contrôle d'accès basé sur ESP32 et lecteur RFID UHF IN-R200.

## Fonctionnalités

- ✅ Lecture de badges RFID UHF (EPC Gen-2)
- ✅ Gestion du personnel (200 personnes max)
- ✅ Plages horaires par jour de la semaine
- ✅ Gestion des jours fériés/congés
- ✅ Historique des accès (30 jours)
- ✅ Interface web responsive (mobile & desktop)
- ✅ WiFi captive portal
- ✅ Export/Import des données (JSON)
- ✅ Simulation gâche avec LED

## Matériel requis

- ESP32 (DevKit ou similaire)
- Module RFID UHF IN-R200 (ou YPD-R200)
- Antenne UHF (860-960 MHz)
- LED + résistance 220Ω (pour simulation)
- Tags RFID UHF passifs

## Câblage

```
ESP32           IN-R200
─────           ───────
5V (VIN)   →    VCC
GND        →    GND
GPIO16     →    TX
GPIO17     →    RX

ESP32           LED
─────           ───
GPIO4      →    Anode (+)
GND        →    Cathode (-) via R 220Ω
```

## Installation

1. Copier tous les fichiers `.ino` dans un dossier nommé `Portail`
2. Copier les fichiers `R200.h` et `R200.cpp` dans le même dossier
3. Installer les bibliothèques Arduino :
   - ArduinoJson (v7.x)
   - LittleFS (inclus avec ESP32)
4. Ouvrir `Portail.ino` dans Arduino IDE
5. Sélectionner la carte ESP32
6. Compiler et téléverser

## Configuration WiFi

- **SSID** : PORTAIL
- **Mot de passe** : 12345678
- **IP** : 192.168.4.1

## Utilisation

1. Connectez-vous au WiFi "PORTAIL"
2. Ouvrez un navigateur → page de connexion automatique (captive portal)
3. Ou accédez à http://192.168.4.1

### Enrôlement d'un badge

1. Aller dans "Personnel"
2. Cliquer "+ Ajouter"
3. Présenter le badge devant le lecteur
4. Cliquer "Actualiser" pour récupérer l'UID
5. Remplir les informations et plages horaires
6. Enregistrer

## Structure des fichiers

```
Portail/
├── Portail.ino       # Fichier principal, configuration
├── rfid_access.ino   # Gestion RFID et contrôle d'accès
├── web_handlers.ino  # API REST
├── web_page.ino      # Interface HTML/CSS/JS
├── R200.h            # Librairie lecteur RFID
└── R200.cpp          # Librairie lecteur RFID
```

## API REST

| Endpoint | Méthode | Description |
|----------|---------|-------------|
| `/api/personnes` | GET | Liste des personnes |
| `/api/personnes` | POST | Ajouter une personne |
| `/api/personnes/update` | POST | Modifier une personne |
| `/api/personnes/delete` | POST | Supprimer une personne |
| `/api/historique` | GET | Historique des accès |
| `/api/stats` | GET | Statistiques |
| `/api/conges` | GET/POST | Gestion des congés |
| `/api/config` | GET/POST | Configuration |
| `/api/export` | GET | Exporter toutes les données |
| `/api/import` | POST | Importer des données |
| `/api/dernier-badge` | GET | Dernier badge lu |

## Stockage

Les données sont stockées sur la flash ESP32 (LittleFS) :
- `/personnes.json` - Liste du personnel
- `/historique.json` - Historique des accès
- `/conges.json` - Jours fériés
- `/config.json` - Configuration
- `/photos/` - Photos du personnel (optionnel)

## Personnalisation

Modifier les constantes dans `Portail.ino` :

```cpp
#define AP_SSID         "PORTAIL"      // Nom du WiFi
#define AP_PASSWORD     "12345678"     // Mot de passe WiFi
#define PIN_LED         4              // GPIO de la LED
#define LED_DURATION_MS 3000           // Durée d'ouverture (ms)
#define MAX_PERSONNES   200            // Nombre max de personnes
```

## Dépannage

### Le badge n'est pas lu
- Vérifier le câblage TX/RX
- Vérifier que l'antenne est connectée
- Regarder la console série (115200 baud)

### Page web inaccessible
- Se connecter au WiFi "PORTAIL"
- Essayer http://192.168.4.1 directement

### Données perdues au redémarrage
- LittleFS doit être formaté au premier démarrage
- Vérifier l'espace disponible (voir page Config)

## Licence

MIT License
