// Config.h - Pinbelegung, harte Sicherheitsgrenzen, Konstanten
// Projekt: Automatische pH-Minus-Dosieranlage (ESP32-C3 Super Mini)
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Firmware-Kennung
// ---------------------------------------------------------------------------
#define FW_NAME     "pH-Minus-Dosieranlage"
#define FW_VERSION  "1.0.0"

// ---------------------------------------------------------------------------
// Pinbelegung ESP32-C3 Super Mini
// ---------------------------------------------------------------------------
// I2C zum ADS1115
static const uint8_t PIN_I2C_SDA = 5;
static const uint8_t PIN_I2C_SCL = 6;

// TMC2209 Steppertreiber
static const uint8_t PIN_STEP    = 3;
static const uint8_t PIN_DIR     = 4;
static const uint8_t PIN_EN      = 7;   // LOW = Treiber aktiv (active low!)
static const uint8_t PIN_TMC_PDN = 10;  // reserviert fuer spaeteres UART

// Ruecknmeldung Umwaelzung / Durchflussschalter (optional, per Settings aktivierbar)
static const uint8_t PIN_FLOW    = 1;

// Onboard-LED des Super Mini (invertiert: LOW = an)
static const uint8_t PIN_LED     = 8;
static const bool    LED_ACTIVE_LOW = true;

// ---------------------------------------------------------------------------
// ADS1115
// ---------------------------------------------------------------------------
static const uint8_t ADS_I2C_ADDR = 0x48;  // ADDR -> GND
static const uint8_t ADS_CH_PH    = 0;     // pH-Board PO an A0

// ---------------------------------------------------------------------------
// HARTE Sicherheitsgrenzen - im Code verankert, per Weboberflaeche NICHT
// ueberschreibbar. Alle Benutzerwerte werden zusaetzlich hierauf begrenzt.
// ---------------------------------------------------------------------------
static const float    HARD_MAX_SINGLE_DOSE_ML = 20.0f;    // ml pro Einzeldosis
static const float    HARD_MAX_DAILY_ML       = 500.0f;   // ml pro Tag
static const uint32_t HARD_MAX_PUMP_RUN_MS    = 180000UL; // 3 min Dauerlauf max
static const uint32_t HARD_MIN_PAUSE_S        = 60;       // min. Pause zw. Dosen
static const float    HARD_MIN_PH_LOCK        = 6.20f;    // darunter NIE dosieren
static const float    HARD_MAX_STEPS_PER_ML   = 20000.0f;
static const float    HARD_MIN_STEPS_PER_ML   = 10.0f;

// Plausibilitaetsfenster Sensor
static const float PH_PLAUS_MIN   = 3.00f;
static const float PH_PLAUS_MAX   = 11.00f;
static const float VOLT_PLAUS_MIN = 0.030f;   // V am ADS-Eingang
static const float VOLT_PLAUS_MAX = 3.250f;   // V am ADS-Eingang

// Messwertaufbereitung
static const uint8_t  PH_SAMPLE_COUNT    = 15;    // Ringpuffer fuer Median
static const uint32_t PH_SAMPLE_PERIOD_MS = 200;  // Abtastintervall
static const float    PH_EMA_ALPHA       = 0.25f; // Glaettung nach Median
static const float    PH_STABLE_BAND     = 0.05f; // max. Spanne fuer "stabil"
static const uint32_t PH_SENSOR_TIMEOUT_MS = 5000;// ohne gueltige Wandlung -> Fehler

// Motor-Defaults
static const float DEFAULT_STEPS_PER_ML  = 1600.0f;  // laut Projektbeschreibung
static const float DEFAULT_STEPS_PER_REV = 3200.0f;  // 1/16 Microstep, 1,8-Grad-Motor
static const float HARD_MAX_REVS         = 20.0f;    // Obergrenze fuer /api/dose/revs
static const float DEFAULT_STEP_RATE    = 1200.0f;  // Schritte/s Zielgeschwindigkeit
static const float DEFAULT_STEP_ACCEL   = 4000.0f;  // Schritte/s^2
static const float MIN_STEP_RATE        = 150.0f;   // Startgeschwindigkeit Rampe
static const float MAX_STEP_RATE        = 8000.0f;

// Access-Point-Fallback
#define AP_SSID_DEFAULT "pH-Dosieranlage"
#define AP_PASS_DEFAULT "dosier1234"
