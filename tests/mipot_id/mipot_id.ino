/*
 * ═══════════════════════════════════════════════════════════════════════════
 *  DIAGNOSTIC RF 434MHz - v29 - DEBOUNCE RÉDUIT
 * ═══════════════════════════════════════════════════════════════════════════
 */

#define RF_DATA_PIN   27

#define SYNC_GAP_MIN  2500
#define RAW_BUFFER_SIZE 200

volatile unsigned int rawTimings[RAW_BUFFER_SIZE];
volatile int rawIndex = 0;
volatile unsigned long lastMicros = 0;
volatile bool frameReady = false;
volatile int frameLength = 0;

unsigned int processBuffer[RAW_BUFFER_SIZE];
int processLength = 0;
unsigned long lastDecodeTime = 0;

int frameCounter = 0;
int rawFrameCounter = 0;  // Toutes les trames reçues

// Anti-répétition séparés par protocole
uint32_t lastProto6Code = 0xFFFFFFFF;
uint64_t lastManchesterSig = 0xFFFFFFFFFFFFFFFF;

void IRAM_ATTR handleRFInterrupt() {
  unsigned long now = micros();
  unsigned int duration = now - lastMicros;
  lastMicros = now;
  
  if (duration < 50) return;
  
  if (duration > SYNC_GAP_MIN) {
    if (rawIndex >= 20 && rawIndex <= 150 && !frameReady) {
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

bool decodeProtocol6(uint32_t* code, uint8_t* bitLength) {
  if (processLength < 23 || processLength > 26) return false;
  
  int shortCount = 0, longCount = 0;
  for (int i = 0; i < processLength; i++) {
    unsigned int t = processBuffer[i];
    if (t >= 250 && t <= 450) shortCount++;
    else if (t >= 550 && t <= 750) longCount++;
  }
  
  if (shortCount < 5 || longCount < 5) return false;
  
  *code = 0;
  *bitLength = 0;
  
  for (int i = 0; i < processLength - 1 && *bitLength < 12; i += 2) {
    unsigned int t1 = processBuffer[i];
    unsigned int t2 = processBuffer[i + 1];
    
    if (t1 >= 550 && t1 <= 750 && t2 >= 250 && t2 <= 450) {
      *code = (*code << 1) | 1;
      (*bitLength)++;
    }
    else if (t1 >= 250 && t1 <= 450 && t2 >= 550 && t2 <= 750) {
      *code = (*code << 1);
      (*bitLength)++;
    }
    else if (t1 >= 250 && t1 <= 450 && t2 >= 250 && t2 <= 450) {
      *code = (*code << 1);
      (*bitLength)++;
    }
  }
  
  return (*bitLength >= 10);
}

bool decodeManchester(uint64_t* code, uint8_t* bitLength) {
  if (processLength < 78 || processLength > 86) return false;
  
  int validCount = 0;
  for (int i = 0; i < processLength; i++) {
    unsigned int t = processBuffer[i];
    if ((t >= 300 && t <= 520) || (t >= 650 && t <= 870)) {
      validCount++;
    }
  }
  if ((float)validCount / processLength < 0.90) return false;
  
  uint8_t rawBits[100];
  int rawBitCount = 0;
  
  for (int i = 0; i < processLength && rawBitCount < 100; i++) {
    rawBits[rawBitCount++] = (processBuffer[i] < 575) ? 1 : 0;
  }
  
  *code = 0;
  *bitLength = 0;
  
  for (int i = 1; i < rawBitCount - 1 && *bitLength < 40; i += 2) {
    if (rawBits[i] == 0 && rawBits[i+1] == 1) {
      *code = (*code << 1);
      (*bitLength)++;
    } else if (rawBits[i] == 1 && rawBits[i+1] == 0) {
      *code = (*code << 1) | 1;
      (*bitLength)++;
    }
  }
  
  return (*bitLength >= 38);
}

void displayProto6(uint32_t code, uint8_t bits) {
  frameCounter++;
  
  uint8_t highByte = (code >> 4) & 0xFF;
  uint8_t lowNibble = code & 0x0F;
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════════════════════════════════╗");
  Serial.printf("║  TRAME #%d - PROTOCOLE 6 (%d bits) - Raw: %d                          \n", frameCounter, bits, rawFrameCounter);
  Serial.println("╠═══════════════════════════════════════════════════════════════════════╣");
  Serial.printf("║  Code: 0x%03lX  |  ID: 0x%02X  |  Bouton: 0x%X                          \n", code, highByte, lowNibble);
  Serial.print("║  Binaire: ");
  for (int i = bits - 1; i >= 0; i--) {
    Serial.print((code >> i) & 1);
    if (i == 4) Serial.print(" | ");
  }
  Serial.println();
  Serial.printf("║  Pulses: %d                                                           \n", processLength);
  Serial.println("╚═══════════════════════════════════════════════════════════════════════╝");
}

void displayManchester(uint64_t code, uint8_t bits) {
  frameCounter++;
  
  uint8_t byte0 = (code >> 32) & 0xFF;
  uint8_t byte1 = (code >> 24) & 0xFF;
  uint8_t byte2 = (code >> 16) & 0xFF;
  uint8_t byte3 = (code >> 8) & 0xFF;
  uint8_t byte4 = code & 0xFF;
  
  uint16_t prefix = ((uint16_t)byte0 << 8) | byte1;
  uint8_t button = byte4 & 0x0F;
  uint8_t counter = (byte4 >> 4) & 0x0F;
  
  String typeStr;
  uint32_t id;
  if (prefix != 0x0000) {
    typeStr = "Type A";
    id = prefix;
  } else {
    typeStr = "Type B";
    id = byte2 & 0x0F;
  }
  
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════════════════════════════════╗");
  Serial.printf("║  TRAME #%d - MANCHESTER %s (%d bits) - Raw: %d                       \n", frameCounter, typeStr.c_str(), bits, rawFrameCounter);
  Serial.println("╠═══════════════════════════════════════════════════════════════════════╣");
  Serial.printf("║  Code: 0x%010llX                                                    \n", code);
  Serial.printf("║  ID: 0x%04X  |  Bouton: %d  |  Compteur: %d                           \n", id, button, counter);
  Serial.println("║                                                                       ║");
  Serial.println("║  OCTETS:                                                              ║");
  Serial.printf("║    [0]=0x%02X [1]=0x%02X [2]=0x%02X [3]=0x%02X [4]=0x%02X              \n", 
                byte0, byte1, byte2, byte3, byte4);
  Serial.print("║  Binaire: ");
  for (int i = 39; i >= 0; i--) {
    Serial.print((code >> i) & 1);
    if (i % 8 == 0 && i > 0) Serial.print(" ");
  }
  Serial.println();
  Serial.printf("║  Pulses: %d                                                           \n", processLength);
  Serial.println("╚═══════════════════════════════════════════════════════════════════════╝");
}

void processFrame() {
  rawFrameCounter++;
  
  // Essayer Proto 6
  uint32_t proto6Code;
  uint8_t proto6Bits;
  
  if (decodeProtocol6(&proto6Code, &proto6Bits)) {
    // Anti-répétition : même code dans les 100ms
    unsigned long now = millis();
    static unsigned long lastProto6Time = 0;
    
    if (proto6Code == lastProto6Code && (now - lastProto6Time) < 100) {
      return;  // Répétition, ignorer
    }
    lastProto6Code = proto6Code;
    lastProto6Time = now;
    
    displayProto6(proto6Code, proto6Bits);
    return;
  }
  
  // Essayer Manchester
  uint64_t manchesterCode;
  uint8_t manchesterBits;
  
  if (decodeManchester(&manchesterCode, &manchesterBits)) {
    uint8_t button = manchesterCode & 0x0F;
    if (button > 3) return;  // Parasite
    
    // Signature = ID + bouton (ignorer compteur)
    uint8_t byte0 = (manchesterCode >> 32) & 0xFF;
    uint8_t byte1 = (manchesterCode >> 24) & 0xFF;
    uint8_t byte2 = (manchesterCode >> 16) & 0xFF;
    uint16_t prefix = ((uint16_t)byte0 << 8) | byte1;
    
    uint32_t id = (prefix != 0x0000) ? prefix : (byte2 & 0x0F);
    uint64_t sig = ((uint64_t)id << 8) | button;
    
    // Anti-répétition
    unsigned long now = millis();
    static unsigned long lastManchesterTime = 0;
    
    if (sig == lastManchesterSig && (now - lastManchesterTime) < 100) {
      return;
    }
    lastManchesterSig = sig;
    lastManchesterTime = now;
    
    displayManchester(manchesterCode, manchesterBits);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n");
  Serial.println("╔═══════════════════════════════════════════════════════════════════════╗");
  Serial.println("║       DIAGNOSTIC RF 434MHz - v29 - DEBOUNCE 100ms                     ║");
  Serial.println("╚═══════════════════════════════════════════════════════════════════════╝\n");
  
  pinMode(RF_DATA_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(RF_DATA_PIN), handleRFInterrupt, CHANGE);
  
  Serial.printf("Récepteur sur GPIO%d\n\n", RF_DATA_PIN);
}

void loop() {
  if (frameReady) {
    noInterrupts();
    processLength = frameLength;
    memcpy(processBuffer, (void*)rawTimings, processLength * sizeof(unsigned int));
    frameReady = false;
    interrupts();
    
    processFrame();
  }
  
  // Afficher stats toutes les 10 secondes
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 10000) {
    Serial.printf("\n[STATS] Trames brutes: %d | Trames valides: %d\n\n", rawFrameCounter, frameCounter);
    lastStats = millis();
  }
  
  delay(1);
}
