// RelayPump.h - AC-Synchronmotor ueber ein 1-Kanal-Relais, rein zeitgesteuert
//
// Kein Schrittmotor mehr: der Motor laeuft mit konstanter Drehzahl, sobald das
// Relais schliesst. Die Dosiermenge ergibt sich allein aus der Laufzeit:
//   Menge [ml] = Laufzeit [s] * Foerderrate settings.mlPerSec
// Die Foerderrate wird per Testlauf kalibriert (Pumpenkalibrierung).
//
// Sicherheit: der Ruhepegel wird in begin() ZUERST gesetzt, damit die Pumpe
// beim Booten nicht anlaeuft. Standard ist aktiv-LOW (Relais an bei GPIO LOW),
// per settings.relayInvert auf aktiv-HIGH umstellbar.
#pragma once

#include <Arduino.h>
#include "Config.h"

class RelayPump {
 public:
  void begin();
  void tick();

  // Dosierung starten (Laufzeit aus ml/mlPerSec). false, wenn schon eine laeuft.
  bool startMl(float ml);
  // Servicelauf fuer Entlueften/Kalibrieren - feste Laufzeit, keine Verbuchung.
  bool startSeconds(float seconds);

  void stop();                       // sofort anhalten (Not-Aus)
  void applyIdle();                  // Ruhepegel gemaess relayInvert neu setzen

  bool     running() const { return running_; }
  float    mlDone() const;           // gefoerderte Menge des laufenden/letzten Auftrags
  float    mlTarget() const { return targetMl_; }
  uint32_t runS() const;             // bisherige Laufzeit des Auftrags [s]
  float    targetS() const { return targetMs_ / 1000.0f; }
  bool     timeoutFault() const { return timeoutFault_; }
  void     clearFault() { timeoutFault_ = false; }

 private:
  bool     running_      = false;
  bool     timeoutFault_ = false;
  uint32_t startMs_      = 0;
  uint32_t targetMs_     = 0;        // Soll-Laufzeit [ms], bereits auf die harte Grenze geklemmt
  float    targetMl_     = 0;        // Soll-Menge (bei Servicelauf nur informativ)
  float    deliveredMl_  = 0;        // eingefrorene Menge nach dem Stopp

  bool run_(uint32_t ms, float targetMl);
  void write_(bool on);
  float liveMl_() const;
};

extern RelayPump pump;
