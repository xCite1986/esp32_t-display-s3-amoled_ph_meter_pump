/*
  Relaistest - Pumpe ueber 1-Kanal-Relais (AC-Synchronmotor)
  ----------------------------------------------------------
  Board : ESP32S3 Dev Module (T-Display S3 AMOLED)
  Zweck : Vor dem Anschluss der Netzseite pruefen, dass das Relais richtig
          herum schaltet und im Ruhezustand ABGEFALLEN ist.

  Verdrahtung: Relais-Signal S = GPIO10, Relais + = 5 V, Relais - = GND,
               10 kOhm Pulldown von S nach GND (haelt die Pumpe beim Booten aus).

  WICHTIG
    - Diesen Test mit ABGEZOGENER Pumpe / OHNE 230 V am Relaiskontakt fahren.
    - Nur LED und Klicken des Relais beobachten.

  Aktiv-LOW ist die Vorgabe (wie in der Firmware, settings.relayInvert = false).
  Viele KY-019-Boards (Header S/+/-, ohne Optokoppler) sind jedoch AKTIV-HIGH -
  dann mit dem Befehl 'inv' umschalten, bis 'off' das Relais wirklich abfallen
  laesst. Das Ergebnis (aktiv-LOW oder -HIGH) entspricht spaeter der
  Einstellung 'set rinv' in der Hauptfirmware.

  Serielle Befehle (115200 Baud):
    on        Relais an
    off       Relais aus
    p <s>     Puls: s Sekunden an, dann von selbst aus (z. B. p 3)
    inv       Schaltlogik umschalten (aktiv-LOW <-> aktiv-HIGH)
    status    aktuellen Zustand zeigen
*/

#include <Arduino.h>

static const uint8_t PIN_RELAY = 10;

static bool activeHigh = false;   // false = aktiv-LOW (Vorgabe)
static bool relayOn    = false;
static uint32_t pulseUntil = 0;

static void applyRelay(bool on) {
  uint8_t onLevel = activeHigh ? HIGH : LOW;
  digitalWrite(PIN_RELAY, on ? onLevel : !onLevel);
  relayOn = on;
}

static void printStatus() {
  Serial.printf("Relais: %s | Logik: %s | GPIO%u = %s\n",
                relayOn ? "AN" : "aus",
                activeHigh ? "aktiv-HIGH (set rinv 1)" : "aktiv-LOW (Vorgabe)",
                PIN_RELAY,
                digitalRead(PIN_RELAY) ? "HIGH" : "LOW");
}

void setup() {
  // Ruhepegel ZUERST - Relais darf beim Start nicht anziehen.
  pinMode(PIN_RELAY, OUTPUT);
  applyRelay(false);

  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Relaistest (Pumpe ueber Relais) ===");
  Serial.println("Pumpe abziehen / keine 230 V am Kontakt! Nur LED/Klicken pruefen.");
  Serial.println("Befehle: on | off | p <s> | inv | status");
  printStatus();
}

void loop() {
  if (pulseUntil && (int32_t)(millis() - pulseUntil) >= 0) {
    pulseUntil = 0;
    applyRelay(false);
    Serial.println("Puls beendet, Relais aus");
    printStatus();
  }

  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') { if (buf.length() < 40) buf += c; continue; }

    buf.trim();
    if (buf.length()) {
      String cmd = buf; String arg = "";
      int sp = buf.indexOf(' ');
      if (sp > 0) { cmd = buf.substring(0, sp); arg = buf.substring(sp + 1); arg.trim(); }
      cmd.toLowerCase();

      if      (cmd == "on")  { pulseUntil = 0; applyRelay(true);  Serial.println("Relais AN"); printStatus(); }
      else if (cmd == "off") { pulseUntil = 0; applyRelay(false); Serial.println("Relais aus"); printStatus(); }
      else if (cmd == "p") {
        float s = arg.toFloat();
        if (s <= 0 || s > 60) Serial.println("Aufruf: p <1..60>");
        else { applyRelay(true); pulseUntil = millis() + (uint32_t)(s * 1000.0f);
               Serial.printf("Puls %.1f s ...\n", s); }
      }
      else if (cmd == "inv") {
        activeHigh = !activeHigh;
        applyRelay(relayOn);   // Pegel an neue Logik anpassen
        Serial.println("Schaltlogik umgeschaltet");
        printStatus();
      }
      else if (cmd == "status") printStatus();
      else Serial.println("Befehle: on | off | p <s> | inv | status");
    }
    buf = "";
  }
}
