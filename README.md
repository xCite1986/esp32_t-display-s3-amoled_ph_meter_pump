# Automatische pH-Minus-Dosieranlage

LilyGo T-Display S3 AMOLED · ADS1115 · pH-Signalboard · TMC2209 · NEMA17 · Peristaltikpumpe

Umsetzung des Vorhabens aus [ph_minus_dosieranlage_entwicklung.md](ph_minus_dosieranlage_entwicklung.md).

**Ein Gerät für alles:** Das T-Display S3 AMOLED misst den pH-Wert, regelt,
treibt die Peristaltikpumpe und ist zugleich Anzeige, Touch-Bedienteil und
Webserver.

---

> **Stand der Umstellung.** Schaltplan und Dokumentation beschreiben bereits
> die Ein-Geräte-Architektur (nur T-Display S3 AMOLED). Die Firmware liegt
> derzeit noch als zwei getrennte Sketches vor — `ph_dosieranlage` (C3) und
> `ph_panel_s3amoled`. Das Zusammenführen zu einer Firmware für den S3 ist der
> nächste Schritt.

## Inhalt

```text
firmware/ph_dosieranlage/    Anlagen-Firmware (ESP32-C3, keine Fremdbibliotheken)
firmware/ph_panel_s3amoled/  Bedienpanel (T-Display S3 AMOLED, LVGL 8.4)
tools/i2c_adc_test/          Phase 1: I²C-Scan und ADS1115-Rohwerte
tools/motor_test/            Phase 2: Motor-, Richtungs- und VREF-Test
docs/LOETANLEITUNG.md        Schritt für Schritt löten, mit Prüfpunkten
docs/SCHALTPLAN.md           Netzliste, Pinbelegung, offene Messpunkte
docs/schaltplan.svg          Verdrahtungsplan als Grafik
docs/INBETRIEBNAHME.md       Phasen 1–7, Kalibrierung, Konsolenbefehle
docs/BEDIENPANEL.md          Touch-Panel: Aufbau, Einrichtung, Bedienung
docs/HOMEASSISTANT.md        REST-Anbindung an Home Assistant
scripts/                     build / flash / monitor (PowerShell)
```

---

## Schnellstart

```bash
powershell -File scripts/build.ps1
```

```bash
powershell -File scripts/flash.ps1
```

```bash
powershell -File scripts/monitor.ps1
```

In der Konsole (115200 Baud) `help` eingeben.

Bedienpanel:

```bash
powershell -File scripts/flash-panel.ps1
```

| Gerät | Board | Port |
|---|---|---|
| Dosieranlage | `esp32:esp32:nologo_esp32c3_super_mini` | COM3 |
| Bedienpanel | `esp32:esp32:esp32s3` (16M, OPI PSRAM, CDC) | COM6 |

Die Skripte finden die in der Arduino IDE 2 gebündelte `arduino-cli`
automatisch; eine separate Installation ist nicht nötig.

---

## Reihenfolge

1. **Löten** nach [docs/LOETANLEITUNG.md](docs/LOETANLEITUNG.md) —
   die Abschnitte bauen aufeinander auf, insbesondere gilt:
   VREF einstellen *bevor* der Motor drankommt, und `PO` messen
   *bevor* es an den ADS1115 geht.
2. **Phase 1–2** mit den Testsketches (`-Sketch i2c`, `-Sketch motor`).
3. **Phase 3–5** mit der Hauptfirmware:
   Pumpe kalibrieren → pH kalibrieren → Regelung scharf schalten.
4. **Phase 6–7**: WLAN, Webinterface, Bedienpanel, optional Home Assistant.

Details in [docs/INBETRIEBNAHME.md](docs/INBETRIEBNAHME.md).

---

## Firmware-Aufbau

| Modul | Aufgabe |
|---|---|
| `Config.h` | Pinbelegung und **harte** Sicherheitsgrenzen |
| `Ads1115.*` | eigener, abhängigkeitsfreier ADS1115-Treiber |
| `PHMeasurement.*` | Abtastung, Median + EMA, Plausibilität, 2-Punkt-Kalibrierung |
| `StepperPump.*` | STEP/DIR/EN nicht blockierend, Rampe, Laufzeitüberwachung |
| `PHController.*` | Zustandsautomat, Verriegelungen, Tages-/Gesamtzähler |
| `Settings.*` | Persistenz im NVS, Begrenzung aller Werte |
| `WebInterface.*` | WLAN/AP, Webserver, JSON-API, OTA |
| `WebPage.h` | Oberfläche als ein HTML-Dokument im Flash |

### Bedienpanel

| Modul | Aufgabe |
|---|---|
| `PanelNet.*` | WLAN, HTTP/JSON gegen die Anlage, eigene Task auf Core 0 |
| `PanelUi.*` | LVGL-Oberfläche, Bestätigungsdialog, AMOLED-Einbrennschutz |

Anzeige: pH-Wert, dosierte Menge der letzten 24 Stunden, Zustand und
Sperrgründe. Tippen aufs Display öffnet die Rückfrage, erst *FREIGEBEN* löst
eine Dosierung über eine feste Anzahl Motorumdrehungen aus. Details und die
Umrechnung Umdrehungen → ml in [docs/BEDIENPANEL.md](docs/BEDIENPANEL.md).

### Dosierprinzip

```text
messen → plausibilisieren → eine kleine definierte Dosis
      → warten und durchmischen → neu messen → ggf. erneut dosieren
```

Es wird nie „auf den Sollwert durchdosiert". Die Schrittzahl bestimmt die
Menge, nicht die Zeit — deshalb ist die Dosierung auch dann exakt, wenn
der WLAN-Stack den Ablauf kurz bremst.

### Sicherheitsgrenzen (nicht abschaltbar)

max. 20 ml Einzeldosis · max. 500 ml/Tag · max. 180 s Pumpenlauf am Stück ·
min. 60 s Pause · nie unter pH 6,20 · gültiger pH nur zwischen 3,00 und 11,00.

Die Automatik lässt sich ohne gültige Kalibrierung nicht einschalten.
Sensorfehler, instabiler Messwert, fehlende Umwälzung oder Not-Halt brechen
eine laufende Dosierung ab. Tages- und Gesamtmenge überleben einen Neustart.

---

## Offene Punkte aus der Projektbeschreibung

Diese Werte lassen sich nicht am Schreibtisch festlegen und sind in den
Dokumenten als Messpunkte markiert:

* Versorgungsspannung und tatsächlicher `PO`-Bereich des pH-Boards
  → [docs/SCHALTPLAN.md](docs/SCHALTPLAN.md), Abschnitt 5
* Spulenpaare des NEMA17 → Lötanleitung, Abschnitt 10
* Motorstrom / VREF → Lötanleitung, Abschnitt 9
* Fördermenge pro Umdrehung, Schritte/ml → Inbetriebnahme, Phase 3
* Sichere Dosiergrenzen für das konkrete Beckenvolumen → Inbetriebnahme, Phase 5

Der Peristaltik-Pumpenkopf und der säurebeständige Schlauch stehen laut
Projektstand noch aus. Für die Firmware ist das unkritisch — `Schritte/ml`
ist ein Kalibrierwert und wird in Phase 3 ermittelt.
