#include "Circulation.h"
#include "Settings.h"
#include "Config.h"

#include <WiFi.h>
#include <HTTPClient.h>

static CircState g_state    = CIRC_UNKNOWN;
static uint32_t  g_lastOkMs = 0;      // letzte gueltige Antwort (ON oder OFF)
static uint32_t  g_lastTryMs= 0;      // letzter Versuch, egal mit welchem Ausgang
static uint32_t  g_queries  = 0;
static bool      g_everTried = false;
static char      g_lastRaw[24] = "";  // zuletzt gelesener Zustandstext

CircState   circState()      { return g_state; }
uint32_t    circQueryCount() { return g_queries; }
const char *circRawState()   { return g_lastRaw; }

uint32_t circAgeS() {
  if (!g_lastOkMs) return 0xFFFFFFFF;
  return (millis() - g_lastOkMs) / 1000;
}

void circInvalidate() { g_lastOkMs = 0; g_lastTryMs = 0; }

const char *circStateText() {
  switch (g_state) {
    case CIRC_UNKNOWN:     return "unbekannt";
    case CIRC_ON:          return "Umwaelzung laeuft";
    case CIRC_OFF:         return "Umwaelzung steht";
    case CIRC_UNREACHABLE: return "Home Assistant nicht erreichbar";
  }
  return "?";
}

// ---------------------------------------------------------------------------
// Antwort auswerten
//
// Home Assistant liefert unter /api/states/<entity>:
//   {"entity_id":"switch.poolpumpe","state":"on","attributes":{...},...}
// Der Zustand steht in der Serialisierung vor den Attributen, das erste
// "state" ist also das gesuchte. Ein Textvergleich genuegt und spart die
// JSON-Bibliothek samt Speicher.
// ---------------------------------------------------------------------------
static bool parseState(const String &body, String &out) {
  int i = body.indexOf("\"state\"");
  if (i < 0) return false;
  int c = body.indexOf(':', i);
  if (c < 0) return false;
  int q1 = body.indexOf('"', c);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  out = body.substring(q1 + 1, q2);
  return out.length() > 0;
}

// Eine Abfrage durchfuehren. Blockiert bis zum Timeout - das ist vertretbar,
// weil sie nur bei stehender Pumpe und hoechstens einmal pro Dosierung
// passiert. Es gehen dabei keine Schritte verloren, weil die Pumpe in diesem
// Moment ohnehin steht; nur die Oberflaeche steht kurz.
static bool query(String &info) {
  g_lastTryMs = millis();
  g_everTried = true;
  g_queries++;

  if (WiFi.status() != WL_CONNECTED) {
    g_state = CIRC_UNREACHABLE;
    info = "kein WLAN";
    return false;
  }
  if (strlen(settings.haHost) == 0 || strlen(settings.haEntity) == 0) {
    g_state = CIRC_UNREACHABLE;
    info = "Adresse oder Entitaet fehlt";
    return false;
  }
  if (strlen(settings.haToken) == 0) {
    g_state = CIRC_UNREACHABLE;
    info = "kein Token hinterlegt";
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setReuse(false);

  String url = String("http://") + settings.haHost + "/api/states/" + settings.haEntity;
  if (!http.begin(url)) {
    g_state = CIRC_UNREACHABLE;
    info = "Adresse ungueltig";
    return false;
  }
  http.addHeader("Authorization", String("Bearer ") + settings.haToken);
  http.addHeader("Content-Type", "application/json");

  int code = http.GET();
  if (code != 200) {
    http.end();
    g_state = CIRC_UNREACHABLE;
    // Die haeufigsten Faelle beim Namen nennen statt nur einer Zahl.
    // 404 ist zweideutig: entweder kennt HA die Entitaet nicht, oder unter
    // der Adresse antwortet gar kein HA. Beides nennen, statt zu raten.
    if      (code == 401) info = "Token abgelehnt (401) - aber HA antwortet";
    else if (code == 404) info = "404: Entitaet unbekannt ODER kein HA unter "
                                 + String(settings.haHost);
    else                  info = "HTTP " + String(code);
    return false;
  }

  String body = http.getString();
  http.end();

  String st;
  if (!parseState(body, st)) {
    g_state = CIRC_UNREACHABLE;
    info = "Antwort unlesbar";
    return false;
  }

  strncpy(g_lastRaw, st.c_str(), sizeof(g_lastRaw) - 1);
  g_lastRaw[sizeof(g_lastRaw) - 1] = 0;

  // "unavailable" und "unknown" sind keine gueltigen Zustaende: HA kennt die
  // Entitaet dann zwar, weiss aber selbst nicht, was Sache ist. Das als "aus"
  // zu werten waere falsch - es ist schlicht keine Auskunft.
  if (st == "unavailable" || st == "unknown") {
    g_state = CIRC_UNREACHABLE;
    info = String(settings.haEntity) + " ist " + st;
    return false;
  }

  bool on = st.equalsIgnoreCase(settings.haOnState);
  g_state    = on ? CIRC_ON : CIRC_OFF;
  g_lastOkMs = millis();
  info       = String(settings.haEntity) + " = " + st;
  return true;
}

bool circProbe(String &info) {
  return query(info);
}

// ---------------------------------------------------------------------------
bool circAllowsDosing() {
  if (!settings.circEnabled) return true;          // Pruefung abgeschaltet

  // Frische Antwort aus dem Zwischenspeicher verwenden, statt neu zu fragen.
  if (g_lastOkMs && circAgeS() < settings.circFreshS) {
    return g_state == CIRC_ON;
  }

  // Nach einem Fehlversuch bzw. bei stehender Umwaelzung nicht sofort erneut
  // anklopfen - das ist die einzige Stelle, an der ueberhaupt wiederholt wird.
  if (g_everTried) {
    uint32_t waitS = (g_state == CIRC_UNREACHABLE) ? settings.circRetryS
                                                   : settings.circOffRetryS;
    if (millis() - g_lastTryMs < waitS * 1000UL) {
      return g_state == CIRC_ON;                   // alter Stand gilt weiter
    }
  }

  String info;
  query(info);
  return g_state == CIRC_ON;
}
