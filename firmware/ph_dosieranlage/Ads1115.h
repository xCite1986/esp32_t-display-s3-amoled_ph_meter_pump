// Ads1115.h - minimaler, abhaengigkeitsfreier Treiber fuer den ADS1115
// Bewusst ohne Fremdbibliothek, damit die Firmware nur den ESP32-Core braucht.
#pragma once

#include <Arduino.h>

enum AdsGain : uint16_t {
  ADS_GAIN_6144 = 0,  // +/- 6.144 V
  ADS_GAIN_4096 = 1,  // +/- 4.096 V  <- Default hier
  ADS_GAIN_2048 = 2,  // +/- 2.048 V
  ADS_GAIN_1024 = 3,  // +/- 1.024 V
  ADS_GAIN_0512 = 4,  // +/- 0.512 V
  ADS_GAIN_0256 = 5   // +/- 0.256 V
};

class Ads1115 {
 public:
  bool begin(uint8_t addr = 0x48);
  bool present() const { return present_; }

  void setGain(AdsGain g) { gain_ = g; }
  AdsGain gain() const { return gain_; }

  // Einzelmessung single-ended an Kanal 0..3. true = erfolgreich.
  bool readSingleEnded(uint8_t channel, int16_t &raw);

  // Rohwert -> Spannung in Volt (abhaengig vom eingestellten PGA)
  float toVolts(int16_t raw) const;

  // Volle Skala der aktuellen Verstaerkung in Volt
  float fullScale() const;

  static const char *gainName(AdsGain g);

 private:
  uint8_t addr_    = 0x48;
  bool    present_ = false;
  AdsGain gain_    = ADS_GAIN_4096;

  bool writeReg(uint8_t reg, uint16_t value);
  bool readReg(uint8_t reg, uint16_t &value);
};
