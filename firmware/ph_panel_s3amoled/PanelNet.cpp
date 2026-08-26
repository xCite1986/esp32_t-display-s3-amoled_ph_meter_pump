#include "PanelNet.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

PanelConfig panelCfg;

static PanelState     g_state;
static SemaphoreHandle_t g_mutex = nullptr;

static volatile bool  g_doseReq     = false;
static volatile bool  g_doseBusy    = false;
static volatile bool  g_resultNew   = false;
static volatile bool  g_resultOk    = false;
static char           g_resultMsg[120] = "";

static const uint32_t POLL_MS       = 2000;
static const uint16_t HTTP_TIMEOUT  = 4000;

static NetMode    g_mode = NM_CONNECTING;
static WebServer  cfgServer(80);
static bool       cfgServerUp = false;
static String     scanHtml;        // vorab gerenderte Netzliste fuer die Konfigseite

NetMode netMode()  { return g_mode; }
String  netApIp()  { return WiFi.softAPIP().toString(); }

void netLock()   { if (g_mutex) xSemaphoreTake(g_mutex, portMAX_DELAY); }
void netUnlock() { if (g_mutex) xSemaphoreGive(g_mutex); }
const PanelState &netState() { return g_state; }

float netDoseMl() {
  netLock();
  float ml = (panelCfg.revs * g_state.stepsPerRev) / g_state.stepsPerMl;
  netUnlock();
  return ml;
}

// ---------------------------------------------------------------------------
// Konfiguration
// ---------------------------------------------------------------------------
static void loadConfig() {
  Preferences p;
  p.begin("phpanel", true);
  strncpy(panelCfg.ssid, p.getString("ssid", "").c_str(), sizeof(panelCfg.ssid) - 1);
  strncpy(panelCfg.pass, p.getString("pass", "").c_str(), sizeof(panelCfg.pass) - 1);
  strncpy(panelCfg.host, p.getString("host", "ph-dosierung.local").c_str(), sizeof(panelCfg.host) - 1);
  strncpy(panelCfg.user, p.getString("user", "").c_str(), sizeof(panelCfg.user) - 1);
  strncpy(panelCfg.pw,   p.getString("pw", "").c_str(),   sizeof(panelCfg.pw) - 1);
  panelCfg.revs = p.getFloat("revs", 5.0f);
  p.end();

  if (panelCfg.revs <= 0 || panelCfg.revs > 20) panelCfg.revs = 5.0f;
  panelCfg.ssid[sizeof(panelCfg.ssid) - 1] = 0;
  panelCfg.pass[sizeof(panelCfg.pass) - 1] = 0;
  panelCfg.host[sizeof(panelCfg.host) - 1] = 0;
}

void netSaveConfig() {
  Preferences p;
  p.begin("phpanel", false);
  p.putString("ssid", panelCfg.ssid);
  p.putString("pass", panelCfg.pass);
  p.putString("host", panelCfg.host);
  p.putString("user", panelCfg.user);
  p.putString("pw",   panelCfg.pw);
  p.putFloat("revs",  panelCfg.revs);
  p.end();
}

void netFactoryReset() {
  Preferences p;
  p.begin("phpanel", false);
  p.clear();
  p.end();
}

String netWifiInfo() {
  if (g_mode == NM_AP)  return String("AP ") + netApIp();
  if (WiFi.status() != WL_CONNECTED) return "getrennt";
  return WiFi.localIP().toString() + "  " + String(WiFi.RSSI()) + " dBm";
}

// ---------------------------------------------------------------------------
// Access-Point-Fallback mit Konfigurationsseite
//
// Absicht: Ersteinrichtung und Netzwechsel gehen vom Handy aus - ohne Kabel und
// ohne serielle Konsole. Der AP geht immer dann auf, wenn keine SSID gespeichert
// ist oder die Verbindung nicht zustande kommt.
// ---------------------------------------------------------------------------
static String htmlEscape(const String &in) {
  String o;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if      (c == '&')  o += "&amp;";
    else if (c == '<')  o += "&lt;";
    else if (c == '>')  o += "&gt;";
    else if (c == '"')  o += "&quot;";
    else                o += c;
  }
  return o;
}

static void buildScanList() {
  int n = WiFi.scanNetworks(false, false);
  scanHtml = "";
  for (int i = 0; i < n && i < 20; i++) {
    String ss = WiFi.SSID(i);
    if (!ss.length()) continue;
    scanHtml += "<option value=\"" + htmlEscape(ss) + "\">" +
                htmlEscape(ss) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
}

static void handleRoot() {
  String h;
  h.reserve(2800);
  h += F("<!doctype html><html lang=de><head><meta charset=utf-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<title>pH-Panel Einrichtung</title><style>"
         "body{margin:0;background:#11151c;color:#e8edf4;font:16px/1.5 system-ui,sans-serif}"
         "main{max-width:460px;margin:0 auto;padding:18px}"
         "h1{font-size:20px;margin:0 0 4px}p.s{color:#93a1b5;font-size:14px;margin:0 0 18px}"
         "label{display:block;font-size:13px;color:#93a1b5;margin:14px 0 4px}"
         "input,select{width:100%;padding:11px;border-radius:8px;border:1px solid #2c3542;"
         "background:#1b212b;color:#e8edf4;font-size:16px;box-sizing:border-box}"
         "button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:8px;"
         "background:#4ea1ff;color:#06121f;font-size:17px;font-weight:600}"
         "a{color:#4ea1ff}</style></head><body><main>"
         "<h1>pH-Panel einrichten</h1>"
         "<p class=s>Netz waehlen, Passwort eingeben, speichern. "
         "Das Panel startet danach neu.</p><form method=POST action=/save>");

  h += F("<label>WLAN (nur 2,4 GHz)</label><select name=ssid>");
  if (scanHtml.length()) h += scanHtml;
  else                   h += F("<option value=''>-- kein Netz gefunden --</option>");
  h += F("</select>");

  h += F("<label>WLAN-Passwort</label><input name=pass type=password autocomplete=off>");
  h += "<label>Adresse der Dosieranlage</label><input name=host value=\"" +
       htmlEscape(panelCfg.host) + "\">";
  h += "<label>Umdrehungen pro Freigabe</label>"
       "<input name=revs type=number step=1 min=1 max=20 value=\"" +
       String(panelCfg.revs, 0) + "\">";
  h += F("<label>Login der Anlage (nur falls dort gesetzt)</label>"
         "<input name=user placeholder='Benutzer' autocomplete=off>"
         "<input name=apw type=password placeholder='Passwort' style='margin-top:8px' autocomplete=off>");
  h += F("<button type=submit>Speichern und neu starten</button></form>"
         "<p class=s style='margin-top:20px'><a href=/rescan>Netze erneut suchen</a></p>"
         "</main></body></html>");
  cfgServer.send(200, "text/html; charset=utf-8", h);
}

static void handleRescan() {
  buildScanList();
  cfgServer.sendHeader("Location", "/");
  cfgServer.send(303, "text/plain", "");
}

static void handleSave() {
  if (cfgServer.hasArg("ssid") && cfgServer.arg("ssid").length())
    strncpy(panelCfg.ssid, cfgServer.arg("ssid").c_str(), sizeof(panelCfg.ssid) - 1);
  if (cfgServer.hasArg("pass"))
    strncpy(panelCfg.pass, cfgServer.arg("pass").c_str(), sizeof(panelCfg.pass) - 1);
  if (cfgServer.hasArg("host") && cfgServer.arg("host").length())
    strncpy(panelCfg.host, cfgServer.arg("host").c_str(), sizeof(panelCfg.host) - 1);
  if (cfgServer.hasArg("user"))
    strncpy(panelCfg.user, cfgServer.arg("user").c_str(), sizeof(panelCfg.user) - 1);
  if (cfgServer.hasArg("apw"))
    strncpy(panelCfg.pw, cfgServer.arg("apw").c_str(), sizeof(panelCfg.pw) - 1);
  if (cfgServer.hasArg("revs")) {
    float r = cfgServer.arg("revs").toFloat();
    if (r >= 1 && r <= 20) panelCfg.revs = r;
  }
  netSaveConfig();

  cfgServer.send(200, "text/html; charset=utf-8",
    F("<!doctype html><meta charset=utf-8>"
      "<body style='background:#11151c;color:#e8edf4;font:16px system-ui;padding:24px'>"
      "<h2>Gespeichert</h2><p>Das Panel startet jetzt neu und verbindet sich.</p></body>"));
  delay(600);
  ESP.restart();
}

static void startAp() {
  WiFi.mode(WIFI_AP_STA);          // AP_STA, damit der Netzscan weiter funktioniert
  WiFi.softAP(PANEL_AP_SSID, PANEL_AP_PASS);
  g_mode = NM_AP;
  buildScanList();

  if (!cfgServerUp) {
    cfgServer.on("/", HTTP_GET, handleRoot);
    cfgServer.on("/save", HTTP_POST, handleSave);
    cfgServer.on("/rescan", HTTP_GET, handleRescan);
    cfgServer.onNotFound([]() {    // jede andere URL landet auf der Einrichtungsseite
      cfgServer.sendHeader("Location", "http://192.168.4.1/");
      cfgServer.send(302, "text/plain", "");
    });
    cfgServer.begin();
    cfgServerUp = true;
  }
  Serial.printf("[WiFi] Access Point \"%s\" aktiv, Passwort \"%s\"\n",
                PANEL_AP_SSID, PANEL_AP_PASS);
  Serial.printf("[WiFi] Einrichtung im Browser: http://%s/\n", netApIp().c_str());
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------
static void applyAuth(HTTPClient &http) {
  if (strlen(panelCfg.user) > 0) http.setAuthorization(panelCfg.user, panelCfg.pw);
}

static String baseUrl() {
  return String("http://") + panelCfg.host;
}

static void copyStr(char *dst, size_t len, const char *src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, len - 1);
  dst[len - 1] = 0;
}

static void pollStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    netLock(); g_state.online = false; netUnlock();
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(HTTP_TIMEOUT);
  http.setTimeout(HTTP_TIMEOUT);
  if (!http.begin(baseUrl() + "/api/status")) return;
  applyAuth(http);

  int code = http.GET();
  if (code != 200) {
    http.end();
    netLock();
    g_state.online = false;
    // Nach 15 s ohne Antwort gilt der Messwert nicht mehr als aktuell.
    if (millis() - g_state.lastOkMs > 15000) g_state.phValid = false;
    netUnlock();
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    netLock(); g_state.online = false; netUnlock();
    return;
  }

  netLock();
  g_state.online   = true;
  g_state.lastOkMs = millis();

  g_state.ph       = doc["ph"] | 7.0f;
  g_state.phValid  = doc["phValid"] | false;
  g_state.stable   = doc["stable"] | false;
  copyStr(g_state.phStatus, sizeof(g_state.phStatus), doc["phStatus"] | "-");

  g_state.setpoint = doc["cfg"]["sp"] | 7.20f;
  g_state.maxDaily = doc["cfg"]["maxd"] | 60.0f;
  g_state.today    = doc["dose"]["today"] | 0.0f;
  g_state.last24h  = doc["dose"]["last24h"] | 0.0f;

  copyStr(g_state.state, sizeof(g_state.state), doc["state"] | "-");
  copyStr(g_state.locks, sizeof(g_state.locks), doc["locks"] | "");
  g_state.autoOn   = doc["auto"]  | false;
  g_state.fault    = doc["fault"] | false;
  g_state.estop    = doc["estop"] | false;

  g_state.pumpRun    = doc["pump"]["run"] | false;
  g_state.pumpMl     = doc["pump"]["ml"] | 0.0f;
  g_state.pumpTarget = doc["pump"]["target"] | 0.0f;

  g_state.stepsPerMl  = doc["cfg"]["spml"]  | 1600.0f;
  g_state.stepsPerRev = doc["cfg"]["sprev"] | 3200.0f;
  if (g_state.stepsPerMl < 1) g_state.stepsPerMl = 1600.0f;
  netUnlock();
}

static void sendDose() {
  bool ok = false;
  String msg;

  if (WiFi.status() != WL_CONNECTED) {
    msg = "Kein WLAN";
  } else {
    HTTPClient http;
    http.setConnectTimeout(HTTP_TIMEOUT);
    http.setTimeout(HTTP_TIMEOUT);
    String url = baseUrl() + "/api/dose/revs?n=" + String(panelCfg.revs, 1);
    if (http.begin(url)) {
      applyAuth(http);
      int code = http.POST("");
      String body = http.getString();
      http.end();

      JsonDocument doc;
      if (!deserializeJson(doc, body)) {
        ok  = doc["ok"] | false;
        msg = String((const char *)(doc["msg"] | "?"));
      } else {
        msg = "Antwort unlesbar (HTTP " + String(code) + ")";
      }
    } else {
      msg = "Verbindung fehlgeschlagen";
    }
  }

  netLock();
  g_resultOk = ok;
  copyStr(g_resultMsg, sizeof(g_resultMsg), msg.c_str());
  g_resultNew = true;
  netUnlock();
}

// ---------------------------------------------------------------------------
// Task
// ---------------------------------------------------------------------------
static void netTask(void *) {
  uint32_t lastPoll = 0;
  for (;;) {
    if (g_mode == NM_AP) {
      cfgServer.handleClient();
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;                      // im Einrichtungsmodus wird nicht dosiert
    }

    if (g_doseReq) {
      g_doseReq  = false;
      g_doseBusy = true;
      sendDose();
      g_doseBusy = false;
      lastPoll = 0;             // direkt danach den Status neu holen
    }

    if (millis() - lastPoll >= POLL_MS) {
      lastPoll = millis();
      pollStatus();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void netRequestDose() { if (!g_doseBusy) g_doseReq = true; }
bool netDosePending() { return g_doseReq || g_doseBusy; }

bool netTakeResult(String &msg, bool &ok) {
  if (!g_resultNew) return false;
  netLock();
  msg = g_resultMsg;
  ok  = g_resultOk;
  g_resultNew = false;
  netUnlock();
  return true;
}

void netBegin() {
  g_mutex = xSemaphoreCreateMutex();
  loadConfig();

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.setHostname("ph-panel");

  if (strlen(panelCfg.ssid) == 0) {
    Serial.println("[WiFi] keine SSID gespeichert - starte Einrichtungs-AP");
    startAp();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(panelCfg.ssid, panelCfg.pass);
    Serial.printf("[WiFi] verbinde mit %s ", panelCfg.ssid);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      g_mode = NM_STA;
      WiFi.setAutoReconnect(true);
      Serial.printf("[WiFi] verbunden, IP %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WiFi] Verbindung fehlgeschlagen - starte Einrichtungs-AP");
      startAp();
    }
  }

  xTaskCreatePinnedToCore(netTask, "netTask", 8192, nullptr, 2, nullptr, 0);
}
