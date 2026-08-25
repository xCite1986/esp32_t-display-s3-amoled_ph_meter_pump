# Bedienpanel — LilyGo T-Display S3 AMOLED

Ein zweites Gerät als Anzeige und Fernbedienung: pH-Wert, dosierte Menge der
letzten 24 Stunden, und eine Freigabe für eine feste Anzahl Motorumdrehungen
per Touch — mit Rückfrage.

---

## 1. Architektur — und warum das Panel nichts entscheidet

```text
┌─────────────────────────┐         WLAN          ┌──────────────────────────┐
│  T-Display S3 AMOLED    │  HTTP/JSON            │  ESP32-C3 Super Mini     │
│  Anzeige + Touch        │ ────────────────────> │  Messung, Regelung,      │
│  KEINE Dosierlogik      │  GET  /api/status     │  Sicherheitsgrenzen,     │
│                         │  POST /api/dose/revs  │  Pumpenansteuerung       │
└─────────────────────────┘                       └──────────────────────────┘
```

Das Panel ist bewusst **dumm**. Es fragt nur ab und stellt Anträge. Jede
Dosieranforderung läuft auf der Anlage durch dieselbe Prüfkette wie eine
manuelle Dosierung über das Webinterface:

* maximale Einzeldosis (Benutzer **und** harte Grenze)
* verbleibende Tagesmenge
* pH-Sperrschwelle, sofern ein gültiger Messwert vorliegt
* Umwälzung, falls aktiviert
* Not-Halt, Pumpenstörung, laufende Pumpe

**Konsequenz:** Ein Absturz, ein Fehlbedienung oder ein kompromittiertes Panel
kann keine Überdosierung auslösen. Fällt das Panel aus, dosiert die Anlage
unverändert weiter. Fällt die Anlage aus, zeigt das Panel „offline" und der
Knopf ist gesperrt.

Deshalb läuft die Freigabe auch über `/api/dose/revs` und **nicht** über
`/api/pump/run`. Letzteres ist der Servicelauf für die Pumpenkalibrierung —
der verbucht keine Menge und prüft keine pH-Sperre. Für eine Bedientaste wäre
das ein Loch in der Mengenbilanz.

---

## 2. Hardware

Nichts zu löten. Das Panel hängt nur an USB-C (oder einem 5-V-Netzteil) und
kommuniziert über WLAN.

| | |
|---|---|
| Board | LilyGo T-Display S3 AMOLED, 536 × 240, Touch |
| Versorgung | USB-C, ca. 150 mA |
| Verbindung | WLAN 2,4 GHz, gleiches Netz wie die Dosieranlage |

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
> `Documents/Arduino/libraries/lv_conf.h` wurden `LV_FONT_MONTSERRAT_32` und
> `LV_FONT_MONTSERRAT_48` von `0` auf `1` gesetzt — vorher waren nur Fonts bis
> 24 px aktiv, womit die pH-Anzeige auf 536 × 240 unlesbar klein geblieben
> wäre. Die Änderung ist rein additiv (etwas mehr Flash) und betrifft alle
> LVGL-Sketches auf diesem Rechner. Sicherung liegt unter
> `lv_conf.h.bak_phpanel`.

---

## 4. Flashen

```bash
powershell -File scripts/flash-panel.ps1
```

```bash
powershell -File scripts/monitor.ps1 -Port COM6
```

Der C3 der Dosieranlage hängt an COM3, das Panel an COM6.

---

## 5. Einrichten

In der seriellen Konsole des Panels (115200 Baud):

```text
wifi <SSID> <Passwort>      WLAN speichern, Panel startet neu
host ph-dosierung.local     Adresse der Anlage (Standard)
auth <user> <pass>          nur nötig, wenn die Anlage ein Web-Login hat
revs 5                      Umdrehungen pro Freigabe
status                      Verbindung und aktuelle Werte prüfen
```

Falls mDNS im Netz nicht zuverlässig ist, statt des Namens die feste IP der
Anlage eintragen — und im Router eine DHCP-Reservierung setzen.

---

## 6. Bedienung

**Anzeige**

* Großer pH-Wert, farbcodiert gegen den Sollwert
  (grün im Zielbereich, gelb darüber, rot bei starker Abweichung oder Fehler)
* Karte rechts: **dosierte Menge der letzten 24 Stunden**, darunter der
  Kalendertagswert gegen das Tageslimit
* Zustandszeile mit den aktiven Sperrgründen der Anlage
* Kopfzeile: IP und Signalstärke

**Dosieren**

1. Irgendwo auf das Display tippen (oder auf den Knopf) → Rückfrage erscheint
   mit der Umrechnung „5 Umdrehungen ≈ X ml".
2. **FREIGEBEN** antippen → die Anfrage geht an die Anlage.
3. Die Antwort erscheint als Einblendung — entweder die dosierte Menge oder
   der Ablehnungsgrund im Klartext, z. B. „ueber max. Einzeldosis (10.00 ml)".

Der Dialog schließt sich nach 12 Sekunden ohne Eingabe von selbst. Während die
Pumpe läuft oder das Panel offline ist, ist der Knopf gesperrt.

---

## 7. Wichtig: 5 Umdrehungen sind nicht automatisch erlaubt

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

## 8. AMOLED: Einbrennschutz

Ein statisches Bild über Monate brennt sich in ein AMOLED ein. Deshalb:

* Nach 60 s ohne Berührung dimmt das Display auf ca. 10 % Helligkeit.
  Eine Berührung weckt es wieder — **dieser erste Tipp löst bewusst nichts
  aus**, er weckt nur.
* Der gesamte Inhalt wandert alle 90 s um wenige Pixel.
* Hintergrund ist echtes Schwarz (am AMOLED sind das ausgeschaltete Pixel:
  kein Einbrennen, weniger Strom).

---

## 9. Zusammenspiel mit dem Webinterface

Panel und Webinterface schließen sich nicht aus — beide sprechen dieselbe API.
Die Mengenbilanz ist gemeinsam: eine Dosierung über das Panel taucht sofort im
Webinterface, in der 24-h-Summe und im Tageszähler auf, und umgekehrt.
