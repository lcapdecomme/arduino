# Projet i3 - Système de minuterie LoRa avec portail captif WiFi

## 📋 Description du projet

Système composé de deux ESP32 communiquant via LoRa :
- **Émetteur** : Envoie un signal au clic d'un bouton
- **Récepteur** : Reçoit le signal, allume une LED pendant une durée configurable via un portail captif WiFi

### Fonctionnalités principales

#### Émetteur
- ✅ Envoi d'un message LoRa au clic d'un bouton poussoir (ou joystick pour les tests)
- ✅ LED de feedback visuel lors de l'envoi
- ✅ Anti-rebond pour éviter les envois multiples
- ✅ Chiffrement XOR des messages

#### Récepteur
- ✅ Réception des messages LoRa avec vérification de clé de sécurité
- ✅ Timer configurable (allume la LED pendant X minutes)
- ✅ Redémarrage du timer à zéro si nouveau signal reçu
- ✅ Portail captif WiFi pour configuration
- ✅ Interface web responsive (optimisée mobile)
- ✅ Sauvegarde de la durée en mémoire persistante (Preferences)
- ✅ Affichage du temps restant

---

## 🔌 Matériel nécessaire

### Pour l'émetteur
- 1x ESP32
- 1x Module LoRa E32 (UART)
- 1x Bouton poussoir (ou joystick pour tests)
- 1x LED (optionnel, feedback visuel)
- 1x Résistance 220Ω (pour la LED)
- Câbles de connexion

### Pour le récepteur
- 1x ESP32
- 1x Module LoRa E32 (UART)
- 1x LED
- 1x Résistance 220Ω (pour la LED)
- Câbles de connexion

---

## 🔧 Branchements

### ÉMETTEUR

#### Module LoRa E32
```
ESP32 Pin 16 (RX) -----> TX du module LoRa
ESP32 Pin 17 (TX) -----> RX du module LoRa
ESP32 Pin 4         -----> AUX du module LoRa
VCC module          -----> 3.3V ou 5V (selon module)
GND module          -----> GND
```

#### Bouton poussoir
```
Une patte du bouton -----> GPIO 32
Autre patte         -----> GND
(Pas de résistance nécessaire : pull-up interne activée)
```

#### LED (optionnel)
```
GPIO 2 -----> Résistance 220Ω -----> Anode LED (+)
Cathode LED (-) -----> GND
```

### RÉCEPTEUR

#### Module LoRa E32
```
ESP32 Pin 16 (RX) -----> TX du module LoRa
ESP32 Pin 17 (TX) -----> RX du module LoRa
ESP32 Pin 4         -----> AUX du module LoRa
VCC module          -----> 3.3V ou 5V (selon module)
GND module          -----> GND
```

#### LED
```
GPIO 25 -----> Résistance 220Ω -----> Anode LED (+)
Cathode LED (-) -----> GND
```

---

## 📚 Librairies à installer

### Pour l'émetteur
Via le gestionnaire de bibliothèques Arduino IDE :
1. **LoRa_E32** by Renzo Mischianti

### Pour le récepteur
Via le gestionnaire de bibliothèques Arduino IDE :
1. **LoRa_E32** by Renzo Mischianti
2. **ArduinoJson** by Benoit Blanchon

> **Note** : Les bibliothèques `WiFi`, `WebServer`, `DNSServer` et `Preferences` sont incluses avec le package ESP32, pas besoin de les installer.

---

## 🚀 Installation et configuration

### 1. Téléverser les programmes

1. Ouvrir `i3_emetteur.ino` dans Arduino IDE
2. Sélectionner la carte : ESP32 Dev Module
3. Téléverser sur le premier ESP32

4. Ouvrir `i3_recepteur_v2.ino` dans Arduino IDE
5. Sélectionner la carte : ESP32 Dev Module
6. Téléverser sur le second ESP32

### 2. Configuration du récepteur

1. Alimenter le récepteur (l'émetteur peut rester éteint)
2. Ouvrir les paramètres WiFi de votre téléphone/ordinateur
3. Se connecter au réseau WiFi : **ESP32-i3-Config** (sans mot de passe)
4. Une page devrait s'ouvrir automatiquement (portail captif)
   - Si ce n'est pas le cas, ouvrir un navigateur et aller à : `http://192.168.4.1`

5. Configurer les paramètres :
   - **Nom de l'entreprise** : Entrer le nom souhaité (par exemple "Mon Entreprise")
   - **Durée du timer** : Entrer la durée en minutes (1 à 120 minutes)
   
6. Cliquer sur **Sauvegarder**

7. La configuration est sauvegardée en mémoire permanente !

### 3. Test du système

1. Alimenter l'émetteur
2. Appuyer sur le bouton de l'émetteur
   - La LED de l'émetteur clignote brièvement (feedback)
   - Le message "✓ Message envoyé" apparaît dans le moniteur série
   
3. Sur le récepteur :
   - La LED s'allume immédiatement
   - Elle reste allumée pendant la durée configurée
   - Le temps restant s'affiche dans le moniteur série toutes les 10 secondes
   
4. Si vous appuyez à nouveau sur le bouton avant la fin du timer :
   - Le timer redémarre à zéro
   - La LED reste allumée pour une nouvelle période complète

---

## 🔍 Moniteur série

### Émetteur (115200 bauds)
```
=================================
  Émetteur i3 - Prêt !
=================================
Appuyez sur le bouton pour envoyer un signal
✓ Message envoyé : SECURE123:12345:TRIGGER
```

### Récepteur (115200 bauds)
```
=================================
  Récepteur i3 - Prêt !
=================================
Point d'accès WiFi : ESP32-i3-Config
Adresse IP : 192.168.4.1
Durée du timer : 10 minutes
Serveur web démarré - En attente de messages LoRa...
Message LoRa reçu : SECURE123:12345:TRIGGER
✓ Message valide - Timer démarré/redémarré
Temps restant : 9:50
Temps restant : 9:40
...
✓ Timer terminé - LED éteinte
```

---

## 🛠️ Paramètres modifiables

### Dans le code émetteur (`i3_emetteur.ino`)

```cpp
// Ligne 14 : Clé de sécurité (doit être identique sur les deux ESP32)
String securityKey = "SECURE123";

// Ligne 9 : Broche du bouton
#define SW_PIN  32

// Ligne 10 : Broche de la LED
#define LED_PIN 2

// Ligne 23 : Délai anti-rebond (en millisecondes)
const unsigned long debounceDelay = 200;
```

### Dans le code récepteur (`i3_recepteur_v2.ino`)

```cpp
// Ligne 14 : Clé de sécurité
String securityKey = "SECURE123";

// Ligne 17 : Nom du réseau WiFi
const char* AP_SSID = "ESP32-i3-Config";

// Ligne 18 : Mot de passe WiFi (vide = pas de mot de passe)
const char* AP_PWD = "";

// Ligne 30 : Durée par défaut (en millisecondes)
unsigned long timerDuration = 10 * 60 * 1000; // 10 minutes

// Ligne 10 : Broche de la LED
#define LED_PIN 25
```

---

## 🐛 Dépannage

### L'émetteur n'envoie pas de messages
- ✅ Vérifier les connexions du module LoRa
- ✅ Vérifier que le bouton est bien branché (GPIO 32 et GND)
- ✅ Vérifier le moniteur série (115200 bauds)
- ✅ Tester le bouton : il doit être à LOW quand appuyé

### Le récepteur ne compile pas
- ✅ Vérifier que les bonnes librairies sont installées :
  - LoRa_E32 by Renzo Mischianti
  - ArduinoJson by Benoit Blanchon
- ✅ Vérifier que vous avez bien sélectionné "ESP32 Dev Module" comme carte
- ✅ Si erreur avec AsyncTCP : utiliser `i3_recepteur_v2.ino` (version WebServer synchrone)

### Le récepteur ne reçoit pas
- ✅ Vérifier que les deux modules LoRa sont sur le même canal/fréquence
- ✅ Vérifier que les clés de sécurité sont identiques
- ✅ Vérifier les connexions du module LoRa
- ✅ Rapprocher les deux ESP32 pour tester la portée

### Le portail captif ne s'ouvre pas
- ✅ Vérifier que vous êtes bien connecté au WiFi "ESP32-i3-Config"
- ✅ Essayer d'ouvrir manuellement : http://192.168.4.1
- ✅ Désactiver les données mobiles sur le téléphone
- ✅ Redémarrer le récepteur

### La LED ne s'allume pas
- ✅ Vérifier le sens de la LED (anode vers résistance, cathode vers GND)
- ✅ Vérifier la résistance (220Ω)
- ✅ Tester avec une LED différente

### La configuration ne se sauvegarde pas
- ✅ Vérifier dans le moniteur série si "Configuration sauvegardée" apparaît
- ✅ Essayer de redémarrer l'ESP32 pour voir si les valeurs persistent
- ✅ Vérifier que la librairie ArduinoJson est bien installée

---

## 📱 Interface web

L'interface web est responsive et optimisée pour les smartphones. Elle permet :

- 📝 Configuration du nom d'entreprise
- ⏱️ Réglage de la durée du timer (1 à 120 minutes)
- 📊 Affichage de la durée actuelle
- 🔴 Indication en temps réel si le timer est actif
- ⏳ Compte à rebours en direct

L'interface se met à jour automatiquement toutes les 2 secondes pour afficher l'état du timer.

---

## 🔐 Sécurité

- Chiffrement XOR basique des messages LoRa
- Clé de sécurité partagée entre émetteur et récepteur
- Validation des messages avec timestamp (anti-replay)
- Point d'accès WiFi ouvert pour faciliter la configuration (peut être sécurisé si besoin)

---

## 📝 Notes importantes

1. **Portée LoRa** : Dépend du module et de l'environnement (intérieur : ~100m, extérieur : ~500m+)
2. **Durée maximale** : Limitée à 120 minutes (2h) dans l'interface web
3. **Persistance** : La durée du timer est sauvegardée, elle persiste après redémarrage
4. **WiFi toujours actif** : Le point d'accès WiFi reste accessible en permanence pour modifications
5. **Librairies** : Utilise WebServer (synchrone) au lieu de ESPAsyncWebServer pour éviter les conflits

---

## 🎯 Cas d'usage

- Système de minuterie industrielle
- Contrôle d'éclairage à distance
- Temporisation d'équipements
- Système de présence/occupation
- Etc.

---

## 📄 Licence

Ce projet est open-source. Libre d'utilisation et de modification.

---

## ✉️ Support

Pour toute question ou problème, consultez :
- Les messages du moniteur série (115200 bauds)
- La section dépannage de ce README
- La documentation de la librairie LoRa_E32

---

**Bon projet ! 🚀**
