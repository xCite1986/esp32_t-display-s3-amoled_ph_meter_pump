// PanelNet.h - WLAN, HTTP-Abfrage der Dosieranlage, Dosieranforderung
//
// Laeuft in einer eigenen FreeRTOS-Task auf Core 0, damit die LVGL-Oberflaeche
// auf Core 1 nicht bei jeder HTTP-Anfrage stehenbleibt.
#pragma once

#include <Arduino.h>

// Momentaufnahme der Anlage. Nur ueber netLock()/netUnlock() anfassen.
struct PanelState {
  bool     online      = false;
  uint32_t lastOkMs    = 0;

  float    ph          = 7.0f;
  bool     phValid     = false;
  char     phStatus[40] = "keine Verbindung";
  bool     stable      = false;

  float    setpoint    = 7.20f;
  float    today       = 0;      // Kalendertag (zaehlt gegen das Tageslimit)
  float    maxDaily    = 60.0f;
  float    last24h     = 0;      // rollierende 24 Stunden

  char     state[28]   = "-";
  char     locks[120]  = "";
  bool     autoOn      = false;
  bool     fault       = false;
  bool     estop       = false;

  bool     pumpRun     = false;
  float    pumpMl      = 0;
  float    pumpTarget  = 0;

  float    stepsPerMl  = 1600.0f;
  float    stepsPerRev = 3200.0f;
};

struct PanelConfig {
  char ssid[33]  = "";
  char pass[65]  = "";
  char host[64]  = "ph-dosierung.local";
  char user[17]  = "";
  char pw[33]    = "";
  float revs     = 5.0f;         // Umdrehungen pro Panel-Freigabe
};

extern PanelConfig panelCfg;

void  netBegin();                       // Config laden, WLAN starten, Task starten
void  netSaveConfig();
void  netFactoryReset();

void  netLock();
void  netUnlock();
const PanelState &netState();           // nur zwischen Lock/Unlock lesen!

// Dosierung anfordern. Das Ergebnis kommt asynchron ueber netTakeResult().
void  netRequestDose();
bool  netDosePending();
bool  netTakeResult(String &msg, bool &ok);   // true, wenn ein neues Ergebnis vorliegt

String netWifiInfo();
float  netDoseMl();                     // Umdrehungen -> ml mit aktuellen Faktoren
