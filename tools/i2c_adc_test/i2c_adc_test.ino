/*
  Phase 1 - Hardwaretest: I2C-Scan + ADS1115-Rohwerte
  ---------------------------------------------------
  Board : esp32:esp32:nologo_esp32c3_super_mini
  Zweck : Vor dem Anschluss des pH-Boards pruefen, ob der ADS1115 sauber
          antwortet, und danach die tatsaechliche PO-Spannung messen.

  Verdrahtung: SDA=GPIO5, SCL=GPIO6, ADS1115 VDD=3.3V, ADDR=GND (0x48)

  Der TMC2209 muss fuer diesen Test NICHT angeschlossen sein.
*/

#include <Wire.h>

static const uint8_t PIN_SDA = 5;
static const uint8_t PIN_SCL = 6;
static const uint8_t ADS_ADDR = 0x48;

// PGA: 1 = +/-4.096 V  (Vollausschlag; LSB = 125 uV)
static const uint16_t PGA_SEL = 1;

static bool writeReg(uint8_t reg, uint16_t v) {
  Wire.beginTransmission(ADS_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(v >> 8));
  Wire.write((uint8_t)(v & 0xFF));
  return Wire.endTransmission() == 0;
}

static bool readReg(uint8_t reg, uint16_t &v) {
  Wire.beginTransmission(ADS_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)ADS_ADDR, 2) != 2) return false;
  v = ((uint16_t)Wire.read() << 8) | Wire.read();
  return true;
}

static bool readChannel(uint8_t ch, int16_t &raw) {
  uint16_t cfg = 0x8000            // Einzelmessung starten
               | (0x4000 | (ch << 12))  // MUX = AINx gegen GND
               | (PGA_SEL << 9)
               | 0x0100            // Single-Shot
               | 0x0080            // 128 SPS
               | 0x0003;           // Komparator aus
  if (!writeReg(0x01, cfg)) return false;
  uint32_t t0 = millis();
  uint16_t st = 0;
  do {
    delay(1);
    if (!readReg(0x01, st)) return false;
    if (millis() - t0 > 50) return false;
  } while ((st & 0x8000) == 0);
  uint16_t v;
  if (!readReg(0x00, v)) return false;
  raw = (int16_t)v;
  return true;
}

static float fullScale() {
  const float fs[] = {6.144f, 4.096f, 2.048f, 1.024f, 0.512f, 0.256f};
  return fs[PGA_SEL];
}

static void scan() {
  Serial.println("I2C-Scan...");
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  gefunden: 0x%02X%s\n", a,
                    (a >= 0x48 && a <= 0x4B) ? "   <- sieht nach ADS1115 aus" : "");
      n++;
    }
  }
  if (!n) Serial.println("  NICHTS gefunden - SDA/SCL/VDD/GND und Pullups pruefen!");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Phase 1: I2C / ADS1115 Test ===");
  Serial.printf("SDA=GPIO%u  SCL=GPIO%u  Adresse 0x%02X  Bereich +/-%.3f V\n",
                PIN_SDA, PIN_SCL, ADS_ADDR, fullScale());
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
  scan();
  Serial.println("\nA0 = pH-Board PO, A1..A3 frei");
  Serial.println("Messwerte (im Sekundentakt):\n");
}

void loop() {
  int16_t raw;
  float lsb = fullScale() / 32768.0f;

  for (uint8_t ch = 0; ch < 4; ch++) {
    if (readChannel(ch, raw)) {
      Serial.printf("A%u: %6d  %8.4f V   ", ch, raw, raw * lsb);
    } else {
      Serial.printf("A%u:   FEHLER          ", ch);
    }
  }
  Serial.println();
  delay(1000);
}
