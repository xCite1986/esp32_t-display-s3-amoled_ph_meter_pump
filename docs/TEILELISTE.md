# Teileliste und Kostenübersicht

> **Die Preise sind Schätzwerte, keine Belege.** Sie geben die Größenordnung
> wieder, die im Sommer 2026 für Österreich/Deutschland realistisch war, und
> sind nicht die tatsächlich bezahlten Beträge dieses Aufbaus. Trag deine
> echten Werte ein, wenn du sie brauchst — die Tabelle ist als Vorlage
> gedacht.
>
> Die Spanne ist erheblich: die Elektronikmodule kosten bei Direktimport
> (AliExpress) oft **die Hälfte** des hier angesetzten Preises, bei
> europäischen Distributoren (Reichelt, Berrybase, Mouser) eher das
> Anderthalbfache. Chemie und Hydraulik schwanken kaum.

---

## 1. Steuerung und Messkette

| Pos | Teil | Menge | ca. € | Anmerkung |
|---|---|---:|---:|---|
| 1 | LilyGo T-Display S3 AMOLED 1,91" (`BOARD_AMOLED_191`) | 1 | 40 | misst, regelt, zeigt an, bedient, Webserver |
| 2 | ADS1115 Breakout, 16 bit, I²C | 1 | 5 | eigener Treiber, kein Fremdcode |
| 3 | pH-Signalboard mit BNC (PH4502C o. ä.) | 1 | 15 | liefert die Analogspannung |
| 4 | pH-Sonde, BNC, laborüblich (E-201-C) | 1 | 20 | Verschleißteil, siehe unten |
| 5 | TMC2209 Stepstick | 1 | 6 | Verschleißteil, Reserve sinnvoll |
| 6 | Treiber-Erweiterungskarte A4988/DRV8825-Raster | 1 | 3 | meist im 3er-Satz |
| 7 | NEMA17 Schrittmotor, 5-mm-Welle | 1 | 15 | 0,4 A laut Datenblatt |
| 8 | Netzteil 12 V / ≥ 3 A | 1 | 12 | |
| 9 | Buck-Converter 12 → 5 V, **≥ 1 A** | 1 | 3 | 0,5 A bricht ein, siehe Lötanleitung |
| | **Zwischensumme** | | **119** | |

## 2. Platine und Kleinteile

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 10 | Lochrasterplatine 100 × 80 mm, RM 2,54 | 1 | 2 |
| 11 | Stift-/Buchsenleisten-Sortiment | 1 Satz | 3 |
| 12 | Schraubklemmen RM 5,0 (3×) und RM 3,5 (2×) | 5 | 3 |
| 13 | Elko 100 µF/25 V low ESR, 2× 10 kΩ, Schottky SS34 | 1 Satz | 2 |
| 14 | Kühlkörper für den TMC2209 | 1 | 1 |
| 15 | Litze 0,5 mm² und 0,25 mm², Schrumpfschlauch | 1 Satz | 6 |
| 16 | Sicherungshalter + Feinsicherung 2 A träge | 1 | 3 |
| | **Zwischensumme** | | **20** |

## 3. Gehäuse und Montage

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 17 | Gehäuse IP54 mit Sichtfenster (Ausschnitt ~43 × 19 mm) | 1 | 15 |
| 18 | Abstandsbolzen M3, Schrauben | 1 Satz | 5 |
| | **Zwischensumme** | | **20** |

## 4. Pumpe

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 19 | Peristaltikkopf, 3D-Druck — [V2 Peristaltic Pump](https://makerworld.com/de/models/2225892-v2-peristaltic-pump-water-pump-measuring-pump) | 1 | 3 |
| 20 | Pumpenschlauch Norprene/Tygon (**kein Silikon**) | 1 m | 10 |
| | **Zwischensumme** | | **13** |

Der Kopf sitzt direkt auf der 5-mm-Welle des NEMA17 (Pos. 7). Materialkosten
sind reines Filament, gerechnet mit rund 100 g PETG. Wer nicht selbst druckt,
liegt bei einem gekauften Peristaltikkopf eher bei 25–50 €.

Der Schlauch ist das eigentliche Verschleißteil der Pumpe — Wechselintervall
typisch 500–1000 Betriebsstunden, Ersatz gleich mitbestellen.

## 5. Hydraulik

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 21 | Saug-/Druckschlauch, säurebeständig | 2 m | 8 |
| 22 | Impfventil (Rückschlagventil) für den Einspritzpunkt | 1 | 12 |
| 23 | Fußventil mit Ansaugfilter für den Kanister | 1 | 10 |
| 24 | Sondenhalter / Messzelle im Bypass | 1 | 15 |
| | **Zwischensumme** | | **45** |

## 6. Kalibrierung und Pflege

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 25 | Pufferlösung pH 7,00 und pH 4,00 | je 1 | 10 |
| 26 | KCl-Aufbewahrungslösung für die Sonde | 1 | 8 |
| | **Zwischensumme** | | **18** |

Puffer altern nach dem Öffnen. Zum Kalibrieren immer aus einem sauberen
Gefäß arbeiten und die Portion danach verwerfen — nie zurück in die Flasche.

---

## Summe

| Gruppe | ca. € |
|---|---:|
| Steuerung und Messkette | 119 |
| Platine und Kleinteile | 20 |
| Gehäuse und Montage | 20 |
| Pumpe | 13 |
| Hydraulik | 45 |
| Kalibrierung und Pflege | 18 |
| **Gesamt** | **235** |

Nicht enthalten: Werkzeug (Lötstation, Multimeter, Seitenschneider — siehe
[LOETANLEITUNG.md](LOETANLEITUNG.md), Abschnitt 1), 3D-Drucker, und das
pH-Minus selbst als laufender Verbrauch.

---

## Laufende Kosten

| Posten | Intervall | ca. € |
|---|---|---:|
| Pumpenschlauch | 500–1000 Betriebsstunden | 10 |
| pH-Sonde | 1–2 Jahre | 20 |
| Pufferlösungen | jährlich | 10 |
| Strom | Dauerbetrieb ~1,5 W, Motor nur sekundenweise | ~4 / Jahr |

Der Stromwert ist gerechnet, nicht gemessen: rund 1,5 W Dauerlast bei
aktivem Display ergeben etwa 13 kWh im Jahr. Der Motor fällt nicht ins
Gewicht — er läuft pro Dosierung wenige Sekunden.

Mit Standby und Nachtabschaltung (siehe [BEDIENPANEL.md](BEDIENPANEL.md))
liegt der reale Verbrauch darunter.

---

## Was man weglassen kann und was nicht

**Weglassen möglich:** Gehäuse mit Sichtfenster (wenn die Anlage ohnehin im
trockenen Technikraum steht und niemand aufs Display schaut — dann reicht ein
einfaches Gehäuse), Sondenhalter (bei vorhandener Messstelle), Reserve-
Schraubklemme KL5.

**Nicht weglassen:**

* Die **2-A-Sicherung** in der 12-V-Zuleitung.
* Das **Impfventil** am Einspritzpunkt — ohne das drückt Poolwasser in die
  Dosierleitung zurück.
* Der **Buck mit mindestens 1 A**. Ein 0,5-A-Typ bricht ein, sobald Display
  und Motor gleichzeitig ziehen, und das äußert sich als sporadischer Reset
  mitten in einer Dosierung.
* Beide **Pufferlösungen**. Eine Ein-Punkt-Kalibrierung kennt die Steilheit
  der Sonde nicht, und genau deren Alterung ist das, was driftet.
