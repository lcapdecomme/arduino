#include "LoRa_E32.h"

#define LORA_RX 16
#define LORA_TX 17
#define LORA_AUX 4
#define LORA_M0 18
#define LORA_M1 19

HardwareSerial loraSerial(1);
LoRa_E32 e32(&loraSerial, LORA_AUX, LORA_M0, LORA_M1);

void printConfig(const Configuration &cfg) {
  Serial.println("--- CONFIG ---");
  Serial.print("TransmissionPower (raw): ");
  Serial.println(cfg.OPTION.transmissionPower);
  Serial.print("AirDataRate (raw): ");
  Serial.println(cfg.SPED.airDataRate);
  Serial.print("UART Baud: ");
  Serial.println(cfg.SPED.uartBaudRate);
  Serial.print("Channel: 0x");
  Serial.println(cfg.CHAN, HEX);
  Serial.println("----------------");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  loraSerial.begin(9600, SERIAL_8N1, LORA_RX, LORA_TX);
  e32.begin();

  // Lecture config actuelle
  ResponseStructContainer rc = e32.getConfiguration();
  if (rc.status.code == 0) {
    Configuration cfg = *(Configuration*)rc.data;
    printConfig(cfg);
    rc.close();
  } else {
    Serial.print("Erreur lecture config: ");
    Serial.println(rc.status.getResponseDescription());
    rc.close();
  }

  // Forcer POWER_20, puis sauvegarder
  Serial.println("Tentative: définir POWER_20...");
  ResponseStructContainer rc2 = e32.getConfiguration();
  Configuration cfg2 = *(Configuration*)rc2.data;
  rc2.close();

  cfg2.OPTION.transmissionPower = POWER_20; // essayer 20dBm
  // Optionnel : améliorer robustesse
  cfg2.SPED.airDataRate = AIR_DATA_RATE_000_03; // plus lent, plus robuste (vérifier disponibilité)
  cfg2.OPTION.fec = FEC_1_ON;

  ResponseStatus rs = e32.setConfiguration(cfg2, WRITE_CFG_PWR_DWN_SAVE);
  Serial.print("setConfiguration status: ");
  Serial.println(rs.getResponseDescription());

  // Relire pour vérifier
  ResponseStructContainer rc3 = e32.getConfiguration();
  if (rc3.status.code == 0) {
    Configuration cfg3 = *(Configuration*)rc3.data;
    printConfig(cfg3);
    rc3.close();
  } else {
    Serial.print("Erreur relecture config: ");
    Serial.println(rc3.status.getResponseDescription());
    rc3.close();
  }
}

void loop() {
  // rien
  delay(10000);
}
