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

### Am Aufbau ermittelter Arbeitspunkt

Diese Werte stammen aus der tatsächlichen Inbetriebnahme, nicht aus Formeln:

| Größe | Wert | wie ermittelt |
|---|---|---|
| Mikroschritte | 1/16, **3200 Schritte/Umdr.** | 1600 Schritte = halbe Umdrehung, gezählt |
| Chopper | **SpreadCycle** (`MS3` auf HIGH) | brachte das nutzbare Drehmoment |
| VREF | **1,2 V** | über die Temperatur eingestellt |
| daraus Strom | **≈ 0,6 A** | aus 45 °C bei Dauerhaltestrom zurückgerechnet |
| Schrittrate | 800/s = 15 U/min | |
| Beschleunigung | 2000/s² | |

**Der Umrechnungsfaktor des Moduls liegt bei rund 0,5 A/V** statt der 1,77 A/V
für 0,11 Ω Sense-Widerstand — ein Unterschied von Faktor 3,5. Nach Formel
eingestellt (0,23 V) wäre die Pumpe nicht angelaufen.

**Thermische Probe:** fünf Minuten Dauerhaltestrom ergaben rund 45 °C. Das ist
der ungünstigste denkbare Fall — voller Strom bei Stillstand, ohne Gegen-EMK.
Im Betrieb läuft der Motor Sekunden alle 30 Minuten und ist dazwischen dank
`hold = 0` stromlos.

### Wenn der Motor nicht dreht

Der Reihe nach messen — jede Zeile schließt eine Ursache aus. Die Schrittzahl
in der Weboberfläche zählt auch dann hoch, wenn am Motor nichts passiert: sie
beweist nur, dass Impulse rausgehen, nicht dass sie Wirkung haben.

| Messung | Sollwert | Bedeutung, wenn abweichend |
|---|---|---|
| `VMOT` gegen `GND` | **12 V** | unter 4,75 V arbeitet der Treiber außerhalb der Spezifikation |
| `VIO` gegen `GND` | **3,3 V** | sagt **nichts** über VMOT aus — zwei getrennte Versorgungen |
| `EN` gegen `GND` **während** eines Laufs | **0 V** | 3,3 V: Leitung zu GPIO10 unterbrochen, R1 hält den Pin hoch |
| `1A` ↔ `1B` und `2A` ↔ `2B` am **leeren** Sockel | je **~3,6 Ω** | offen: Unterbrechung in Klemme, Litze oder Motorstecker |
| `VREF` am Trimmerschleifer | **0,3–0,5 V** | 0 V: keine Stromvorgabe, der Motor zittert höchstens |

**Symptome einordnen:**

* *Nichts, völlig kraftlos* — kein Strom: VMOT, EN oder VREF prüfen.
* *Hält die Position, rückt aber nie vor* — Strom ist da, Schrittimpulse
  kommen nicht an. Siehe „DIR und STEP" unten.
* *Zittern ohne Drehung* — entweder nur eine Spule bestromt (Wicklung nicht
  durchgängig), oder **`DIR` hängt in der Luft**.
* *Brummen, dreht schwer* — zu wenig Drehmoment: SpreadCycle einschalten,
  Mikroschritte vergröbern, VREF anheben.
* *Alle Werte stimmen, trotzdem nichts* — Treiber prüfen (siehe unten).

**Der schnellste Test überhaupt:** `hold = 1` setzen und die Welle von Hand
drehen. Rastet sie spürbar, sind Strom, Treiber, Motor und Verkabelung in
Ordnung — dann liegt der Fehler ausschließlich bei `STEP` oder `DIR`.

#### DIR und STEP

Ein **floatender `DIR`-Eingang** erzeugt ein sehr irreführendes Bild: Der
CMOS-Eingang nimmt jede Störung mit, die Richtung kippt ständig, und der Motor
macht einen Schritt vor und einen zurück. Er zittert und brummt, dreht sich
aber nicht — was leicht als Drehmomentproblem missverstanden wird.

**Achtung, naheliegender Fehlgriff:** `GPIO15` liegt direkt neben `GPIO12` und
ist in `Config.h` als UART-Reserve auf `INPUT` gesetzt, also hochohmig. Landet
`DIR` dort statt auf `GPIO12`, entsteht genau dieses Bild.

Ebenso wirkt ein **vertauschtes STEP/DIR**: Der statische Pegel auf `STEP`
löst keine Schritte aus, der Impuls auf `DIR` ändert nur die Richtung. Der
Motor hält, rückt aber nie vor.

Am Pin messen hilft nicht — bei 4 µs Impulsdauer und 200 Schritten/s liegt das
Tastverhältnis bei 0,08 %, ein Multimeter zeigt praktisch null. Also stromlos
den **Durchgang** prüfen: `GPIO11` ↔ `STEP`, `GPIO12` ↔ `DIR`.

#### TMC2209 auf einer A4988/DRV8825-Trägerkarte

Diese Kombination ist verbreitet und verwirrend, weil die Karte die Pins nach
dem **A4988** beschriftet:

| Position | Aufdruck der Karte | TMC2209 hat dort |
|---|---|---|
| 2 | MS1 | MS1 |
| 3 | MS2 | MS2 |
| 4 | **MS3** | häufig **SPREAD** |

`MS3` schaltet beim TMC2209 also nicht die Auflösung, sondern oft die
Chopper-Betriebsart: **auf HIGH läuft er in SpreadCycle** statt StealthChop —
deutlich mehr nutzbares Drehmoment unter Last, dafür hörbares Sirren statt
nahezu lautlos. Für eine Dosierpumpe ist das der bessere Kompromiss.

Verlass dich nicht auf die Beschriftung, sondern **zähle Umdrehungen**:
1600 Schritte ergeben eine halbe Umdrehung bei 1/16 (3200/Umdr.) und eine
ganze bei 1/8 (1600/Umdr.). Dieser Wert gehört in `sprev` — steht er falsch,
dosiert die Anlage später um Faktor 2 daneben.

> **Vor dem Aufgeben: Schutzabschaltung zurücksetzen.** Der TMC2209 rastet nach
> Kurzschluss oder Übertemperatur ab und kommt von selbst nicht zurück. Dafür
> muss **VMOT tatsächlich weg** — ein Reset des ESP32 genügt nicht. Also 12 V
> und USB trennen, zehn Sekunden warten, neu einschalten.

> **Nach jedem Ziehen des Moduls** die Ausrichtung mit dem Aufdruck vergleichen
> und von der Seite prüfen, ob ein Pin untergeknickt ist. Verdreht eingesetzt
> überlebt ein Stepstick das Einschalten meist nicht.

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

> **Bekommst du die Spanne nicht unter 20 mV, ist fast immer die Masse schuld,
> nicht die Sonde.** Siehe „Wenn die Messung rauscht" weiter unten — dort steht
> ein Test, der in fünf Minuten Gewissheit bringt und nichts kostet.

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

### Wenn die Messung rauscht

Der häufigste und am schwersten zu findende Fehler dieser Anlage. Das Bild:
auf der Werkbank misst die Sonde ruhig, im Becken schwankt sie um mehrere
pH-Einheiten und lässt sich nicht kalibrieren.

**Die Ursache ist fast nie die Sonde.** Eine pH-Elektrode hat bis zu 250 MΩ
Innenwiderstand. Der Ableitstrom eines Schaltnetzteils fließt über dessen
Y-Kondensatoren nach Erde und sucht sich den Rückweg — im Zweifel durch die
Glaselektrode ins geerdete Beckenwasser. **Zwei Nanoampere genügen für einen
halben Volt Fehler.** Zum Vergleich: der Ableitstrom eines gewöhnlichen
Steckernetzteils liegt bei 0,1 bis 0,5 Milliampere, fünf Größenordnungen
darüber.

#### Der Powerbank-Test

Kostet nichts, dauert fünf Minuten und ist eindeutig.

1. **12-V-Netzteil komplett aus der Steckdose ziehen** — nicht nur die Klemme
   lösen. Es geht um die Netzverbindung, nicht um die Spannung.
2. **USB-C des Displayboards an eine Powerbank.** Das funktioniert, weil
   `VBUS` board-intern parallel zur USB-Schiene liegt und das pH-Board hinter
   D1 an derselben 5-V-Schiene hängt. D1 sperrt Richtung Buck.
3. Sonde dort lassen, wo sie ist. Eine Minute warten, bis WLAN steht und der
   Messpuffer voll ist.
4. Im Webinterface unter *Messwert* die Zeile *Spanne* ablesen.

**Eine echte Powerbank, kein PC-USB-Port und kein Ladegerät** — beide hängen
am Schutzleiter, dann hast du die Netzverbindung nur verlagert. Und kein
zweites Kabel nebenher.

Der Motor läuft dabei mangels 12 V nicht. Für den Test wird er nicht
gebraucht.

| Ergebnis | Bedeutung |
|---|---|
| Spanne fällt auf einstellige mV | Der Störweg läuft über die Netzverbindung. Die galvanische Trennung behebt es. |
| Spanne bleibt hoch | Die Störung sitzt woanders — Kabel, Steckerfeuchtigkeit, Streustrom im Becken. |

#### Was an diesem Aufbau gemessen wurde

Alle Werte sind der vom Gerät gemeldete Median der Spanne, gleiche Firmware,
gleiche Sonde, nur unterschiedliche Umgebung:

| Aufbau | Spanne |
|---|---:|
| Sonde im Becken, Netzteil | 468 mV |
| Sonde im Kübel Poolwasser, Netzteil | 348 mV |
| Sonde im Becherglas Puffer, Netzteil | 712 mV |
| **Sonde im Glas, Powerbank** | **0,7 mV** |

Die ersten drei Zeilen unterscheiden sich um Faktor zwei — Ort, Medium und
sogar die verwendete Sonde haben also kaum etwas ausgemacht. Die letzte Zeile
fällt um Faktor tausend heraus. 0,7 mV ist zugleich der Datenblattwert der
Elektrode; die Messkette arbeitet dann nach Spezifikation.

Vorher war noch eine zweite Ursache im Spiel, die sich ähnlich anfühlte: die
Firmware wandelte einmal alle 200 ms und hatte damit **keinerlei
Netzunterdrückung** — jede Messung traf eine 50-Hz-Störung in zufälliger
Phase. Seit Version 2.1.0 mittelt sie über genau eine Netzperiode. Wer eine
ältere Firmware fährt, sollte zuerst aktualisieren.

Woran man die beiden unterscheidet: bei reiner Netzeinstreuung ist die
Verteilung der Messwerte **randbetont** (Arcussinus, typisch für einen
abgetasteten Sinus), bei einem Masseproblem eher mittenbetont mit Ausreißern.

#### Die Behebung

Galvanische Trennung der Messseite: ISO1540, B0509S-1W und AMS1117-5.0,
zusammen rund 10 €. Aufbau in [LOETANLEITUNG.md](LOETANLEITUNG.md),
Abschnitt 6, Netzliste in [SCHALTPLAN.md](SCHALTPLAN.md), Abschnitt 2.5.

Ein **eigener Stromkreis hilft nicht.** Der Ableitstrom entsteht im Netzteil
selbst und findet an jeder Steckdose denselben Weg. Ein Trafo-Netzteil würde
ihn um Faktor zwanzig bis vierzig senken, kostet aber mehr als die Trennung
und macht den Weg nur hochohmiger, statt ihn zu schließen.

---

## Phase 5 — Regelung scharf schalten

Erst jetzt Parameter setzen. Empfohlene Startwerte für einen Pool:

```text
set sp 7.20         pH-Sollwert
set db 0.05         Totband
set dose 3.0        Einzeldosis in ml
set maxs 5.0        maximale Einzeldosis
set maxd 60.0       maximale Tagesmenge
set pause 1800      30 min Durchmischung zwischen Dosierungen
set phlock 6.80     unter pH 6,80 wird nie dosiert
set phmax 9.50      darüber gilt der Messwert als unplausibel
set srate 1200      in Phase 2 ermittelte Schrittrate
set filt 30         Filterzeit der Messwertglaettung in s
set avgs 600        Mittelungsfenster fuer die Dosierentscheidung in s
```

`filt` und `avgs` bestimmen, **worauf** geregelt wird. Angezeigt und
aufgezeichnet wird der mit `filt` geglättete Wert; entschieden wird nach
dem Mittel der letzten `avgs` Sekunden. Rauscht die Sonde im strömenden
Wasser, beide Werte erhöhen — die Regelung wird dadurch nicht träger,
denn ein Pool ändert seinen pH über Stunden, nicht über Sekunden.

Solange das Mittelungsfenster nach einem Neustart noch nicht gefüllt ist,
steht die Sperre *instabil* — das ist beabsichtigt.

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
powershell -File scripts/ota.ps1 -Target 192.168.0.61
```

Ist ein Web-Passwort gesetzt, zusätzlich `-Password <passwort>` angeben.

> **Wenn OTA mit „No response from device" abbricht**, blockt die Windows-
> Firewall den Rückkanal: `espota` überträgt nicht selbst, sondern lädt das
> Board ein, sich zum PC **zurück** zu verbinden. Die Freigaben in der
> Firewall hängen am Dateipfad und damit an der Core-Version — nach einem
> Core-Update fehlen sie wieder. Die betroffene Datei nennt das Skript im
> Fehlerfall.
>
> Vor jedem OTA-Update löst die Firmware selbst einen Not-Halt aus. Die Pumpe
> steht während der Übertragung also garantiert still.

---

## Phase 7 — Dauerbetrieb

Vor dem ersten unbeaufsichtigten Lauf:

- [ ] Wasser im System durch pH-Minus ersetzt, Leitungen entlüftet
- [ ] Rückschlagventil am Einspritzpunkt montiert
- [ ] Säurebehälter tiefer als die Pumpe
- [ ] Umwälzung sichergestellt: entweder hängt die Anlage am selben
      geschalteten Stromkreis wie die Pumpe, **oder** die Prüfung über
      Home Assistant ist eingerichtet und getestet
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
