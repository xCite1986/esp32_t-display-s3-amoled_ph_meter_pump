// PanelUi.h - LVGL-Oberflaeche des Bedienpanels
#pragma once

#include <Arduino.h>

void uiBegin(int16_t w, int16_t h);
void uiTick();          // zyklisch aus loop() - Helligkeit, Pixel-Shift, Dialogtimeout
void uiRefresh();       // Werte aus netState() in die Oberflaeche uebernehmen
void uiToast(const String &msg, bool ok);
