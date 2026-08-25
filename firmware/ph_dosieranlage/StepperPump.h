// StepperPump.h - TMC2209 ueber STEP/DIR/EN, nicht blockierend, mit Rampe
//
// Wichtig: die Dosiergenauigkeit haengt an der EXAKTEN Schrittzahl, nicht an
// perfektem Timing. Deshalb wird nicht blockierend im loop() gepulst - wenn
// WLAN/Webserver kurz bremsen, wird die Pumpe minimal langsamer, aber sie
// foerdert exakt die gleiche Menge.
#pragma once

#include <Arduino.h>
#include "Config.h"

class StepperPump {
 public:
  void begin();
  void tick();

  // Dosierung starten. Gibt false zurueck, wenn bereits eine laeuft.
  bool startSteps(uint32_t steps, bool forward = true);
  bool startMl(float ml, bool forward = true);

  void stop();                       // sofort anhalten (Not-Aus)
  void enableDriver(bool on);

  bool     running() const { return running_; }
  uint32_t stepsRemaining() const { return remaining_; }
  uint32_t stepsDone() const { return done_; }
  float    mlDone() const;           // gefoerderte Menge des laufenden Auftrags
  float    mlTarget() const { return targetMl_; }
  uint32_t runtimeMs() const;
  bool     timeoutFault() const { return timeoutFault_; }
  void     clearFault() { timeoutFault_ = false; }

 private:
  bool     running_    = false;
  bool     driverOn_   = false;
  bool     timeoutFault_ = false;
  uint32_t remaining_  = 0;
  uint32_t total_      = 0;
  uint32_t done_       = 0;
  uint32_t decelAt_    = 0;          // ab dieser Restschrittzahl bremsen
  float    targetMl_   = 0;
  float    speed_      = MIN_STEP_RATE;   // aktuelle Schritte/s
  uint32_t intervalUs_ = 1000;
  uint32_t nextStepUs_ = 0;
  uint32_t startMs_    = 0;

  void pulse();
  void updateSpeed();
};

extern StepperPump pump;
