// WebInterface.h - WLAN, Webserver, JSON-API, OTA
#pragma once

#include <Arduino.h>

class WebInterface {
 public:
  void begin();
  void tick();

  bool apMode() const { return apMode_; }
  String ip() const;
  String modeText() const { return apMode_ ? "AP" : "STA"; }
  int rssi() const;

  // Statusobjekt als JSON - wird auch von der seriellen Konsole genutzt.
  String statusJson() const;

 private:
  bool apMode_ = false;
  uint32_t lastReconnect_ = 0;

  void startWifi();
  void setupRoutes();
  bool guard();          // HTTP-Basic-Auth, falls konfiguriert
};

extern WebInterface web;
