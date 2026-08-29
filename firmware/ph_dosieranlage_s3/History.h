// History.h - Verlauf der letzten 7 Tage in Stundenauflösung
//
// Bewusst KEINE Einzelmesswerte: 7 Tage bei 200 ms Abtastung waeren drei
// Millionen Werte. Gespeichert wird stattdessen pro Stunde ein Mittelwert
// samt Minimum und Maximum - das zeigt Verlauf UND Schwankungsbreite - sowie
// die in dieser Stunde dosierte Menge.
//
//   168 Stunden x 10 Byte = 1,7 kB im RAM, einmal pro Stunde ins NVS
//
// Das reicht fuer einen 7-Tage-Chart mit voller Aussagekraft und bleibt
// schnell genug, um den Verlauf in einem Rutsch als JSON auszuliefern.
#pragma once

#include <Arduino.h>

#define HIST_SLOTS 168          // 7 Tage x 24 Stunden

struct HistSlot {
  uint32_t hour;     // Stundennummer seit Epoch (epoch/3600), 0 = leer
  int16_t  phAvg;    // pH x 100, -1 = kein Messwert in dieser Stunde
  int16_t  phMin;
  int16_t  phMax;
  uint16_t ml10;     // dosierte Menge x 10
};

void histBegin();
void histTick();                     // zyklisch: Stundenwechsel erkennen
void histAddSample(float ph);        // gueltigen Messwert einsortieren
void histAddDose(float ml);          // Dosierung der laufenden Stunde zuschlagen
String histJson();                   // Verlauf fuer den Chart
float histDayMl(uint8_t daysAgo);    // Tagesmenge, 0 = heute
void histClear();
