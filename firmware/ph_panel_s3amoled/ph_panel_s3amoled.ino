/*
  Bedienpanel fuer die pH-Minus-Dosieranlage
  ------------------------------------------
  Hardware : LilyGo T-Display S3 AMOLED (536 x 240, Touch)
  Board    : esp32:esp32:esp32s3  ("ESP32S3 Dev Module")
             Flash 16MB, PSRAM OPI, USB-Mode "Hardware CDC and JTAG"
  Bibliotheken: LilyGo-AMOLED-Series, lvgl 8.4, ArduinoJson 7

  Das Panel ist reine Anzeige und Fernbedienung. Es enthaelt bewusst KEINE
  eigene Dosierlogik: es fragt den ESP32-C3 der Anlage ueber dessen JSON-API
  ab und schickt Dosieranforderungen dorthin. Saemtliche Sicherheitsgrenzen
  (Einzeldosis, Tagesmenge, pH-Sperre, Umwaelzung, Mindestpause) werden von
  der Anlage geprueft - ein Fehler oder Ausfall des Panels kann keine
  Dosierung ausloesen.

  Bedienung: Tippen auf das Display oeffnet die Rueckfrage, erst der
  zweite Tipp auf FREIGEBEN loest die Dosierung aus.

  Serielle Konsole: 115200 Baud, "help" eingeben.
*/

#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <lvgl.h>
#include <WiFi.h>

#include "PanelNet.h"
#include "PanelUi.h"

LilyGo_Class amoled;

// ---------------------------------------------------------------------------
// Serielle Konsole
// ---------------------------------------------------------------------------
static void printHelp() {
  Serial.println(F(
    "\nBefehle:\n"
    "  help                  diese Liste\n"
    "  status                Verbindung und letzte Werte\n"
    "  wifi <ssid> <pass>    WLAN speichern und neu starten\n"
    "  ap                    Einrichtungs-AP erzwingen (Konfig per Handy)\n"
    "  host <name|ip>        Adresse der Dosieranlage (Standard ph-dosierung.local)\n"
    "  auth <user> <pass>    Zugangsdaten, falls die Anlage ein Login hat\n"
    "  noauth                Login-Daten loeschen\n"
    "  revs <n>              Umdrehungen pro Freigabe (1..20, Standard 5)\n"
    "  reboot                Neustart\n"
    "  factory               Panel-Einstellungen loeschen\n"));
}

static void printStatus() {
  netLock();
  PanelState s = netState();
  netUnlock();

  Serial.printf("WLAN   : %s (%s)\n", netWifiInfo().c_str(), panelCfg.ssid);
  Serial.printf("Anlage : %s -> %s\n", panelCfg.host, s.online ? "erreichbar" : "KEINE ANTWORT");
  if (s.online) {
    Serial.printf("pH     : %.2f (%s) | Soll %.2f\n", s.ph, s.phStatus, s.setpoint);
    Serial.printf("Mengen : 24 h %.2f ml | heute %.2f / %.2f ml\n",
                  s.last24h, s.today, s.maxDaily);
    Serial.printf("Zustand: %s | Sperren: %s\n", s.state, s.locks);
    Serial.printf("Faktor : %.0f Schritte/Umdr., %.1f Schritte/ml\n",
                  s.stepsPerRev, s.stepsPerMl);
  }
  Serial.printf("Freigabe: %.0f Umdrehungen = ca. %.2f ml\n", panelCfg.revs, netDoseMl());
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

  if      (cmd == "help")   printHelp();
  else if (cmd == "status") printStatus();
  else if (cmd == "wifi") {
    strncpy(panelCfg.ssid, a1.c_str(), sizeof(panelCfg.ssid) - 1);
    strncpy(panelCfg.pass, a2.c_str(), sizeof(panelCfg.pass) - 1);
    netSaveConfig();
    Serial.println("WLAN gespeichert - Neustart");
    delay(200); ESP.restart();
  }
  else if (cmd == "host") {
    if (!a1.length()) { Serial.println("Aufruf: host <name|ip>"); return; }
    strncpy(panelCfg.host, a1.c_str(), sizeof(panelCfg.host) - 1);
    netSaveConfig();
    Serial.println("Adresse gespeichert: " + String(panelCfg.host));
  }
  else if (cmd == "auth") {
    strncpy(panelCfg.user, a1.c_str(), sizeof(panelCfg.user) - 1);
    strncpy(panelCfg.pw,   a2.c_str(), sizeof(panelCfg.pw) - 1);
    netSaveConfig();
    Serial.println("Zugangsdaten gespeichert");
  }
  else if (cmd == "noauth") {
    panelCfg.user[0] = 0; panelCfg.pw[0] = 0;
    netSaveConfig();
    Serial.println("Zugangsdaten geloescht");
  }
  else if (cmd == "revs") {
    float n = a1.toFloat();
    if (n < 1 || n > 20) { Serial.println("Aufruf: revs <1..20>"); return; }
    panelCfg.revs = n;
    netSaveConfig();
    Serial.printf("%.0f Umdrehungen = ca. %.2f ml\n", n, netDoseMl());
  }
  else if (cmd == "ap") {
    // WLAN-Daten verwerfen; netBegin() oeffnet danach den Einrichtungs-AP
    panelCfg.ssid[0] = 0;
    panelCfg.pass[0] = 0;
    netSaveConfig();
    Serial.println("WLAN-Daten geloescht - Neustart in den Einrichtungs-AP");
    delay(200); ESP.restart();
  }
  else if (cmd == "reboot") { Serial.println("Neustart..."); delay(200); ESP.restart(); }
  else if (cmd == "factory") {
    netFactoryReset();
    Serial.println("Panel-Einstellungen geloescht - Neustart");
    delay(200); ESP.restart();
  }
  else Serial.println("unbekannt: " + cmd + "  (help)");
}

static void serialTask() {
  static String buf;
  static bool lastWasCr = false;
  while (Serial.available()) {
    char c = Serial.read();

    // CR, LF und CRLF gelten alle als Zeilenende. Terminals sind sich da nicht
    // einig - und eine Konsole, die auf Enter nicht reagiert, wirkt kaputt.
    if (c == '\r' || c == '\n') {
      if (c == '\n' && lastWasCr) { lastWasCr = false; continue; }
      lastWasCr = (c == '\r');
      Serial.println();
      handleCommand(buf);
      buf = "";
      Serial.print("> ");
      continue;
    }
    lastWasCr = false;

    if (c == 8 || c == 127) {                     // Backspace
      if (buf.length()) { buf.remove(buf.length() - 1); Serial.print("\b \b"); }
      continue;
    }
    // Echo, damit sichtbar ist was ankommt
    if (buf.length() < 120) { buf += c; Serial.print(c); }
  }
}

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);

  Serial.println();
  Serial.println(F("=========================================="));
  Serial.println(F("  pH-Dosieranlage - Bedienpanel v1.0.0"));
  Serial.println(F("=========================================="));

  if (!amoled.begin()) {
    while (true) {
      Serial.println("Display konnte nicht initialisiert werden!");
      delay(2000);
    }
  }
  Serial.printf("[Display] %d x %d, Touch: %s\n",
                amoled.width(), amoled.height(),
                amoled.hasTouch() ? "ja" : "NEIN");
  if (!amoled.hasTouch()) {
    Serial.println("[Display] Ohne Touch laesst sich nur anzeigen, nicht dosieren.");
  }

  beginLvglHelper(amoled);

  netBegin();
  uiBegin(amoled.width(), amoled.height());

  Serial.printf("[Cfg] Anlage: %s | Freigabe: %.0f Umdrehungen\n",
                panelCfg.host, panelCfg.revs);
  Serial.println(F("[Sys] bereit - 'help' fuer Befehle\n"));
  Serial.print("> ");
}

void loop() {
  lv_timer_handler();

  static uint32_t lastRefresh = 0;
  if (millis() - lastRefresh >= 500) {
    lastRefresh = millis();
    uiRefresh();
  }

  uiTick();
  serialTask();
  delay(5);
}
