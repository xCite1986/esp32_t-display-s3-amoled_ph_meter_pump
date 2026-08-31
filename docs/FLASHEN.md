# Firmware aufspielen

Drei Wege, alle gleichwertig im Ergebnis:

| Weg | Wann |
|---|---|
| [Arduino IDE 2](#1-arduino-ide-2) | einmalige Einrichtung, Fehlersuche, serielle Konsole |
| [arduino-cli über die Skripte](#2-arduino-cli) | Alltag — ein Befehl, immer dieselben Einstellungen |
| [OTA über WLAN](#3-ota-über-wlan) | Gerät hängt schon im Technikraum, kein USB-Kabel dran |

Das erste Aufspielen muss über USB laufen. OTA setzt eine Firmware voraus,
die schon im WLAN ist.

---

## 0. Abhängigkeiten

Getestet ist genau diese Kombination:

| Komponente | Version | Anmerkung |
|---|---|---|
| ESP32-Core (`esp32:esp32`) | **3.3.8** | Espressif, nicht `arduino:esp32` |
| LilyGo-AMOLED-Series | **1.2.4** | Display, Touch, Power |
| lvgl | **8.4.0** | **nicht** auf 9.x heben, siehe unten |
| SensorLib | **0.3.3** | **nicht** auf 0.4.1 heben, siehe unten |
| XPowersLib | **0.3.3** | wird von LilyGo-AMOLED-Series gezogen |
| arduino-cli | 1.5.1 | in der Arduino IDE 2 enthalten |

Aus dem ESP32-Core kommen ohne Zusatzinstallation: `WiFi`, `WebServer`,
`ESPmDNS`, `ArduinoOTA`, `HTTPClient`, `Preferences`, `Wire`.

Bewusst **nicht** benutzt: eine JSON-Bibliothek (das Statusobjekt wird von
Hand gebaut, die HA-Antwort per Textsuche gelesen), ein ADS1115-Treiber
(eigener in `Ads1115.cpp`) und eine Stepper-Bibliothek (die Schrittausgabe
muss nicht blockierend sein, damit das Display flüssig bleibt).

### Zwei Versionen, die man nicht anheben darf

**SensorLib 0.4.1 ist defekt.** Das Release-ZIP enthält ein leeres
Verzeichnis `src/REG/`; in 0.3.3 liegen dort 18 Header. Der Build bricht ab
mit:

```text
SensorBMA423.cpp: fatal error: REG/BMA423Config.h: No such file or directory
```

Das ist kein Konfigurationsfehler auf dieser Seite — die Datei fehlt im
Paket. Deshalb festgenagelt auf 0.3.3.

**lvgl muss bei 8.4 bleiben.** Die Oberfläche in `PanelUi.cpp` benutzt
`lv_img_set_zoom()` auf einem Canvas, um über die größte eingebaute
Schriftgröße hinauszukommen (der pH-Wert steht mit rund 136 px auf dem
Display). In lvgl 9 heißt und arbeitet diese API anders. Ein Sprung auf 9.x
ist Arbeit, kein Versionswechsel.

### Installieren

Über die Bibliotheksverwaltung der IDE oder auf der Kommandozeile:

```bash
arduino-cli core install esp32:esp32@3.3.8
```

```bash
arduino-cli lib install "LilyGo-AMOLED-Series@1.2.4" "lvgl@8.4.0" "SensorLib@0.3.3" "XPowersLib@0.3.3"
```

Falls SensorLib schon in 0.4.1 liegt, vorher `arduino-cli lib uninstall
SensorLib` — sonst bleibt die kaputte Fassung stehen.

> **`lv_conf.h`:** Die Datei kommt von LilyGo-AMOLED-Series mit und muss
> nicht angefasst werden. Wer eine eigene im Bibliotheksordner liegen hat,
> bekommt Übersetzungsfehler in `lvgl` selbst — dann die eigene wegräumen.

---

## 1. Arduino IDE 2

### Board-Paket einrichten

*Datei → Einstellungen → Zusätzliche Boardverwalter-URLs:*

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Dann *Werkzeuge → Board → Boardverwalter*, nach `esp32` suchen und
**esp32 by Espressif Systems** installieren. Nicht „Arduino ESP32 Boards" —
das ist ein anderes Paket und kennt dieses Board nicht.

### Sketch öffnen

`firmware/ph_dosieranlage_s3/ph_dosieranlage_s3.ino`

Alle `.h`/`.cpp` daneben öffnen sich als Reiter mit. Es gibt kein
Projektfile, das ist ein reiner Arduino-Sketch.

### Board-Einstellungen

*Werkzeuge → Board → esp32 → **ESP32S3 Dev Module***, dann exakt so:

| Einstellung | Wert |
|---|---|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | **Enabled** |
| CPU Frequency | 240 MHz (WiFi) |
| Core Debug Level | None |
| USB DFU On Boot | Disabled |
| Erase All Flash Before Sketch Upload | Disabled |
| Events Run On | Core 1 |
| Flash Mode | QIO 80 MHz |
| Flash Size | **16 MB (128 Mb)** |
| JTAG Adapter | Disabled |
| Arduino Runs On | Core 1 |
| USB Firmware MSC On Boot | Disabled |
| Partition Scheme | **16 M Flash (3 MB APP / 9,9 MB FATFS)** |
| PSRAM | **OPI PSRAM** |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 921600 |
| USB Mode | **Hardware CDC and JTAG** |

Die fett gesetzten fünf sind die, bei denen ein falscher Wert nicht als
Compilerfehler auffällt, sondern als Gerät, das nicht startet:

* **PSRAM: OPI** — mit `Disabled` fehlt LVGL der Speicher, das Display
  bleibt schwarz.
* **Flash Size 16 MB** und **Partition Scheme 3 MB APP** — der Build braucht
  rund 1,8 MB; die Standardpartition mit 1,3 MB reicht nicht.
* **USB Mode: Hardware CDC and JTAG** plus **USB CDC On Boot: Enabled** —
  sonst meldet sich nach dem Flashen kein serieller Port und der Monitor
  bleibt stumm.

Als eine Zeile für `arduino-cli` ist das dieselbe Auswahl:

```text
esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=app3M_fat9M_16MB
```

### Port und Upload

USB-C anstecken — **Datenkabel, kein reines Ladekabel.** Das ist der
häufigste Grund für „kein Port da".

*Werkzeuge → Port* → der neue COM-Port. Dann *Hochladen*.

Meldet sich kein Port: Boot-Taste gedrückt halten, kurz Reset drücken,
Boot loslassen. Das Board ist dann im Download-Modus und erscheint als Port.
Nach dem Flashen einmal Reset drücken.

### Serielle Konsole

*Werkzeuge → Serieller Monitor*, **115200 Baud**. `help` eingeben, dann
kommt die Befehlsliste. Die Konsole ist der Weg, wenn WLAN oder Display
einmal nicht mitspielen — siehe [INBETRIEBNAHME.md](INBETRIEBNAHME.md).

---

## 2. arduino-cli

Die Skripte im Ordner `scripts/` setzen FQBN und Port selbst, damit nicht bei
jedem Flashen eine Einstellung verrutscht.

```bash
powershell -File scripts/build.ps1
```

```bash
powershell -File scripts/flash.ps1
```

```bash
powershell -File scripts/monitor.ps1
```

Der Port steht in `scripts/acli.ps1` als `$script:DefaultPort` (COM6). Ist er
nicht da, sucht `Get-EspPort` selbst den ersten seriellen Port mit
ESP32-Kern. Mit `-Port COM7` lässt sich das überschreiben.

Eine eigene arduino-cli-Installation ist nicht nötig: findet das Skript keine
im `PATH`, nimmt es die aus der Arduino IDE 2 unter
`C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\`.

> **`--jobs 1`** steht mit Absicht in den Skripten. Bei paralleler
> Bibliothekserkennung bleibt arduino-cli auf diesem Rechner reproduzierbar
> hängen.

---

## 3. OTA über WLAN

```bash
powershell -File scripts/ota.ps1 -Host 192.168.0.62
```

Statt der IP geht auch `ph-dosierung.local`, solange mDNS antwortet. Die
Adresse steht auf dem Display und unter *Netz* im Webinterface.

**Vor der Übertragung löst die Firmware selbst einen Not-Halt aus** — die
Pumpe steht also garantiert still, während geflasht wird.

### Wenn OTA „No response from device" sagt

Das war hier zweimal die **Windows-Firewall**, nicht das Gerät. Die
Freigaben für `espota.exe` sind an den vollen Pfad gebunden, und der enthält
die Core-Version:

```text
...\packages\esp32\hardware\esp32\3.3.8\tools\espota.exe
```

Nach einem Core-Update zeigt die alte Regel auf einen Pfad, den es nicht mehr
gibt, und die neue `espota.exe` wird stumm geblockt. Entweder die Regel für
den neuen Pfad anlegen oder beim Aufruf den Port festnageln:

```bash
espota.exe -i 192.168.0.62 -p 3232 -P 3233 -f build/ph_dosieranlage_s3.ino.bin
```

Prüfen lässt sich das von außen nicht — ein blockiertes Paket sieht genauso
aus wie ein Gerät, das nicht antwortet. Deshalb im Zweifel einmal über USB
flashen; klappt das, liegt es nicht am Gerät.

---

## 4. Nach dem ersten Flashen

Das Gerät kennt noch kein WLAN und öffnet einen Einrichtungs-AP. Ablauf,
Kalibrierung und die ersten Parameter stehen in
[INBETRIEBNAHME.md](INBETRIEBNAHME.md).

Kurzfassung über die serielle Konsole:

```text
wifi <ssid> <passwort>
```

Danach startet das Gerät neu und zeigt seine IP auf dem Display.

> **Ein Update setzt keine Einstellungen zurück.** Kalibrierung, WLAN,
> Grenzwerte und der 7-Tage-Verlauf liegen im NVS und überleben das Flashen.
> Nur `factory` auf der Konsole räumt sie weg.
