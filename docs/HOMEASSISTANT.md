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
  "fw": "1.0.0", "up": 84213,
  "ph": 7.31, "phValid": true, "phStatus": "OK", "stable": true,
  "volt": 1.71234, "raw": 14021, "slope": -238.4,
  "state": "Bereit", "locks": "Sollwert erreicht", "lockBits": 2048,
  "auto": true, "estop": false, "fault": false, "flow": true,
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
  `ph_automatik` mit `on: 0` aufrufen. Sauberer ist allerdings die
  Hardware-Rückmeldung an GPIO1 (`set flowreq 1`), weil die dann auch
  ohne Home Assistant wirkt.

**Bewusst nicht empfohlen:** eine Automation, die zyklisch `ph_dosieren`
aufruft. Die Dosierlogik samt Wartezeiten und Tageslimit gehört in die
Firmware — dort greift sie auch, wenn das WLAN weg ist oder HA neu startet.
Home Assistant sollte beobachten und im Notfall abschalten, nicht regeln.

---

## 4. Sicherheitshinweis

Die Anlage kann über das Netzwerk Säure dosieren. Deshalb:

* Web-Benutzer und Passwort setzen (Webinterface → Netzwerk).
* Die Anlage **nicht** ins Internet portfreigeben. Zugriff von außen nur
  über VPN oder den Home-Assistant-Fernzugriff.
* Am besten in ein separates IoT-VLAN/-WLAN legen.
