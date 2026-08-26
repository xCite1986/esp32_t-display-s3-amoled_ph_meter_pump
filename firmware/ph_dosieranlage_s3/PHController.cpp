#include "PHController.h"
#include "PHMeasurement.h"
#include "StepperPump.h"
#include "Settings.h"
#include "Circulation.h"
#include "Config.h"
#include <time.h>
#include <Preferences.h>

PHController controller;

void PHController::begin() {
  dayWindowStartMs_ = millis();
  memset(doseLog_, 0, sizeof(doseLog_));
  loadDoseLog();
  checkDayRollover();
}

// --- Ringpuffer der Dosierungen fuer die rollierende 24-h-Summe -------------

void PHController::loadDoseLog() {
  Preferences p;
  p.begin("phdos", true);
  size_t got = p.getBytes("dlog", doseLog_, sizeof(doseLog_));
  doseLogIdx_ = p.getUChar("dlogi", 0);
  p.end();
  if (got != sizeof(doseLog_)) memset(doseLog_, 0, sizeof(doseLog_));
  if (doseLogIdx_ >= DOSE_LOG_SIZE) doseLogIdx_ = 0;
}

void PHController::saveDoseLog() {
  Preferences p;
  p.begin("phdos", false);
  p.putBytes("dlog", doseLog_, sizeof(doseLog_));
  p.putUChar("dlogi", doseLogIdx_);
  p.end();
}

float PHController::ml24h() const {
  time_t now = time(nullptr);
  bool haveTime = (now > 1700000000);
  uint32_t upNow = millis() / 1000;
  float sum = 0;

  for (uint8_t i = 0; i < DOSE_LOG_SIZE; i++) {
    const DoseEvent &e = doseLog_[i];
    if (e.ml <= 0) continue;

    if (e.epoch != 0 && haveTime) {
      if ((uint32_t)now >= e.epoch && (uint32_t)now - e.epoch < 86400UL) sum += e.ml;
    } else {
      // Ohne gueltigen Zeitstempel wird ab Geraetestart gerechnet. Im Zweifel
      // wird die Dosis mitgezaehlt statt verschwiegen.
      if (upNow >= e.upS && upNow - e.upS < 86400UL) sum += e.ml;
    }
  }
  return sum;
}

uint32_t PHController::currentDayStamp() {
  time_t now = time(nullptr);
  if (now < 1700000000) return 0;          // NTP noch nicht synchron
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return (uint32_t)(tmNow.tm_year * 1000 + tmNow.tm_yday);
}

void PHController::checkDayRollover() {
  uint32_t ds = currentDayStamp();
  if (ds != 0) {
    if (settings.dayStamp != ds) {
      settings.dayStamp = ds;
      settings.dailyMl  = 0.0f;
      doseCountToday_   = 0;
      settings.saveCounters();
    }
    return;
  }
  // Kein NTP: rollierendes 24-h-Fenster ab Start bzw. ab letztem Rollover.
  if (millis() - dayWindowStartMs_ >= 86400000UL) {
    dayWindowStartMs_ += 86400000UL;
    settings.dailyMl = 0.0f;
    doseCountToday_  = 0;
    settings.saveCounters();
  }
}

float PHController::dailyMl() const { return settings.dailyMl; }

float PHController::dailyRemainingMl() const {
  float rem = settings.maxDailyMl - settings.dailyMl;
  return rem > 0 ? rem : 0.0f;
}

uint32_t PHController::secondsSinceLastDose() const {
  if (!hadDose_) return 0xFFFFFFFF;
  return (millis() - lastDoseEndMs_) / 1000;
}

uint32_t PHController::pauseRemainingS() const {
  if (!hadDose_) return 0;
  uint32_t el = (millis() - lastDoseEndMs_) / 1000;
  return el >= settings.pauseS ? 0 : settings.pauseS - el;
}

uint16_t PHController::evaluateLocks() {
  uint16_t l = LK_NONE;

  if (estop_)                 l |= LK_ESTOP;
  if (pump.timeoutFault())    l |= LK_PUMP_FAULT;
  if (!settings.autoEnabled)  l |= LK_AUTO_OFF;
  if (!settings.calValid)     l |= LK_NO_CALIB;

  PhStatus ps = phMeas.status();
  if (ps == PH_NO_SENSOR || ps == PH_VOLT_RANGE || ps == PH_WARMUP) l |= LK_SENSOR;
  if (ps == PH_PH_RANGE)                                            l |= LK_SENSOR;
  if (millis() - phMeas.lastGoodMs() > PH_SENSOR_TIMEOUT_MS)        l |= LK_SENSOR;

  if (phMeas.valid()) {
    if (!phMeas.stable())                    l |= LK_UNSTABLE;
    if (phMeas.ph() < settings.phMinLock)    l |= LK_PH_LOW;
    if (phMeas.ph() < HARD_MIN_PH_LOCK)      l |= LK_PH_LOW;
    if (phMeas.ph() > settings.phMaxPlaus)   l |= LK_PH_HIGH;
    if (phMeas.ph() <= settings.phSetpoint + settings.phDeadband) l |= LK_AT_TARGET;
  }

  if (dailyRemainingMl() <= 0.001f) l |= LK_DAILY_MAX;
  if (pauseRemainingS() > 0)        l |= LK_PAUSE;

  return l;
}

void PHController::bookDose(float ml) {
  settings.dailyMl += ml;
  settings.totalMl += ml;
  lastDoseMl_ = ml;
  doseCountToday_++;
  hadDose_ = true;
  settings.saveCounters();

  if (ml > 0) {
    time_t now = time(nullptr);
    DoseEvent &e = doseLog_[doseLogIdx_];
    e.epoch = (now > 1700000000) ? (uint32_t)now : 0;
    e.upS   = millis() / 1000;
    e.ml    = ml;
    doseLogIdx_ = (doseLogIdx_ + 1) % DOSE_LOG_SIZE;
    saveDoseLog();
  }
}

void PHController::tick() {
  checkDayRollover();
  locks_ = evaluateLocks();

  if (pump.timeoutFault()) {
    state_ = ST_FAULT;
    return;
  }

  if (pump.running()) {
    state_ = ST_DOSING;
    // Laufende Ueberwachung waehrend der Dosierung: ein Not-Halt bricht sofort
    // ab. Ein reiner Servicelauf ist davon ausgenommen.
    if (!serviceRun_ && (locks_ & LK_ESTOP)) {
      float done = pump.mlDone();
      pump.stop();
      lastDoseEndMs_ = millis();
      bookDose(done);
      state_ = ST_MIXING;
    }
    return;
  }

  // Pumpe steht: soeben beendeten Auftrag verbuchen
  if (state_ == ST_DOSING) {
    if (!serviceRun_) {
      lastDoseEndMs_ = millis();
      bookDose(pump.mlDone());
      state_ = ST_MIXING;
    } else {
      serviceRun_ = false;
      state_ = ST_IDLE;
    }
    return;
  }

  if (state_ == ST_MIXING && pauseRemainingS() > 0) return;

  // --- Automatikentscheidung ---
  const uint16_t hardBlock = LK_ESTOP | LK_PUMP_FAULT | LK_SENSOR | LK_NO_CALIB |
                             LK_UNSTABLE | LK_PH_LOW | LK_PH_HIGH |
                             LK_DAILY_MAX;
  const uint16_t softBlock = LK_AUTO_OFF | LK_PAUSE | LK_AT_TARGET;

  if (locks_ & hardBlock) { state_ = ST_LOCKED; return; }
  if (locks_ & softBlock) { state_ = ST_IDLE;   return; }

  // Letzte Instanz vor dem Start: laeuft die Umwaelzung? Diese Abfrage geht
  // ueber das Netz und steht deshalb bewusst ganz am Ende - erst wenn alles
  // andere bereits "dosieren" sagt. Damit liegen zwei Abfragen mindestens
  // eine Mindestpause auseinander.
  if (!circAllowsDosing()) {
    locks_ |= LK_NO_CIRC;
    state_ = ST_LOCKED;
    return;
  }

  // Alles frei: eine einzelne, begrenzte Dosis abgeben.
  float ml = settings.doseMl;
  if (ml > settings.maxSingleMl)    ml = settings.maxSingleMl;
  if (ml > HARD_MAX_SINGLE_DOSE_ML) ml = HARD_MAX_SINGLE_DOSE_ML;
  if (ml > dailyRemainingMl())      ml = dailyRemainingMl();
  if (ml < 0.05f) { state_ = ST_LOCKED; return; }

  serviceRun_ = false;
  if (pump.startMl(ml, true)) state_ = ST_DOSING;
}

bool PHController::manualDose(float ml, String &err) {
  if (pump.running())       { err = "Pumpe laeuft bereits"; return false; }
  if (estop_)               { err = "Not-Halt aktiv"; return false; }
  if (pump.timeoutFault())  { err = "Pumpenfehler - erst quittieren"; return false; }
  if (ml <= 0)              { err = "Menge muss groesser 0 sein"; return false; }

  if (ml > settings.maxSingleMl)    { err = "ueber max. Einzeldosis"; return false; }
  if (ml > HARD_MAX_SINGLE_DOSE_ML) { err = "ueber harte Sicherheitsgrenze"; return false; }
  checkDayRollover();
  if (ml > dailyRemainingMl())      { err = "Tagesmenge reicht nicht mehr"; return false; }

  // Liegt ein gueltiger Messwert vor, gilt die pH-Sperre auch manuell.
  if (phMeas.valid() && phMeas.ph() < settings.phMinLock) {
    err = "pH unter Sperrschwelle";
    return false;
  }

  // Auch von Hand wird nicht in stehendes Wasser dosiert.
  if (!circAllowsDosing()) {
    err = String("Umwaelzung: ") + circStateText();
    return false;
  }

  serviceRun_ = false;
  if (!pump.startMl(ml, true)) { err = "Pumpe konnte nicht gestartet werden"; return false; }
  state_ = ST_DOSING;
  return true;
}

bool PHController::servicePump(uint32_t steps, bool forward, String &err) {
  if (pump.running())      { err = "Pumpe laeuft bereits"; return false; }
  if (estop_)              { err = "Not-Halt aktiv"; return false; }
  if (pump.timeoutFault()) { err = "Pumpenfehler - erst quittieren"; return false; }
  if (steps == 0 || steps > 2000000UL) { err = "Schrittzahl unplausibel"; return false; }

  serviceRun_ = true;
  if (!pump.startSteps(steps, forward)) { err = "Start fehlgeschlagen"; return false; }
  state_ = ST_DOSING;
  return true;
}

void PHController::emergencyStop() {
  estop_ = true;
  if (pump.running()) {
    bool wasService = serviceRun_;
    float done = pump.mlDone();
    pump.stop();
    if (!wasService) { lastDoseEndMs_ = millis(); bookDose(done); }
  }
  pump.enableDriver(false);
  state_ = ST_LOCKED;
}

void PHController::clearEmergency() { estop_ = false; }

void PHController::clearFault() {
  pump.clearFault();
  estop_ = false;
  if (state_ == ST_FAULT) state_ = ST_IDLE;
}

void PHController::resetDaily() {
  memset(doseLog_, 0, sizeof(doseLog_));
  doseLogIdx_ = 0;
  saveDoseLog();
  settings.dailyMl  = 0.0f;
  doseCountToday_   = 0;
  dayWindowStartMs_ = millis();
  settings.dayStamp = currentDayStamp();
  settings.saveCounters();
}

const char *PHController::stateText() const {
  switch (state_) {
    case ST_IDLE:   return "Bereit";
    case ST_DOSING: return "Dosiert";
    case ST_MIXING: return "Durchmischung";
    case ST_LOCKED: return "Gesperrt";
    case ST_FAULT:  return "Stoerung";
  }
  return "?";
}

String PHController::lockText() const {
  if (locks_ == LK_NONE) return "keine";
  String s;
  struct LockName { uint16_t bit; const char *txt; };
  static const LockName map[] = {
    { LK_ESTOP,      "Not-Halt" },
    { LK_NO_CIRC,    "keine Umwaelzung" },
    { LK_PUMP_FAULT, "Pumpen-Laufzeitfehler" },
    { LK_AUTO_OFF,   "Automatik aus" },
    { LK_NO_CALIB,   "nicht kalibriert" },
    { LK_SENSOR,     "Sensorfehler" },
    { LK_UNSTABLE,   "Messwert instabil" },
    { LK_PH_LOW,     "pH unter Sperrschwelle" },
    { LK_PH_HIGH,    "pH unplausibel hoch" },
    { LK_DAILY_MAX,  "Tagesmenge erreicht" },
    { LK_PAUSE,      "Mindestpause laeuft" },
    { LK_AT_TARGET,  "Sollwert erreicht" },
  };
  for (const LockName &m : map) {
    if (locks_ & m.bit) { if (s.length()) s += ", "; s += m.txt; }
  }
  return s;
}
