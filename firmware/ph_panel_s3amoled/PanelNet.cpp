#include "PanelNet.h"

#include <WiFi.h>
#include <HTTPClient.h>
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
  if (WiFi.status() != WL_CONNECTED) return "getrennt";
  return WiFi.localIP().toString() + "  " + String(WiFi.RSSI()) + " dBm";
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
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname("ph-panel");
  if (strlen(panelCfg.ssid) > 0) {
    WiFi.begin(panelCfg.ssid, panelCfg.pass);
    Serial.printf("[WiFi] verbinde mit %s ...\n", panelCfg.ssid);
  } else {
    Serial.println("[WiFi] keine SSID gespeichert - 'wifi <ssid> <pass>' eingeben");
  }
  WiFi.setAutoReconnect(true);

  xTaskCreatePinnedToCore(netTask, "netTask", 8192, nullptr, 2, nullptr, 0);
}
