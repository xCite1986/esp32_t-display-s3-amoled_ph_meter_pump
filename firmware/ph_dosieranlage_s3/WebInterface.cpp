#include "WebInterface.h"
#include "WebPage.h"
#include "Settings.h"
#include "PHMeasurement.h"
#include "StepperPump.h"
#include "PHController.h"
#include "PanelUi.h"
#include "Circulation.h"
#include "Config.h"

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <time.h>

WebInterface web;
static WebServer server(80);

// Zeitzone Mitteleuropa inkl. Sommerzeitregel
static const char *TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

static String jbool(bool b) { return b ? "true" : "false"; }

static String jstr(const String &s) {
  String o = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if ((uint8_t)c < 0x20) continue;
    else o += c;
  }
  o += "\"";
  return o;
}

static String jnum(float v, uint8_t dec = 3) {
  if (isnan(v) || isinf(v)) return "null";
  return String(v, (unsigned int)dec);
}

// ---------------------------------------------------------------------------
// WLAN
// ---------------------------------------------------------------------------
void WebInterface::startWifi() {
  WiFi.persistent(false);
  WiFi.setHostname(settings.hostname);

  if (strlen(settings.wifiSsid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.wifiSsid, settings.wifiPass);
    Serial.printf("[WiFi] verbinde mit %s", settings.wifiSsid);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      apMode_ = false;
      Serial.printf("[WiFi] verbunden, IP %s\n", WiFi.localIP().toString().c_str());
      configTzTime(TZ_INFO, "pool.ntp.org", "time.nist.gov");
      return;
    }
    Serial.println("[WiFi] Verbindung fehlgeschlagen - starte Access Point");
  } else {
    Serial.println("[WiFi] keine SSID konfiguriert - starte Access Point");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID_DEFAULT, AP_PASS_DEFAULT);
  apMode_ = true;
  Serial.printf("[WiFi] AP \"%s\" aktiv, IP %s\n",
                AP_SSID_DEFAULT, WiFi.softAPIP().toString().c_str());
}

String WebInterface::ip() const {
  return apMode_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

int WebInterface::rssi() const {
  return apMode_ ? 0 : WiFi.RSSI();
}

// ---------------------------------------------------------------------------
// Statusobjekt
// ---------------------------------------------------------------------------
String WebInterface::statusJson() const {
  const Settings &s = settings;
  String j;
  j.reserve(1600);

  j += "{";
  j += "\"fw\":" + jstr(FW_VERSION);
  j += ",\"up\":" + String(millis() / 1000);
  j += ",\"heap\":" + String((uint32_t)ESP.getFreeHeap());

  j += ",\"ph\":" + jnum(phMeas.ph(), 3);
  j += ",\"phValid\":" + jbool(phMeas.valid());
  j += ",\"phStatus\":" + jstr(phMeas.statusText());
  j += ",\"stable\":" + jbool(phMeas.stable());
  j += ",\"spread\":" + jnum(phMeas.spread(), 3);
  j += ",\"volt\":" + jnum(phMeas.voltage(), 5);
  j += ",\"voltRaw\":" + jnum(phMeas.voltageRaw(), 5);
  j += ",\"raw\":" + String(phMeas.rawAdc());
  j += ",\"slope\":" + jnum(phMeas.slopePerPh(), 2);

  j += ",\"cal\":{\"ok\":" + jbool(s.calValid) +
       ",\"phA\":" + jnum(s.calPhA, 2) + ",\"vA\":" + jnum(s.calVoltA, 5) +
       ",\"phB\":" + jnum(s.calPhB, 2) + ",\"vB\":" + jnum(s.calVoltB, 5) + "}";

  j += ",\"state\":" + jstr(controller.stateText());
  j += ",\"locks\":" + jstr(controller.lockText());
  j += ",\"lockBits\":" + String(controller.locks());
  j += ",\"auto\":" + jbool(s.autoEnabled);
  j += ",\"estop\":" + jbool(controller.estopActive());
  j += ",\"fault\":" + jbool(pump.timeoutFault());
  j += ",\"disp\":" + jstr(uiStateText());
  j += ",\"circ\":" + jstr(settings.circEnabled ? circStateText() : "nicht geprueft");
  j += ",\"circRaw\":" + jstr(circRawState());
  j += ",\"circN\":" + String(circQueryCount());

  j += ",\"pump\":{\"run\":" + jbool(pump.running()) +
       ",\"ml\":" + jnum(pump.mlDone(), 3) +
       ",\"target\":" + jnum(pump.mlTarget(), 3) +
       ",\"steps\":" + String(pump.stepsDone()) +
       ",\"remain\":" + String(pump.stepsRemaining()) + "}";

  uint32_t since = controller.secondsSinceLastDose();
  j += ",\"dose\":{\"today\":" + jnum(s.dailyMl, 3) +
       ",\"remain\":" + jnum(controller.dailyRemainingMl(), 3) +
       ",\"count\":" + String(controller.doseCountToday()) +
       ",\"last\":" + jnum(controller.lastDoseMl(), 3) +
       ",\"sinceS\":" + String(since == 0xFFFFFFFF ? 0 : since) +
       ",\"pauseS\":" + String(controller.pauseRemainingS()) +
       ",\"total\":" + jnum(s.totalMl, 2) +
       ",\"last24h\":" + jnum(controller.ml24h(), 3) + "}";

  j += ",\"wifi\":{\"mode\":" + jstr(modeText()) +
       ",\"ip\":" + jstr(ip()) +
       ",\"rssi\":" + String(rssi()) + "}";

  char tbuf[24] = "nicht synchron";
  time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm tmNow;
    localtime_r(&now, &tmNow);
    strftime(tbuf, sizeof(tbuf), "%d.%m.%Y %H:%M:%S", &tmNow);
  }
  j += ",\"time\":" + jstr(tbuf);

  // Konfiguration (ohne Passwoerter!)
  j += ",\"cfg\":{";
  j += "\"sp\":" + jnum(s.phSetpoint, 2);
  j += ",\"db\":" + jnum(s.phDeadband, 2);
  j += ",\"dose\":" + jnum(s.doseMl, 2);
  j += ",\"maxs\":" + jnum(s.maxSingleMl, 2);
  j += ",\"maxd\":" + jnum(s.maxDailyMl, 2);
  j += ",\"pause\":" + String(s.pauseS);
  j += ",\"phlock\":" + jnum(s.phMinLock, 2);
  j += ",\"phmax\":" + jnum(s.phMaxPlaus, 2);
  j += ",\"spml\":" + jnum(s.stepsPerMl, 1);
  j += ",\"sprev\":" + jnum(s.stepsPerRev, 0);
  j += ",\"prevs\":" + jnum(s.panelRevs, 0);
  j += ",\"stby\":" + String(s.standbyS);
  j += ",\"shft\":" + String(s.shiftS);
  j += ",\"nite\":" + jbool(s.nightEnabled);
  j += ",\"nfrom\":" + String(s.nightFrom);
  j += ",\"nto\":" + String(s.nightTo);
  j += ",\"circen\":" + jbool(s.circEnabled);
  j += ",\"hahost\":" + jstr(s.haHost);
  j += ",\"haent\":" + jstr(s.haEntity);
  j += ",\"haon\":" + jstr(s.haOnState);
  j += ",\"hatok\":" + jbool(strlen(s.haToken) > 0);   // nie den Token selbst
  j += ",\"circfr\":" + String(s.circFreshS);
  j += ",\"circrt\":" + String(s.circRetryS);
  j += ",\"circof\":" + String(s.circOffRetryS);
  j += ",\"srate\":" + jnum(s.stepRate, 0);
  j += ",\"sacc\":" + jnum(s.stepAccel, 0);
  j += ",\"invdir\":" + jbool(s.invertDir);
  j += ",\"hold\":" + jbool(s.holdEnabled);
  j += ",\"gain\":" + String(s.adcGain);
  j += ",\"ssid\":" + jstr(s.wifiSsid);
  j += ",\"host\":" + jstr(s.hostname);
  j += ",\"wuser\":" + jstr(s.webUser);
  j += "}";

  j += ",\"limits\":{\"maxSingle\":" + jnum(HARD_MAX_SINGLE_DOSE_ML, 1) +
       ",\"maxDaily\":" + jnum(HARD_MAX_DAILY_ML, 1) +
       ",\"minPh\":" + jnum(HARD_MIN_PH_LOCK, 2) + "}";
  j += "}";
  return j;
}

// ---------------------------------------------------------------------------
// Routen
// ---------------------------------------------------------------------------
bool WebInterface::guard() {
  if (strlen(settings.webUser) == 0) return true;
  if (server.authenticate(settings.webUser, settings.webPass)) return true;
  server.requestAuthentication();
  return false;
}

static void reply(bool ok, const String &msg) {
  server.send(ok ? 200 : 409, "application/json",
              String("{\"ok\":") + (ok ? "true" : "false") +
              ",\"msg\":" + jstr(msg) + "}");
}

static float argF(const char *name, float def) {
  if (!server.hasArg(name)) return def;
  return server.arg(name).toFloat();
}

static long argI(const char *name, long def) {
  if (!server.hasArg(name)) return def;
  return server.arg(name).toInt();
}

static bool argB(const char *name, bool def) {
  if (!server.hasArg(name)) return def;
  String v = server.arg(name);
  return (v == "1" || v == "true" || v == "on");
}

void WebInterface::setupRoutes() {
  server.on("/", HTTP_GET, [this]() {
    if (!guard()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  server.on("/api/status", HTTP_GET, [this]() {
    if (!guard()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", statusJson());
  });

  // --- Steuerung ---
  server.on("/api/auto", HTTP_POST, [this]() {
    if (!guard()) return;
    bool on = argB("on", false);
    if (on && !settings.calValid) { reply(false, "erst pH-Kalibrierung durchfuehren"); return; }
    settings.autoEnabled = on;
    settings.save();
    reply(true, on ? "Automatik ein" : "Automatik aus");
  });

  server.on("/api/dose", HTTP_POST, [this]() {
    if (!guard()) return;
    float ml = argF("ml", 0);
    String err;
    if (controller.manualDose(ml, err)) reply(true, "dosiere " + String(ml, 2) + " ml");
    else reply(false, err);
  });

  // Dosierung in Motorumdrehungen - laeuft bewusst durch manualDose(),
  // damit alle Sicherheitsgrenzen und die Mengenverbuchung greifen.
  server.on("/api/dose/revs", HTTP_POST, [this]() {
    if (!guard()) return;
    float n = argF("n", 0);
    if (n <= 0)              { reply(false, "Umdrehungen fehlen"); return; }
    if (n > HARD_MAX_REVS)   { reply(false, "ueber harte Grenze von " +
                                     String(HARD_MAX_REVS, 0) + " Umdrehungen"); return; }
    float ml = (n * settings.stepsPerRev) / settings.stepsPerMl;
    String err;
    if (controller.manualDose(ml, err))
      reply(true, String(n, 1) + " Umdr. = " + String(ml, 2) + " ml");
    else
      reply(false, err + " (" + String(ml, 2) + " ml)");
  });

  // I2C-Bus des ADS1115 scannen. Bei der Inbetriebnahme haengt das Geraet oft
  // schon im Netz, aber kein USB-Kabel mehr dran - dann ist das hier der
  // einzige Weg, zwischen "nichts da" und "falsche Adresse" zu unterscheiden.
  server.on("/api/i2c/scan", HTTP_POST, [this]() {
    if (!guard()) return;
    String found;
    uint8_t n = 0;
    for (uint8_t a = 1; a < 127; a++) {
      Wire1.beginTransmission(a);
      if (Wire1.endTransmission() == 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "0x%02X", a);
        if (n) found += ", ";
        found += buf;
        n++;
      }
    }
    if (!n) reply(false, String("nichts gefunden auf SDA=GPIO") + PIN_I2C_SDA +
                        " SCL=GPIO" + PIN_I2C_SCL +
                        " - Verdrahtung, Versorgung oder Chip defekt");
    else    reply(true, String(n) + " Geraet(e): " + found +
                        "  (erwartet 0x48)");
  });

  // Einmalige Abfrage der Entitaet - fuer die Einrichtung
  server.on("/api/circ/test", HTTP_POST, [this]() {
    if (!guard()) return;
    String info;
    bool ok = circProbe(info);
    reply(ok, info);
  });

  server.on("/api/stop", HTTP_POST, [this]() {
    if (!guard()) return;
    pump.stop();
    reply(true, "Pumpe gestoppt");
  });

  server.on("/api/estop", HTTP_POST, [this]() {
    if (!guard()) return;
    settings.autoEnabled = false;
    settings.save();
    controller.emergencyStop();
    reply(true, "NOT-HALT aktiv - Automatik abgeschaltet");
  });

  server.on("/api/clearfault", HTTP_POST, [this]() {
    if (!guard()) return;
    controller.clearFault();
    reply(true, "Stoerung quittiert");
  });

  server.on("/api/daily/reset", HTTP_POST, [this]() {
    if (!guard()) return;
    controller.resetDaily();
    reply(true, "Tageszaehler zurueckgesetzt");
  });

  // --- pH-Kalibrierung ---
  server.on("/api/cal", HTTP_POST, [this]() {
    if (!guard()) return;
    bool pointA = server.arg("point") != "b";
    float ph = argF("ph", pointA ? 7.00f : 4.00f);
    String err;
    if (phMeas.calibratePoint(pointA, ph, err)) {
      reply(true, err.length() ? err : "Punkt gespeichert");
    } else {
      reply(false, err);
    }
  });

  server.on("/api/cal/reset", HTTP_POST, [this]() {
    if (!guard()) return;
    settings.calValid    = false;
    settings.autoEnabled = false;
    settings.save();
    reply(true, "Kalibrierung verworfen, Automatik aus");
  });

  // --- Pumpenkalibrierung ---
  server.on("/api/pump/run", HTTP_POST, [this]() {
    if (!guard()) return;
    long steps = argI("steps", 0);
    bool fwd   = argB("dir", true);
    String err;
    if (steps <= 0) { reply(false, "Schrittzahl fehlt"); return; }
    if (controller.servicePump((uint32_t)steps, fwd, err))
      reply(true, String("fahre ") + steps + " Schritte");
    else reply(false, err);
  });

  server.on("/api/pump/calc", HTTP_POST, [this]() {
    if (!guard()) return;
    float steps = argF("steps", 0);
    float ml    = argF("ml", 0);
    if (steps < 1 || ml <= 0) { reply(false, "Schritte und ml angeben"); return; }
    float spml = steps / ml;
    if (spml < HARD_MIN_STEPS_PER_ML || spml > HARD_MAX_STEPS_PER_ML) {
      reply(false, "Ergebnis unplausibel: " + String(spml, 1) + " Schritte/ml");
      return;
    }
    settings.stepsPerMl = spml;
    settings.save();
    reply(true, String("neu: ") + String(spml, 1) + " Schritte/ml");
  });

  // --- Einstellungen ---
  server.on("/api/settings", HTTP_POST, [this]() {
    if (!guard()) return;
    Settings &s = settings;

    s.phSetpoint  = argF("sp", s.phSetpoint);
    s.phDeadband  = argF("db", s.phDeadband);
    s.doseMl      = argF("dose", s.doseMl);
    s.maxSingleMl = argF("maxs", s.maxSingleMl);
    s.maxDailyMl  = argF("maxd", s.maxDailyMl);
    s.pauseS      = (uint32_t)argI("pause", s.pauseS);
    s.phMinLock   = argF("phlock", s.phMinLock);
    s.phMaxPlaus  = argF("phmax", s.phMaxPlaus);

    s.stepsPerMl  = argF("spml", s.stepsPerMl);
    s.stepsPerRev = argF("sprev", s.stepsPerRev);
    s.panelRevs   = argF("prevs", s.panelRevs);
    s.standbyS    = (uint16_t)argI("stby", s.standbyS);
    s.shiftS      = (uint16_t)argI("shft", s.shiftS);
    s.nightEnabled= argB("nite", s.nightEnabled);
    s.nightFrom   = (uint8_t)argI("nfrom", s.nightFrom);
    s.nightTo     = (uint8_t)argI("nto", s.nightTo);
    s.circEnabled = argB("circen", s.circEnabled);
    if (server.hasArg("hahost"))
      strncpy(s.haHost, server.arg("hahost").c_str(), sizeof(s.haHost) - 1);
    if (server.hasArg("haent") && server.arg("haent").length())
      strncpy(s.haEntity, server.arg("haent").c_str(), sizeof(s.haEntity) - 1);
    if (server.hasArg("haon") && server.arg("haon").length())
      strncpy(s.haOnState, server.arg("haon").c_str(), sizeof(s.haOnState) - 1);
    // Leeres Feld heisst "unveraendert lassen" - sonst wuerde ein Speichern
    // in einem anderen Abschnitt den Token loeschen.
    if (server.hasArg("hatok") && server.arg("hatok").length())
      strncpy(s.haToken, server.arg("hatok").c_str(), sizeof(s.haToken) - 1);
    s.circFreshS  = (uint16_t)argI("circfr", s.circFreshS);
    s.circRetryS  = (uint16_t)argI("circrt", s.circRetryS);
    s.circOffRetryS = (uint16_t)argI("circof", s.circOffRetryS);
    circInvalidate();          // geaenderte Adresse sofort neu pruefen
    s.stepRate    = argF("srate", s.stepRate);
    s.stepAccel   = argF("sacc", s.stepAccel);
    s.invertDir   = argB("invdir", s.invertDir);
    s.holdEnabled = argB("hold", s.holdEnabled);

    uint8_t g = (uint8_t)argI("gain", s.adcGain);
    if (g != s.adcGain) { s.adcGain = g; }

    if (server.hasArg("ssid"))  strncpy(s.wifiSsid, server.arg("ssid").c_str(), sizeof(s.wifiSsid) - 1);
    if (server.hasArg("pass"))  strncpy(s.wifiPass, server.arg("pass").c_str(), sizeof(s.wifiPass) - 1);
    if (server.hasArg("host") && server.arg("host").length())
      strncpy(s.hostname, server.arg("host").c_str(), sizeof(s.hostname) - 1);
    if (server.hasArg("wuser")) strncpy(s.webUser, server.arg("wuser").c_str(), sizeof(s.webUser) - 1);
    if (server.hasArg("wpass")) strncpy(s.webPass, server.arg("wpass").c_str(), sizeof(s.webPass) - 1);

    s.save();
    phMeas.applySettings();
    reply(true, "gespeichert");
  });

  server.on("/api/reboot", HTTP_POST, [this]() {
    if (!guard()) return;
    reply(true, "Neustart...");
    delay(300);
    ESP.restart();
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "not found");
  });
}

// ---------------------------------------------------------------------------
void WebInterface::begin() {
  startWifi();
  setupRoutes();
  server.begin();

  if (MDNS.begin(settings.hostname)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[Web] erreichbar unter http://%s.local/\n", settings.hostname);
  }

  ArduinoOTA.setHostname(settings.hostname);
  if (strlen(settings.webPass) > 0) ArduinoOTA.setPassword(settings.webPass);
  ArduinoOTA.onStart([]() {
    // Waehrend eines OTA-Updates darf die Pumpe auf keinen Fall laufen.
    controller.emergencyStop();
  });
  ArduinoOTA.begin();
}

void WebInterface::tick() {
  server.handleClient();
  ArduinoOTA.handle();

  // STA-Reconnect alle 30 s versuchen, aber niemals im AP-Modus umschalten.
  if (!apMode_ && WiFi.status() != WL_CONNECTED && millis() - lastReconnect_ > 30000) {
    lastReconnect_ = millis();
    WiFi.reconnect();
  }
}
