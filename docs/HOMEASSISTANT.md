# Home-Assistant-Anbindung (optional)

Die Firmware stellt eine JSON-API bereit. Damit lässt sich die Anlage ohne
zusätzliche Software in Home Assistant einbinden — MQTT ist nicht nötig.

> Die REST-Plattform von Home Assistant hat keine Oberfläche zum Anlegen,
> sie wird in `configuration.yaml` konfiguriert. **Automationen und
> Dashboards dagegen bitte in der HA-Oberfläche anlegen**, nicht in YAML.

---

## 1. API-Endpunkte

| Methode | Pfad | Wirkung |
|---|---|---|
| GET | `/api/status` | vollständiger Status als JSON |
| GET | `/api/history` | 7-Tage-Verlauf, Stundenauflösung |
| POST | `/api/dose?ml=3.0` | manuelle Dosierung |
| POST | `/api/dose/revs?n=5` | Dosierung in Motorumdrehungen |
| POST | `/api/stop` | Pumpe anhalten |
| POST | `/api/estop` | Not-Halt, Automatik aus |
| POST | `/api/clearfault` | Störung quittieren |
| POST | `/api/auto?on=1` | Automatik ein/aus |
| POST | `/api/settings?sp=7.2&dose=3` | Parameter setzen |
| POST | `/api/cal?point=a&ph=7.00` | Kalibrierpunkt speichern |
| POST | `/api/pump/run?steps=16000&dir=1` | Servicelauf |
| POST | `/api/pump/calc?steps=16000&ml=9.4` | Schritte/ml berechnen |
| POST | `/api/circ/test` | Umwälzprüfung sofort ausführen |
| POST | `/api/daily/reset` | Tageszähler zurücksetzen |
| POST | `/api/reboot` | Neustart |

Ist im Webinterface ein Web-Benutzer gesetzt, verlangen alle Endpunkte
HTTP-Basic-Auth. Parameter nimmt das Gerät sowohl im Query-String als auch
als Formular-Body an — die Beispiele hier nutzen den Query-String, weil er
in Home Assistant ohne `content_type` auskommt.

Beispielantwort (gekürzt):

```json
{
  "fw": "2.0.0", "up": 84213,
  "ph": 7.31, "phValid": true, "phStatus": "OK", "stable": true,
  "phAvg": 7.29, "avgOk": true, "spreadmV": 4.6,
  "volt": 1.71234, "raw": 14021, "slope": -238.4,
  "state": "Bereit", "locks": "Sollwert erreicht", "lockBits": 2048,
  "auto": true, "estop": false, "fault": false, "circ": "Umwaelzung laeuft",
  "pump": { "run": false, "ml": 0.0, "target": 0.0 },
  "dose": { "today": 12.5, "remain": 47.5, "count": 4, "last": 3.0,
            "pauseS": 280, "total": 312.4, "last24h": 15.5 },
  "wifi": { "mode": "STA", "ip": "192.168.0.62", "rssi": -59 },
  "cfg": { "sp": 7.20, "maxd": 60.00, "spml": 1702.1,
           "filt": 30, "avgs": 600 }
}
```

---

## 2. Einbinden

Fertig vorbereitet liegt alles in
[../homeassistant/ph_dosieranlage.yaml](../homeassistant/ph_dosieranlage.yaml):
17 Sensoren, 6 Binärsensoren, 7 Befehle, ein Schalter für die Automatik und
drei Skripte für die Dashboard-Schaltflächen.

### Einbauen

1. Datei nach `config/packages/ph_dosieranlage.yaml` kopieren
   (Verzeichnis `packages` gegebenenfalls anlegen).
2. In `configuration.yaml` einmalig freischalten — falls noch nicht vorhanden:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

3. *Entwicklerwerkzeuge → YAML → Konfiguration prüfen*, dann
   *Alle YAML-Konfigurationen neu laden*. Kommt nur das Package dazu, ist kein
   Neustart nötig.

Wer keine Packages nutzt, kann den Inhalt stattdessen direkt in
`configuration.yaml` einhängen — die vier Blöcke `rest:`, `rest_command:`,
`template:` und `script:` müssen dann zu den dort schon vorhandenen passen
(YAML erlaubt jeden Schlüssel nur einmal).

### Was entsteht

| Entität | Inhalt |
|---|---|
| `sensor.ph_wert_pool` | aktueller, geglätteter Messwert |
| `sensor.ph_wert_pool_mittel` | **die Größe, nach der dosiert wird** |
| `sensor.ph_minus_heute` / `_letzte_24_h` / `_gesamt` | Dosiermengen |
| `sensor.ph_dosieranlage_zustand` | Klartext, mit der Konfiguration als Attribute |
| `sensor.ph_dosieranlage_sperren` | warum gerade nicht dosiert wird |
| `sensor.ph_dosieranlage_restpause` | verbleibende Durchmischungszeit |
| `binary_sensor.ph_dosieranlage_stoerung` / `_not_halt` / `_sensorfehler` | Problemmelder |
| `binary_sensor.ph_dosieranlage_pumpe` | läuft die Dosierpumpe gerade |
| `switch.ph_automatik` | Automatik ein/aus |
| `script.ph_dosieranlage_testdosis` | manuelle Dosis mit Mengenauswahl |
| `script.ph_dosieranlage_not_halt` / `_quittieren` | Not-Halt und Quittieren |

Dazu Diagnosewerte — Sondenspannung, Messrauschen, Steilheit, Laufzeit,
WLAN-Pegel —, die standardmäßig eingeklappt sind.

Ein einziger REST-Aufruf alle 30 s versorgt alles. Der Verlauf muss nicht
übertragen werden: sobald die Sensoren existieren, führt Home Assistant seine
eigene Historie in voller Auflösung. Der 7-Tage-Chart im Webinterface bleibt
davon unberührt und funktioniert auch dann, wenn HA gerade nicht läuft.

### Wenn ein Web-Benutzer gesetzt ist

Dann im Package unter `rest:` die beiden Zeilen `username:`/`password:`
aktivieren und das Passwort in `secrets.yaml` ablegen:

```yaml
ph_dosieranlage_pw: "dein-passwort"
```

Die `rest_command:`-URLs brauchen die Zugangsdaten dann ebenfalls, als
`http://benutzer:passwort@ph-dosierung.local/...`.

### Dashboard

[../homeassistant/dashboard_karte.yaml](../homeassistant/dashboard_karte.yaml)
enthält eine fertige Karte. In der Oberfläche: Dashboard bearbeiten →
Karte hinzufügen → *Manuell* → Inhalt einfügen.

---

## 3. Sinnvolle Automationen

Diese in der HA-Oberfläche anlegen (Einstellungen → Automationen), nicht
in YAML:

* **Störung melden** — Auslöser: `binary_sensor.ph_dosieranlage_stoerung`
  oder `..._not_halt` wechselt auf *Problem*. Aktion: Benachrichtigung.
  Als Text taugt `sensor.ph_dosieranlage_sperren` — der nennt den Grund.
* **Sensorausfall melden** — Auslöser: `binary_sensor.ph_dosieranlage_sensorfehler`
  ist länger als 15 Minuten *Problem*.
* **Tagesmenge ungewöhnlich hoch** — Auslöser: `sensor.ph_minus_heute`
  über 80 % der eingestellten Tagesmenge (steht als Attribut `maxd` an
  `sensor.ph_dosieranlage_zustand`). Das ist ein Hinweis auf eine driftende
  Sonde oder ein hydraulisches Problem, nicht auf normalen Betrieb.
* **Anlage stumm** — `sensor.ph_wert_pool` länger als 10 Minuten
  *nicht verfügbar*. Dann antwortet das Gerät nicht mehr.

**Bewusst nicht empfohlen:** eine Automation, die zyklisch `ph_dosieren`
aufruft. Die Dosierlogik samt Wartezeiten und Tageslimit gehört in die
Firmware — dort greift sie auch, wenn das WLAN weg ist oder HA neu startet.
Home Assistant sollte beobachten und im Notfall abschalten, nicht regeln.

---

## 4. Umwälz-Verriegelung über eine HA-Entität

Die Anlage kann sich vor jeder Dosierung bei Home Assistant vergewissern, dass
die Umwälzpumpe läuft. Abgefragt wird der Zustand einer Entität:

```text
GET http://<ha-host>:8123/api/states/switch.poolpumpe
Authorization: Bearer <Long-Lived Access Token>
```

Erwartet wird `"state": "on"`. Alles andere — `off`, `unavailable`, `unknown`,
keine Antwort — verhindert die Dosierung.

### Wann genau abgefragt wird

**Nicht zyklisch.** Die Abfrage ist die *letzte* Prüfung vor dem Pumpenstart und
läuft erst, wenn alles andere bereits „dosieren" sagt:

```text
Automatik an → kalibriert → Sensor gültig → Messwert stabil
→ pH über Soll+Totband → Tagesbudget übrig → Durchmischungspause abgelaufen
→ ERST JETZT: Entität abfragen
```

Daraus folgt der Abfragetakt von selbst:

| | |
|---|---|
| frühester Abstand zweier Abfragen | die Durchmischungspause, Standard **1800 s** |
| Obergrenze pro Tag | Tagesmenge ÷ Einzeldosis, also 60 ml ÷ 3 ml = **20** |
| bei erreichtem Sollwert | **0** — `LK_AT_TARGET` greift vorher |

Dazu kommen Wiederholungen: nach einem Fehlversuch nach 60 s, bei stehender
Umwälzung nach 120 s. Eine Antwort, die jünger als 120 s ist, wird
wiederverwendet statt neu abgefragt. Alle drei Zeiten sind einstellbar.

### Einrichten

Im Webinterface unter *Umwälzung (Home Assistant)*:

| Feld | Beispiel |
|---|---|
| Home Assistant Host:Port | `192.168.0.10:8123` |
| Entität | `switch.poolpumpe` |
| Zustand für „läuft" | `on` |
| Long-Lived Access Token | *(in HA unter Profil → Sicherheit erzeugen)* |

Danach **Jetzt testen** drücken — die Antwort nennt den gelesenen Zustand im
Klartext, etwa `switch.poolpumpe = on`, oder den Grund des Fehlschlags
(`Token abgelehnt (401)`, `Entitaet unbekannt (404)`).

Alternativ über die serielle Konsole:

```text
ha 192.168.0.10:8123 switch.poolpumpe
hatoken <token>
set circen 1
circ
```

### Zum Token

Ein Long-Lived Access Token gibt **vollen Zugriff auf die Home-Assistant-API** —
nicht nur auf diese eine Entität. Deshalb:

* In HA einen **eigenen, nicht-administrativen Benutzer** anlegen und den Token
  unter dessen Profil erzeugen.
* Der Token wird nur gespeichert, nie zurückgeliefert. Im Webinterface zeigt
  das Feld *Token hinterlegt* lediglich `ja`/`nein`; ein leeres Eingabefeld
  lässt ihn unverändert.
* Nur HTTP auf dem lokalen Port 8123. Für HTTPS bräuchte das Gerät eine
  Zertifikatsverwaltung, die den Aufwand hier nicht wert ist — deshalb gehört
  die Anlage ohnehin nicht ins Internet.

### Wenn Home Assistant ausfällt

Dann wird **nicht dosiert**. Das ist die sichere Richtung, aber es heißt auch:
Die Regelung hängt an der Verfügbarkeit von HA. Wer das nicht will, hängt die
Anlage stattdessen an denselben geschalteten Stromkreis wie die Pumpe — siehe
[SCHALTPLAN.md](SCHALTPLAN.md), Abschnitt 2.6. Beides zusammen geht auch.

---

## 5. Sicherheitshinweis

Die Anlage kann über das Netzwerk Säure dosieren. Deshalb:

* Web-Benutzer und Passwort setzen (Webinterface → Netzwerk).
* Die Anlage **nicht** ins Internet portfreigeben. Zugriff von außen nur
  über VPN oder den Home-Assistant-Fernzugriff.
* Am besten in ein separates IoT-VLAN/-WLAN legen.
