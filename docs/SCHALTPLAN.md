# Schaltplan / Netzliste

Grafik: [schaltplan.svg](schaltplan.svg) (im Browser oder in Inkscape öffnen, druckbar auf A3)

**Ein Gerät:** Der LilyGo T-Display S3 AMOLED übernimmt alles — Messung,
Regelung, Pumpenansteuerung, Anzeige, Bedienung und Webinterface. Ein
separater ESP32-C3 wird nicht mehr verwendet.

---

## 1. Übersicht

```text
pH-Sonde ──BNC──> pH-Signalboard ──PO──> R2 10k ──> ADS1115 A0
                                                        │ I²C (Wire1, GPIO13/14)
                                                        v
12 V ──┬──> TMC2209 VMOT ──> NEMA17 ──> Peristaltikpumpe
       │         ^
       │         │ STEP/DIR/EN (GPIO11/12/10)
       │         │
       └──> Buck 5 V ──D1──> T-Display S3 AMOLED ──3V3──> ADS1115, TMC2209 VIO
                     └──> pH-Board V+
```

### Was das für die Sicherheit bedeutet

In der Zwei-Geräte-Variante liefen Dosierlogik und Bedienoberfläche auf
getrennten Mikrocontrollern — ein Absturz der Oberfläche konnte die Dosierung
nicht beeinflussen. Das ist jetzt nicht mehr so: **derselbe Chip zeichnet das
Display und steuert die Pumpe.**

Zwei Dinge fangen das in der Firmware ab:

* Die Schrittimpulse werden nicht blockierend erzeugt. Wenn LVGL für einen
  Bildaufbau ein paar Millisekunden braucht, läuft der Motor kurz
  unregelmäßiger — die **Schrittzahl und damit die Dosiermenge bleibt exakt**.
* `EN` des TMC2209 hängt über R1 auf 3,3 V. Startet der S3 neu oder hängt er
  im Reset, ist der Treiber stromlos und die Pumpe steht — unabhängig davon,
  was die Software gerade tut.

Die harten Dosiergrenzen liegen weiterhin im nichtflüchtigen Speicher und
werden bei jedem Dosierauftrag geprüft.

---

## 2. Netzliste

### 2.1 Leistung

| Netz | Von | Nach | Querschnitt | Farbe |
|---|---|---|---|---|
| +12 V | Netzteil + | TMC2209 `VMOT` | 0,5 mm² | rot |
| +12 V | Netzteil + | Buck `IN+` | 0,5 mm² | rot |
| GND-12V | Netzteil − | TMC2209 `GND` (Leistungsseite) | 0,5 mm² | schwarz |
| GND-12V | Netzteil − | Buck `IN−` | 0,5 mm² | schwarz |
| C1 | TMC2209 `VMOT` | TMC2209 `GND` | 100 µF / 25 V, **direkt am Modul** | – |
| +5 V | Buck `OUT+` | D1 Anode | 0,25 mm² | orange |
| +5 V | D1 Kathode | S3 AMOLED `VBUS` (linke Leiste) | 0,25 mm² | orange |
| +5 V | D1 Kathode | pH-Board `V+` *(nach Messung, s. Abschnitt 5)* | 0,25 mm² | orange |
| GND | Buck `OUT−` | GND-Sternpunkt | 0,5 mm² | schwarz |

**D1** = Schottky SS34 / 1N5819, Durchlassrichtung Buck → Displayboard.
Sie verhindert, dass beim gleichzeitigen Anstecken von USB und Netzteil
5 V aus dem USB in den Buck-Ausgang zurückgespeist werden.

**Strombedarf:** Der S3 mit AMOLED zieht je nach Helligkeit 150–300 mA, dazu
das pH-Board (~20 mA) und Reserve. **Buck mit mindestens 1 A auslegen.**

**Einspeisepunkt:** Die 5 V gehen auf einen der beiden **`VBUS`**-Pads der
linken Stiftleiste, GND auf ein `GND`-Pad daneben.

`VBUS` liegt board-intern parallel zur 5-V-Schiene des USB-C-Anschlusses.
Genau deshalb sitzt **D1** in der Zuleitung: ohne sie würden Netzteil und USB
gegeneinander arbeiten, sobald beide stecken. Der Akkuanschluss (JST GH
1,25 mm) bleibt frei — die Ladeelektronik wird nicht gebraucht.

### 2.2 Masse (Sternpunkt)

Alle folgenden GND müssen **auf einen gemeinsamen Punkt** (Klemmleiste oder
ein Lötstützpunkt auf der Platine):

```text
Netzteil GND · TMC2209 GND · Buck IN− und OUT− · S3 AMOLED GND
ADS1115 GND · pH-Board G
```

Leistungs-GND (Motor) und Signal-GND laufen erst am Sternpunkt zusammen —
nicht den Motorstrom über die Signalmasse führen.

### 2.3 Steuersignale S3 AMOLED → TMC2209

| S3 AMOLED | TMC2209 | Funktion |
|---|---|---|
| GPIO11 | `STEP` | Schrittimpuls |
| GPIO12 | `DIR` | Drehrichtung |
| GPIO10 | `EN` | Freigabe, **aktiv LOW** |
| 3V3 | `VIO` | Logikversorgung des Treibers |
| GND | `GND` (Logikseite) | |
| GPIO15 | `PDN/UART` | **derzeit nicht verdrahtet**, für spätere UART-Erweiterung freihalten |

Zusätzlich am TMC2209-Modul:

| Pin | Beschaltung | Wirkung |
|---|---|---|
| `MS1` | auf `VIO` (3,3 V) | zusammen mit MS2: 1/16 Microstep |
| `MS2` | auf `VIO` (3,3 V) | → 3200 Schritte pro Umdrehung |
| `SPREAD` | offen lassen | StealthChop = leiser Lauf |
| `DIAG`, `INDEX` | offen lassen | nicht genutzt |
| `R1` | 10 kΩ von `EN` nach `VIO` | Treiber bleibt beim Booten/Reset sicher gesperrt |

TMC2209-Microstep-Tabelle (MS2, MS1):

| MS2 | MS1 | Auflösung | Schritte/Umdr. (1,8°-Motor) |
|---|---|---|---|
| L | L | 1/8 | 1600 |
| L | H | 1/32 | 6400 |
| H | L | 1/64 | 12800 |
| **H** | **H** | **1/16** | **3200** ← so verdrahten |

> Praktisch gegenprüfen: 3200 Schritte müssen **genau** eine Umdrehung ergeben.

### 2.4 Motor

| TMC2209 | Motorader | Bemerkung |
|---|---|---|
| `1A` | rot | Spule 1 |
| `1B` | grün | Spule 1 |
| `2A` | blau | Spule 2 |
| `2B` | schwarz | Spule 2 |

Der 6-polige Motorstecker ist belegt: `rot | frei | grün | blau | frei | schwarz`.

**Vor dem Anschließen mit dem Multimeter prüfen** (Widerstandsmessung):

* rot ↔ grün: kleiner Widerstand (typ. 2–10 Ω) → gehören zusammen
* blau ↔ schwarz: kleiner Widerstand → gehören zusammen
* rot ↔ blau, rot ↔ schwarz usw.: **kein Durchgang**

Falls die gemessenen Paare anders liegen als vermutet, gilt die Messung.
Eine vertauschte Spule ändert nur die Drehrichtung — korrigierbar per
`set invdir 1` in der Firmware.

### 2.5 Messkette

| Von | Nach | Bemerkung |
|---|---|---|
| pH-Sonde BNC | pH-Board BNC-Buchse | Kabel kurz, nicht parallel zu Motorleitungen |
| pH-Board `V+` | +5 V *(erst nach Messung, s. Abschnitt 5)* | |
| pH-Board `G` | GND-Sternpunkt | |
| pH-Board `PO` | R2 (10 kΩ) → ADS1115 `A0` | Analogsignal |
| pH-Board `TO`, `DO` | – | nicht benötigt |
| ADS1115 `VDD` | S3 AMOLED `3V3` | **nicht 5 V!** |
| ADS1115 `GND` | GND-Sternpunkt | |
| ADS1115 `SDA` | S3 AMOLED `GPIO13` | zweiter I²C-Bus (`Wire1`) |
| ADS1115 `SCL` | S3 AMOLED `GPIO14` | zweiter I²C-Bus (`Wire1`) |
| ADS1115 `ADDR` | `GND` | ergibt I²C-Adresse 0x48 |
| ADS1115 `A1`–`A3` | frei | Reserve |

**R2 (10 kΩ in Serie zu A0)** begrenzt den Strom in die Schutzdioden des
ADS1115, falls `PO` kurzzeitig über 3,3 V steigt. Der Widerstand verfälscht
die Messung nicht nennenswert und wird durch die 2-Punkt-Kalibrierung ohnehin
mit erfasst.

#### Warum ein eigener I²C-Bus

Der Touchcontroller CST816T hängt bereits auf einem I²C-Bus (`GPIO3` = SDA,
`GPIO2` = SCL) und wird von LVGL laufend abgefragt. Adressseitig gäbe es mit
dem ADS1115 (0x48) keinen Konflikt — trotzdem bekommt der ADS1115 den
**zweiten Hardware-I²C-Bus** auf GPIO13/14.

Grund: Der ADS1115 sitzt am Ende von Kabeln, oft 10–30 cm, in der Nähe der
Motorleitungen. Diese Leitungskapazität und die eingekoppelten Störungen dem
Touchbus aufzubürden hieße, die Bedienbarkeit des Displays von der Qualität der
Sensorverkabelung abhängig zu machen. Getrennte Busse kosten zwei GPIOs und
lösen das Problem vollständig.

### 2.6 Umwälzung

Es gibt **keinen** verdrahteten Rückmelde-Eingang. Statt einen
Strömungswächter anzuschließen, wird die Anlage an denselben geschalteten
Stromkreis wie die Umwälzpumpe gehängt:

```text
Zeitschaltung / Shelly der Poolpumpe
        │
        ├──> Umwälzpumpe
        └──> 12-V-Netzteil der Dosieranlage
```

Damit kann die Anlage **physisch nicht** in stehendes Wasser dosieren — kein
Kontakt, keine Leitung, keine Software, die versagen könnte. Der Preis ist,
dass die Messung nur läuft, während die Pumpe läuft; für die Regelung ist das
kein Nachteil, weil ohne Umwälzung ohnehin nicht dosiert werden darf.

> Wird die Anlage dauerhaft versorgt, muss die Umwälzung anders sichergestellt
> werden — etwa über eine Automation, die den Sollwert-Betrieb außerhalb der
> Pumpenzeiten abschaltet.

---

## 3. GPIO-Belegung des T-Display S3 AMOLED

Variante **`BOARD_AMOLED_191`** (1,91", QSPI, Touch CST816T, ohne PMU, ohne
SD-Karte). Erkennbar daran, dass der I²C-Scan beim Booten nur `0x15` findet —
die SPI/SD-Variante hätte zusätzlich den Ladechip BQ25896 auf `0x6B`.

### Vom Board belegt — nicht anfassen

| GPIO | Funktion |
|---|---|
| 5, 6, 7, 9, 17, 18, 47, 48 | AMOLED QSPI: D3, CS, D1, TE, RES, D0, CLK, D2 |
| 8 | TFT_SDO |
| **2, 3, 21** | **Touch CST816T (SCL, SDA, IRQ)** |
| 0 | BOOT-Taster, Strapping-Pin |
| 4 | Akkuspannungsmessung (BAT ADC) |
| 38 | grüne LED / PMIC Enable |
| 19, 20 | USB D− / D+ |
| 26–37 | Flash und OPI-PSRAM |
| 45, 46 | Strapping-Pins — freihalten |

> **Achtung, verlockende Falle:** GPIO **2 und 3 sind auf der linken
> Stiftleiste herausgeführt** und sehen dort frei aus. Bei der Touch-Variante
> hängt aber der CST816T daran. Wer sie belegt, verliert die Touchbedienung.

### Für dieses Projekt vorgesehen

| GPIO | Funktion |
|---|---|
| 10 | TMC2209 `EN` (aktiv LOW, Pull-up nach 3,3 V) |
| 11 | TMC2209 `STEP` |
| 12 | TMC2209 `DIR` |
| 13 | ADS1115 `SDA` (Wire1) |
| 14 | ADS1115 `SCL` (Wire1) |
| 15 | Reserve für TMC-UART |

**Alle sieben liegen auf der linken Stiftleiste** und sind damit gegen das
offizielle Pinout bestätigt. Die Leiste führt von oben nach unten:

```text
links:   3V3 · 1 · 2 · 3 · 10 · 11 · 12 · 13 · 14 · 15 · GND · VBUS · VBUS · 16
rechts:  GND · GND · 46 · 45 · 44 · 43 · 42 · 41 · 40 · GND · GND · 3V3 · 3V3 · 39
```

Frei bleiben zusätzlich: **1** und **16** (links) sowie
**39, 40, 41, 42** (rechts).
`43`/`44` sind UART0 und gleichzeitig der Qwiic-Port, siehe unten.

### Alternative: ADS1115 über den Qwiic-Port

Das Board hat einen **STEMMA-QT/Qwiic-Anschluss** (JST-SH 1,0 mm, 4-polig) mit
`GND · 3V3 · GPIO43 · GPIO44`. Wer einen ADS1115 mit Qwiic-Buchse hat, spart
sich damit vier Lötstellen und bekommt Versorgung und Bus in einem Stecker.

Dafür in `Config.h` ändern:

```cpp
static const uint8_t PIN_I2C_SDA = 43;
static const uint8_t PIN_I2C_SCL = 44;
```

Zwei Punkte dazu:

* Die Zuordnung SDA/SCL am Stecker ist die übliche Qwiic-Reihenfolge
  (GND, 3V3, SDA, SCL) — vor dem ersten Versuch am Board gegenprüfen. Falls
  nichts gefunden wird, die beiden Pins tauschen.
* GPIO43/44 sind zugleich UART0 (TXD/RXD). Solange die Konsole über USB-CDC
  läuft, ist das unkritisch — man verliert nur die serielle Notfallebene über
  UART0. **Der Standard bleibt deshalb GPIO13/14.**

---

## 4. Anzeige und Bedienung

Display und Touch sitzen auf demselben Board — dafür ist nichts zu verdrahten.
Was die Oberfläche zeigt und wie die Dosierfreigabe per Touch abläuft, steht in
[BEDIENPANEL.md](BEDIENPANEL.md).

---

## 5. Noch zu verifizierende Punkte

Diese Punkte **vor** der endgültigen Verdrahtung klären:

1. **Herausgeführte GPIOs des Displayboards** (siehe Abschnitt 3).
2. **5-V-Einspeisepunkt am Displayboard** (siehe Abschnitt 2.1).
3. **Versorgungsspannung des pH-Boards.** Die meisten laufen mit 5 V. Steht
   auf der Platine „3.3–5 V", ist auch 3,3 V möglich — dann liegt `PO` sicher
   im ADS-Bereich, das Signal wird aber kleiner.
4. **Spannungsbereich von `PO`.** Board mit Sonde in pH-7-Puffer betreiben und
   `PO` gegen `G` messen, danach in pH-4-Puffer. Beide Werte notieren.
   * Maximalwert ≤ 3,2 V → direkt über R2 an `A0`.
   * Maximalwert > 3,2 V → zusätzlich Spannungsteiler (z. B. 10 kΩ / 20 kΩ).
     Die Kalibrierung rechnet den Teiler automatisch heraus.
5. **Potentiometer auf dem pH-Board** nicht verstellen, solange die Funktion
   nicht geklärt ist. Position vorher fotografieren.
6. **I²C-Pull-ups am ADS1115.** Fast alle Breakouts haben 10 kΩ nach VDD an
   Bord. Stromlos zwischen `SDA` und `VDD` messen: ca. 10 kΩ → gut. Fehlen sie,
   je 4,7 kΩ von SDA und SCL nach 3,3 V ergänzen.
