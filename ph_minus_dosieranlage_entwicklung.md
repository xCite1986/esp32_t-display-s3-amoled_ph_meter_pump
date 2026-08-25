# Projektzusammenfassung – Automatische pH-Minus-Dosieranlage

## 1. Ziel

Entwicklung einer kompakten automatischen pH-Minus-Dosieranlage auf Basis eines **ESP32-C3 Super Mini**.

Die Anlage soll:
- den pH-Wert kontinuierlich erfassen,
- Messwerte digital auswerten,
- eine Peristaltik-/Schlauchpumpe über einen Schrittmotor ansteuern,
- pH-Minus fein und reproduzierbar dosieren,
- Kalibrierwerte und Dosierparameter speichern,
- später optional ein Webinterface und/oder Home Assistant unterstützen,
- durch harte Sicherheitsgrenzen eine Überdosierung verhindern.

Ein OLED-Display ist vorerst **nicht vorgesehen**.

---

## 2. Aktuell geplante Hardware

### Steuerung
- ESP32-C3 Super Mini

### pH-Messung
- pH-Sonde mit BNC-Anschluss
- mitgeliefertes pH-Signalaufbereitungsboard
- ADS1115, 16-Bit I²C ADC mit PGA

**Wichtig:** Der ADS1115 ersetzt **nicht** das pH-Sondenboard.

Signalweg:

```text
pH-Sonde
   |
   | BNC
   v
pH-Sondenboard / Signalaufbereitung
   |
   | Analogausgang PO
   v
ADS1115 A0
   |
   | I²C
   v
ESP32-C3 Super Mini
```

Das Sondenboard ist nötig, weil die pH-Elektrode ein sehr hochohmiges und kleines Signal liefert. Der ADS1115 übernimmt danach die präzise Analog-Digital-Wandlung.

### Pumpenantrieb
- NEMA17 Schrittmotor
- 12 V
- 1,8° Schrittwinkel
- ca. 28 Ncm
- TMC2209 Steppertreiber
- Peristaltik-/Schlauchpumpenkopf noch auszuwählen bzw. zu montieren

---

## 3. Schrittmotor – bekannte Verkabelung

Am Motor befindet sich ein 6-poliger Stecker mit vier belegten Positionen:

```text
Rot | frei | Grün | Blau | frei | Schwarz
```

Vermutete Spulenpaare:

```text
Spule 1: Rot + Grün
Spule 2: Blau + Schwarz
```

Vor dem Anschluss unbedingt mit einem Multimeter prüfen:

```text
Rot  <-> Grün      kleiner Widerstand / Durchgang
Blau <-> Schwarz   kleiner Widerstand / Durchgang
```

Zwischen Leitungen unterschiedlicher Spulen sollte kein Durchgang bestehen.

Vorgesehener Anschluss am TMC2209:

```text
Motor        TMC2209
--------------------
Rot       -> 1A
Grün      -> 1B
Blau      -> 2A
Schwarz   -> 2B
```

Falls eine Spule umgekehrt angeschlossen wird, ändert sich nur die Drehrichtung.

---

## 4. TMC2209 – ESP32-C3 Pinplanung

Aktuelle geplante Pinbelegung:

```text
ESP32-C3       TMC2209
-----------------------
GPIO3       -> STEP
GPIO4       -> DIR
GPIO7       -> EN
GPIO10      -> PDN/UART optional

3.3V        -> VIO
GND         -> GND
```

Für die erste Version genügt STEP/DIR.

UART kann später ergänzt werden, z. B. für:
- Stromkonfiguration,
- Microstepping,
- Diagnose,
- Treiberstatus.

---

## 5. Motorversorgung

Der NEMA17 wird **nicht** über den ESP32 versorgt.

Geplanter Aufbau:

```text
12-V-Netzteil
   |
   +----> TMC2209 VMOT
   |
   +----> 12V-auf-5V-Buck-Converter
               |
               +----> ESP32-C3 5V
```

Alle Massen müssen gemeinsam verbunden werden:

```text
12-V-Netzteil GND
TMC2209 GND
ESP32 GND
ADS1115 GND
pH-Board GND
```

Wichtig: Den Motorstecker niemals bei eingeschaltetem TMC2209 ein- oder ausstecken.

---

## 6. ADS1115 – ESP32-C3

Geplante Verbindung:

```text
ADS1115       ESP32-C3
-----------------------
VDD        -> 3.3V
GND        -> GND
SDA        -> GPIO5
SCL        -> GPIO6
```

Analog:

```text
pH-Board PO -> ADS1115 A0
```

A1, A2 und A3 bleiben zunächst frei.

---

## 7. pH-Sondenboard

Auf dem vorhandenen Board sind vermutlich folgende Pins vorhanden:

```text
TO
DO
PO
G
G
V+
```

Für die pH-Messung benötigen wir voraussichtlich:

```text
V+ -> Versorgung
G  -> GND
PO -> ADS1115 A0
```

`DO` ist ein digitaler Schwellwertausgang und wird für die Softwareregelung nicht benötigt.

`TO` wird für die eigentliche pH-Messung voraussichtlich ebenfalls nicht benötigt.

### Noch zu verifizieren
Vor dem endgültigen Anschluss:
- Versorgungsspannung des Boards bestätigen,
- tatsächlichen Spannungsbereich von `PO` messen,
- Funktion von `TO` prüfen,
- Bedeutung der Potentiometer klären,
- zulässigen Ausgangsspannungsbereich prüfen.

Vorher keinen festen Spannungsteiler einplanen.

Empfohlene Messung:

```text
PO gegen GND
```

bei eingeschalteter Platine und idealerweise in pH-7-Pufferlösung.

---

## 8. Geplanter Gesamtaufbau

```text
pH-Sonde
   |
   v
pH-Sondenboard
   |
   | PO
   v
ADS1115 A0
   |
   | I²C
   v
ESP32-C3 Super Mini
   |
   +---- STEP/DIR/EN ----> TMC2209 ----> NEMA17 ----> Peristaltikpumpe
```

---

## 9. Dosierprinzip

Die Pumpe soll nicht nur zeitgesteuert laufen. Ziel ist eine kalibrierte Dosierung in **Millilitern**.

Beispiel:

```text
3200 Schritte = 1 Motorumdrehung
1 Motorumdrehung = 2,0 ml
```

Dann:

```text
1600 Schritte = 1,0 ml
800 Schritte  = 0,5 ml
```

Die tatsächliche Fördermenge muss mit dem endgültigen Pumpenkopf kalibriert werden.

### Pumpenkalibrierung
1. Schlauch korrekt einsetzen.
2. Wasser oder geeignetes Testmedium verwenden.
3. Definierte Anzahl Motorschritte fahren.
4. Fördermenge in ml messen.
5. Faktor `Schritte pro ml` berechnen.
6. Wert dauerhaft im ESP32 speichern.

---

## 10. Geplante pH-Regelung

Keine permanente Regelung direkt auf den Sollwert.

Stattdessen langsame, kontrollierte Dosierung:

```text
Soll-pH:        7,20
Ist-pH:         7,35
Einzeldosis:    3 ml
Wartezeit:      5-10 Minuten
danach erneute Messung
```

Das Wasser muss nach jeder Dosierung ausreichend durchmischt werden.

---

## 11. Sicherheitsfunktionen

Mindestens vorgesehen:

- maximale Einzeldosis,
- maximale Tagesdosis,
- Mindestwartezeit zwischen zwei Dosierungen,
- Dosierung nur bei gültigem Sensorsignal,
- Plausibilitätsprüfung des pH-Werts,
- Sperre bei zu niedrigem pH,
- Sperre bei Sensorfehler,
- Sperre bei fehlender Umwälzung,
- Zähler für Tagesdosierung,
- Speicherung wichtiger Parameter im nichtflüchtigen Speicher.

Optional später:
- Rückmeldung der Pool-/Umwälzpumpe,
- Durchflussschalter,
- Alarm über Home Assistant,
- maximale Motorlaufzeit pro Dosierung,
- Benachrichtigung bei unplausiblem pH-Verlauf.

---

## 12. pH-Kalibrierung

Geplant ist eine softwarebasierte **2-Punkt-Kalibrierung**.

Empfohlene Pufferlösungen:

```text
pH 7,00
pH 4,00
```

Der ESP32 speichert die beiden ADC-/Spannungswerte und berechnet daraus:
- Steigung,
- Offset,
- resultierenden pH-Wert.

Die Kalibrierwerte werden dauerhaft gespeichert.

---

## 13. Software – geplante Module

### `PHMeasurement`
- ADS1115 auslesen
- Mittelwertbildung
- Filterung
- Spannung berechnen
- pH berechnen
- Kalibrierwerte verwalten

### `StepperPump`
- TMC2209 STEP/DIR/EN ansteuern
- definierte Schrittzahl fahren
- Geschwindigkeit einstellen
- `ml -> Schritte` umrechnen
- manuelle Dosierung

### `PHController`
- Sollwertregelung
- Hysterese
- Wartezeiten
- Einzeldosis bestimmen
- Sicherheitsgrenzen überwachen

### `Settings`
Persistente Speicherung über ESP32 Preferences:
- WLAN
- pH-Sollwert
- Kalibrierwerte
- Schritte/ml
- maximale Einzeldosis
- maximale Tagesdosis
- Mindestpause

### `WebInterface`
Später optional:
- aktueller pH-Wert
- Sollwert
- Pumpenstatus
- Dosiermenge heute
- manuelle Dosierung
- Pumpenkalibrierung
- pH-Kalibrierung
- Systemeinstellungen

---

## 14. Home Assistant – optional

Mögliche Entitäten:

```text
sensor.ph_wert
sensor.ph_dosierung_heute
sensor.ph_pumpenstatus
sensor.ph_adc_spannung

number.ph_sollwert
number.ph_max_tagesdosis
number.ph_einzeldosis

button.ph_manuell_dosieren
button.ph_kalibrieren

binary_sensor.ph_sensor_ok
binary_sensor.ph_dosierung_gesperrt
```

Mögliche Anbindung:
- MQTT
- REST
- eigene ESP32-Web-API

---

## 15. Offene Punkte

1. Genaues Modell und Pinout des pH-Sondenboards verifizieren.
2. Ausgangsspannung `PO` messen.
3. Versorgungsspannung des pH-Boards bestätigen.
4. TMC2209-Modul exakt identifizieren.
5. Motorstrom des TMC2209 passend zum NEMA17 einstellen.
6. Spulenpaare des Motors mit Multimeter bestätigen.
7. Peristaltik-Pumpenkopf auswählen und montieren.
8. Geeigneten säurebeständigen Pumpenschlauch auswählen.
9. Fördermenge in ml/Umdrehung bestimmen.
10. `Schritte/ml` kalibrieren.
11. pH-Sonde mit pH-7- und pH-4-Puffer kalibrieren.
12. Sichere Dosiergrenzen definieren.
13. Rückmeldung der Umwälzpumpe festlegen.
14. Webinterface entwickeln.
15. Optional Home Assistant integrieren.

---

## 16. Empfohlene Roadmap

### Phase 1 – Hardwaretest
- ESP32-C3 in Betrieb nehmen.
- ADS1115 über I²C testen.
- Rohwerte von A0 ausgeben.
- pH-Board anschließen.
- PO-Spannung messen.
- pH-Sonde mit Testlösungen prüfen.

### Phase 2 – Motorsteuerung
- TMC2209 anschließen.
- Motorstrom einstellen.
- Spulen des Motors prüfen.
- NEMA17 langsam drehen lassen.
- Drehrichtung testen.
- Microstepping festlegen.

### Phase 3 – Pumpenkopf
- Peristaltik-Pumpenkopf montieren.
- Wasser fördern.
- Schritte pro ml ermitteln.
- reproduzierbare Dosierung testen.

### Phase 4 – pH-Kalibrierung
- pH 7,00 messen.
- pH 4,00 messen.
- lineare Kalibrierung berechnen.
- Werte dauerhaft speichern.
- Messwertfilter implementieren.

### Phase 5 – Regelung
- pH-Sollwert definieren.
- Hysterese implementieren.
- Dosierzyklen programmieren.
- Misch-/Wartezeiten implementieren.
- Tagesmaximum einbauen.
- Fehlerzustände definieren.

### Phase 6 – Webinterface
- Statusseite
- Sollwert
- manuelle Dosierung
- Kalibrierung
- Pumpenparameter
- Sicherheitsparameter
- Diagnosewerte

### Phase 7 – Integration
Optional:
- Home Assistant
- MQTT
- OTA-Firmwareupdate
- Logging
- Historie
- Alarme

---

## 17. Aktueller Projektstatus

Stand: 25.08.2026

Bereits vorhanden bzw. bestellt:
- ESP32-C3 Super Mini
- NEMA17 12 V / ca. 28 Ncm
- TMC2209
- pH-Sonde
- pH-Signalaufbereitungsboard
- ADS1115

Nicht mehr vorgesehen:
- 0,96"-OLED-Display

Noch erforderlich bzw. offen:
- Peristaltik-Pumpenkopf
- passende Stromversorgung / Buck-Converter
- säurebeständiger Schlauch
- endgültige Verdrahtung nach Spannungsmessungen
- Firmware
- Kalibrierung
- Sicherheitstests

---

## 18. Grundsatz für die weitere Entwicklung

Die Anlage darf pH-Minus niemals ausschließlich aufgrund eines einzelnen Messwerts unbeschränkt dosieren.

Die Firmware sollte nach diesem Prinzip arbeiten:

```text
Messen
  ->
Plausibilisieren
  ->
kleine definierte Dosis
  ->
warten / durchmischen
  ->
neu messen
  ->
bei Bedarf erneut dosieren
```

Zusätzlich muss immer eine harte Obergrenze für die maximal mögliche Dosiermenge vorhanden sein.
