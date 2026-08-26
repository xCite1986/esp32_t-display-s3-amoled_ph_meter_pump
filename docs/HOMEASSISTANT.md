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
| POST | `/api/dose?ml=3.0` | manuelle Dosierung |
| POST | `/api/stop` | Pumpe anhalten |
| POST | `/api/estop` | Not-Halt, Automatik aus |
| POST | `/api/clearfault` | Störung quittieren |
| POST | `/api/auto?on=1` | Automatik ein/aus |
| POST | `/api/settings?sp=7.2&dose=3` | Parameter setzen |
| POST | `/api/cal?point=a&ph=7.00` | Kalibrierpunkt speichern |
| POST | `/api/pump/run?steps=16000&dir=1` | Servicelauf |
| POST | `/api/pump/calc?steps=16000&ml=9.4` | Schritte/ml berechnen |
| POST | `/api/daily/reset` | Tageszähler zurücksetzen |
| POST | `/api/reboot` | Neustart |

Ist im Webinterface ein Web-Benutzer gesetzt, verlangen alle Endpunkte
HTTP-Basic-Auth.

Beispielantwort (gekürzt):

```json
{
  "fw": "2.0.0", "up": 84213,
  "ph": 7.31, "phValid": true, "phStatus": "OK", "stable": true,
  "volt": 1.71234, "raw": 14021, "slope": -238.4,
  "state": "Bereit", "locks": "Sollwert erreicht", "lockBits": 2048,
  "auto": true, "estop": false, "fault": false, "circ": "Umwaelzung laeuft",
  "pump": { "run": false, "ml": 0.0, "target": 0.0 },
  "dose": { "today": 12.5, "remain": 47.5, "count": 4, "last": 3.0,
            "pauseS": 280, "total": 312.4 },
  "cfg": { "sp": 7.20, "maxd": 60.00, "spml": 1702.1 }
}
```

---

## 2. configuration.yaml

Ein einziger REST-Aufruf versorgt alle Sensoren — der `scan_interval`
gilt für die ganze Gruppe.

```yaml
rest:
  - resource: http://ph-dosierung.local/api/status
    scan_interval: 30
    # username: dosier
    # password: !secret ph_dosier_pw
    sensor:
      - name: "pH-Wert"
        unique_id: ph_dosier_wert
        value_template: "{{ value_json.ph | round(2) }}"
        state_class: measurement
        icon: mdi:ph
      - name: "pH ADC-Spannung"
        unique_id: ph_dosier_volt
        value_template: "{{ value_json.volt | round(4) }}"
        unit_of_measurement: "V"
        device_class: voltage
        state_class: measurement
      - name: "pH-Minus Dosierung heute"
        unique_id: ph_dosier_heute
        value_template: "{{ value_json.dose.today | round(2) }}"
        unit_of_measurement: "mL"
        state_class: total_increasing
      - name: "pH-Minus Gesamtmenge"
        unique_id: ph_dosier_gesamt
        value_template: "{{ value_json.dose.total | round(1) }}"
        unit_of_measurement: "mL"
        state_class: total_increasing
      - name: "pH-Anlage Zustand"
        unique_id: ph_dosier_zustand
        value_template: "{{ value_json.state }}"
      - name: "pH-Anlage Sperren"
        unique_id: ph_dosier_sperren
        value_template: "{{ value_json.locks }}"
    binary_sensor:
      - name: "pH-Sensor OK"
        unique_id: ph_dosier_sensor_ok
        value_template: "{{ value_json.phValid }}"
        device_class: problem
        payload_on: "False"
        payload_off: "True"
      - name: "pH-Anlage Störung"
        unique_id: ph_dosier_stoerung
        value_template: "{{ value_json.fault or value_json.estop }}"
        device_class: problem
        payload_on: "True"
        payload_off: "False"
      - name: "pH-Automatik aktiv"
        unique_id: ph_dosier_auto
        value_template: "{{ value_json.auto }}"
        payload_on: "True"
        payload_off: "False"

rest_command:
  ph_dosieren:
    url: "http://ph-dosierung.local/api/dose?ml={{ ml }}"
    method: POST
  ph_not_halt:
    url: "http://ph-dosierung.local/api/estop"
    method: POST
  ph_automatik:
    url: "http://ph-dosierung.local/api/auto?on={{ on }}"
    method: POST
  ph_sollwert:
    url: "http://ph-dosierung.local/api/settings?sp={{ sp }}"
    method: POST
```

Nach dem Eintragen: *Entwicklerwerkzeuge → YAML → REST-Entitäten neu laden.*

---

## 3. Sinnvolle Automationen

Diese in der HA-Oberfläche anlegen (Einstellungen → Automationen), nicht
in YAML:

* **Störung melden** — Auslöser: `binary_sensor.ph_anlage_stoerung` wechselt
  auf *Problem*. Aktion: Benachrichtigung aufs Handy.
* **Sensorausfall melden** — Auslöser: `binary_sensor.ph_sensor_ok` ist
  länger als 15 Minuten *Problem*.
* **Tagesmenge ungewöhnlich hoch** — Auslöser: `sensor.ph_minus_dosierung_heute`
  über 80 % der eingestellten Tagesmenge. Das ist ein Hinweis auf eine
  driftende Sonde oder ein hydraulisches Problem, nicht auf normalen Betrieb.
* **Automatik an Poolpumpe koppeln** — wenn die Umwälzpumpe aus ist,
  `ph_automatik` mit `on: 0` aufrufen. Robuster ist allerdings, die Anlage
  gleich an denselben geschalteten Stromkreis wie die Pumpe zu hängen: das
  wirkt auch dann, wenn Home Assistant gerade nicht läuft.

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
