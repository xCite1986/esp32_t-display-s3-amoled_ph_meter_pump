// Settings.h - persistente Konfiguration im NVS (ESP32 Preferences)
#pragma once

#include <Arduino.h>
#include "Config.h"
#include "Ads1115.h"

struct Settings {
  // --- Netzwerk ---
  char  wifiSsid[33]   = "";
  char  wifiPass[65]   = "";
  char  hostname[32]   = "ph-dosierung";
  char  webUser[17]    = "";       // leer = kein Login
  char  webPass[33]    = "";

  // --- pH-Kalibrierung (2-Punkt) ---
  bool  calValid       = false;
  float calPhA         = 7.00f;    // Puffer A (typisch 7.00)
  float calVoltA       = 1.65f;    // gemessene Spannung bei Puffer A
  float calPhB         = 4.00f;    // Puffer B (typisch 4.00)
  float calVoltB       = 2.00f;    // gemessene Spannung bei Puffer B
  uint8_t adcGain      = (uint8_t)ADS_GAIN_4096;

  // --- Pumpe ---
  float stepsPerMl     = DEFAULT_STEPS_PER_ML;
  float stepsPerRev    = DEFAULT_STEPS_PER_REV;  // fuer Bedienpanel: Umdrehungen -> ml

  // --- Umwaelzung ueber Home Assistant ---
  bool  circEnabled    = false;             // Pruefung aktiv
  char  haHost[64]     = "";                // z.B. 192.168.0.10:8123
  char  haToken[260]   = "";                // Long-Lived Access Token
  char  haEntity[64]   = "switch.poolpumpe";
  char  haOnState[16]  = "on";              // Zustand, der "laeuft" bedeutet
  uint16_t circFreshS  = 120;               // juengere Antwort gilt als aktuell
  uint16_t circRetryS  = 60;                // Wartezeit nach Fehlversuch
  uint16_t circOffRetryS = 120;             // Wartezeit, wenn die Umwaelzung steht
  float stepRate       = DEFAULT_STEP_RATE;
  float stepAccel      = DEFAULT_STEP_ACCEL;
  bool  invertDir      = false;
  bool  holdEnabled    = false;    // Treiber zwischen Dosierungen bestromt lassen

  // --- Regelung ---
  bool  autoEnabled    = false;    // Automatik erst nach Kalibrierung einschalten
  float phSetpoint     = 7.20f;
  float phDeadband     = 0.05f;    // erst ab Soll+Deadband dosieren
  float doseMl         = 3.00f;    // Einzeldosis
  float maxSingleMl    = 5.00f;    // Obergrenze Einzeldosis (Benutzer)
  float maxDailyMl     = 60.00f;   // Obergrenze Tagesmenge (Benutzer)
  uint32_t pauseS      = 1800;     // Durchmischung nach Dosierung: rund 30 min,
                                   // bis die Saeure wirklich verteilt ist
  float phMinLock      = 6.80f;    // darunter Dosiersperre
  float phMaxPlaus     = 9.50f;    // darueber unplausibel -> Sperre

  // --- Anzeige ---
  bool  rot180         = true;     // Anzeige um 180 Grad gedreht (Einbaulage)
  uint16_t standbyS    = 300;      // Standby nach x s ohne Beruehrung
  uint16_t shiftS      = 300;      // Position im Standby alle x s versetzen
  bool  nightEnabled   = true;     // nachts Display ganz aus
  uint8_t nightFrom    = 20;       // ab Stunde
  uint8_t nightTo      = 5;        // bis Stunde
  float panelRevs      = 5.0f;     // Umdrehungen pro Touch-Freigabe


  // --- Zaehler (persistent, damit ein Reboot das Tageslimit nicht umgeht) ---
  float    dailyMl     = 0.0f;
  uint32_t dayStamp    = 0;        // Tagesnummer (epoch/86400) bzw. 0 = unbekannt
  float    totalMl     = 0.0f;     // Lebensdauer-Zaehler

  void load();
  void save();
  void saveCounters();             // nur die Zaehler (haeufiger geschrieben)
  void factoryReset();

  // Alle Werte gegen die harten Grenzen aus Config.h klemmen.
  void clampAll();
};

extern Settings settings;
