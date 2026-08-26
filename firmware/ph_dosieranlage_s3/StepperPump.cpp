#include "StepperPump.h"
#include "Settings.h"

StepperPump pump;

// Wie viele Schritte duerfen pro tick()-Aufruf maximal nachgeholt werden.
// Begrenzt die Blockierzeit im loop() und verhindert Puls-Bursts.
static const uint8_t MAX_STEPS_PER_TICK = 6;

void StepperPump::begin() {
  // EN ZUERST auf HIGH (= Treiber aus), bevor irgendetwas anderes passiert.
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, HIGH);
  driverOn_ = false;

  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);

  // PDN/UART bleibt hochohmig, solange kein UART genutzt wird.
  pinMode(PIN_TMC_PDN, INPUT);
}

void StepperPump::enableDriver(bool on) {
  digitalWrite(PIN_EN, on ? LOW : HIGH);   // active low
  driverOn_ = on;
}

bool StepperPump::startSteps(uint32_t steps, bool forward) {
  if (running_ || steps == 0) return false;

  bool dir = forward ? !settings.invertDir : settings.invertDir;
  digitalWrite(PIN_DIR, dir ? HIGH : LOW);
  delayMicroseconds(20);              // DIR-Setup-Zeit vor dem ersten STEP

  enableDriver(true);
  delayMicroseconds(200);             // Treiber-Einschaltzeit

  total_     = steps;
  remaining_ = steps;
  done_      = 0;
  targetMl_  = (float)steps / settings.stepsPerMl;
  speed_     = MIN_STEP_RATE;
  intervalUs_= (uint32_t)(1000000.0f / speed_);
  nextStepUs_= micros();
  startMs_   = millis();
  running_   = true;

  // Bremsweg: v^2 = v0^2 + 2*a*s  ->  s = (v^2 - v0^2) / (2a)
  float vmax = settings.stepRate;
  float a    = settings.stepAccel;
  float ds   = (vmax * vmax - MIN_STEP_RATE * MIN_STEP_RATE) / (2.0f * a);
  if (ds < 1) ds = 1;
  decelAt_ = (uint32_t)ds;
  if (decelAt_ > steps / 2) decelAt_ = steps / 2;   // symmetrisches Dreiecksprofil
  return true;
}

bool StepperPump::startMl(float ml, bool forward) {
  if (ml <= 0) return false;
  float steps = ml * settings.stepsPerMl;
  if (steps < 1) return false;
  if (steps > 4000000.0f) return false;
  return startSteps((uint32_t)lroundf(steps), forward);
}

void StepperPump::stop() {
  running_   = false;
  remaining_ = 0;
  digitalWrite(PIN_STEP, LOW);
  if (!settings.holdEnabled) enableDriver(false);
}

void StepperPump::pulse() {
  digitalWrite(PIN_STEP, HIGH);
  delayMicroseconds(4);     // TMC2209 braucht min. 100 ns - 4 us ist sicher
  digitalWrite(PIN_STEP, LOW);
}

void StepperPump::updateSpeed() {
  float a  = settings.stepAccel;
  float dt = (float)intervalUs_ / 1000000.0f;

  if (remaining_ <= decelAt_) {
    speed_ -= a * dt;                       // Bremsphase
    if (speed_ < MIN_STEP_RATE) speed_ = MIN_STEP_RATE;
  } else {
    speed_ += a * dt;                       // Beschleunigungsphase
    if (speed_ > settings.stepRate) speed_ = settings.stepRate;
  }
  if (speed_ < MIN_STEP_RATE) speed_ = MIN_STEP_RATE;
  intervalUs_ = (uint32_t)(1000000.0f / speed_);
  if (intervalUs_ < 60) intervalUs_ = 60;   // harte Obergrenze der Pulsrate
}

void StepperPump::tick() {
  if (!running_) return;

  // Harte Laufzeitbegrenzung - unabhaengig von allem anderen.
  if (millis() - startMs_ > HARD_MAX_PUMP_RUN_MS) {
    timeoutFault_ = true;
    stop();
    return;
  }

  uint32_t now = micros();
  uint8_t guard = 0;
  while (remaining_ > 0 && (int32_t)(now - nextStepUs_) >= 0) {
    pulse();
    remaining_--;
    done_++;
    updateSpeed();
    nextStepUs_ += intervalUs_;

    // Wenn wir stark hinterherhinken (z.B. WLAN-Stack hat gebremst), nicht
    // aufholen, sondern neu aufsetzen. Schrittzahl bleibt exakt.
    if ((int32_t)(now - nextStepUs_) > 5000) nextStepUs_ = now;
    if (++guard >= MAX_STEPS_PER_TICK) break;
  }

  if (remaining_ == 0) stop();
}

float StepperPump::mlDone() const {
  return (float)done_ / settings.stepsPerMl;
}

uint32_t StepperPump::runtimeMs() const {
  return running_ ? (millis() - startMs_) : 0;
}
