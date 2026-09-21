# Schaltplan / Netzliste

Grafik: [schaltplan.svg](schaltplan.svg) (im Browser oder in Inkscape öffnen, druckbar auf A3)

**Ein Gerät:** Der LilyGo T-Display S3 AMOLED übernimmt alles — Messung,
Regelung, Pumpenansteuerung, Anzeige, Bedienung und Webinterface. Ein
separater ESP32-C3 wird nicht mehr verwendet.

> **Ab Firmware 2.3.0: Pumpe an Netzspannung statt Schrittmotor.** Die Pumpe
> ist jetzt ein **AC-Synchronmotor** (230 V, konstante Drehzahl), der über ein
> **1-Kanal-Relais** nur ein- und ausgeschaltet wird. TMC2209, NEMA17, das
> 12-V-Netzteil und der Buck-Converter entfallen. Die **Dosiermenge ergibt sich
> aus der Laufzeit** (ml/s, per Testlauf kalibriert), nicht mehr aus Schritten.

---

## ⚠️ Sicherheit: Netzspannung

Dieser Aufbau schaltet **230 V** direkt. Das ist kein Kleinspannungsprojekt
mehr:

* Aufbau, Absicherung und Inbetriebnahme der Netzseite gehören in die Hand
  einer **elektrotechnisch befähigten Person**. Im Zweifel eine Fachkraft
  hinzuziehen.
* Alles Netzführende (L, N, Relaiskontakte, TSP-05-Eingang, Motorleitung) muss
  in ein **geschlossenes, berührungssicheres Gehäuse** mit Zugentlastung und
  ausreichenden **Luft- und Kriechstrecken** (mind. 3 mm) zur Kleinspannung.
* Eine **Vorsicherung** (träge, passend zur Last — 1 A genügt hier reichlich)
  in die Phase, vor TSP-05 und Relais.
* Schutzleiter (PE) anschließen, wo der Motor/Aufbau ihn vorsieht.
* **Nur spannungsfrei arbeiten** — Netzstecker ziehen, nicht nur schalten.

Die Firmware-Grenzen sind die **zweite** Verteidigungslinie, nicht die erste.

---

## 1. Übersicht

```text
     GALVANISCH GETRENNTE MESSSEITE          |        NETZBEZOGENE SEITE
                                             |
pH-Sonde ─BNC─> pH-Board ─PO─> R2 ─> ADS1115 |
                                        │    |
                                    SDA/SCL ─┼─ ISO1540 ─> GPIO13/14 (Wire1)
                                             |                  │
                                             |                  v
230 V ──┬── TSP-05 (AC/DC) ─5 V─D1─┬─> T-Display S3 AMOLED ─3V3─> ISO1540 VCC1
 (L,N)  │                          ├─> Relais-Modul (+ / −), S <── GPIO10
        │                          └─> B0509S ─9 V iso─> AMS1117-5.0 ─> pH-Board V+,
        │                                                              ADS1115 VDD,
        │                                                              ISO1540 VCC2
        │
        └── L ─> Relais COM ── NO ─> AC-Synchronmotor ─> N   (Peristaltikpumpe)
```

Die senkrechte Linie ist die Trennstelle der **Messkette**. Über sie gehen
**nur** SDA und SCL im ISO1540 und die Energie im Übertrager des B0509S —
**keine Masse.**

Die **Netzseite** (230 V) und die 5-V-Logik trennt das **Relais selbst**: seine
Spule (5 V) und seine Kontakte (230 V) sind im Bauteil isoliert. Die Logik
berührt die Netzspannung nirgends.

### Was das für die Sicherheit bedeutet

Derselbe Chip zeichnet das Display und steuert die Pumpe. Drei Dinge sorgen
dafür, dass die Pumpe im Zweifel **steht**:

* Das Relais schaltet über den **Schließer (NO)**. Ist die Spule stromlos —
  bei Reset, Absturz oder Stromausfall —, ist der Motorkreis offen.
* Der Steuereingang `S` bekommt einen **10 kΩ Pulldown nach GND**. Während der
  S3 bootet (GPIO kurz hochohmig), bleibt die Spule sicher stromlos.
* Die Firmware lädt **zuerst** die Einstellungen und setzt dann den
  Relais-Ruhepegel (aus) — die Polarität steht fest, bevor der Ausgang treibt
  (siehe `RelayPump::begin()`).

Die harten Dosiergrenzen (max. Einzeldosis, Tagesmenge, 180 s Dauerlauf,
Sperre unter pH 6,20) liegen weiterhin im nichtflüchtigen Speicher und werden
bei jedem Dosierauftrag geprüft.

---

## 2. Netzliste

### 2.1 Versorgung 230 V → 5 V

Das **TSP-05** (bzw. ein gleichwertiges AC/DC-Modul) macht aus der Netzspannung
die 5 V für die gesamte Kleinspannungsseite.

| Netz | Von | Nach | Bemerkung |
|---|---|---|---|
| L (230 V) | Netz/Sicherung | TSP-05 `AC` | über Vorsicherung |
| N (230 V) | Netz | TSP-05 `AC` | AC ist ungepolt |
| +5 V | TSP-05 `+Vo` | D1 Anode | |
| +5 V | D1 Kathode | S3 AMOLED `VBUS` (linke Leiste) | anti-Backfeed gegen USB |
| +5 V | D1 Kathode | Relais-Modul `+` | Spulenversorgung |
| +5 V | D1 Kathode | B0509S `+Vin` | speist die isolierte Messseite |
| +5 V | TSP-05 `+Vo` | **C_bulk 470–1000 µF** + 100 nF | direkt an der 5-V-Schiene |
| GND | TSP-05 `−Vo` | GND-Sternpunkt | |

> **Reserve des Netzteils.** Das TSP-05 liefert **3 W ≈ 600 mA**. Der S3 mit
> AMOLED zieht 150–300 mA, WLAN-Sendespitzen gehen bis ~500 mA (bei schwachem
> Empfang läuft der Sender auf Volllast), dazu die Relaisspule (~70–80 mA) und
> die Messseite (~25 mA). Damit ist das TSP-05 **grenzwertig**: ohne
> Stützkondensator drohen Brownouts/Resets beim Senden oder beim Anziehen des
> Relais. **Empfehlung: mindestens 470 µF (besser 1000 µF) an die 5-V-Schiene**
> — und bei anhaltenden Resets ein größeres 5-V-Netzteil (**≥ 1 A / ≥ 5 W**,
> z. B. HLK-10M05).

**D1** = Schottky SS34 / 1N5819, Durchlassrichtung TSP-05 → Displayboard.
Sie verhindert, dass beim gleichzeitigen Anstecken von USB und Netzteil 5 V aus
dem USB in den TSP-05-Ausgang zurückgespeist werden.

**Einspeisepunkt:** Die 5 V gehen auf einen der beiden **`VBUS`**-Pads der
linken Stiftleiste, GND auf ein `GND`-Pad daneben. `VBUS` liegt board-intern
parallel zur 5-V-Schiene des USB-C-Anschlusses — genau deshalb sitzt D1 in der
Zuleitung. Der Akkuanschluss (JST GH 1,25 mm) bleibt frei.

### 2.2 Pumpe: Relais und Motor (230 V)

| Netz | Von | Nach | Bemerkung |
|---|---|---|---|
| Steuerung | S3 `GPIO10` | Relais `S` | + 10 kΩ Pulldown `S` → GND |
| +5 V | 5-V-Schiene | Relais `+` | Spulenversorgung |
| GND | GND-Sternpunkt | Relais `−` | gemeinsame Masse mit dem S3 |
| L (230 V) | Netz/Sicherung | Relais `COM` | geschaltete Phase |
| L geschaltet | Relais `NO` | Motor L | **NO**, nicht NC → Ruhezustand = aus |
| N (230 V) | Netz | Motor N | |

Das Relaismodul (TONGLING JQC-3FF-S-Z, 5-V-Spule, Kontakte 10 A/250 V) hat den
3-poligen Steuerheader **`S · + · −`** (Signal, +5 V, GND) und die Schraubklemme
**`NC · COM · NO`**. Verwendet wird **`COM` + `NO`**: bei stromloser Spule ist
der Kontakt offen, der Motor steht.

> **Schaltlogik prüfen (aktiv-LOW/HIGH).** Dieses KY-019-Board hat **keinen
> Optokoppler** (nur LED + Freilaufdiode, direkte Transistoransteuerung) und ist
> damit typischerweise **aktiv-HIGH**: `S` HIGH → Relais an. Die Firmware ist
> per `settings.relayInvert` umstellbar (Vorgabe aktiv-LOW). **Vor dem
> Anschluss der Netzseite** mit abgezogener Pumpe testen: `run 3` — das Relais
> muss anziehen und nach 3 s von selbst abfallen. Fällt es verkehrt, `set rinv`
> umschalten (bzw. „Relais invertieren" im Webinterface).

> **3,3-V-Ansteuerung.** Gerade weil es kein Opto-Board ist, treibt der
> 3,3-V-GPIO den Transistor sauber. Der 10 kΩ Pulldown an `S` stellt sicher,
> dass die Spule beim Booten/Reset nicht ungewollt anzieht.

### 2.3 Masse (Sternpunkt)

Alle folgenden GND müssen **auf einen gemeinsamen Punkt** (Klemmleiste oder ein
Lötstützpunkt auf der Platine):

```text
TSP-05 −Vo · S3 AMOLED GND · Relais − · B0509S −Vin · ISO1540 GND1
```

Der isolierte Massepunkt `GND iso` der Messseite läuft **nicht** hier zusammen
(siehe 2.5). Die 230-V-Masse existiert nicht — N ist der Netz-Neutralleiter und
wird **nie** mit dem Sternpunkt verbunden.

### 2.4 Steuersignal S3 AMOLED → Relais

| S3 AMOLED | Relais-Modul | Funktion |
|---|---|---|
| GPIO10 | `S` | Schaltsignal (+ 10 kΩ Pulldown nach GND) |
| 3V3/5V | `+` | **5 V** (Spulenversorgung, nicht 3,3 V!) |
| GND | `−` | gemeinsame Masse |

> Die 5-V-Spule des JQC-3FF-S-Z zieht erst ab ~3,75 V sicher an — das Modul-`+`
> gehört an **5 V**, nicht an 3,3 V. Nur das **Signal** `S` kommt vom 3,3-V-GPIO.

Die früheren Pins `GPIO11` (STEP), `GPIO12` (DIR) und `GPIO15` (UART-Reserve)
sind jetzt **frei**.

### 2.5 Messkette (unverändert, isoliert)

**Die gesamte Messkette liegt auf der isolierten Seite.** Ihre Masse heißt hier
`GND iso` und ist ein eigener Lötstützpunkt, **nicht** der Sternpunkt. Versorgt
wird sie über B0509S → AMS1117 aus der 5-V-Schiene des TSP-05 — an der Messkette
selbst ändert der Pumpen-Umbau **nichts**.

| Von | Nach | Bemerkung |
|---|---|---|
| pH-Sonde BNC | pH-Board BNC-Buchse | Kabel kurz, nicht parallel zu Netzleitungen |
| pH-Board `V+` | +5 V iso (AMS1117 `OUT`) | |
| pH-Board `G` | **GND iso** | nicht an den Sternpunkt! |
| pH-Board `PO` | R2 (10 kΩ) → ADS1115 `A0` | Analogsignal |
| pH-Board `TO`, `DO` | – | nicht benötigt |
| ADS1115 `VDD` | +5 V iso | dieselbe Schiene wie das pH-Board |
| ADS1115 `GND` | **GND iso** | nicht an den Sternpunkt! |
| ADS1115 `SDA` | ISO1540 `SDA2` | |
| ADS1115 `SCL` | ISO1540 `SCL2` | |
| ADS1115 `ADDR` | `GND iso` | ergibt I²C-Adresse 0x48 |
| ADS1115 `A1`–`A3` | frei | Reserve |

Isolierte Versorgung:

| Netz | Von | Nach | Bemerkung |
|---|---|---|---|
| +5 V | 5-V-Schiene (D1-Kathode) | B0509S `+Vin` | letzte Verbindung zur Netzseite |
| GND | GND-Sternpunkt | B0509S `−Vin` | |
| +9 V iso | B0509S `+Vout` | AMS1117-5.0 `IN` | 10 µF direkt am Wandlerausgang |
| GND iso | B0509S `−Vout` | **isolierter Massepunkt** | eigener Lötstützpunkt |
| +5 V iso | AMS1117 `OUT` | pH-Board `V+`, ADS1115 `VDD`, ISO1540 `VCC2` | 22 µF + 100 nF am Ausgang |

**Warum nicht B0505S?** Ein ungeregelter 1-W-Wandler steigt bei geringer Last
über seine Nennspannung. Der Bedarf hier liegt bei rund 25 mA von 200 mA, also
12 % Last — da sind 5,5 bis 6 V zu erwarten, und der ADS1115 ist für maximal
5,5 V spezifiziert. Der Umweg über 9 V und einen Linearregler kostet 50 Cent
und liefert lastunabhängig saubere 5 V. Der Regler dämpft nebenbei die
100-kHz-Welligkeit des Wandlers.

Und die Trennstelle selbst:

| Von | Nach | Bemerkung |
|---|---|---|
| ISO1540 `VCC1` | S3 AMOLED `3V3` | netzbezogene Seite |
| ISO1540 `GND1` | GND-Sternpunkt | |
| ISO1540 `SDA1` | S3 AMOLED `GPIO13` | `Wire1` |
| ISO1540 `SCL1` | S3 AMOLED `GPIO14` | `Wire1` |
| ISO1540 `VCC2` | +5 V iso | isolierte Seite |
| ISO1540 `GND2` | GND iso | |
| Pull-up | `SDA1`/`SCL1` → 3,3 V | je 4,7 kΩ, **falls das Modul keine mitbringt** |

**Die Pull-ups auf Seite 1 sind die häufigste Fehlerquelle beim Umbau.** Die
10 kΩ des ADS1115-Breakouts sitzen hinter der Trennstelle — die ESP32-Seite
steht ohne Pull-up da und der Bus ist tot. Das sieht aus wie ein defekter
Isolator und ist keiner.

**R2 (10 kΩ in Serie zu A0)** begrenzt den Strom in die Schutzdioden des
ADS1115, falls `PO` kurzzeitig über die Versorgung steigt. Der Widerstand
verfälscht die Messung nicht nennenswert und wird durch die 2-Punkt-
Kalibrierung ohnehin mit erfasst.

#### Warum ein eigener I²C-Bus

Der Touchcontroller CST816T hängt bereits auf einem I²C-Bus (`GPIO3` = SDA,
`GPIO2` = SCL) und wird von LVGL laufend abgefragt. Der ADS1115 bekommt trotz
fehlenden Adresskonflikts den **zweiten Hardware-I²C-Bus** auf GPIO13/14: Er
sitzt am Ende von Kabeln in der Nähe der (jetzt netzführenden) Motorleitung.
Diese Störungen dem Touchbus aufzubürden hieße, die Bedienbarkeit des Displays
von der Qualität der Sensorverkabelung abhängig zu machen.

### 2.6 Umwälzung

Es gibt **keinen** verdrahteten Rückmelde-Eingang. Statt einen Strömungswächter
anzuschließen, wird die Anlage an denselben geschalteten Stromkreis wie die
Umwälzpumpe gehängt:

```text
Zeitschaltung / Shelly der Poolpumpe
        │
        ├──> Umwälzpumpe
        └──> 230-V-Versorgung der Dosieranlage (TSP-05 + Dosierpumpe)
```

Damit kann die Anlage **physisch nicht** in stehendes Wasser dosieren — sie hat
schlicht keinen Strom, wenn die Umwälzpumpe steht. Der Preis ist, dass die
Messung nur läuft, während die Pumpe läuft; für die Regelung ist das kein
Nachteil, weil ohne Umwälzung ohnehin nicht dosiert werden darf. Das ist der
Grund, warum der gleitende Mittelwert nach jedem Einschalten erst wieder
einige Minuten füllen muss, bevor dosiert wird (siehe INBETRIEBNAHME.md).

Wird die Anlage dauerhaft versorgt — etwa weil der pH-Wert auch außerhalb der
Pumpenzeiten sichtbar sein soll —, prüft die Firmware die Umwälzung stattdessen
**über eine Home-Assistant-Entität**. Details in
[HOMEASSISTANT.md](HOMEASSISTANT.md), Abschnitt 4.

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
| 10 | Relais `S` (Schaltsignal, 10 kΩ Pulldown nach GND) |
| 13 | ADS1115 `SDA` (Wire1, über ISO1540) |
| 14 | ADS1115 `SCL` (Wire1, über ISO1540) |
| 11, 12, 15 | **frei** (früher STEP/DIR/UART) |

**Alle liegen auf der linken Stiftleiste** und sind damit gegen das offizielle
Pinout bestätigt. Die Leiste führt von oben nach unten:

```text
links:   3V3 · 1 · 2 · 3 · 10 · 11 · 12 · 13 · 14 · 15 · GND · VBUS · VBUS · 16
rechts:  GND · GND · 46 · 45 · 44 · 43 · 42 · 41 · 40 · GND · GND · 3V3 · 3V3 · 39
```

Frei bleiben zusätzlich: **1, 11, 12, 15, 16** (links) sowie
**39, 40, 41, 42** (rechts). `43`/`44` sind UART0 und gleichzeitig der
Qwiic-Port, siehe unten.

### Alternative: ADS1115 über den Qwiic-Port

Das Board hat einen **STEMMA-QT/Qwiic-Anschluss** (JST-SH 1,0 mm, 4-polig) mit
`GND · 3V3 · GPIO43 · GPIO44`.

> Mit galvanischer Trennung scheidet der Qwiic-Port allerdings aus: er führt
> Masse und 3,3 V der netzbezogenen Seite direkt heran und würde die Trennstelle
> der Messkette überbrücken. Er bleibt nur eine Option, wenn **ohne** Isolation
> gearbeitet wird — davon ist im Beckenbetrieb abzuraten.

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
3. **Schaltlogik des Relaismoduls** (aktiv-LOW/HIGH) mit `run 3` und abgezogener
   Pumpe prüfen, `set rinv` entsprechend setzen (siehe Abschnitt 2.2).
4. **Reserve des 5-V-Netzteils** unter WLAN-Last messen: fällt die 5-V-Schiene
   beim Senden oder beim Anziehen des Relais unter ~4,7 V, Stützkondensator
   vergrößern oder ein stärkeres Netzteil einsetzen.
5. **Versorgungsspannung des pH-Boards.** Die meisten laufen mit 5 V. Steht
   auf der Platine „3.3–5 V", ist auch 3,3 V möglich — dann liegt `PO` sicher
   im ADS-Bereich, das Signal wird aber kleiner.
6. **Spannungsbereich von `PO`.** Board mit Sonde in pH-7-Puffer betreiben und
   `PO` gegen `G` messen, danach in pH-4-Puffer. Beide Werte notieren.
   * Maximalwert ≤ 3,2 V → direkt über R2 an `A0`.
   * Maximalwert > 3,2 V → zusätzlich Spannungsteiler (z. B. 10 kΩ / 20 kΩ).
     Die Kalibrierung rechnet den Teiler automatisch heraus.
7. **Potentiometer auf dem pH-Board** nicht verstellen, solange die Funktion
   nicht geklärt ist. Position vorher fotografieren.
8. **I²C-Pull-ups auf Seite 1 des ISO1540** (je 4,7 kΩ nach 3,3 V), falls das
   Isolatormodul keine mitbringt — sonst bleibt der Bus tot.
