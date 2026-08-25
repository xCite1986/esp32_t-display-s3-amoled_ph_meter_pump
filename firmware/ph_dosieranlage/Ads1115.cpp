#include "Ads1115.h"
#include <Wire.h>

// Registeradressen
static const uint8_t REG_CONVERT = 0x00;
static const uint8_t REG_CONFIG  = 0x01;

// Config-Bits
static const uint16_t CFG_OS_SINGLE   = 0x8000;  // Einzelmessung starten
static const uint16_t CFG_MODE_SINGLE = 0x0100;  // Single-Shot-Modus
static const uint16_t CFG_DR_128SPS   = 0x0080;  // 128 Samples/s
static const uint16_t CFG_COMP_OFF    = 0x0003;  // Komparator deaktiviert

bool Ads1115::begin(uint8_t addr) {
  addr_ = addr;
  Wire.beginTransmission(addr_);
  present_ = (Wire.endTransmission() == 0);
  return present_;
}

bool Ads1115::writeReg(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(addr_);
  Wire.write(reg);
  Wire.write((uint8_t)(value >> 8));
  Wire.write((uint8_t)(value & 0xFF));
  return Wire.endTransmission() == 0;
}

bool Ads1115::readReg(uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(addr_);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)addr_, 2) != 2) return false;
  value = ((uint16_t)Wire.read() << 8);
  value |= Wire.read();
  return true;
}

bool Ads1115::readSingleEnded(uint8_t channel, int16_t &raw) {
  if (channel > 3) return false;

  uint16_t cfg = CFG_OS_SINGLE | CFG_MODE_SINGLE | CFG_DR_128SPS | CFG_COMP_OFF;
  cfg |= (uint16_t)(0x4000 | ((uint16_t)channel << 12));  // MUX = AINx vs GND
  cfg |= (uint16_t)((uint16_t)gain_ << 9);                // PGA

  if (!writeReg(REG_CONFIG, cfg)) { present_ = false; return false; }

  // 128 SPS -> ca. 7.8 ms Wandlungszeit; auf OS-Bit pollen mit Timeout
  uint32_t start = millis();
  uint16_t status = 0;
  do {
    delay(1);
    if (!readReg(REG_CONFIG, status)) { present_ = false; return false; }
    if (millis() - start > 50) return false;   // Wandlung haengt
  } while ((status & CFG_OS_SINGLE) == 0);     // OS=0 -> Wandlung laeuft noch

  uint16_t v = 0;
  if (!readReg(REG_CONVERT, v)) { present_ = false; return false; }
  raw = (int16_t)v;
  present_ = true;
  return true;
}

float Ads1115::fullScale() const {
  switch (gain_) {
    case ADS_GAIN_6144: return 6.144f;
    case ADS_GAIN_4096: return 4.096f;
    case ADS_GAIN_2048: return 2.048f;
    case ADS_GAIN_1024: return 1.024f;
    case ADS_GAIN_0512: return 0.512f;
    case ADS_GAIN_0256: return 0.256f;
  }
  return 4.096f;
}

float Ads1115::toVolts(int16_t raw) const {
  return (float)raw * (fullScale() / 32768.0f);
}

const char *Ads1115::gainName(AdsGain g) {
  switch (g) {
    case ADS_GAIN_6144: return "+/-6.144V";
    case ADS_GAIN_4096: return "+/-4.096V";
    case ADS_GAIN_2048: return "+/-2.048V";
    case ADS_GAIN_1024: return "+/-1.024V";
    case ADS_GAIN_0512: return "+/-0.512V";
    case ADS_GAIN_0256: return "+/-0.256V";
  }
  return "?";
}
