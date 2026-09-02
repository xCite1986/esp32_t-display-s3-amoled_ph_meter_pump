#include "PHMeasurement.h"
#include "Settings.h"
#include <Wire.h>

PHMeasurement phMeas;

bool PHMeasurement::begin() {
  Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.setClock(100000);
  bool ok = ads_.begin(ADS_I2C_ADDR);
  applySettings();
  status_ = ok ? PH_WARMUP : PH_NO_SENSOR;
  return ok;
}

void PHMeasurement::applySettings() {
  ads_.setGain((AdsGain)settings.adcGain);

  // Zeitkonstante in einen EMA-Faktor umrechnen: alpha = 1 - e^(-Ts/tau).
  float ts  = PH_SAMPLE_PERIOD_MS / 1000.0f;
  float tau = settings.filterS;
  if (tau < 0.1f) tau = 0.1f;
  emaAlpha_ = 1.0f - expf(-ts / tau);
  if (emaAlpha_ > 1.0f) emaAlpha_ = 1.0f;
  if (emaAlpha_ < 0.0005f) emaAlpha_ = 0.0005f;
}

// Alle PH_AVG_PERIOD_MS einen Wert in den Ringpuffer, daraus den Mittelwert
// ueber das eingestellte Fenster. Der Puffer haelt bis zu 60 Minuten vor.
void PHMeasurement::updateAverage() {
  uint32_t now = millis();
  if (lastAvgMs_ && now - lastAvgMs_ < PH_AVG_PERIOD_MS) return;
  lastAvgMs_ = now;

  avgBuf_[avgIdx_] = ph_;
  avgIdx_ = (avgIdx_ + 1) % PH_AVG_SLOTS;
  if (avgUsed_ < PH_AVG_SLOTS) avgUsed_++;

  uint16_t want = (uint16_t)((uint32_t)settings.phAvgS * 1000UL / PH_AVG_PERIOD_MS);
  if (want < 1) want = 1;
  if (want > PH_AVG_SLOTS) want = PH_AVG_SLOTS;

  uint16_t n = (avgUsed_ < want) ? avgUsed_ : want;
  double sum = 0;
  for (uint16_t i = 0; i < n; i++) {
    uint16_t idx = (avgIdx_ + PH_AVG_SLOTS - 1 - i) % PH_AVG_SLOTS;
    sum += avgBuf_[idx];
  }
  avgPh_ = (float)(sum / n);
  // Erst wenn das Fenster wirklich gefuellt ist, taugt der Mittelwert als
  // Grundlage fuer eine Dosierung.
  avgReady_ = (n >= want);
}

float PHMeasurement::median() const {
  if (bufCount_ == 0) return 0;
  float tmp[PH_SAMPLE_COUNT];
  for (uint8_t i = 0; i < bufCount_; i++) tmp[i] = buf_[i];
  // Insertion Sort - bei max. 15 Werten voellig ausreichend
  for (uint8_t i = 1; i < bufCount_; i++) {
    float k = tmp[i];
    int8_t j = i - 1;
    while (j >= 0 && tmp[j] > k) { tmp[j + 1] = tmp[j]; j--; }
    tmp[j + 1] = k;
  }
  return tmp[bufCount_ / 2];
}

float PHMeasurement::voltToPh(float v) const {
  const Settings &s = settings;
  float dv = s.calVoltB - s.calVoltA;
  if (fabsf(dv) < 0.001f) return NAN;
  float slope = (s.calPhB - s.calPhA) / dv;      // pH pro Volt
  return s.calPhA + (v - s.calVoltA) * slope;
}

float PHMeasurement::slopePerPh() const {
  const Settings &s = settings;
  // Ohne gueltige Kalibrierung waere das nur der Default-Platzhalter -
  // der darf nicht wie ein echter Messwert aussehen.
  if (!s.calValid) return NAN;
  float dph = s.calPhB - s.calPhA;
  if (fabsf(dph) < 0.01f) return NAN;
  return (s.calVoltB - s.calVoltA) * 1000.0f / dph;  // mV pro pH
}

// Ein Messzyklus besteht jetzt aus einem Buendel von PH_BURST_SAMPLES
// Wandlungen ueber genau eine Netzperiode. Das Buendel laeuft ueber viele
// Schleifendurchlaeufe verteilt ab - blockierend waeren 20 ms am Stueck genau
// das, was die Schrittausgabe der Pumpe nicht vertraegt.
void PHMeasurement::tick() {
  if (burstOn_) { serviceBurst(); return; }

  uint32_t now = millis();
  if (now - lastSample_ < PH_SAMPLE_PERIOD_MS) return;
  lastSample_ = now;
  startBurst();
}

void PHMeasurement::noteFailure() {
  if (failCount_ < 250) failCount_++;
  if (failCount_ >= 3) {
    status_ = PH_NO_SENSOR;
    // I2C-Bus neu anstossen - haeufigste Ursache ist ein kurzer Wackler
    if (failCount_ % 10 == 0) ads_.begin(ADS_I2C_ADDR);
  }
}

void PHMeasurement::startBurst() {
  if (!ads_.startContinuous(ADS_CH_PH)) { noteFailure(); return; }
  burstOn_  = true;
  burstIdx_ = 0;
  burstSum_ = 0;
  burstT0_  = micros() + PH_BURST_SETTLE_US;
}

// Holt die faellig gewordenen Abtastwerte. Die Abstaende sind so gewaehlt,
// dass die Summe genau eine Netzperiode ueberdeckt.
void PHMeasurement::serviceBurst() {
  const uint32_t step = PH_MAINS_PERIOD_US / PH_BURST_SAMPLES;

  while (burstIdx_ < PH_BURST_SAMPLES) {
    uint32_t due = burstT0_ + (uint32_t)burstIdx_ * step;
    if ((int32_t)(micros() - due) < 0) return;      // noch nicht faellig

    int16_t r = 0;
    if (!ads_.readContinuous(r)) { burstOn_ = false; noteFailure(); return; }
    burstSum_ += r;
    burstIdx_++;
  }

  burstOn_ = false;

  // Hat die Schleife zwischendurch zu lange gebraucht, sind die Abtastpunkte
  // nicht mehr gleichmaessig verteilt und die Netzunterdrueckung stimmt nicht.
  // Dann lieber gar kein Wert als ein falsch gemittelter.
  if (micros() - burstT0_ > 2 * PH_MAINS_PERIOD_US) {
    burstDrop_++;
    return;
  }

  failCount_ = 0;
  processSample((int16_t)(burstSum_ / PH_BURST_SAMPLES));
}

void PHMeasurement::processSample(int16_t raw) {
  uint32_t now = millis();
  rawAdc_  = raw;
  voltRaw_ = ads_.toVolts(raw);

  buf_[bufIdx_] = voltRaw_;
  bufIdx_ = (bufIdx_ + 1) % PH_SAMPLE_COUNT;
  if (bufCount_ < PH_SAMPLE_COUNT) bufCount_++;

  float med = median();
  if (!emaInit_) { voltage_ = med; emaInit_ = true; }
  else           { voltage_ += emaAlpha_ * (med - voltage_); }

  // Stabilitaet: Spanne der Rohspannungen im Fenster, in pH umgerechnet
  float vmin = buf_[0], vmax = buf_[0];
  for (uint8_t i = 1; i < bufCount_; i++) {
    if (buf_[i] < vmin) vmin = buf_[i];
    if (buf_[i] > vmax) vmax = buf_[i];
  }

  // Spannungsspanne immer fuehren - sie sagt etwas ueber die Messkette aus,
  // auch ohne Kalibrierung. Genau vor dem Kalibrieren wird sie gebraucht.
  spreadV_ = vmax - vmin;

  // --- Statusbewertung ---
  if (bufCount_ < PH_SAMPLE_COUNT) { status_ = PH_WARMUP; return; }

  if (voltage_ < VOLT_PLAUS_MIN || voltage_ > VOLT_PLAUS_MAX) {
    status_ = PH_VOLT_RANGE;
    return;
  }

  // Ab hier liefert der Sensor brauchbare Werte. Das ist die richtige Stelle
  // fuer den Frische-Zeitstempel: der Waechter in PHController ueberwacht den
  // SENSOR. Ob kalibriert ist, sperrt ohnehin separat ueber LK_NO_CALIB -
  // vorher stand hier faelschlich "Sensorfehler" bei intaktem Sensor.
  lastGood_ = now;

  if (!settings.calValid) {
    ph_ = voltToPh(voltage_);          // Anzeige trotzdem berechnen
    status_ = PH_NO_CALIB;
    return;
  }

  float p = voltToPh(voltage_);
  if (isnan(p)) { status_ = PH_NO_CALIB; return; }
  ph_ = p;

  float pmin = voltToPh(vmin), pmax = voltToPh(vmax);
  spread_ = fabsf(pmax - pmin);
  stable_ = (spread_ <= PH_STABLE_BAND);

  if (ph_ < PH_PLAUS_MIN || ph_ > PH_PLAUS_MAX) {
    status_ = PH_PH_RANGE;
    return;
  }

  status_   = PH_OK;
  lastGood_ = now;
  updateAverage();
}

bool PHMeasurement::calibratePoint(bool pointA, float phValue, String &err) {
  if (!ads_.present())             { err = "ADS1115 nicht erreichbar"; return false; }
  if (bufCount_ < PH_SAMPLE_COUNT) { err = "Messung noch nicht eingeschwungen"; return false; }
  if (voltage_ < VOLT_PLAUS_MIN || voltage_ > VOLT_PLAUS_MAX) {
    err = "Spannung ausserhalb des plausiblen Bereichs";
    return false;
  }
  float vmin = buf_[0], vmax = buf_[0];
  for (uint8_t i = 1; i < bufCount_; i++) {
    if (buf_[i] < vmin) vmin = buf_[i];
    if (buf_[i] > vmax) vmax = buf_[i];
  }
  if ((vmax - vmin) > 0.020f) { err = "Messwert noch nicht stabil (>20 mV Spanne)"; return false; }
  if (phValue < 0.0f || phValue > 14.0f) { err = "pH-Wert des Puffers unplausibel"; return false; }

  if (pointA) { settings.calPhA = phValue; settings.calVoltA = voltage_; }
  else        { settings.calPhB = phValue; settings.calVoltB = voltage_; }

  // Gueltig, sobald beide Punkte sinnvoll auseinanderliegen
  bool ok = fabsf(settings.calPhA - settings.calPhB) >= 0.5f &&
            fabsf(settings.calVoltA - settings.calVoltB) >= 0.010f;
  settings.calValid = ok;
  settings.save();
  emaInit_ = false;   // Filter neu einschwingen lassen
  if (!ok) err = "Erster Punkt gespeichert - zweiter Punkt fehlt noch";
  return true;
}

const char *PHMeasurement::statusText() const {
  switch (status_) {
    case PH_OK:         return "OK";
    case PH_NO_SENSOR:  return "ADS1115 nicht erreichbar";
    case PH_NO_CALIB:   return "nicht kalibriert";
    case PH_VOLT_RANGE: return "Spannung unplausibel";
    case PH_PH_RANGE:   return "pH unplausibel";
    case PH_WARMUP:     return "Messung schwingt ein";
  }
  return "?";
}
