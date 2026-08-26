// PanelUi.h - LVGL-Oberflaeche auf dem T-Display S3 AMOLED
//
// Die Oberflaeche liest die Werte direkt aus den lokalen Modulen (phMeas,
// controller, pump, settings) - es gibt keinen Netzwerkweg mehr zwischen
// Anzeige und Regelung, beides laeuft auf demselben Chip.
#pragma once

#include <Arduino.h>

enum DispState : uint8_t {
  DS_ACTIVE = 0,   // bedient: volle Helligkeit, alle Angaben
  DS_SAVER,        // Standby: nur der pH-Wert, gedimmt, wandernd
  DS_OFF           // Nachtmodus: Display dunkel, Touch weckt
};

void uiBegin(int16_t w, int16_t h);
void uiTick();          // zyklisch: Anzeigezustand, Dialogtimeout, Einbrennschutz
void uiRefresh();       // Werte uebernehmen
void uiToast(const String &msg, bool ok);

DispState uiState();
const char *uiStateText();
void uiWake();          // von aussen aufwecken (z.B. nach einer Dosierung)
