# Automatische pH-Minus-Dosieranlage

LilyGo T-Display S3 AMOLED · ADS1115 · pH-Signalboard · TMC2209 · NEMA17 · Peristaltikpumpe

Umsetzung des Vorhabens aus [ph_minus_dosieranlage_entwicklung.md](ph_minus_dosieranlage_entwicklung.md).

> Jenes Dokument ist die **ursprüngliche Projektbeschreibung** und unverändert
> erhalten. Es geht noch von einem ESP32-C3 Super Mini plus separatem Display
> aus — diese Aufteilung wurde später zugunsten eines einzigen Geräts
> aufgegeben. Verbindlich für den Aufbau sind
> [docs/SCHALTPLAN.md](docs/SCHALTPLAN.md) und
> [docs/LOETANLEITUNG.md](docs/LOETANLEITUNG.md).
> Dosierprinzip, Sicherheitsgrundsätze und Roadmap der Beschreibung gelten
> unverändert.

**Ein Gerät für alles:** Das T-Display S3 AMOLED misst den pH-Wert, regelt,
treibt die Peristaltikpumpe und ist zugleich Anzeige, Touch-Bedienteil und
Webserver.

---

## Inhalt

```text
firmware/ph_dosieranlage_s3/ die komplette Firmware (Messung, Regelung, UI, Web)
tools/i2c_adc_test/          Phase 1: I²C-Scan und ADS1115-Rohwerte
tools/motor_test/            Phase 2: Motor-, Richtungs- und VREF-Test
docs/LOETANLEITUNG.md        Schritt für Schritt löten, mit Prüfpunkten
docs/SCHALTPLAN.md           Netzliste, Pinbelegung, offene Messpunkte
docs/schaltplan.svg          Verdrahtungsplan als Grafik
docs/INBETRIEBNAHME.md       Phasen 1–7, Kalibrierung, Konsolenbefehle
docs/BEDIENPANEL.md          Display und Touch-Bedienung
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

Board: `esp32:esp32:esp32s3` mit 16 MB Flash, OPI PSRAM und USB-CDC · Port `COM6`

> Der erste Build dauert rund 20 Minuten — LVGL sind ~450 Übersetzungseinheiten,
> und die Skripte bauen mit `--jobs 1`. Grund dafür in
> [docs/BEDIENPANEL.md](docs/BEDIENPANEL.md), Abschnitt 4.

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
| `WebPage.h` | Weboberfläche als ein HTML-Dokument im Flash |
| `PanelUi.*` | LVGL-Oberfläche, Rückfrage, Standby und Nachtmodus |

Anzeige: pH-Wert in ~136 px Höhe, dosierte Menge der letzten 24 Stunden,
Zustand und Sperrgründe. Tippen aufs Display öffnet die Rückfrage, erst
*FREIGEBEN* löst eine Dosierung über eine feste Anzahl Motorumdrehungen aus.

Nach der Standby-Zeit zeigt das Display nur noch den pH-Wert, gedimmt und
regelmäßig versetzt; im Nachtfenster bleibt es ganz dunkel und wacht auf
Berührung auf. Zeiten sind im Webinterface einstellbar. Details in
[docs/BEDIENPANEL.md](docs/BEDIENPANEL.md).

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
Sensorfehler, instabiler Messwert oder Not-Halt verhindern beziehungsweise
brechen eine Dosierung ab. Tages- und Gesamtmenge überleben einen Neustart.

Für die Umwälzung gibt es bewusst keinen verdrahteten Eingang: Die Anlage
hängt am selben geschalteten Stromkreis wie die Poolpumpe und kann dadurch
physisch nicht in stehendes Wasser dosieren.

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
