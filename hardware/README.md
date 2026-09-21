# Hardware — Druckteile und Datenblätter

```text
pumpe/Peristaltic_Pump_V2.stl     Peristaltikkopf, 3D-Druckteil
datenblaetter/Steppermotor_DE.pdf Datenblatt des NEMA17 (früherer Stepper-Aufbau)
```

> **Ab Firmware 2.3.0: AC-Synchronmotor statt Schrittmotor.** Die Pumpe wird
> jetzt über ein 1-Kanal-Relais nur ein-/ausgeschaltet; die Menge ergibt sich
> aus der Laufzeit (ml/s). Das NEMA17-Datenblatt und der Schrittmotor-Abschnitt
> unten beschreiben den **früheren** Aufbau und bleiben als Referenz erhalten.

---

## Peristaltikkopf

`pumpe/Peristaltic_Pump_V2.stl` — 7884 Dreiecke, Binär-STL.

**„V2 Peristaltische Pumpe / Wasserpumpe / Dosierpumpe" von Max Puschmann**,
veröffentlicht am 10.01.2026 auf [MakerWorld](https://makerworld.com/de/models/2225892-v2-peristaltic-pump-water-pump-measuring-pump), lizenziert unter
[Creative Commons Attribution (CC BY)](https://creativecommons.org/licenses/by/4.0/deed.de).
Das Modell ist ein Remix einer früheren Peristaltikpumpe desselben Autors.

Die STL liegt hier unverändert mit bei — CC BY erlaubt das Weitergeben
ausdrücklich und verlangt dafür nur die Namensnennung. Die steht in diesem
Absatz.

### Druckparameter des Autors

| | |
|---|---|
| Schichthöhe | 0,2 mm |
| Wandungen | 4 |
| Füllung | 30 Prozent |
| Material | PLA, rund 119 g |
| Druckzeit | etwa 3 h auf 4 Platten |

Zusätzlich nötig: **vier 608-Kugellager** — drei als Rollen im Rotor, eines
in der Wellenaufnahme — und weicher Schlauch für den Pumpenkopf.

![Pumpenkopf montiert](../docs/bilder/03-pumpenkopf.jpg)

*Der gedruckte Kopf montiert (hier auf dem früheren NEMA17): drei Rollenlager
im Rotor, in der Mitte das 608er Kugellager der Wellenaufnahme.*

### Anpassung an den Motor — verstärkte Wellenaufnahme

Die **V2 des Modells hat eine verstärkte Wellenaufnahme** — genau die Stelle,
an der das gesamte Pumpenmoment vom Motor auf den Rotor übergeht. Beim
Schlauchquetschen ist das kein kleines Moment: der Rotor drückt den Schlauch
über die ganze Umschlingung zusammen, und das Losbrechmoment beim Anfahren
liegt deutlich über dem Dauermoment.

Eine dünn gedruckte Aufnahme leiert dort im Lauf der Zeit aus, die
Madenschraube gräbt sich ein, und die Pumpe fördert dann zu wenig, ohne dass
sich elektrisch etwas ändert — die Firmware verbucht weiter Milliliter nach
Laufzeit, die nie im Becken ankommen. **Das gehört zu den wenigen Fehlern, die
die Firmware nicht bemerken kann** — wie ein leerer Kanister oder ein
gerissener Schlauch: die Pumpe läuft, die Zählung stimmt, gefördert wird
nichts. Auffallen kann so etwas nur daran, dass der pH-Wert trotz Dosierung
nicht nachgibt. Deshalb ist die verstärkte Fassung hier die richtige.

> **Wellenaufnahme an den AC-Motor anpassen.** Der Kopf war ursprünglich für
> die **5-mm-Welle des NEMA17** gezeichnet. Der jetzt verwendete
> AC-Synchronmotor hat andere Wellen-/Flanschmaße — Wellendurchmesser,
> Abflachung/Passfeder und Befestigung des konkreten Motors **messen** und die
> Aufnahme (bzw. einen Adapter) darauf auslegen, bevor gedruckt wird. Die
> Madenschraube gehört auf die Abflachung der Welle, nicht auf das runde Stück.

Montage und Hydraulik: [../docs/LOETANLEITUNG.md](../docs/LOETANLEITUNG.md),
Abschnitt 13.

Der Schlauch im Kopf ist das Verschleißteil der ganzen Anlage — Norprene oder
Tygon, **kein Silikon** (quillt und wird von Säure angegriffen).

---

## Pumpenmotor (AC-Synchronmotor)

Aktuell treibt ein **AC-Synchron-Getriebemotor** die Pumpe — z. B. der im
Aufbau verwendete McMETEOR der SRF63-Serie. Werte laut Typenschild:

| | |
|---|---|
| Versorgung | 220–240 V AC, 50/60 Hz |
| Leistung | 8 W |
| Drehzahl | 10–12 U/min (Getriebeabtrieb) |
| Drehrichtung | eine Richtung (CW) |
| Isolationsklasse | F |

Eigenschaften, die für die Firmware zählen:

* **Konstante Drehzahl**, sobald Spannung anliegt — die Fördermenge ist damit
  reine Funktion der **Laufzeit**. Kalibriert wird als `ml/s`
  ([../docs/INBETRIEBNAHME.md](../docs/INBETRIEBNAHME.md), Phase 3).
* **Nur ein/aus** über ein 1-Kanal-Relais (kein Drehzahl-, kein Richtungs-
  wechsel). Verdrahtung und Sicherheit: [../docs/SCHALTPLAN.md](../docs/SCHALTPLAN.md)
  und [../docs/LOETANLEITUNG.md](../docs/LOETANLEITUNG.md).
* ⚠️ **230 V** — Aufbau der Netzseite durch eine befähigte Person.

> Beim Motortausch die Werte des konkreten Typenschilds übernehmen (Spannung,
> Leistung, Drehzahl, Drehrichtung) und die Förderrate `ml/s` neu kalibrieren.
> Wellen-/Flanschmaße bestimmen die Pumpenkopf-Aufnahme (siehe oben).

---

## Schrittmotor (früherer Aufbau, nur noch Referenz)

> Dieser Abschnitt gilt für den **früheren** Aufbau mit NEMA17 + TMC2209 und
> ist seit Firmware 2.3.0 nicht mehr aktuell. Das Datenblatt bleibt im Repo,
> weil die Datei referenziert und lizenziert dokumentiert ist.

`datenblaetter/Steppermotor_DE.pdf` — „Quick Start Anleitung,
Zweiphasen-Hybrid-Schrittmotor 42".

Kennwerte laut Seite 3:

| | |
|---|---|
| Referenz | NEMA17 |
| Strom/Phase | **0,4 A** |
| Schrittwinkel | 1,8° (200 Vollschritte/Umdrehung) |
| Phasen | 2 |
| Nennspannung | 12 V |
| Rahmengröße | 42 × 42 mm, Höhe 34 mm |
| Haltedrehmoment | „28 Nm" (siehe unten) |

### Spulenzuordnung

Seite 5 des Datenblatts, wörtlich:

> Spule A (grünes Kabel, schwarzes Kabel)
> Spule B (rotes Kabel, blaues Kabel).

Das deckte sich mit der Widerstandsmessung bei der Inbetriebnahme (je 3,6 Ω
zwischen den Adern eines Paares) und **widersprach der ursprünglichen
Projektbeschreibung**, die von rot+grün und blau+schwarz ausging.

### Zwei Stellen, an denen das Datenblatt nicht zum Aufbau passte

**„Haltedrehmoment 28 Nm" ist ein Fehler im Datenblatt.** 28 N·m wäre die
Größenordnung eines Industrieservos; ein NEMA17 mit 34 mm Baulänge liefert
typisch 0,28 N·m. Gemeint sind offensichtlich **28 N·cm**.

**Der eingestellte Strom lag über dem Nennstrom.** Das Datenblatt nennt
0,4 A pro Phase; im Betrieb stand VREF auf 1,2 V (rund 0,6 A am verwendeten
TMC2209-Modul), ermittelt über die Gehäusetemperatur. Mit 0,4 A rutschte der
Pumpenkopf unter Last durch. Für den aktuellen AC-Aufbau ist das
gegenstandslos.
