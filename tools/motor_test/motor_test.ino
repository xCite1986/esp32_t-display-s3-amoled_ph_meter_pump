/*
  Phase 2 - Motortest TMC2209 / NEMA17
  ------------------------------------
  Board : esp32:esp32:nologo_esp32c3_super_mini
  Zweck : Drehrichtung, Motorstrom und Microstepping pruefen, BEVOR der
          Pumpenkopf montiert wird.

  Verdrahtung: STEP=GPIO3, DIR=GPIO4, EN=GPIO7 (active LOW)

  WICHTIG
    - Motorstecker NIEMALS bei eingeschaltetem VMOT ziehen/stecken.
    - Erst 12 V einschalten, wenn der Motor fest verdrahtet ist.
    - VREF am TMC2209 vor dem ersten Lauf einstellen (siehe Loetanleitung).

  Serielle Befehle (115200 Baud):
    r <n>   n Schritte vorwaerts
    l <n>   n Schritte rueckwaerts
    s <hz>  Schrittrate setzen (Standard 800)
    e 0|1   Treiber aus/ein
    t       Testlauf: 1 Umdrehung vor, 1 Umdrehung zurueck
    m <n>   Schritte pro Umdrehung setzen (Standard 3200 = 1/16 Microstep)
*/

#include <Arduino.h>

static const uint8_t PIN_STEP = 3;
static const uint8_t PIN_DIR  = 4;
static const uint8_t PIN_EN   = 7;

static uint32_t rateHz      = 800;
static uint32_t stepsPerRev = 3200;

static void driver(bool on) {
  digitalWrite(PIN_EN, on ? LOW : HIGH);   // active low
}

static void run(uint32_t steps, bool forward) {
  digitalWrite(PIN_DIR, forward ? HIGH : LOW);
  delayMicroseconds(20);
  driver(true);
  delayMicroseconds(500);

  uint32_t half = 500000UL / rateHz;
  if (half < 3) half = 3;

  Serial.printf("fahre %lu Schritte %s bei %lu Hz ...\n",
                (unsigned long)steps, forward ? "vorwaerts" : "rueckwaerts",
                (unsigned long)rateHz);

  for (uint32_t i = 0; i < steps; i++) {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(half);
    digitalWrite(PIN_STEP, LOW);
    delayMicroseconds(half);
    if ((i & 0x3FF) == 0) yield();
  }
  driver(false);
  Serial.println("fertig, Treiber wieder aus");
}

void setup() {
  // Treiber SOFORT abschalten, bevor irgendetwas anderes passiert
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, HIGH);
  pinMode(PIN_STEP, OUTPUT);
  digitalWrite(PIN_STEP, LOW);
  pinMode(PIN_DIR, OUTPUT);
  digitalWrite(PIN_DIR, LOW);

  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Phase 2: TMC2209 Motortest ===");
  Serial.printf("STEP=GPIO%u DIR=GPIO%u EN=GPIO%u (EN aktiv LOW)\n",
                PIN_STEP, PIN_DIR, PIN_EN);
  Serial.printf("Rate %lu Hz, %lu Schritte/Umdrehung\n",
                (unsigned long)rateHz, (unsigned long)stepsPerRev);
  Serial.println("Befehle: r <n> | l <n> | s <hz> | m <n> | e 0|1 | t");
}

void loop() {
  static String buf;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') { if (buf.length() < 40) buf += c; continue; }

    buf.trim();
    if (!buf.length()) { continue; }
    char cmd = buf[0];
    long arg = buf.substring(1).toInt();

    switch (cmd) {
      case 'r': run(arg > 0 ? arg : stepsPerRev, true);  break;
      case 'l': run(arg > 0 ? arg : stepsPerRev, false); break;
      case 's': if (arg >= 50 && arg <= 20000) { rateHz = arg;
                  Serial.printf("Rate = %lu Hz\n", (unsigned long)rateHz); }
                else Serial.println("Rate 50..20000 Hz");
                break;
      case 'm': if (arg > 0) { stepsPerRev = arg;
                  Serial.printf("%lu Schritte/Umdrehung\n", (unsigned long)stepsPerRev); }
                break;
      case 'e': driver(arg != 0); Serial.println(arg ? "Treiber EIN" : "Treiber AUS"); break;
      case 't': run(stepsPerRev, true); delay(500); run(stepsPerRev, false); break;
      default:  Serial.println("Befehle: r <n> | l <n> | s <hz> | m <n> | e 0|1 | t");
    }
    buf = "";
  }
}
