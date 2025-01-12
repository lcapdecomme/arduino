// Projet panneau de LEDs 
// V2 : 11/1/2024 : Programme définitif


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

// Inclure la police TomThumb (police petite taille)
//#include <Fonts/TomThumb.h>
// Inclure la police Mini9 (un peu plus grande)
//#include <Fonts/FreeSans9pt7b.h>


// 5 variables pour modifier le comportement du programme
#define PIN_MATRICE 8         // Port de la matrice sur l'arduino 
#define LUMINOSITE 50         // Luminosité de l'affichage sachant que cela varie de 0 à 255
#define HAUTEUR_MATRICE 8     // Nombre de pixel en hauteur. On laisse 8 sur ce modèle
#define LARGEUR_MATRICE 64    // Nombre de pixel en largeur. On laisse 64 sur ce modèle (2 matrices de 32 pixels)

#define DELAI   1000          // Attente en milliseconde entre deux calculs d'heure


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
  Serial.begin(9600);
  
  
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(LUMINOSITE);
    matrix.setTextColor(matrix.Color(255, 0, 0)); // Couleur du texte (rouge ici)
  // Changer la taille de la police
  // matrix.setFont(&TomThumb);  // Police de petite taille
  //matrix.setFont(&FreeSans9pt7b);  // Police un peu plus grande
  matrix.clear();



  // Initialise composant time
  rtc.begin();


  delay(1000);
}



// Affichage fixe
void loop() {

  // Date cible : 1er janvier 2030 à 00:00:00
  DateTime targetDate(2036, 1, 1, 0, 0, 0);

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
  Serial.print("Il reste ");
  Serial.print(daysLeft);
  Serial.print(" jours, ");
  Serial.print(hoursLeft);
  Serial.print(" heures, ");
  Serial.print(minutesLeft);
  Serial.print(" minutes et ");
  Serial.println(secondsLeftFinal);

    String daysLeftj=String(daysLeft)+"";
    
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
    //matrix.setCursor(10,6); 
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
  Serial.print("Il reste ");
  Serial.print(daysLeft);
  Serial.print(" jours, ");
  Serial.print(hoursLeft);
  Serial.print(" heures, ");
  Serial.print(minutesLeft);
  Serial.print(" minutes et ");
  Serial.println(secondsLeftFinal);

    String resultat=String(daysLeft)+"."+String(hoursLeft)+"."+String(minutesLeft)+"."+String(secondsLeftFinal);
  Serial.println(resultat);
    
  // Position 64 à -30
  for(int x=64; x>=-68; x-=1) { 
    matrix.setCursor(1, 1); 
    matrix.print(resultat); 
    matrix.show();
    delay(80);
    matrix.fillScreen(0);
  }
}
