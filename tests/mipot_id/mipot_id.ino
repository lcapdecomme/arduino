/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  RÉCEPTEUR RF 434MHz - DÉCODEUR FINAL v15
 * ═══════════════════════════════════════════════════════════════════════════
 *  Protocole découvert:
 *  - Encodage Manchester décalé (01=0, 10=1, décalage +1 bit)
 *  - 40 bits de données
 *  - Structure: [Préfixe 16 bits: 0x5FAA] [Données 16 bits] [Bouton+Compteur 8 bits]
 */

#define RF_DATA_PIN   27

#define SYNC_GAP_MIN  2500
#define MIN_PULSES    78
#define MAX_PULSES    86
#define THRESHOLD     575

#define RAW_BUFFER_SIZE 150
volatile unsigned int rawTimings[RAW_BUFFER_SIZE];
volatile int rawIndex = 0;
volatile unsigned long lastMicros = 0;
volatile bool frameReady = false;
volatile int frameLength = 0;

unsigned int processBuffer[RAW_BUFFER_SIZE];
int processLength = 0;
unsigned long lastDecodeTime = 0;
uint8_t lastButton = 0xFF;

void IRAM_ATTR handleRawInterrupt() {
  unsigned long now = micros();
  unsigned int duration = now - lastMicros;
  lastMicros = now;
  
  if (duration < 80) return;
  
  if (duration > SYNC_GAP_MIN) {
    if (rawIndex >= MIN_PULSES && rawIndex <= MAX_PULSES && !frameReady) {
      frameReady = true;
      frameLength = rawIndex;
    }
    rawIndex = 0;
    return;
  }
  
  if (rawIndex < RAW_BUFFER_SIZE && !frameReady) {
    rawTimings[rawIndex++] = duration;
  }
}

bool isValidSignal() {
  int validCount = 0;
  for (int i = 0; i < processLength; i++) {
    unsigned int t = processBuffer[i];
    if ((t >= 300 && t <= 520) || (t >= 650 && t <= 870)) {
      validCount++;
    }
  }
  return ((float)validCount / processLength) > 0.90;
}

const char* getButtonName(uint8_t code) {
  // Les 4 bits de poids faible semblent encoder le bouton
  // À ajuster après avoir testé les 4 boutons
  switch(code & 0x0F) {
    case 0x00: return "HAUT";
    case 0x01: return "GAUCHE";
    case 0x02: return "DROITE";
    case 0x03: return "BAS";
    default: return "INCONNU";
  }
}

void decodeFrame() {
  if (!isValidSignal()) return;
  
  unsigned long now = millis();
  if ((now - lastDecodeTime) < 250) return;
  lastDecodeTime = now;
  
  // Convertir timings en bits bruts (Court=1, Long=0)
  uint8_t rawBits[100];
  int rawBitCount = 0;
  
  for (int i = 0; i < processLength && rawBitCount < 100; i++) {
    rawBits[rawBitCount++] = (processBuffer[i] < THRESHOLD) ? 1 : 0;
  }
  
  // Décodage Manchester décalé +1 bit (01=0, 10=1)
  uint64_t code = 0;
  int bitCount = 0;
  
  for (int i = 1; i < rawBitCount - 1 && bitCount < 40; i += 2) {
    if (rawBits[i] == 0 && rawBits[i+1] == 1) {
      code = (code << 1);  // 0
      bitCount++;
    } else if (rawBits[i] == 1 && rawBits[i+1] == 0) {
      code = (code << 1) | 1;  // 1
      bitCount++;
    }
    // Ignorer les paires invalides
  }
  
  if (bitCount < 38) return;
  
  // Extraire les champs
  uint16_t prefix = (code >> 24) & 0xFFFF;
  uint8_t data1 = (code >> 16) & 0xFF;
  uint8_t data2 = (code >> 8) & 0xFF;
  uint8_t buttonCounter = code & 0xFF;
  
  // Vérifier le préfixe
  if (prefix != 0x5FAA) {
    Serial.printf("[Autre télécommande] Préfixe: 0x%04X, Code: 0x%010llX\n", prefix, code);
    return;
  }
  
  // Extraire bouton et compteur
  uint8_t button = buttonCounter & 0x0F;  // 4 bits de poids faible = bouton
  uint8_t counter = (buttonCounter >> 4) & 0x0F;  // 4 bits de poids fort = compteur
  
  // Anti-répétition
  if (button == lastButton) {
    return;
  }
  lastButton = button;
  
  // Affichage
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════════════════════╗");
  Serial.printf("║  🎯 BOUTON %s DÉTECTÉ !                                      ║\n", getButtonName(button));
  Serial.println("╚══════════════════════════════════════════════════════════════════╝");
  
  Serial.printf("\n  Code complet: 0x%010llX\n", code);
  Serial.printf("  Préfixe: 0x%04X\n", prefix);
  Serial.printf("  Données: 0x%02X 0x%02X\n", data1, data2);
  Serial.printf("  Bouton: 0x%X → %s\n", button, getButtonName(button));
  Serial.printf("  Compteur: %d\n", counter);
  
  // Binaire
  Serial.print("\n  Binaire: ");
  for (int i = 39; i >= 0; i--) {
    Serial.print((code >> i) & 1);
    if (i % 8 == 0 && i > 0) Serial.print(" ");
  }
  Serial.println("\n");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n╔══════════════════════════════════════════════════════════════════╗");
  Serial.println("║       RF 434MHz - DÉCODEUR FINAL v15                             ║");
  Serial.println("╚══════════════════════════════════════════════════════════════════╝\n");
  
  Serial.println("Protocole découvert:");
  Serial.println("  • Encodage: Manchester décalé");
  Serial.println("  • Préfixe: 0x5FAA");
  Serial.println("  • Dernier octet: [Compteur 4 bits][Bouton 4 bits]");
  Serial.println("");
  Serial.println("Teste les 4 boutons pour vérifier le mapping !");
  Serial.println("──────────────────────────────────────────────────────────────────\n");
  
  pinMode(RF_DATA_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RF_DATA_PIN), handleRawInterrupt, CHANGE);
}

void loop() {
  if (frameReady) {
    noInterrupts();
    processLength = frameLength;
    memcpy(processBuffer, (void*)rawTimings, processLength * sizeof(unsigned int));
    frameReady = false;
    interrupts();
    
    decodeFrame();
  }
  
  delay(1);
}
