// PHMeasurement.h - ADS1115 auslesen, filtern, pH berechnen, Kalibrierung
#pragma once

#include <Arduino.h>
#include "Ads1115.h"
#include "Config.h"

enum PhStatus : uint8_t {
  PH_OK = 0,
  PH_NO_SENSOR,      // ADS1115 antwortet nicht
  PH_NO_CALIB,       // keine gueltige Kalibrierung
  PH_VOLT_RANGE,     // Spannung ausserhalb des plausiblen Fensters
  PH_PH_RANGE,       // pH ausserhalb des plausiblen Fensters
  PH_WARMUP          // noch nicht genug Messwerte
};

class PHMeasurement {
 public:
  bool begin();
  void tick();                       // zyklisch aus loop() aufrufen

  bool     valid() const { return status_ == PH_OK; }
  PhStatus status() const { return status_; }
  const char *statusText() const;

  float voltage() const { return voltage_; }     // gefilterte Spannung [V]
  float voltageRaw() const { return voltRaw_; }  // letzter Einzelwert [V]
  float ph() const { return ph_; }               // gefilterter pH
  int16_t rawAdc() const { return rawAdc_; }
  bool  stable() const { return stable_; }       // Spanne < PH_STABLE_BAND
  float spread() const { return spread_; }       // Spanne im Fenster [pH]
  float spreadV() const { return spreadV_; }     // Spanne im Fenster [V] - immer gueltig

  // Gleitender Mittelwert ueber settings.phAvgS. Die Regelung entscheidet
  // danach, nicht nach dem Momentanwert - ein einzelner Ausreisser soll keine
  // Dosierung ausloesen.
  float phAverage() const { return avgPh_; }
  bool  averageReady() const { return avgReady_; }
  uint16_t averageCount() const { return avgUsed_; }

  // Anzahl der Messbuendel, die wegen Zeitverzug verworfen wurden. Steigt der
  // Wert dauernd, kommt die Schleife nicht schnell genug durch.
  uint32_t burstsDropped() const { return burstDrop_; }
  uint32_t lastGoodMs() const { return lastGood_; }

  // Kalibrierung: aktuelle (gefilterte) Spannung als Punkt A oder B ablegen.
  bool calibratePoint(bool pointA, float phValue, String &err);
  void applySettings();               // Gain / Kalibrierwerte neu uebernehmen

  float voltToPh(float v) const;
  float slopePerPh() const;           // mV pro pH-Einheit (Info/Diagnose)
  bool  sensorPresent() const { return ads_.present(); }

 private:
  Ads1115  ads_;
  PhStatus status_   = PH_WARMUP;
  float    voltage_  = 0;
  float    voltRaw_  = 0;
  float    ph_       = 7.0f;
  int16_t  rawAdc_   = 0;
  bool     stable_   = false;
  float    spread_   = 99.0f;
  float    spreadV_  = 0;
  float    avgBuf_[PH_AVG_SLOTS];
  uint16_t avgIdx_   = 0;
  uint16_t avgUsed_  = 0;
  uint32_t lastAvgMs_ = 0;
  float    avgPh_    = NAN;
  bool     avgReady_ = false;
  float    emaAlpha_ = PH_EMA_ALPHA;
  void updateAverage();
  bool     emaInit_  = false;
  uint32_t lastSample_ = 0;
  uint32_t lastGood_   = 0;
  uint8_t  failCount_  = 0;

  float    buf_[PH_SAMPLE_COUNT];
  uint8_t  bufCount_ = 0;
  uint8_t  bufIdx_   = 0;

  // Netzsynchrone Mittelung als Zustandsautomat. Die 20 ms werden ueber viele
  // Schleifendurchlaeufe verteilt abgetastet, damit die Schrittausgabe der
  // Pumpe nicht blockiert wird - siehe loop() in der .ino.
  bool     burstOn_   = false;
  uint8_t  burstIdx_  = 0;
  int32_t  burstSum_  = 0;
  uint32_t burstT0_   = 0;    // micros() des ersten Abtastzeitpunkts
  uint32_t burstDrop_ = 0;    // wegen Zeitverzug verworfene Buendel

  void startBurst();
  void serviceBurst();
  void processSample(int16_t raw);
  void noteFailure();

  float median() const;
};

extern PHMeasurement phMeas;
