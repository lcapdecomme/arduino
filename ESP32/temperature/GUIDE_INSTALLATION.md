# Guide d'Installation - Moniteur de Température ESP32

## 📋 Liste du Matériel

- ESP32 (n'importe quel modèle)
- Sonde DS18B20 (3 fils : rouge, noir, jaune)
- 1× Résistance 4.7kΩ (jaune-violet-rouge)
- 2× LEDs (1 rouge, 1 verte)
- 2× Résistances 1kΩ (marron-noir-rouge) - **ou 220Ω si disponibles**
- Breadboard
- Câbles jumper

**Note:** Les résistances de 1kΩ (1000Ω) fonctionnent parfaitement pour les LEDs. Elles seront juste un peu moins lumineuses qu'avec des 220Ω, ce qui n'est pas un problème.

## 🔌 Schéma de Câblage Détaillé

```
ESP32                    DS18B20 (Sonde)
------------------------
3.3V ─────────────────→ Fil ROUGE
                    ┌─→ Fil ROUGE (via résistance 4.7kΩ)
GPIO 4 ─────────────┼─→ Fil JAUNE
                    │
GND ──────────────────→ Fil NOIR


ESP32                    LED Rouge
------------------------
GPIO 2 ───[1kΩ]─────→ Anode (patte longue +)
GND ─────────────────→ Cathode (patte courte -)


ESP32                    LED Verte
------------------------
GPIO 15 ──[1kΩ]─────→ Anode (patte longue +)
GND ─────────────────→ Cathode (patte courte -)
```

### Astuce pour identifier les pattes des LEDs :
- **Patte LONGUE** = Anode (+) → va vers la résistance puis GPIO
- **Patte COURTE** = Cathode (-) → va vers GND

### Important pour la résistance 4.7kΩ :
Elle doit être connectée ENTRE le fil jaune et le fil rouge du DS18B20.
Cette résistance est appelée "pull-up" et est essentielle pour le fonctionnement.

## 💻 Installation du Logiciel

### 1. Installation de l'IDE Arduino

1. Télécharge Arduino IDE depuis : https://www.arduino.cc/en/software
2. Installe-le sur ton ordinateur

### 2. Configuration de l'ESP32 dans Arduino IDE

1. Ouvre Arduino IDE
2. Va dans **Fichier** → **Préférences**
3. Dans "URL de gestionnaire de cartes supplémentaires", ajoute :
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Clique sur **OK**
5. Va dans **Outils** → **Type de carte** → **Gestionnaire de carte**
6. Cherche "ESP32" et installe "esp32 by Espressif Systems"

### 2. Installation des Bibliothèques

1. Va dans **Croquis** → **Inclure une bibliothèque** → **Gérer les bibliothèques**
2. Installe les bibliothèques suivantes (cherche et clique sur "Installer") :
   - **OneWire** (par Paul Stoffregen)
   - **DallasTemperature** (par Miles Burton)

**Note:** La bibliothèque **DNSServer** est déjà incluse avec l'ESP32, pas besoin de l'installer séparément.

### 3. Téléversement du Code

1. Ouvre le fichier `esp32_temperature.ino`
2. Connecte ton ESP32 à l'ordinateur via USB
3. Dans **Outils**, configure :
   - **Type de carte** : Sélectionne ton modèle d'ESP32 (par exemple "ESP32 Dev Module")
   - **Port** : Sélectionne le port COM de ton ESP32
4. Clique sur le bouton **Téléverser** (→)
5. Attends que le téléversement se termine

## 🚀 Utilisation

### 1. Connexion au WiFi (Portail Captif)

1. Une fois le code téléversé, l'ESP32 crée un réseau WiFi nommé : **ESP32-Temperature**
2. Sur ton téléphone ou ordinateur :
   - Ouvre les paramètres WiFi
   - Connecte-toi au réseau "ESP32-Temperature" (pas de mot de passe)
3. **Magie du portail captif** 🎩✨ :
   - Sur la plupart des appareils (smartphone, tablette), une notification apparaîtra automatiquement
   - Clique sur cette notification OU ouvre ton navigateur
   - Tu seras **automatiquement redirigé** vers l'interface de température !
   - C'est exactement comme dans les hôtels, aéroports, cafés...

**Si la redirection automatique ne fonctionne pas :**
- Ouvre manuellement un navigateur et va sur n'importe quel site (ex: google.com)
- Ou tape directement l'adresse : **http://192.168.4.1**

### 2. Configuration des Limites

Dans l'interface web :
1. Entre la **Limite Maximale** (ex: 30°C)
2. Entre la **Limite Minimale** (ex: 10°C)
3. Clique sur **Enregistrer les Limites**

### 3. Fonctionnement des LEDs

- **LED ROUGE** : S'allume quand la température dépasse la limite MAX
- **LED VERTE** : S'allume quand la température descend sous la limite MIN

### 4. Historique

- Les 5 derniers dépassements sont automatiquement enregistrés
- Consulte-les dans la section "Historique des Dépassements"
- Tu peux effacer l'historique avec le bouton "Effacer"

## 🔧 Dépannage

### La sonde affiche 0°C ou ne fonctionne pas :
- Vérifie que la résistance 4.7kΩ est bien connectée entre le fil jaune et rouge
- Vérifie que tous les câbles sont bien enfoncés
- Essaie de reconnecter la sonde

### Impossible de se connecter au WiFi :
- Assure-toi que l'ESP32 est bien alimenté (LED bleue allumée)
- Redémarre l'ESP32 (bouton RESET)
- Vérifie que le réseau "ESP32-Temperature" apparaît dans la liste WiFi

### Les LEDs ne s'allument pas :
- Vérifie le sens des LEDs (patte longue vers la résistance)
- Vérifie que les résistances 220Ω sont bien en place
- Teste avec d'autres valeurs de limites

### Page web inaccessible :
- Confirme que tu es bien connecté au réseau "ESP32-Temperature"
- Essaie l'adresse : http://192.168.4.1
- Vide le cache du navigateur

## 📱 Fonctionnalités

✅ Affichage en temps réel de la température
✅ Configuration des limites min/max
✅ Sauvegarde automatique des paramètres (persistent après redémarrage)
✅ Historique des 5 derniers dépassements avec date/heure
✅ Interface responsive (fonctionne sur téléphone, tablette, PC)
✅ LEDs indicatrices
✅ Pas de mot de passe requis (réseau captif)
✅ Interface 100% en français

## 🎨 Personnalisation

Tu peux modifier dans le code :
- Le nom du réseau WiFi : `const char* ssid = "ESP32-Temperature";`
- Les broches GPIO si besoin
- Les limites par défaut : `limiteMax` et `limiteMin`

## 📞 Support

Si tu rencontres des problèmes :
1. Ouvre le **Moniteur Série** dans Arduino IDE (115200 bauds)
2. Regarde les messages de debug
3. Vérifie ton montage avec le schéma ci-dessus
