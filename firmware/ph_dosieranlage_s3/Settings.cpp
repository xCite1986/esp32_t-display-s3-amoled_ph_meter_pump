#include "Settings.h"
#include <Preferences.h>

Settings settings;

static Preferences prefs;
static const char *NS = "phdos";

static float clampf(float v, float lo, float hi) {
  if (isnan(v)) return lo;
  return v < lo ? lo : (v > hi ? hi : v);
}

void Settings::clampAll() {
  stepsPerMl  = clampf(stepsPerMl, HARD_MIN_STEPS_PER_ML, HARD_MAX_STEPS_PER_ML);
  panelRevs   = clampf(panelRevs, 0.5f, HARD_MAX_REVS);

  if (standbyS < STANDBY_MIN_S) standbyS = STANDBY_MIN_S;
  if (standbyS > STANDBY_MAX_S) standbyS = STANDBY_MAX_S;
  if (shiftS   < SHIFT_MIN_S)   shiftS   = SHIFT_MIN_S;
  if (shiftS   > SHIFT_MAX_S)   shiftS   = SHIFT_MAX_S;
  if (nightFrom > 23) nightFrom = 20;
  if (nightTo   > 23) nightTo   = 5;

  if (circFreshS   < 10)  circFreshS   = 10;
  if (circFreshS   > 3600) circFreshS  = 3600;
  if (circRetryS   < 15)  circRetryS   = 15;
  if (circRetryS   > 3600) circRetryS  = 3600;
  if (circOffRetryS < 15) circOffRetryS = 15;
  if (circOffRetryS > 3600) circOffRetryS = 3600;
  haHost[sizeof(haHost) - 1]     = 0;
  haToken[sizeof(haToken) - 1]   = 0;
  haEntity[sizeof(haEntity) - 1] = 0;
  haOnState[sizeof(haOnState) - 1] = 0;
  if (strlen(haOnState) == 0) strcpy(haOnState, "on");
  // Ohne Adresse, Entitaet oder Token laesst sich nichts pruefen - dann bleibt
  // die Verriegelung aus, statt jede Dosierung zu blockieren.
  if (strlen(haHost) == 0 || strlen(haEntity) == 0 || strlen(haToken) == 0)
    circEnabled = false;
  stepsPerRev = clampf(stepsPerRev, 200.0f, 51200.0f);
  stepRate    = clampf(stepRate, MIN_STEP_RATE, MAX_STEP_RATE);
  stepAccel   = clampf(stepAccel, 200.0f, 100000.0f);

  phSetpoint  = clampf(phSetpoint, 6.0f, 8.5f);
  phDeadband  = clampf(phDeadband, 0.01f, 1.0f);
  phMinLock   = clampf(phMinLock, HARD_MIN_PH_LOCK, 7.5f);
  phMaxPlaus  = clampf(phMaxPlaus, 8.0f, PH_PLAUS_MAX);

  maxSingleMl = clampf(maxSingleMl, 0.1f, HARD_MAX_SINGLE_DOSE_ML);
  doseMl      = clampf(doseMl, 0.1f, maxSingleMl);
  maxDailyMl  = clampf(maxDailyMl, 0.1f, HARD_MAX_DAILY_ML);
  if (pauseS < HARD_MIN_PAUSE_S) pauseS = HARD_MIN_PAUSE_S;
  if (pauseS > 86400UL) pauseS = 86400UL;

  if (adcGain > (uint8_t)ADS_GAIN_0256) adcGain = (uint8_t)ADS_GAIN_4096;

  calPhA = clampf(calPhA, 0.0f, 14.0f);
  calPhB = clampf(calPhB, 0.0f, 14.0f);
  calVoltA = clampf(calVoltA, -6.2f, 6.2f);
  calVoltB = clampf(calVoltB, -6.2f, 6.2f);
  // Zwei Kalibrierpunkte muessen sich unterscheiden, sonst ist die Gerade sinnlos.
  if (fabsf(calPhA - calPhB) < 0.5f || fabsf(calVoltA - calVoltB) < 0.010f) {
    calValid = false;
  }

  dailyMl = clampf(dailyMl, 0.0f, HARD_MAX_DAILY_ML);
  if (totalMl < 0) totalMl = 0;

  hostname[sizeof(hostname) - 1] = 0;
  wifiSsid[sizeof(wifiSsid) - 1] = 0;
  wifiPass[sizeof(wifiPass) - 1] = 0;
  webUser[sizeof(webUser) - 1]   = 0;
  webPass[sizeof(webPass) - 1]   = 0;
}

static void getStr(Preferences &p, const char *key, char *dst, size_t len, const char *def) {
  String s = p.getString(key, def);
  strncpy(dst, s.c_str(), len - 1);
  dst[len - 1] = 0;
}

void Settings::load() {
  prefs.begin(NS, true);  // read-only

  getStr(prefs, "ssid", wifiSsid, sizeof(wifiSsid), "");
  getStr(prefs, "pass", wifiPass, sizeof(wifiPass), "");
  getStr(prefs, "host", hostname, sizeof(hostname), "ph-dosierung");
  getStr(prefs, "wuser", webUser, sizeof(webUser), "");
  getStr(prefs, "wpass", webPass, sizeof(webPass), "");

  calValid   = prefs.getBool("calok", false);
  calPhA     = prefs.getFloat("cpha", 7.00f);
  calVoltA   = prefs.getFloat("cva", 1.65f);
  calPhB     = prefs.getFloat("cphb", 4.00f);
  calVoltB   = prefs.getFloat("cvb", 2.00f);
  adcGain    = prefs.getUChar("gain", (uint8_t)ADS_GAIN_4096);

  stepsPerMl = prefs.getFloat("spml", DEFAULT_STEPS_PER_ML);
  panelRevs  = prefs.getFloat("prevs", 5.0f);
  standbyS   = (uint16_t)prefs.getULong("stby", 300);
  shiftS     = (uint16_t)prefs.getULong("shft", 300);
  nightEnabled = prefs.getBool("nite", true);
  nightFrom  = prefs.getUChar("nfrom", 20);
  nightTo    = prefs.getUChar("nto", 5);

  circEnabled = prefs.getBool("circen", false);
  getStr(prefs, "hahost", haHost, sizeof(haHost), "");
  getStr(prefs, "hatok", haToken, sizeof(haToken), "");
  getStr(prefs, "haent", haEntity, sizeof(haEntity), "switch.poolpumpe");
  getStr(prefs, "haon", haOnState, sizeof(haOnState), "on");
  circFreshS  = (uint16_t)prefs.getULong("circfr", 120);
  circRetryS  = (uint16_t)prefs.getULong("circrt", 60);
  circOffRetryS = (uint16_t)prefs.getULong("circof", 120);
  stepsPerRev= prefs.getFloat("sprev", DEFAULT_STEPS_PER_REV);
  stepRate   = prefs.getFloat("srate", DEFAULT_STEP_RATE);
  stepAccel  = prefs.getFloat("sacc", DEFAULT_STEP_ACCEL);
  invertDir  = prefs.getBool("invdir", false);
  holdEnabled= prefs.getBool("hold", false);

  autoEnabled= prefs.getBool("auto", false);
  phSetpoint = prefs.getFloat("sp", 7.20f);
  phDeadband = prefs.getFloat("db", 0.05f);
  doseMl     = prefs.getFloat("dose", 3.00f);
  maxSingleMl= prefs.getFloat("maxs", 5.00f);
  maxDailyMl = prefs.getFloat("maxd", 60.00f);
  pauseS     = prefs.getULong("pause", 1800);
  phMinLock  = prefs.getFloat("phlock", 6.80f);
  phMaxPlaus = prefs.getFloat("phmax", 9.50f);


  dailyMl    = prefs.getFloat("dml", 0.0f);
  dayStamp   = prefs.getULong("dstamp", 0);
  totalMl    = prefs.getFloat("tml", 0.0f);

  prefs.end();
  clampAll();
}

void Settings::save() {
  clampAll();
  prefs.begin(NS, false);

  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  prefs.putString("host", hostname);
  prefs.putString("wuser", webUser);
  prefs.putString("wpass", webPass);

  prefs.putBool("calok", calValid);
  prefs.putFloat("cpha", calPhA);
  prefs.putFloat("cva", calVoltA);
  prefs.putFloat("cphb", calPhB);
  prefs.putFloat("cvb", calVoltB);
  prefs.putUChar("gain", adcGain);

  prefs.putFloat("spml", stepsPerMl);
  prefs.putFloat("prevs", panelRevs);
  prefs.putULong("stby", standbyS);
  prefs.putULong("shft", shiftS);
  prefs.putBool("nite", nightEnabled);
  prefs.putUChar("nfrom", nightFrom);
  prefs.putUChar("nto", nightTo);

  prefs.putBool("circen", circEnabled);
  prefs.putString("hahost", haHost);
  prefs.putString("hatok", haToken);
  prefs.putString("haent", haEntity);
  prefs.putString("haon", haOnState);
  prefs.putULong("circfr", circFreshS);
  prefs.putULong("circrt", circRetryS);
  prefs.putULong("circof", circOffRetryS);
  prefs.putFloat("sprev", stepsPerRev);
  prefs.putFloat("srate", stepRate);
  prefs.putFloat("sacc", stepAccel);
  prefs.putBool("invdir", invertDir);
  prefs.putBool("hold", holdEnabled);

  prefs.putBool("auto", autoEnabled);
  prefs.putFloat("sp", phSetpoint);
  prefs.putFloat("db", phDeadband);
  prefs.putFloat("dose", doseMl);
  prefs.putFloat("maxs", maxSingleMl);
  prefs.putFloat("maxd", maxDailyMl);
  prefs.putULong("pause", pauseS);
  prefs.putFloat("phlock", phMinLock);
  prefs.putFloat("phmax", phMaxPlaus);


  prefs.putFloat("dml", dailyMl);
  prefs.putULong("dstamp", dayStamp);
  prefs.putFloat("tml", totalMl);

  prefs.end();
}

void Settings::saveCounters() {
  prefs.begin(NS, false);
  prefs.putFloat("dml", dailyMl);
  prefs.putULong("dstamp", dayStamp);
  prefs.putFloat("tml", totalMl);
  prefs.end();
}

void Settings::factoryReset() {
  prefs.begin(NS, false);
  prefs.clear();
  prefs.end();
  *this = Settings();  // Defaults aus der Struktur uebernehmen
  save();
}
