# Anzeige und Bedienung — T-Display S3 AMOLED

Das Displayboard **ist** die Steuerung: pH-Wert, dosierte Menge der letzten
24 Stunden, Zustand und Sperrgründe, dazu eine Freigabe für eine feste Anzahl
Motorumdrehungen per Touch — mit Rückfrage.

---

## 1. Architektur

```text
┌──────────────────────────── T-Display S3 AMOLED ────────────────────────────┐
│  PanelUi (LVGL)  ──liest──>  PHMeasurement · PHController · StepperPump     │
│  Touch  ──manualDose()──>    dieselbe Pruefkette wie das Webinterface       │
│  WebInterface    ──dieselben Objekte, dieselbe Mengenbilanz                 │
└─────────────────────────────────────────────────────────────────────────────┘
```

Display, Touch, Messung und Regelung laufen auf **einem** Chip. Die Oberfläche
liest die Werte direkt aus den Modulen der Firmware — es gibt keinen
Netzwerkweg zwischen Anzeige und Regelung.

Ein Tipp auf das Display löst nicht selbst eine Dosierung aus, sondern ruft
dieselbe Funktion auf wie das Webinterface. Damit gilt fuer jede Freigabe die
volle Pruefkette:

* maximale Einzeldosis (Benutzer **und** harte Grenze)
* verbleibende Tagesmenge
* pH-Sperrschwelle, sofern ein gültiger Messwert vorliegt
* Not-Halt, Pumpenstörung, laufende Pumpe

Die Freigabe geht bewusst über `manualDose()` und **nicht** über den
Servicelauf `/api/pump/run`. Letzterer ist für die Pumpenkalibrierung gedacht,
verbucht keine Menge und prüft keine pH-Sperre — als Bedientaste wäre das ein
Loch in der Mengenbilanz.

> **Was das kostet:** Weil beides auf demselben Chip läuft, kann eine hängende
> Oberfläche die Regelung mitreißen. Dagegen stehen die nicht blockierende
> Schrittausgabe (die Dosiermenge bleibt exakt) und der EN-Pullup, der den
> Treiber bei jedem Reset stromlos hält.

---

## 2. Hardware

Display und Touch sind auf dem Board integriert — dafür ist nichts zu löten.
Verdrahtet werden nur Versorgung, ADS1115 und TMC2209, siehe
[SCHALTPLAN.md](SCHALTPLAN.md).

| | |
|---|---|
| Board | LilyGo T-Display S3 AMOLED, 536 × 240, Touch (`BOARD_AMOLED_191`) |
| Versorgung | 5 V aus dem Buck-Converter (min. 1 A gesamt) oder USB-C |
| Netz | WLAN 2,4 GHz; ohne Verbindung öffnet sich ein Einrichtungs-AP |

---

## 3. Board-Einstellungen

Die Skripte setzen das automatisch. Für die Arduino IDE von Hand:

| Einstellung | Wert |
|---|---|
| Board | ESP32S3 Dev Module |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Enabled |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |

FQBN:

```text
esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB
```

### Bibliotheken

Bereits installiert und unverändert übernommen:

* `LilyGo-AMOLED-Series` 1.2.4
* `lvgl` 8.4.0
* `ArduinoJson` 7.4.3
* `XPowersLib`, `SensorLib` (Abhängigkeiten der AMOLED-Bibliothek)

> **Eine Änderung außerhalb dieses Projekts:** In der gemeinsam genutzten
> `Documents/Arduino/libraries/lv_conf.h` wurden die Montserrat-Größen
> **28, 32, 36 und 48** von `0` auf `1` gesetzt — ab Werk sind nur Fonts bis
> 24 px aktiv. Die Änderung ist rein additiv (etwas mehr Flash) und betrifft
> alle LVGL-Sketches auf diesem Rechner. Sicherung liegt unter
> `lv_conf.h.bak_phpanel`.
>
> Wird `lv_conf.h` angefasst, übersetzt arduino-cli LVGL komplett neu — der
> nächste Build dauert dann wieder rund 20 Minuten.

### Schriftgrößen — und warum der pH-Wert eine Canvas ist

Das Panel misst 1,91" in der Diagonale. 536 × 240 Pixel klingen nach viel, sind
physikalisch aber nur etwa 43 × 19 mm: **Montserrat 48 ergibt gerade einmal
~4 mm Zifferhöhe** — auf Leseabstand am Becken zu wenig.

Die größte eingebaute LVGL-Schrift *ist* Montserrat 48. Der pH-Wert wird daher
nicht als Label gesetzt, sondern in eine `lv_canvas` gezeichnet und diese als
Bild mit `lv_img_set_zoom(…, 512)` doppelt vergrößert dargestellt:

```text
Montserrat 48  →  Canvas 152 × 58 px  →  2× Zoom  →  ~96 px ≈ 8 mm
```

Die Canvas wird nur neu gezeichnet, wenn sich Wert oder Farbe geändert haben.
Kosten: 17 kB statischer Puffer, sonst nichts.

Alle übrigen Texte liegen zwei Stufen über der ursprünglichen Auslegung:

| Element | Schrift |
|---|---|
| pH-Wert | Montserrat 48, 2× gezoomt (~96 px) |
| 24-h-Menge | 36 |
| Sollwert, Zustand, Knopf | 20–24 |
| Sperrgründe, Kartentexte, Netz | 16 |
| Dialogtitel / Dialogknöpfe | 28 / 22 |

---

## 4. Zwei Stolpersteine in der Toolchain

Beide traten beim ersten Build auf und sind in den Skripten bzw. der
Bibliotheksinstallation bereits erledigt — hier dokumentiert, damit sie auf
einem anderen Rechner nicht erneut Zeit kosten.

### SensorLib 0.4.1 ist defekt

`SensorLib 0.4.1` wird im offiziellen Bibliotheksindex ohne das Verzeichnis
`src/REG/` ausgeliefert — dort fehlen **alle** Registerheader. Der Build bricht
ab mit:

```text
SensorBMA423.cpp:32:10: fatal error: REG/BMA423Config.h: No such file or directory
```

Das ist kein lokaler Installationsschaden: auch nach Deinstallation und
Neuinstallation bleibt das Verzeichnis leer, und im heruntergeladenen Archiv
`SensorLib-0.4.1.zip` sind 0 `REG/`-Dateien enthalten. Version 0.3.3 hat sie
vollständig (18 Dateien).

Da `SensorLib` über `LilyGo-AMOLED-Series` hereinkommt und arduino-cli
grundsätzlich **alle** `.cpp` einer eingebundenen Bibliothek übersetzt, betrifft
das jeden Sketch mit AMOLED-Display — auch solche, die weder Beschleunigungs-
sensor noch IMU nutzen.

**Behoben durch:**

```bash
arduino-cli lib install SensorLib@0.3.3
```

Die LilyGo-Bibliothek verlangt laut eigener Angabe „0.3.2+", die Version passt
also. Eine Sicherung der defekten 0.4.1 liegt unter
`Documents/Arduino/SensorLib_backup_*`.

### Parallele Bibliothekserkennung hängt

Mit den Standardeinstellungen bleibt `arduino-cli compile` in der
Erkennungsphase stehen: ein `xtensa-esp-elf-g++` lebt mit 0 % CPU, es entsteht
keine einzige Objektdatei, und der Vorgang läuft unbegrenzt weiter — ohne
Fehlermeldung. Genau dadurch blieb der SensorLib-Fehler oben zunächst
unsichtbar.

**Behoben durch** `--jobs 1` im Build-Skript. Der Build dauert dadurch beim
ersten Mal rund 20 Minuten (LVGL sind ~450 Übersetzungseinheiten); danach
greift der Cache.

---

## 5. Flashen

```bash
powershell -File scripts/flash.ps1
```

```bash
powershell -File scripts/monitor.ps1
```

Board und Port stehen in `scripts/acli.ps1`: ESP32-S3 Dev Module an COM6.

---

## 6. Einrichten

Ohne gespeicherte Zugangsdaten — oder wenn die Verbindung scheitert — öffnet
die Anlage einen **Access Point mit vollem Webinterface**:

```text
WLAN     pH-Dosieranlage
Passwort dosier1234
Browser  http://192.168.4.1
```

Dort unter *Netzwerk* das Heim-WLAN eintragen. Danach ist die Anlage unter
`http://ph-dosierung.local/` erreichbar.

Alternativ über die serielle Konsole (115200 Baud):

```text
wifi <SSID> <Passwort>   WLAN speichern und neu starten
ap                       WLAN verwerfen, zurueck in den Einrichtungs-AP
status                   Uebersicht inkl. Anzeigezustand
```

Der ESP32-S3 kann wie jeder ESP32 **nur 2,4 GHz** — ein reines 5-GHz-Netz
sieht er nicht.

---

## 7. Bedienung

**Anzeige**

* pH-Wert in ~96 px Höhe, farbcodiert gegen den Sollwert
  (grün im Zielbereich, gelb darüber, rot bei starker Abweichung oder Fehler)
* Karte rechts: **dosierte Menge der letzten 24 Stunden**, darunter der
  Kalendertagswert gegen das Tageslimit
* Zustandszeile, darunter die aktiven Sperrgründe — liegt kein gültiger
  Messwert vor, steht dort stattdessen der Sensorgrund im Klartext
* Kopfzeile: Sollwert links, IP und Signalstärke rechts

**Dosieren**

1. Irgendwo auf das Display tippen → Rückfrage erscheint mit der Umrechnung
   „5 Umdrehungen ≈ X ml".
2. **FREIGEBEN** antippen → die Dosierung wird angefordert.
3. Die Antwort erscheint als Einblendung — entweder die dosierte Menge oder
   der Ablehnungsgrund im Klartext, z. B. „ueber max. Einzeldosis".

Der Dialog schließt sich nach 12 Sekunden ohne Eingabe von selbst.

### Anzeigezustände

| Zustand | Wann | Was zu sehen ist |
|---|---|---|
| **aktiv** | nach einer Berührung | volle Helligkeit, pH ~136 px, 24-h-Menge, Zustand, Sperrgründe |
| **Standby** | nach der eingestellten Zeit ohne Berührung | nur der pH-Wert, gedimmt, ~102 px, wandert |
| **Nacht** | im eingestellten Zeitfenster | Display komplett dunkel |

Eine Berührung weckt immer auf — **dieser erste Tipp löst bewusst nichts aus**,
er weckt nur. Während einer laufenden Dosierung bleibt die Anzeige wach.

Zeiten im Webinterface unter *Anzeige*: Standby-Zeit, Wanderintervall,
Nachtfenster von/bis, Umdrehungen pro Freigabe. Der Nachtmodus greift nur bei
gültiger Uhrzeit — ohne NTP bleibt es beim Standby.

---

## 8. Wichtig: 5 Umdrehungen sind nicht automatisch erlaubt

Die Anlage rechnet Umdrehungen in Milliliter um und prüft **die ml**, nicht die
Umdrehungen:

```text
ml = Umdrehungen × Schritte_pro_Umdrehung ÷ Schritte_pro_ml
```

Mit den Werten vor der Pumpenkalibrierung (3200 Schritte/Umdrehung,
1600 Schritte/ml) sind das:

```text
5 × 3200 ÷ 1600 = 10,0 ml
```

Die Voreinstellung für die maximale Einzeldosis ist **5,0 ml**. Eine Freigabe
über 5 Umdrehungen würde also mit „ueber max. Einzeldosis" abgelehnt werden.
Das ist kein Fehler, sondern die Sicherheitsgrenze bei der Arbeit.

Nach Phase 3 (Pumpenkalibrierung) steht der echte Wert `Schritte/ml` fest.
Dann entweder:

* `revs` im Panel auf eine Zahl setzen, die unter `maxs` bleibt, **oder**
* `set maxs <ml>` auf der Anlage anheben — bewusst und mit Blick auf das
  Beckenvolumen, nicht reflexartig.

Die harte Obergrenze von 20 ml pro Einzeldosis bleibt in jedem Fall bestehen.

---

## 9. AMOLED: Einbrennschutz

Ein statisches Bild über Monate brennt sich in ein AMOLED ein. Deshalb:

* Im Standby läuft die Helligkeit auf ca. 18 %, und es bleibt nur der pH-Wert
  stehen — alles andere wird ausgeblendet.
* Dieser Wert **wandert** im eingestellten Intervall über sechs verteilte
  Positionen der Anzeigefläche, nicht nur um ein paar Pixel. Dafür ist er im
  Standby etwas kleiner (3× statt 4×), damit überhaupt Platz zum Wandern ist.
* Nachts geht das Display ganz aus.
* Der Hintergrund ist echtes Schwarz — am AMOLED sind das ausgeschaltete
  Pixel: kein Einbrennen, weniger Strom.

---

## 10. Zusammenspiel mit dem Webinterface

Touch und Webinterface schließen sich nicht aus. Die Mengenbilanz ist
gemeinsam: eine Dosierung per Touch taucht sofort im Webinterface, in der
24-h-Summe und im Tageszähler auf — und umgekehrt.
