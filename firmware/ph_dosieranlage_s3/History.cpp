#include "History.h"
#include <Preferences.h>
#include <time.h>

static HistSlot hist[HIST_SLOTS];
static uint32_t curHour   = 0;       // Stundennummer des offenen Slots
static double   sumPh     = 0;       // Zwischensummen der laufenden Stunde
static uint32_t cntPh     = 0;
static bool     dirty     = false;
static uint32_t lastSaveMs = 0;

static const char *NS = "phhist";

// Ohne gueltige Uhrzeit ergibt eine Stundennummer keinen Sinn - dann wird
// nichts aufgezeichnet. Lieber eine Luecke als ein falsch einsortierter Wert.
static uint32_t nowHour() {
  time_t t = time(nullptr);
  if (t < 1700000000) return 0;
  return (uint32_t)(t / 3600);
}

static int slotOf(uint32_t hour) { return (int)(hour % HIST_SLOTS); }

void histClear() {
  memset(hist, 0, sizeof(hist));
  for (int i = 0; i < HIST_SLOTS; i++) hist[i].phAvg = -1;
  curHour = 0; sumPh = 0; cntPh = 0;
  dirty = true;
}

static void load() {
  Preferences p;
  p.begin(NS, true);
  size_t got = p.getBytes("h", hist, sizeof(hist));
  curHour = p.getULong("cur", 0);
  p.end();
  if (got != sizeof(hist)) histClear();
}

static void save() {
  Preferences p;
  p.begin(NS, false);
  p.putBytes("h", hist, sizeof(hist));
  p.putULong("cur", curHour);
  p.end();
  dirty = false;
  lastSaveMs = millis();
}

void histBegin() {
  load();
  curHour = nowHour();
  sumPh = 0; cntPh = 0;
}

// Offenen Slot abschliessen: Mittelwert festschreiben.
static void closeSlot() {
  if (!curHour || cntPh == 0) return;
  HistSlot &s = hist[slotOf(curHour)];
  s.phAvg = (int16_t)lroundf((float)(sumPh / cntPh) * 100.0f);
  dirty = true;
}

static void openSlot(uint32_t hour) {
  HistSlot &s = hist[slotOf(hour)];
  // Slot gehoert noch zu einer aelteren Woche: zuruecksetzen statt fortschreiben
  if (s.hour != hour) {
    s.hour  = hour;
    s.phAvg = -1;
    s.phMin = 0;
    s.phMax = 0;
    s.ml10  = 0;
  }
  curHour = hour;
  sumPh = 0;
  cntPh = 0;
  dirty = true;
}

void histTick() {
  uint32_t h = nowHour();
  if (!h) return;                          // ohne NTP keine Aufzeichnung

  if (h != curHour) {
    closeSlot();
    openSlot(h);
    save();                                // einmal pro Stunde ins NVS
    return;
  }
  // Sicherheitsnetz: auch ohne Stundenwechsel gelegentlich sichern
  if (dirty && millis() - lastSaveMs > 900000UL) save();
}

void histAddSample(float ph) {
  if (!curHour) return;
  if (ph < 0 || ph > 14) return;

  HistSlot &s = hist[slotOf(curHour)];
  if (s.hour != curHour) openSlot(curHour);

  int16_t v = (int16_t)lroundf(ph * 100.0f);
  if (cntPh == 0) { s.phMin = v; s.phMax = v; }
  else {
    if (v < s.phMin) s.phMin = v;
    if (v > s.phMax) s.phMax = v;
  }
  sumPh += ph;
  cntPh++;

  // Laufenden Mittelwert gleich mitfuehren, damit die aktuelle Stunde im
  // Chart nicht als Luecke erscheint.
  s.phAvg = (int16_t)lroundf((float)(sumPh / cntPh) * 100.0f);
  dirty = true;
}

void histAddDose(float ml) {
  if (!curHour || ml <= 0) return;
  HistSlot &s = hist[slotOf(curHour)];
  if (s.hour != curHour) openSlot(curHour);

  uint32_t add = (uint32_t)s.ml10 + (uint32_t)lroundf(ml * 10.0f);
  s.ml10 = (add > 65535) ? 65535 : (uint16_t)add;
  dirty = true;
  save();                                  // Dosierungen sofort sichern
}

float histDayMl(uint8_t daysAgo) {
  uint32_t h = nowHour();
  if (!h) return 0;
  uint32_t endH   = h - (uint32_t)daysAgo * 24;
  uint32_t startH = (endH >= 23) ? endH - 23 : 0;
  float sum = 0;
  for (uint32_t x = startH; x <= endH; x++) {
    const HistSlot &s = hist[slotOf(x)];
    if (s.hour == x) sum += s.ml10 / 10.0f;
  }
  return sum;
}

// Kompaktes JSON: drei gleich lange Arrays plus die Stundennummer des ersten
// Eintrags. Leere Stunden erscheinen als null, damit der Chart Luecken zeigen
// kann statt sie zu ueberbruecken.
String histJson() {
  uint32_t h = nowHour();
  String j;
  j.reserve(4200);
  j += "{\"hour\":" + String(h);
  j += ",\"slots\":" + String(HIST_SLOTS);

  if (!h) {                                 // ohne Uhrzeit gibt es nichts
    j += ",\"ph\":[],\"min\":[],\"max\":[],\"ml\":[]}";
    return j;
  }

  uint32_t first = (h >= HIST_SLOTS - 1) ? h - (HIST_SLOTS - 1) : 0;
  j += ",\"first\":" + String(first);

  const char *keys[4] = { "ph", "min", "max", "ml" };
  for (uint8_t k = 0; k < 4; k++) {
    j += ",\"" + String(keys[k]) + "\":[";
    for (uint32_t x = first; x <= h; x++) {
      if (x != first) j += ',';
      const HistSlot &s = hist[slotOf(x)];
      bool have = (s.hour == x);
      if (k == 3) {                          // Dosiermenge: 0 ist ein Wert
        j += have ? String(s.ml10 / 10.0f, 1) : String("0");
      } else if (!have || s.phAvg < 0) {
        j += "null";
      } else {
        int16_t v = (k == 0) ? s.phAvg : (k == 1 ? s.phMin : s.phMax);
        j += String(v / 100.0f, 2);
      }
    }
    j += ']';
  }
  j += '}';
  return j;
}
