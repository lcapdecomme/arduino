// Projet panneau de LEDs 
// V2 : 11/1/2024 : Mise au point
// V3 : 27/1/2024 : Programme définitif


// Librairies à utiliser : 
// Adafruit_neopixel :  https://github.com/adafruit/Adafruit_NeoPixel/releases
// Adafruit_gfx :       https://github.com/adafruit/Adafruit-GFX-Library/releases
// Adafruit_neomatrix : https://github.com/adafruit/Adafruit_NeoMatrix/releases
// Adafruit_neomatrix : https://github.com/adafruit/Adafruit_NeoMatrix/releases
// Busi :               https://github.com/adafruit/Adafruit_BusIO/releases


#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <RTClib.h>

// Si changement de Police 
// Inclure la police TomThumb (police petite taille)
// #include <Fonts/TomThumb.h>
// Inclure la police Mini9 (un peu plus grande)
// #include <Fonts/FreeSans9pt7b.h>


// 5 variables pour modifier le comportement du programme
#define PIN_MATRICE 6         // Port de la matrice sur l'arduino 
#define LUMINOSITE 50         // Luminosité de l'affichage sachant que cela varie de 0 à 255
#define HAUTEUR_MATRICE 8     // Nombre de pixel en hauteur. On laisse 8 sur ce modèle
#define LARGEUR_MATRICE 64    // Nombre de pixel en largeur. On laisse 64 sur ce modèle (2 matrices de 32 pixels)

#define DELAI   1000          // Attente en milliseconde entre deux calculs d'heure

#define RED      255          // Si blanc, mettre toutes les valeurs à 255
#define GREEN    0            // Si Red, mettre RED a 255 et les autres valeurs à 0
#define BLUE     0 

// Si Date cible : Par exemple 23/02/2025 s'écrit : 2025, 2, 23
// Si Date cible : Par exemple 1/1/2036 s'écrit : 2036, 1, 1
// Date Apophis : 14 avril 2036 mettre 2036, 4, 14 
#define DATE_CIBLE targetDate(2036, 4, 14, 0, 0, 0);


// Clock se branche sur les SCL (21 sur mega) et SDA (20 sur mega)
 
// ****** Début du programme ***********

// Initialisation du temps
RTC_DS3231 rtc;

// Initialisation de la matrice
// Largeur : 2x32 pixels
// hauteur en pixels : HAUTEUR_MATRICE
// Matrice sur la pin num. :  PIN_MATRICE
// NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  : Première led en bas à droite 
// NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG : branchement en colonne de type zigzag
// NEO_GRB + NEO_KHZ800 : type de led
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(LARGEUR_MATRICE,HAUTEUR_MATRICE,PIN_MATRICE,
  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT  +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

void setup(){  
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(LUMINOSITE);
  matrix.setTextColor(matrix.Color(RED, GREEN, BLUE)); // Couleur du texte (rouge ici)
  // Changer la taille de la police
  // matrix.setFont(&TomThumb);  // Police de petite taille
  //matrix.setFont(&FreeSans9pt7b);  // Police un peu plus grande
  matrix.clear();

  // Initialise composant time
  rtc.begin();

  delay(1000);
  pinMode(13, OUTPUT);
}



// Affichage fixe
void loop() {

  DateTime DATE_CIBLE
  
  DateTime now = rtc.now();
  // Serial.println(now.unixtime());
 
  // Calculer la différence en jours, heures, minutes et secondes
  long secondsLeft = targetDate.unixtime() - now.unixtime();
  
  // Convertir en jours, heures, minutes, secondes
  long daysLeft = secondsLeft / 86400; // 86400 secondes par jour
  secondsLeft %= 86400;
  
  int hoursLeft = secondsLeft / 3600; // 3600 secondes par heure
  secondsLeft %= 3600;
  
  int minutesLeft = secondsLeft / 60; // 60 secondes par minute
  int secondsLeftFinal = secondsLeft % 60;

  // Afficher les résultats

    String daysLeftj="";
    if (daysLeft<1000) {
      daysLeftj=daysLeftj+"0";
    }
    if (daysLeft<100) {
      daysLeftj=daysLeftj+"0";
    }
    if (daysLeft<10) {
      daysLeftj=daysLeftj+"0";
    }
    daysLeftj=daysLeftj+String(daysLeft)+"";
    
    String daysLefth="";
    if (hoursLeft<10) {
      daysLefth=daysLefth+"0";
    }
    daysLefth=daysLefth+String(hoursLeft)+"";
    
    String daysLeftm="";
    if (minutesLeft<10) {
      daysLeftm=daysLeftm+"0";
    }
    daysLeftm=daysLeftm+String(minutesLeft)+"";
    
    String daysLefts="";
    if (secondsLeftFinal<10) {
      daysLefts=daysLefts+"0";
    }
    daysLefts=daysLefts+String(secondsLeftFinal);
    
    matrix.fillScreen(0);
    matrix.setCursor(1,0);    
    matrix.print(daysLeftj);
    matrix.setCursor(27,0);    
    matrix.print(daysLefth);
    matrix.setCursor(40,0);    
    matrix.print(daysLeftm);
    matrix.setCursor(53,0);    
    matrix.print(daysLefts);
    matrix.show();  
    delay(DELAI);
}





// Test de défilement : Non utilisé dans ce programme
void loopDefilement() {

  // Date cible : 1er janvier 2030 à 00:00:00
  DateTime targetDate(2036, 3, 1, 0, 0, 0);

  DateTime now = rtc.now();
 
  // Calculer la différence en jours, heures, minutes et secondes
  long secondsLeft = targetDate.unixtime() - now.unixtime();
  
  // Convertir en jours, heures, minutes, secondes
  long daysLeft = secondsLeft / 86400; // 86400 secondes par jour
  secondsLeft %= 86400;
  
  int hoursLeft = secondsLeft / 3600; // 3600 secondes par heure
  secondsLeft %= 3600;
  
  int minutesLeft = secondsLeft / 60; // 60 secondes par minute
  int secondsLeftFinal = secondsLeft % 60;

  // Afficher les résultats

    String resultat=String(daysLeft)+"."+String(hoursLeft)+"."+String(minutesLeft)+"."+String(secondsLeftFinal);
    
  // Position 64 à -30
  for(int x=64; x>=-68; x-=1) { 
    matrix.setCursor(1, 1); 
    matrix.print(resultat); 
    matrix.show();
    delay(80);
    matrix.fillScreen(0);
  }
}
