// Première version du variateur de ventilo pour  rduino nano

#define mosfet 15        // module MOSFET en pin 3 (Sortie digital en PWM : Important ! )
#define MAX 50          // Vitesse du variateur

void setup()
{
  // déclaration des différents capteurs en entrée ou en sortie
  pinMode(mosfet, OUTPUT);       // module MOSFET déclaré en sortie
  Serial.begin(9600);
  analogWrite(mosfet, MAX);
}

void loop()
{
  digitalWrite(mosfet, HIGH);    // Activer le module MOSFET
  Serial.println("a fond ?");
  delay (1000);                  // Attendre 3 secondes
  digitalWrite(mosfet, LOW);     // désactiver le MOSFET
  Serial.println("stop! ");
  delay (1000);                  
  /* démarrage progressif
  for(int i=0; i<=MAX; i++) {
  analogWrite(mosfet, i);
    Serial.println(i);
  delay(100);
   }
  // arrêt progressif
  for(int i=MAX; i>=0; i--) {
  analogWrite(mosfet, i);
    Serial.println(i);
  delay(100);
   }
  */
}
