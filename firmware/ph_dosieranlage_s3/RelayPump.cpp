#include "RelayPump.h"
#include "Settings.h"

RelayPump pump;

// Ausgangspegel fuer "an": aktiv-LOW -> LOW schaltet das Relais, bei
// relayInvert -> aktiv-HIGH. "aus" ist jeweils der invertierte Pegel.
void RelayPump::write_(bool on) {
  uint8_t onLevel = settings.relayInvert ? HIGH : LOW;
  digitalWrite(PIN_PUMP_RELAY, on ? onLevel : !onLevel);
}

void RelayPump::begin() {
  // Ruhepegel ZUERST - noch vor allem anderen, damit die Pumpe nicht anlaeuft.
  // settings muss dafuer bereits geladen sein (siehe setup()-Reihenfolge).
  pinMode(PIN_PUMP_RELAY, OUTPUT);
  write_(false);
  running_ = false;
}

void RelayPump::applyIdle() {
  if (!running_) write_(false);      // nur im Ruhezustand nachziehen
}

bool RelayPump::run_(uint32_t ms, float targetMl) {
  if (running_ || ms == 0) return false;
  if (ms > HARD_MAX_PUMP_RUN_MS) ms = HARD_MAX_PUMP_RUN_MS;   // harte Laufzeitgrenze
  targetMs_    = ms;
  targetMl_    = targetMl;
  deliveredMl_ = 0;
  startMs_     = millis();
  running_     = true;
  write_(true);
  return true;
}

bool RelayPump::startMl(float ml) {
  if (ml <= 0) return false;
  float mlps = settings.mlPerSec;
  if (mlps < 1e-4f) return false;
  uint32_t ms = (uint32_t)(ml / mlps * 1000.0f);
  if (ms < HARD_MIN_DOSE_MS) ms = HARD_MIN_DOSE_MS;
  return run_(ms, ml);
}

bool RelayPump::startSeconds(float seconds) {
  if (seconds <= 0) return false;
  uint32_t ms = (uint32_t)(seconds * 1000.0f);
  // targetMl nur informativ - ein Servicelauf wird nicht als Dosis verbucht.
  return run_(ms, seconds * settings.mlPerSec);
}

void RelayPump::stop() {
  if (running_) deliveredMl_ = liveMl_();
  running_ = false;
  write_(false);
}

float RelayPump::liveMl_() const {
  uint32_t el = millis() - startMs_;
  if (el > targetMs_) el = targetMs_;
  float ml = (float)el / 1000.0f * settings.mlPerSec;
  return (ml > targetMl_) ? targetMl_ : ml;
}

float RelayPump::mlDone() const {
  return running_ ? liveMl_() : deliveredMl_;
}

uint32_t RelayPump::runS() const {
  return running_ ? (millis() - startMs_) / 1000 : 0;
}

void RelayPump::tick() {
  if (!running_) return;

  uint32_t el = millis() - startMs_;
  if (el >= targetMs_) { stop(); return; }        // Auftrag fertig

  // Reines Sicherheitsnetz: targetMs_ ist bereits geklemmt, dieser Zweig
  // sollte nie greifen. Tut er es doch, ist etwas grundlegend falsch.
  if (el >= HARD_MAX_PUMP_RUN_MS) { timeoutFault_ = true; stop(); }
}
