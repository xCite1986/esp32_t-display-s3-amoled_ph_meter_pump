# Inbetriebnahme

Die Phasen folgen der Roadmap aus `ph_minus_dosieranlage_entwicklung.md`.
Jede Phase hat ein klares Abbruchkriterium — nicht weitermachen, solange
eine Phase nicht sauber durchläuft.

---

## 0. Werkzeugkette

`arduino-cli` muss nicht separat installiert werden — die Arduino IDE 2
bringt eine mit, die Skripte finden sie automatisch.

Vorhanden auf diesem Rechner:

* `arduino-cli 1.5.1` (gebündelt in `C:\Program Files\Arduino IDE\...`)
* ESP32-Core `esp32:esp32 3.3.8`
* Board-FQBN:
  `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB`
* Port: `COM6`

Benötigte Bibliotheken: `LilyGo-AMOLED-Series`, `lvgl 8.4`, `ArduinoJson`,
`XPowersLib`, `SensorLib` (**Version 0.3.3** — 0.4.1 ist defekt, siehe
[BEDIENPANEL.md](BEDIENPANEL.md), Abschnitt 4). Der ADS1115-Treiber liegt als
eigenes Modul bei und braucht keine Fremdbibliothek.

Kurztest:

```bash
powershell -File scripts/build.ps1
```

---

## Phase 1 — ADS1115 und pH-Messung

**Voraussetzung:** Lötanleitung Abschnitte 1–7 erledigt.
TMC2209 nicht gesteckt, 12 V aus.

```bash
powershell -File scripts/flash.ps1 -Sketch i2c
```

```bash
powershell -File scripts/monitor.ps1
```

> Der Touchcontroller des Displays hängt auf dem **anderen** I²C-Bus
> (GPIO2/3) und taucht in diesem Scan nicht auf. Das ist richtig so.

Erwartete Ausgabe:

```text
=== Phase 1: I2C / ADS1115 Test ===
SDA=GPIO13  SCL=GPIO14  Adresse 0x48  Bereich +/-4.096 V
I2C-Scan...
  gefunden: 0x48   <- sieht nach ADS1115 aus
A0:   XXXXX   X.XXXX V   A1: ...
```

**Abbruchkriterium:** 0x48 wird gefunden und A0 liefert stabile Werte.

Danach das pH-Board anschließen (Lötanleitung Abschnitt 8) und die
Sonde in pH-7-Puffer stellen. Die Spannung an A0 muss ruhig stehen
(Schwankung < 5 mV über 30 s). Springt sie, liegt es am Kabelweg —
siehe Fehlertabelle in der Lötanleitung.

---

## Phase 2 — Motor und Treiber

**Voraussetzung:** VREF eingestellt (Lötanleitung Abschnitt 9),
Motor angeschlossen (Abschnitt 10), Pumpenkopf **noch nicht** montiert.

```bash
powershell -File scripts/flash.ps1 -Sketch motor
```

Danach 12 V einschalten und im Monitor:

| Eingabe | Wirkung |
|---|---|
| `m 3200` | 3200 Schritte pro Umdrehung annehmen |
| `s 400` | langsame Schrittrate zum Anfangen |
| `t` | eine Umdrehung vor, eine zurück |
| `r 800` | 800 Schritte vorwärts |
| `l 800` | 800 Schritte rückwärts |
| `e 0` | Treiber abschalten |

Zu prüfen:

1. **Dreht der Motor überhaupt?** Brummt er nur, sind die Spulenpaare
   vertauscht — 12 V aus, Paare erneut durchmessen.
2. **Ist eine Umdrehung wirklich eine Umdrehung?** Markierung auf die
   Welle kleben, `t` ausführen. Passt es nicht, stimmt die
   MS1/MS2-Beschaltung nicht.
3. **Drehrichtung.** `r` soll die Richtung sein, in der die Pumpe später
   fördert. Ist es andersherum, später in der Firmware `set invdir 1`
   setzen — nicht umlöten.
4. **Schrittrate hochtasten:** `s 800`, `s 1200`, `s 1600`. Sobald der
   Motor Schritte verliert oder stehenbleibt, eine Stufe zurückgehen.
   Der gefundene Wert kommt später in `set srate`.
5. **VREF nachjustieren**, falls unter Last Schritte verloren gehen:
   in 0,05-V-Schritten erhöhen, Motortemperatur im Auge behalten.

**Abbruchkriterium:** reproduzierbare, saubere Umdrehungen bei der
gewünschten Schrittrate.

---

## Phase 3 — Pumpenkopf und Schritte/ml

Jetzt den Peristaltikkopf montieren und die Hauptfirmware flashen:

```bash
powershell -File scripts/flash.ps1
```

```bash
powershell -File scripts/monitor.ps1
```

Vorgehen (mit **Wasser**, nicht mit Säure):

1. Saug- und Druckschlauch einlegen, Saugseite in ein Wasserglas.
2. Entlüften: `steps 20000` — so lange wiederholen, bis blasenfrei Wasser
   am Ausgang kommt.
3. Messgefäß (10-ml-Spritze oder Messzylinder) an den Ausgang.
4. Definierte Zahl fahren: `steps 16000`
5. Geförderte Menge ablesen, z. B. 9,4 ml.
6. Faktor berechnen lassen: `spml 16000 9.4`
   → Firmware speichert `1702,1 Schritte/ml`.
7. Gegenprobe: `dose 5` — es müssen ca. 5 ml kommen.
8. Bei Abweichung > 3 % Schritt 4–6 wiederholen. Peristaltikpumpen
   fördern erst nach ein paar Minuten Einlaufzeit reproduzierbar.

**Abbruchkriterium:** drei aufeinanderfolgende Dosierungen von 5 ml
liegen innerhalb ±3 %.

---

## Phase 4 — pH-Kalibrierung

1. Sonde mit destilliertem Wasser spülen, abtupfen (nicht abreiben).
2. In **pH-7,00-Puffer** stellen, 2–3 Minuten warten.
3. Werte beobachten: `mon 60`
   Die Spannung muss stehen (Spanne < 20 mV), sonst nimmt die Firmware
   den Punkt nicht an.
4. `cal a 7.00`
5. Sonde spülen, in **pH-4,00-Puffer**, wieder 2–3 Minuten warten.
6. `cal b 4.00`
7. `status` prüfen: „Kalibrierung gueltig" und eine plausible Steilheit.

**Plausible Steilheit:** theoretisch −59,2 mV/pH bei 25 °C an der Elektrode.
Das pH-Board verstärkt und invertiert das Signal, deshalb ist der von der
Firmware angezeigte Wert boardabhängig. Wichtig ist:

* Der Betrag liegt in einer sinnvollen Größenordnung (typisch 150–350 mV/pH
  bei diesen Boards).
* Er ändert sich zwischen zwei Kalibrierungen nicht sprunghaft.
  Fällt er im Lauf der Zeit deutlich ab, ist die Sonde am Ende ihrer
  Lebensdauer.

**Abbruchkriterium:** Sonde zurück in pH-7-Puffer → angezeigter Wert
7,00 ± 0,05.

> Kalibrierung mindestens alle 4–8 Wochen wiederholen. Eine driftende Sonde
> ist die häufigste Ursache für Fehldosierungen.

---

## Phase 5 — Regelung scharf schalten

Erst jetzt Parameter setzen. Empfohlene Startwerte für einen Pool:

```text
set sp 7.20         pH-Sollwert
set db 0.05         Totband
set dose 3.0        Einzeldosis in ml
set maxs 5.0        maximale Einzeldosis
set maxd 60.0       maximale Tagesmenge
set pause 900       15 min Durchmischung zwischen Dosierungen
set phlock 6.80     unter pH 6,80 wird nie dosiert
set phmax 9.50      darüber gilt der Messwert als unplausibel
set srate 1200      in Phase 2 ermittelte Schrittrate
```

Die Werte für `maxd` müssen zum Beckenvolumen passen. Faustregel für die
erste Woche: **die Tagesmenge so klein wählen, dass sie den pH rechnerisch
um höchstens 0,2 senken kann.** Lieber mehrere Tage regeln lassen, als
einmal überdosieren.

Dann:

```text
auto on
```

**Was jetzt passiert:** Die Firmware misst, prüft die Plausibilität, gibt
eine einzelne Dosis ab, wartet die Durchmischungszeit ab, misst erneut.
Es wird nie „auf den Sollwert durchdosiert".

Die ersten Zyklen mitprotokollieren (`status` bzw. Webinterface) und
gegen eine manuelle Referenzmessung mit Tröpfchentest oder Fotometer
prüfen. Erst wenn drei Zyklen plausibel verlaufen, die Anlage
unbeaufsichtigt laufen lassen.

---

## Phase 6 — WLAN und Webinterface

```text
wifi MeinWLAN meinPasswort
```

Der ESP32 startet neu und verbindet sich. Danach:

* `http://ph-dosierung.local/` oder die IP aus `status`
* Ohne WLAN-Daten (oder bei fehlgeschlagener Verbindung) öffnet die
  Anlage einen Einrichtungs-Access-Point mit Konfigurationsseite:
  SSID `pH-Panel`, Passwort `panel1234`, IP `192.168.4.1`.
  Das Display zeigt diese Angaben dann groß an.

### Anzeige einstellen

Im Webinterface unter *Anzeige*:

| Feld | Bedeutung | Standard |
|---|---|---|
| Standby nach [s] | ohne Berührung in die sparsame Ansicht | 300 |
| Position versetzen alle [s] | Einbrennschutz im Standby | 300 |
| Nachtabschaltung | Display nachts ganz aus | ein |
| Nacht von / bis [Stunde] | Fenster der Abschaltung | 20 / 5 |
| Umdrehungen pro Touch-Freigabe | was ein Tipp + Bestätigung auslöst | 5 |

Der Nachtmodus greift nur bei gültiger Uhrzeit — ohne NTP bleibt es beim
Standby. Eine Berührung weckt immer auf; dieser erste Tipp löst bewusst
nichts aus.

**Zugriffsschutz setzen**, bevor die Anlage dauerhaft im Netz hängt:
im Webinterface unter *Netzwerk* einen Web-Benutzer und ein Passwort
eintragen. Ohne Login kann jeder im Netz dosieren lassen.

Das Web-Passwort dient gleichzeitig als OTA-Passwort. Danach sind Updates
auch ohne USB möglich:

```bash
powershell -File scripts/build.ps1
```

---

## Phase 7 — Dauerbetrieb

Vor dem ersten unbeaufsichtigten Lauf:

- [ ] Wasser im System durch pH-Minus ersetzt, Leitungen entlüftet
- [ ] Rückschlagventil am Einspritzpunkt montiert
- [ ] Säurebehälter tiefer als die Pumpe
- [ ] Anlage hängt am selben geschalteten Stromkreis wie die Umwälzpumpe
      (sonst kann in stehendes Wasser dosiert werden)
- [ ] Tagesmenge konservativ eingestellt
- [ ] Web-Login gesetzt
- [ ] Notfall bekannt: `estop` in der Konsole bzw. NOT-HALT im Webinterface

Wöchentlich prüfen: Schlauchzustand, Füllstand, Vergleich der Anzeige mit
einer unabhängigen Messung.

---

## Befehlsreferenz der seriellen Konsole

115200 Baud, `help` zeigt die Liste.

| Befehl | Wirkung |
|---|---|
| `status` | Kurzstatus |
| `json` | vollständiger Status als JSON |
| `scan` | I²C-Bus scannen |
| `mon [n]` | n Sekunden Rohwerte im Sekundentakt |
| `auto on\|off` | Automatik ein/aus (ein nur nach Kalibrierung) |
| `dose <ml>` | manuelle Dosierung, alle Sicherheitsgrenzen gelten |
| `steps <n> [rev]` | Servicelauf ohne Mengenverbuchung |
| `spml <schritte> <ml>` | Schritte/ml aus Testlauf berechnen |
| `stop` | Pumpe anhalten |
| `estop` | Not-Halt, schaltet die Automatik ab |
| `clear` | Störung/Not-Halt quittieren |
| `cal a\|b <ph>` | Kalibrierpunkt speichern |
| `calreset` | Kalibrierung verwerfen |
| `set <key> <wert>` | Parameter setzen |
| `wifi <ssid> <pass>` | WLAN speichern und neu starten |
| `daily reset` | Tageszähler zurücksetzen |
| `reboot` | Neustart |
| `factory` | Werkseinstellungen, löscht alles |

---

## Sicherheitsgrenzen, die sich nicht abschalten lassen

Diese Werte sind in `firmware/ph_dosieranlage_s3/Config.h` fest verdrahtet und
begrenzen jede Benutzereingabe:

| Grenze | Wert |
|---|---|
| maximale Einzeldosis | 20 ml |
| maximale Tagesmenge | 500 ml |
| maximale Pumpenlaufzeit am Stück | 180 s |
| minimale Pause zwischen Dosierungen | 60 s |
| absolute pH-Untergrenze für jede Dosierung | 6,20 |
| plausibler pH-Bereich | 3,00 … 11,00 |
| plausibler Spannungsbereich am ADC | 0,030 … 3,250 V |

Zusätzlich gilt immer:

* Ohne gültige Kalibrierung lässt sich die Automatik nicht einschalten.
* Bei Sensorfehler oder instabilem Messwert wird nicht dosiert — ein
  Not-Halt bricht eine laufende Dosierung sofort ab.
* Tages- und Gesamtmenge liegen im nichtflüchtigen Speicher; ein Neustart
  umgeht das Tageslimit nicht.
* Während eines OTA-Updates wird der Treiber zwangsweise abgeschaltet.
