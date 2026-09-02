// Config.h - Pinbelegung, harte Sicherheitsgrenzen, Konstanten
// Projekt: Automatische pH-Minus-Dosieranlage (LilyGo T-Display S3 AMOLED)
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Firmware-Kennung
// ---------------------------------------------------------------------------
#define FW_NAME     "pH-Minus-Dosieranlage"
#define FW_VERSION  "2.1.0"

// ---------------------------------------------------------------------------
// Pinbelegung LilyGo T-Display S3 AMOLED (Variante BOARD_AMOLED_191)
//
// Gegen das offizielle Pinout bestaetigt. Linke Stiftleiste von oben:
//   3V3 . 1 . 2 . 3 . 10 . 11 . 12 . 13 . 14 . 15 . GND . VBUS . VBUS . 16
// Rechte Stiftleiste von oben:
//   GND . GND . 46 . 45 . 44 . 43 . 42 . 41 . 40 . GND . GND . 3V3 . 3V3 . 39
//
// Vom Board belegt und tabu:
//   5,6,7,8,9,17,18,47,48  AMOLED QSPI     2,3,21  Touch CST816T
//   0 BOOT   4 Akku-ADC   38 gruene LED   19,20 USB   26-37 Flash/PSRAM
//   45,46 Strapping
// ACHTUNG: 2 und 3 sind zwar herausgefuehrt, haengen aber am Touchcontroller.
// 5 V werden auf VBUS eingespeist (parallel zur USB-Schiene, daher D1).
// Frei bleiben ausserdem: 1, 16, 39, 40, 41, 42
// Alternative fuer den ADS1115: Qwiic-Port GND/3V3/GPIO43/GPIO44
// ---------------------------------------------------------------------------

// Zweiter I2C-Bus (Wire1) zum ADS1115. Bewusst NICHT der Touchbus:
// der ADS haengt an langen Leitungen neben dem Motor - diese Last gehoert
// nicht auf den Bus, von dem die Bedienbarkeit des Displays abhaengt.
static const uint8_t PIN_I2C_SDA = 13;
static const uint8_t PIN_I2C_SCL = 14;

// TMC2209 Steppertreiber
static const uint8_t PIN_STEP    = 11;
static const uint8_t PIN_DIR     = 12;
static const uint8_t PIN_EN      = 10;  // LOW = Treiber aktiv (active low!)
static const uint8_t PIN_TMC_PDN = 15;  // reserviert fuer spaeteres UART


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

// Netzsynchrone Mittelung.
//
// Eine einzelne Wandlung alle 200 ms trifft eine 50-Hz-Stoerung in zufaelliger
// Phase - die Messkette hat dann null Netzunterdrueckung. Gemessen wurden im
// Becken 1283 mV Spitze-Spitze, das sind ueber zehn pH-Einheiten.
//
// Der Mittelwert eines Sinus ueber eine ganze Periode ist null. Deshalb wird
// ueber genau 20 ms gemittelt: das ist eine volle 50-Hz-Periode und zugleich
// zwei volle 100-Hz-Perioden, deckt also auch die Gleichrichterwelligkeit
// eines Netzteils oder einer Salzelektrolysezelle ab.
static const uint32_t PH_MAINS_PERIOD_US = 20000;  // 50 Hz
static const uint8_t  PH_BURST_SAMPLES   = 16;     // gleichmaessig verteilt
static const uint32_t PH_BURST_SETTLE_US = 2500;   // zwei Wandlungen verwerfen
static const uint32_t PH_SAMPLE_PERIOD_MS = 200;  // Abtastintervall
// Die Glaettung wird aus der einstellbaren Filterzeit berechnet, siehe
// PHMeasurement::applySettings(). Ein Pool aendert seinen pH ueber Stunden -
// eine lange Zeitkonstante kostet also nichts an Regelguete, macht die
// Messung aber unempfindlich gegen Stroemung und eingekoppelte Stoerungen.
static const float    PH_EMA_ALPHA       = 0.25f; // nur noch Rueckfallwert
static const float    PH_STABLE_BAND     = 0.05f; // max. Spanne fuer "stabil"
static const uint32_t PH_SENSOR_TIMEOUT_MS = 5000;// ohne gueltige Wandlung -> Fehler

// Gleitender Mittelwert fuer die Dosierentscheidung: alle 10 s ein Wert,
// Ringpuffer fuer bis zu 60 Minuten.
static const uint32_t PH_AVG_PERIOD_MS = 10000;
static const uint16_t PH_AVG_SLOTS     = 360;
static const uint16_t PH_AVG_MIN_S     = 60;
static const uint16_t PH_AVG_MAX_S     = 3600;
static const uint16_t PH_FILTER_MIN_S  = 1;
static const uint16_t PH_FILTER_MAX_S  = 300;

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

// ---------------------------------------------------------------------------
// Anzeige
// ---------------------------------------------------------------------------
static const uint8_t  BRIGHT_ACTIVE   = 240;   // bedient
static const uint8_t  BRIGHT_SAVER    = 45;    // Standby: nur die Ziffern
static const uint32_t DIALOG_TIMEOUT  = 12000; // Rueckfrage schliesst von selbst
static const uint32_t WAKE_GUARD_MS   = 450;   // Weck-Tipp loest nichts aus

// Grenzen fuer die konfigurierbaren Anzeigezeiten
static const uint16_t STANDBY_MIN_S   = 15;
static const uint16_t STANDBY_MAX_S   = 3600;
static const uint16_t SHIFT_MIN_S     = 30;
static const uint16_t SHIFT_MAX_S     = 3600;
