/*
  Automatische pH-Minus-Dosieranlage - ein Geraet
  ----------------------------------------------
  Hardware : LilyGo T-Display S3 AMOLED (1,91", BOARD_AMOLED_191)
             + ADS1115 + pH-Signalboard + TMC2209 + NEMA17
  Board    : ESP32S3 Dev Module, 16MB Flash, OPI PSRAM, USB-CDC an
  FQBN     : esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,
             CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB

  Bibliotheken: LilyGo-AMOLED-Series, lvgl 8.4, XPowersLib,
                SensorLib 0.3.3 (0.4.1 ist defekt - siehe docs/BEDIENPANEL.md)

  Derselbe Chip zeichnet das Display und steuert die Pumpe. Deshalb:
    - Schrittimpulse nicht blockierend, die Schrittzahl bleibt exakt
    - EN des TMC2209 haengt ueber R1 auf 3,3 V: bei Reset steht die Pumpe

  Pinbelegung siehe Config.h. Sicherheitsgrundsatz siehe PHController.h.
  Serielle Konsole: 115200 Baud, "help" eingeben.
*/

#include "Config.h"
#include "Settings.h"
#include "Ads1115.h"
#include "PHMeasurement.h"
#include "StepperPump.h"
#include "PHController.h"
#include "WebInterface.h"
#include "PanelUi.h"
#include "Circulation.h"
#include "History.h"

#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include <Wire.h>

LilyGo_Class amoled;

// ---------------------------------------------------------------------------
// Serielle Konsole
// ---------------------------------------------------------------------------
static void i2cScan() {
  Serial.printf("[I2C] Scan auf Wire1: SDA=%u SCL=%u\n", PIN_I2C_SDA, PIN_I2C_SCL);
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire1.beginTransmission(a);
    if (Wire1.endTransmission() == 0) {
      Serial.printf("       Geraet gefunden: 0x%02X\n", a);
      found++;
    }
  }
  if (!found) Serial.println("       nichts gefunden - Verdrahtung/Pullups pruefen");
  Serial.println("       (der Touchcontroller 0x15 haengt auf Wire, nicht hier)");
}

static void printHelp() {
  Serial.println(F(
    "\nBefehle:\n"
    "  help                  diese Liste\n"
    "  status                Kurzstatus\n"
    "  json                  vollstaendiger Status als JSON\n"
    "  scan                  I2C-Bus des ADS1115 scannen\n"
    "  mon [n]               n Sekunden Rohwerte im Sekundentakt\n"
    "  auto on|off           Automatik ein-/ausschalten\n"
    "  dose <ml>             manuelle Dosierung\n"
    "  revs <n>              Dosierung in Motorumdrehungen (1..20)\n"
    "  steps <n> [rev]       Servicelauf (Pumpenkalibrierung/Entlueften)\n"
    "  spml <schritte> <ml>  Schritte/ml aus Testlauf berechnen\n"
    "  stop | estop | clear  Pumpe anhalten / Not-Halt / quittieren\n"
    "  cal <a|b> <ph>        Kalibrierpunkt speichern\n"
    "  calreset              Kalibrierung verwerfen\n"
    "  set <key> <wert>      sp db dose maxs maxd pause phlock phmax spml\n"
    "                        sprev prevs srate sacc gain invdir hold\n"
    "                        filt avgs\n"
    "                        stby shft nite nfrom nto rot180\n"
    "                        circen circfr circrt circof\n"
    "  ha <host> <entity>    Home Assistant fuer die Umwaelzpruefung\n"
    "  hatoken <token>       Long-Lived Access Token hinterlegen\n"
    "  wifi <ssid> <pass>    WLAN speichern und neu starten\n"
    "  ap                    WLAN verwerfen, Einrichtungs-AP oeffnen\n"
    "  wake                  Display aufwecken\n"
    "  circ                  Umwaelzung jetzt bei Home Assistant abfragen\n"
    "  daily reset           Tageszaehler zuruecksetzen\n"
    "  reboot | factory      Neustart / Werkseinstellungen\n"));
}

static void printStatus() {
  float slope = phMeas.slopePerPh();
  char slopeTxt[24];
  if (isnan(slope)) strcpy(slopeTxt, "-");
  else              snprintf(slopeTxt, sizeof(slopeTxt), "%.1f mV/pH", slope);

  Serial.printf("pH %.2f (%s%s) | %.4f V | roh %d | Steilheit %s\n",
                phMeas.ph(), phMeas.statusText(),
                phMeas.stable() ? ", stabil" : ", schwankt",
                phMeas.voltage(), phMeas.rawAdc(), slopeTxt);
  Serial.printf("Zustand: %s | Sperren: %s\n",
                controller.stateText(), controller.lockText().c_str());
  Serial.printf("Soll %.2f | Automatik %s | Kalibrierung %s\n",
                settings.phSetpoint, settings.autoEnabled ? "EIN" : "AUS",
                settings.calValid ? "gueltig" : "FEHLT");
  Serial.printf("Heute %.2f/%.2f ml in %lu Dosierungen | Restpause %lu s | gesamt %.1f ml\n",
                settings.dailyMl, settings.maxDailyMl,
                (unsigned long)controller.doseCountToday(),
                (unsigned long)controller.pauseRemainingS(), settings.totalMl);
  Serial.printf("Letzte 24 h: %.2f ml | heute %.2f ml (Verlauf)\n",
                controller.ml24h(), histDayMl(0));
  Serial.printf("Mittelwert (%u s): %.3f %s | Filter %u s\n",
                settings.phAvgS, phMeas.phAverage(),
                phMeas.averageReady() ? "" : "(Fenster noch nicht voll)",
                settings.filterS);
  Serial.printf("Pumpe: %s (%lu/%lu Schritte) | %.1f Schritte/ml | Freigabe %.0f Umdr.\n",
                pump.running() ? "laeuft" : "steht",
                (unsigned long)pump.stepsDone(),
                (unsigned long)(pump.stepsDone() + pump.stepsRemaining()),
                settings.stepsPerMl, settings.panelRevs);
  Serial.printf("Anzeige: %s | Standby nach %u s | Wandern alle %u s | Nacht %s %u-%u Uhr\n",
                uiStateText(), settings.standbyS, settings.shiftS,
                settings.nightEnabled ? "ein" : "aus",
                settings.nightFrom, settings.nightTo);
  if (settings.circEnabled)
    Serial.printf("Umwaelzung: %s | %s = \"%s\" | %lu Abfragen seit Start\n",
                  circStateText(), settings.haEntity, circRawState(),
                  (unsigned long)circQueryCount());
  else
    Serial.println("Umwaelzung: nicht geprueft");
  Serial.printf("Netz: %s %s\n", web.modeText().c_str(), web.ip().c_str());
}

static uint32_t monUntil = 0;
static uint32_t monLast  = 0;

static void handleSet(const String &key, const String &val) {
  Settings &s = settings;
  float f = val.toFloat();
  long  i = val.toInt();
  bool  b = (val == "1" || val == "on" || val == "true");

  if      (key == "sp")      s.phSetpoint  = f;
  else if (key == "db")      s.phDeadband  = f;
  else if (key == "dose")    s.doseMl      = f;
  else if (key == "maxs")    s.maxSingleMl = f;
  else if (key == "maxd")    s.maxDailyMl  = f;
  else if (key == "pause")   s.pauseS      = (uint32_t)i;
  else if (key == "phlock")  s.phMinLock   = f;
  else if (key == "phmax")   s.phMaxPlaus  = f;
  else if (key == "spml")    s.stepsPerMl  = f;
  else if (key == "sprev")   s.stepsPerRev = f;
  else if (key == "prevs")   s.panelRevs   = f;
  else if (key == "srate")   s.stepRate    = f;
  else if (key == "sacc")    s.stepAccel   = f;
  else if (key == "gain")    s.adcGain     = (uint8_t)i;
  else if (key == "filt")    s.filterS     = (uint16_t)i;
  else if (key == "avgs")    s.phAvgS      = (uint16_t)i;
  else if (key == "invdir")  s.invertDir   = b;
  else if (key == "hold")    s.holdEnabled = b;
  else if (key == "stby")    s.standbyS    = (uint16_t)i;
  else if (key == "shft")    s.shiftS      = (uint16_t)i;
  else if (key == "nite")    s.nightEnabled= b;
  else if (key == "rot180")  { s.rot180 = b; s.save(); uiApplyRotation(); }
  else if (key == "nfrom")   s.nightFrom   = (uint8_t)i;
  else if (key == "nto")     s.nightTo     = (uint8_t)i;
  else if (key == "circen")  s.circEnabled = b;
  else if (key == "haon")    { strncpy(s.haOnState, val.c_str(), sizeof(s.haOnState) - 1);
                               s.haOnState[sizeof(s.haOnState) - 1] = 0; circInvalidate(); }
  else if (key == "circfr")  s.circFreshS  = (uint16_t)i;
  else if (key == "circrt")  s.circRetryS  = (uint16_t)i;
  else if (key == "circof")  s.circOffRetryS = (uint16_t)i;
  else { Serial.println("unbekannter Parameter: " + key); return; }

  s.save();
  phMeas.applySettings();
  Serial.println("gespeichert (nach Begrenzung auf zulaessige Werte)");
}

static void handleCommand(String line) {
  line.trim();
  if (!line.length()) return;

  String cmd = line, a1 = "", a2 = "";
  int sp = line.indexOf(' ');
  if (sp > 0) {
    cmd = line.substring(0, sp);
    String rest = line.substring(sp + 1); rest.trim();
    int sp2 = rest.indexOf(' ');
    if (sp2 > 0) { a1 = rest.substring(0, sp2); a2 = rest.substring(sp2 + 1); a2.trim(); }
    else a1 = rest;
  }
  cmd.toLowerCase();

  String err;
  if      (cmd == "help")   printHelp();
  else if (cmd == "status") printStatus();
  else if (cmd == "json")   Serial.println(web.statusJson());
  else if (cmd == "scan")   i2cScan();
  else if (cmd == "wake")   { uiWake(); Serial.println("Display geweckt"); }
  else if (cmd == "circ") {
    String info;
    bool ok = circProbe(info);
    Serial.println((ok ? "OK: " : "Fehler: ") + info);
  }
  else if (cmd == "ha") {
    if (a1.length()) strncpy(settings.haHost, a1.c_str(), sizeof(settings.haHost) - 1);
    if (a2.length()) strncpy(settings.haEntity, a2.c_str(), sizeof(settings.haEntity) - 1);
    settings.save();
    circInvalidate();
    Serial.printf("Home Assistant: %s | Entitaet: %s\n",
                  settings.haHost, settings.haEntity);
  }
  else if (cmd == "hatoken") {
    String rest = line.substring(line.indexOf(' ') + 1);
    rest.trim();
    if (!rest.length()) { Serial.println("Aufruf: hatoken <token>"); return; }
    strncpy(settings.haToken, rest.c_str(), sizeof(settings.haToken) - 1);
    settings.haToken[sizeof(settings.haToken) - 1] = 0;
    settings.save();
    circInvalidate();
    Serial.printf("Token gespeichert (%u Zeichen)\n", (unsigned)strlen(settings.haToken));
  }
  else if (cmd == "mon") {
    uint32_t n = a1.length() ? (uint32_t)a1.toInt() : 30;
    if (n > 3600) n = 3600;
    monUntil = millis() + n * 1000UL;
    Serial.println("Rohwertausgabe fuer " + String(n) + " s (beliebige Eingabe bricht ab)");
  }
  else if (cmd == "auto") {
    bool on = (a1 == "on" || a1 == "1");
    if (on && !settings.calValid) { Serial.println("abgelehnt: erst kalibrieren"); return; }
    settings.autoEnabled = on;
    settings.save();
    Serial.println(on ? "Automatik EIN" : "Automatik AUS");
  }
  else if (cmd == "dose") {
    if (controller.manualDose(a1.toFloat(), err)) Serial.println("dosiere " + a1 + " ml");
    else Serial.println("abgelehnt: " + err);
  }
  else if (cmd == "revs") {
    float n = a1.toFloat();
    if (n <= 0 || n > HARD_MAX_REVS) { Serial.println("Aufruf: revs <1..20>"); return; }
    float ml = (n * settings.stepsPerRev) / settings.stepsPerMl;
    if (controller.manualDose(ml, err))
      Serial.printf("%.1f Umdrehungen = %.2f ml\n", n, ml);
    else
      Serial.printf("abgelehnt: %s (%.2f ml)\n", err.c_str(), ml);
  }
  else if (cmd == "steps") {
    bool fwd = !(a2 == "rev" || a2 == "r");
    if (controller.servicePump((uint32_t)a1.toInt(), fwd, err))
      Serial.println("Servicelauf " + a1 + (fwd ? " Schritte vorwaerts" : " Schritte rueckwaerts"));
    else Serial.println("abgelehnt: " + err);
  }
  else if (cmd == "spml") {
    float st = a1.toFloat(), ml = a2.toFloat();
    if (st < 1 || ml <= 0) { Serial.println("Aufruf: spml <schritte> <ml>"); return; }
    float v = st / ml;
    if (v < HARD_MIN_STEPS_PER_ML || v > HARD_MAX_STEPS_PER_ML) {
      Serial.printf("unplausibel: %.1f Schritte/ml\n", v);
      return;
    }
    settings.stepsPerMl = v;
    settings.save();
    Serial.printf("neu: %.1f Schritte/ml (%.2f ul pro Schritt)\n", v, 1000.0f / v);
  }
  else if (cmd == "stop")  { pump.stop(); Serial.println("Pumpe gestoppt"); }
  else if (cmd == "estop") {
    settings.autoEnabled = false; settings.save();
    controller.emergencyStop();
    Serial.println("NOT-HALT aktiv, Automatik abgeschaltet");
  }
  else if (cmd == "clear") { controller.clearFault(); Serial.println("quittiert"); }
  else if (cmd == "cal") {
    bool pointA = !(a1 == "b" || a1 == "B");
    float ph = a2.length() ? a2.toFloat() : (pointA ? 7.00f : 4.00f);
    if (phMeas.calibratePoint(pointA, ph, err))
      Serial.printf("Punkt %s: pH %.2f = %.5f V %s\n", pointA ? "A" : "B",
                    ph, phMeas.voltage(), err.length() ? err.c_str() : "");
    else Serial.println("abgelehnt: " + err);
  }
  else if (cmd == "calreset") {
    settings.calValid = false; settings.autoEnabled = false; settings.save();
    Serial.println("Kalibrierung verworfen, Automatik aus");
  }
  else if (cmd == "set")   handleSet(a1, a2);
  else if (cmd == "wifi") {
    strncpy(settings.wifiSsid, a1.c_str(), sizeof(settings.wifiSsid) - 1);
    strncpy(settings.wifiPass, a2.c_str(), sizeof(settings.wifiPass) - 1);
    settings.save();
    Serial.println("WLAN gespeichert - Neustart");
    delay(200);
    ESP.restart();
  }
  else if (cmd == "ap") {
    settings.wifiSsid[0] = 0;
    settings.wifiPass[0] = 0;
    settings.save();
    Serial.println("WLAN verworfen - Neustart in den Einrichtungs-AP");
    delay(200);
    ESP.restart();
  }
  else if (cmd == "daily") { controller.resetDaily(); Serial.println("Tageszaehler zurueckgesetzt"); }
  else if (cmd == "reboot") { Serial.println("Neustart..."); delay(200); ESP.restart(); }
  else if (cmd == "factory") {
    settings.factoryReset();
    Serial.println("Werkseinstellungen wiederhergestellt - Neustart");
    delay(200);
    ESP.restart();
  }
  else Serial.println("unbekannt: " + cmd + "  (help)");
}

static void serialTask() {
  static String buf;
  static bool   lastWasCr = false;

  while (Serial.available()) {
    char c = Serial.read();

    // CR, LF und CRLF gelten alle als Zeilenende - Terminals sind sich da
    // nicht einig, und eine Konsole ohne Reaktion auf Enter wirkt kaputt.
    if (c == '\r' || c == '\n') {
      if (c == '\n' && lastWasCr) { lastWasCr = false; continue; }
      lastWasCr = (c == '\r');
      Serial.println();
      if (monUntil) { monUntil = 0; Serial.println("Rohwertausgabe beendet"); buf = ""; }
      else          { handleCommand(buf); buf = ""; }
      Serial.print("> ");
      continue;
    }
    lastWasCr = false;

    if (c == 8 || c == 127) {                   // Backspace
      if (buf.length()) { buf.remove(buf.length() - 1); Serial.print("\b \b"); }
      continue;
    }
    if (buf.length() < 120) { buf += c; Serial.print(c); }   // Echo
  }

  if (monUntil) {
    if ((int32_t)(millis() - monUntil) >= 0) { monUntil = 0; Serial.println("-- Ende --"); return; }
    if (millis() - monLast >= 1000) {
      monLast = millis();
      Serial.printf("roh %6d  U=%.5f V  gefiltert %.5f V  pH %.3f  Spanne %.3f  [%s]\n",
                    phMeas.rawAdc(), phMeas.voltageRaw(), phMeas.voltage(),
                    phMeas.ph(), phMeas.spread(), phMeas.statusText());
    }
  }
}

// ---------------------------------------------------------------------------
void setup() {
  // 1) ZUERST den Treiber sicher abschalten - noch vor allem anderen.
  pump.begin();

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.printf ("  %s  v%s\n", FW_NAME, FW_VERSION);
  Serial.println(F("=========================================="));

  settings.load();
  Serial.printf("[Cfg] Soll pH %.2f | Dosis %.2f ml | max %.1f ml/Tag | Pause %lu s\n",
                settings.phSetpoint, settings.doseMl, settings.maxDailyMl,
                (unsigned long)settings.pauseS);
  Serial.printf("[Cfg] %.1f Schritte/ml | Kalibrierung %s | Automatik %s\n",
                settings.stepsPerMl,
                settings.calValid ? "gueltig" : "FEHLT",
                settings.autoEnabled ? "EIN" : "AUS");

  if (!amoled.begin()) {
    Serial.println("[Display] Initialisierung fehlgeschlagen - Boardeinstellungen pruefen.");
    // Ohne Display laeuft die Anlage weiter: Messen und Dosieren haengen
    // nicht an der Anzeige, und die Bedienung geht ueber Web und Konsole.
  } else {
    Serial.printf("[Display] %d x %d, Touch: %s\n",
                  amoled.width(), amoled.height(),
                  amoled.hasTouch() ? "ja" : "NEIN");
    beginLvglHelper(amoled);
    uiBegin(amoled.width(), amoled.height());
  }

  if (!phMeas.begin()) {
    Serial.println("[ADS] ADS1115 nicht gefunden! Verdrahtung/Adresse pruefen.");
    i2cScan();
  } else {
    Serial.printf("[ADS] ADS1115 auf 0x%02X, Messbereich %s\n",
                  ADS_I2C_ADDR, Ads1115::gainName((AdsGain)settings.adcGain));
  }

  histBegin();
  controller.begin();
  web.begin();

  Serial.println(F("[Sys] bereit - 'help' fuer Befehle\n"));
  Serial.print("> ");
}

void loop() {
  pump.tick();        // hoechste Prioritaet: Schrittimpulse
  phMeas.tick();
  controller.tick();
  web.tick();

  lv_timer_handler();

  static uint32_t lastUi = 0;
  // Waehrend die Pumpe laeuft seltener zeichnen: jeder Bildaufbau
  // unterbricht die Schrittausgabe kurz.
  uint32_t uiPeriod = pump.running() ? 1000 : 400;
  if (millis() - lastUi >= uiPeriod) {
    lastUi = millis();
    uiRefresh();
  }

  uiTick();
  serialTask();
}
