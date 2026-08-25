# Schaltplan / Netzliste

Grafik: [schaltplan.svg](schaltplan.svg) (im Browser oder in Inkscape öffnen, druckbar auf A3)

---

## 1. Übersicht

```text
pH-Sonde ──BNC──> pH-Signalboard ──PO──> R2 10k ──> ADS1115 A0
                                                        │ I²C (GPIO5/6)
                                                        v
12 V ──┬──> TMC2209 VMOT ──> NEMA17 ──> Peristaltikpumpe
       │         ^
       │         │ STEP/DIR/EN (GPIO3/4/7)
       │         │
       └──> Buck 5 V ──D1──> ESP32-C3 5V ──3V3──> ADS1115, TMC2209 VIO
                     └──> pH-Board V+
```

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
| +5 V | D1 Kathode | ESP32-C3 `5V` | 0,25 mm² | orange |
| +5 V | D1 Kathode | pH-Board `V+` *(nach Messung, s. u.)* | 0,25 mm² | orange |
| GND | Buck `OUT−` | GND-Sternpunkt | 0,5 mm² | schwarz |

**D1** = Schottky SS34 / 1N5819, Durchlassrichtung Buck → ESP32.
Sie verhindert, dass beim gleichzeitigen Anstecken von USB und Netzteil
5 V aus dem USB in den Buck-Ausgang zurückgespeist werden.

### 2.2 Masse (Sternpunkt)

Alle folgenden GND müssen **auf einen gemeinsamen Punkt** (Klemmleiste oder
ein Lötstützpunkt auf der Platine):

```text
Netzteil GND · TMC2209 GND · Buck IN− und OUT− · ESP32-C3 GND
ADS1115 GND · pH-Board G
```

Leistungs-GND (Motor) und Signal-GND laufen erst am Sternpunkt zusammen —
nicht den Motorstrom über die Signalmasse führen.

### 2.3 Steuersignale ESP32-C3 → TMC2209

| ESP32-C3 | TMC2209 | Funktion |
|---|---|---|
| GPIO3 | `STEP` | Schrittimpuls |
| GPIO4 | `DIR` | Drehrichtung |
| GPIO7 | `EN` | Freigabe, **aktiv LOW** |
| 3V3 | `VIO` | Logikversorgung des Treibers |
| GND | `GND` (Logikseite) | |
| GPIO10 | `PDN/UART` | **derzeit nicht verdrahtet**, für spätere UART-Erweiterung freihalten |

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

> Die Tabelle unbedingt praktisch gegenprüfen: mit `tools/motor_test`
> `m 3200` setzen und `t` ausführen — der Motor muss **genau** eine
> Umdrehung machen.

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
Eine vertauschte Spule (z. B. 1A/1B getauscht) ändert nur die Drehrichtung —
das lässt sich später per `set invdir 1` in der Firmware korrigieren.

### 2.5 Messkette

| Von | Nach | Bemerkung |
|---|---|---|
| pH-Sonde BNC | pH-Board BNC-Buchse | Kabel kurz, nicht parallel zu Motorleitungen |
| pH-Board `V+` | +5 V *(erst nach Messung, s. Abschnitt 4)* | |
| pH-Board `G` | GND-Sternpunkt | |
| pH-Board `PO` | R2 (10 kΩ) → ADS1115 `A0` | Analogsignal |
| pH-Board `TO` | – | nicht benötigt |
| pH-Board `DO` | – | nicht benötigt (digitaler Schwellwertausgang) |
| ADS1115 `VDD` | ESP32-C3 `3V3` | **nicht 5 V!** |
| ADS1115 `GND` | ESP32-C3 `GND` | |
| ADS1115 `SDA` | ESP32-C3 `GPIO5` | |
| ADS1115 `SCL` | ESP32-C3 `GPIO6` | |
| ADS1115 `ADDR` | `GND` | ergibt I²C-Adresse 0x48 |
| ADS1115 `A1`–`A3` | frei | Reserve |

**R2 (10 kΩ in Serie zu A0)** begrenzt den Strom in die Schutzdioden des
ADS1115, falls `PO` kurzzeitig über 3,3 V steigt. Der Widerstand
verfälscht die Messung nicht nennenswert (Eingangsstrom des ADS1115 im
nA-Bereich) und wird durch die 2-Punkt-Kalibrierung ohnehin mit erfasst.

### 2.6 Optional: Umwälz-Rückmeldung

| ESP32-C3 | Nach | Bemerkung |
|---|---|---|
| GPIO1 | potenzialfreier Kontakt | zweite Seite des Kontakts an GND |

Der Pin ist in der Firmware mit internem Pull-up konfiguriert.
Standard: **Kontakt geschlossen = Umwälzung läuft**. Über
`set flowinv 1` lässt sich die Logik umdrehen. Aktiviert wird die
Auswertung mit `set flowreq 1` bzw. im Webinterface.

Es darf **keine Netzspannung** an GPIO1 gelangt — nur ein potenzialfreier
Relais- oder Strömungswächterkontakt.

---

## 3. Genutzte GPIOs — und warum

| GPIO | Funktion | Hinweis |
|---|---|---|
| 1 | Umwälz-Eingang | ADC-fähig, frei nutzbar |
| 3 | STEP | |
| 4 | DIR | |
| 5 | SDA | |
| 6 | SCL | |
| 7 | EN | mit Pull-up nach 3,3 V |
| 8 | Onboard-LED | invertiert (LOW = an), Strapping-Pin |
| 9 | **frei lassen** | BOOT-Strapping-Pin |
| 10 | reserviert für TMC-UART | derzeit unbeschaltet |
| 20/21 | UART0 | bleibt frei |

---

## 4. Noch zu verifizierende Punkte (aus der Projektbeschreibung)

Diese Messungen **vor** der endgültigen Verdrahtung durchführen:

1. **Versorgungsspannung des pH-Boards.** Die meisten dieser Boards laufen
   mit 5 V. Steht auf der Platine „3.3–5 V", ist auch 3,3 V möglich —
   dann liegt `PO` sicher im ADS-Bereich, das Signal wird aber kleiner.
   Betriebsspannung erst anlegen, wenn die Beschriftung eindeutig ist.
2. **Spannungsbereich von `PO`.** Board mit Sonde in pH-7-Puffer betreiben
   und `PO` gegen `G` messen. Typisch liegen ~2,5 V (bei 5 V Versorgung)
   für pH 7 an; pH 4 gibt eine höhere, pH 10 eine niedrigere Spannung.
   Dann die Sonde in pH-4-Puffer stellen und den zweiten Wert notieren.
3. **Ergebnis auswerten:**
   * Maximalwert ≤ 3,2 V → direkt über R2 an `A0`, nichts weiter nötig.
   * Maximalwert > 3,2 V → zusätzlich Spannungsteiler
     (z. B. 10 kΩ / 20 kΩ, Teiler 1:1,5) zwischen `PO` und `A0` einbauen.
     Die Kalibrierung rechnet den Teiler automatisch mit heraus.
4. **Potentiometer auf dem pH-Board** nicht verstellen, solange die
   Funktion nicht geklärt ist. Üblich sind: ein Poti für den Offset des
   Analogausgangs, eines für die Schaltschwelle von `DO`. Position vor
   dem ersten Verdrehen fotografieren.
5. **I²C-Pull-ups.** Fast alle ADS1115-Breakouts haben 10 kΩ nach VDD
   bereits an Bord. Mit dem Multimeter zwischen `SDA` und `VDD` messen
   (Modul stromlos): ca. 10 kΩ → alles gut. Ist kein Pull-up vorhanden,
   je 4,7 kΩ von SDA und SCL nach 3,3 V ergänzen.
