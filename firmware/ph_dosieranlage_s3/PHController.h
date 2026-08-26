// PHController.h - Regel-Zustandsautomat, Sicherheitsverriegelungen, Zaehler
//
// Grundsatz (siehe Projektbeschreibung Abschnitt 18):
//   messen -> plausibilisieren -> kleine definierte Dosis -> warten/durchmischen
//   -> neu messen -> ggf. erneut dosieren
#pragma once

#include <Arduino.h>

enum CtrlState : uint8_t {
  ST_IDLE = 0,     // bereit, keine Aktion noetig
  ST_DOSING,       // Pumpe laeuft
  ST_MIXING,       // Wartezeit zur Durchmischung laeuft
  ST_LOCKED,       // Dosierung durch eine Verriegelung gesperrt
  ST_FAULT         // Fehler, manuelles Quittieren noetig
};

// Bitmaske der aktiven Sperrgruende
enum LockBits : uint16_t {
  LK_NONE       = 0,
  LK_AUTO_OFF   = 1 << 0,   // Automatik aus
  LK_SENSOR     = 1 << 1,   // Sensor/ADC-Fehler
  LK_NO_CALIB   = 1 << 2,   // nicht kalibriert
  LK_UNSTABLE   = 1 << 3,   // Messwert schwankt zu stark
  LK_PH_LOW     = 1 << 4,   // pH unter Sperrschwelle
  LK_PH_HIGH    = 1 << 5,   // pH unplausibel hoch
  LK_DAILY_MAX  = 1 << 6,   // Tagesmenge erschoepft
  LK_PAUSE      = 1 << 7,   // Mindestpause laeuft noch
  LK_PUMP_FAULT = 1 << 9,   // Pumpen-Laufzeitfehler
  LK_ESTOP      = 1 << 10,  // manueller Not-Halt aktiv
  LK_AT_TARGET  = 1 << 11   // Sollwert erreicht (kein Fehler)
};

class PHController {
 public:
  void begin();
  void tick();

  CtrlState state() const { return state_; }
  const char *stateText() const;
  uint16_t locks() const { return locks_; }
  String lockText() const;

  // Manuelle Dosierung in ml. Prueft Sicherheitsgrenzen, false + Grund bei Ablehnung.
  bool manualDose(float ml, String &err);
  // Reiner Motorlauf ohne Mengenverbuchung - nur fuer Pumpenkalibrierung/Entlueften.
  bool servicePump(uint32_t steps, bool forward, String &err);

  void emergencyStop();               // haelt an und verriegelt
  void clearEmergency();
  void clearFault();
  bool estopActive() const { return estop_; }

  float dailyMl() const;
  float dailyRemainingMl() const;
  uint32_t secondsSinceLastDose() const;
  uint32_t pauseRemainingS() const;
  float lastDoseMl() const { return lastDoseMl_; }
  // Echte rollierende 24-Stunden-Summe (unabhaengig vom Kalendertag-Limit)
  float ml24h() const;
  uint32_t doseCountToday() const { return doseCountToday_; }

  void resetDaily();

 private:
  CtrlState state_ = ST_IDLE;
  uint16_t  locks_ = LK_AUTO_OFF;
  bool      estop_ = false;
  bool      serviceRun_ = false;      // laufender Auftrag ist Service, nicht Dosis
  uint32_t  lastDoseEndMs_ = 0;
  bool      hadDose_ = false;
  float     lastDoseMl_ = 0;
  uint32_t  doseCountToday_ = 0;
  uint32_t  dayWindowStartMs_ = 0;    // Fallback ohne NTP
  uint32_t  lastCounterSave_ = 0;

  // Ringpuffer der letzten Dosierungen fuer die 24-h-Summe.
  // Bewusst mit Zeitstempel statt Stundeneimern - dadurch exakt und
  // unabhaengig davon, ob NTP schon synchron war.
  struct DoseEvent { uint32_t epoch; uint32_t upS; float ml; };
  static const uint8_t DOSE_LOG_SIZE = 48;
  DoseEvent doseLog_[DOSE_LOG_SIZE];
  uint8_t   doseLogIdx_ = 0;
  void loadDoseLog();
  void saveDoseLog();

  uint16_t evaluateLocks();
  void     checkDayRollover();
  void     bookDose(float ml);
  static uint32_t currentDayStamp();  // 0 = Zeit unbekannt
};

extern PHController controller;
