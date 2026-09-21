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

> **Ab Firmware 2.3.0: Pumpe an Netzspannung.** Statt Schrittmotor (NEMA17)
> und TMC2209 treibt jetzt ein **AC-Synchronmotor** (230 V) über ein
> **1-Kanal-Relais**; die Versorgung kommt aus einem **AC/DC-Netzteil
> 230 V → 5 V**. Das 12-V-Netzteil, der Buck-Converter und der Treiber
> entfallen. ⚠️ Damit ist Netzspannung im Spiel — siehe Sicherheitshinweise in
> [SCHALTPLAN.md](SCHALTPLAN.md) und [LOETANLEITUNG.md](LOETANLEITUNG.md).

| Pos | Teil | Menge | ca. € | Anmerkung |
|---|---|---:|---:|---|
| 1 | LilyGo T-Display S3 AMOLED 1,91" (`BOARD_AMOLED_191`) | 1 | 40 | misst, regelt, zeigt an, bedient, Webserver |
| 2 | ADS1115 Breakout, 16 bit, I²C | 1 | 5 | eigener Treiber, kein Fremdcode |
| 3 | pH-Signalboard mit BNC (PH4502C o. ä.) | 1 | 15 | liefert die Analogspannung |
| 4 | pH-Sonde, BNC, laborüblich (E-201-C) | 1 | 20 | Verschleißteil, siehe unten |
| 5 | 1-Kanal-Relaismodul, 5-V-Spule (JQC-3FF-S-Z, 10 A/250 V) | 1 | 3 | schaltet die Pumpe, Header `S·+·−` |
| 6 | AC-Synchronmotor 230 V (z. B. McMETEOR SRF63DA, 10–12 U/min, 8 W) | 1 | 15 | konstante Drehzahl, eine Drehrichtung |
| 7 | AC/DC-Netzteil 230 V → 5 V, **≥ 1 A** | 1 | 5 | TSP-05 (3 W) grenzwertig; HLK-10M05 (2 A) robuster |
| 8 | I²C-Isolator ISO1540/ISO1541, als Modul | 1 | 7 | **nicht weglassen**, siehe unten |
| 9 | Isolierter DC-DC 5 → 9 V, 1 W (B0509S-1W) | 1 | 2 | versorgt die Messseite |
| 10 | Linearregler AMS1117-5.0 | 1 | 1 | macht daraus geregelte 5 V |
| | **Zwischensumme** | | **113** | |

**Warum die drei Teile in Position 8–10 nicht optional sind:** Eine pH-Sonde
hat bis zu 250 MΩ Innenwiderstand. Der Ableitstrom eines Schaltnetzteils sucht
über die Y-Kondensatoren einen Weg zur Erde und nimmt ihn durch die
Glaselektrode — zwei Nanoampere genügen für einen halben Volt Fehler.

Gemessen an diesem Aufbau: **712 mV Messspanne am Netzteil, 0,7 mV an einer
Powerbank.** Faktor tausend, bei sonst identischem Aufbau. Ohne galvanische
Trennung ist die Messung im Becken nicht kalibrierbar. Details in
[INBETRIEBNAHME.md](INBETRIEBNAHME.md).

## 2. Platine und Kleinteile

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 11 | Lochrasterplatine 100 × 80 mm, RM 2,54 | 1 | 2 |
| 12 | Stift-/Buchsenleisten-Sortiment | 1 Satz | 3 |
| 13 | Schraubklemmen Kleinspannung + **netzspannungsfeste** Klemmen für L/N/Motor | 1 Satz | 4 |
| 14 | Elko 470–1000 µF/25 V (**C_bulk**) + 100 nF, 10 kΩ (**Pulldown S**), Schottky SS34 (**D1**) | 1 Satz | 2 |
| 15 | Litze 0,25 mm² (Signal) + **H05VV-F 0,75 mm² für 230 V**, Schrumpfschlauch | 1 Satz | 7 |
| 16 | Sicherungshalter + Feinsicherung **1 A träge (Netzseite)** | 1 | 3 |
| | **Zwischensumme** | | **21** |

## 3. Gehäuse und Montage

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 17 | Gehäuse IP54 mit Sichtfenster (Ausschnitt ~43 × 19 mm), **berührungssicher für 230 V** | 1 | 15 |
| 18 | Abstandsbolzen M3, Schrauben | 1 Satz | 5 |
| | **Zwischensumme** | | **20** |

## 4. Pumpe

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 19 | Peristaltikkopf, 3D-Druck — [V2 Peristaltic Pump](https://makerworld.com/de/models/2225892-v2-peristaltic-pump-water-pump-measuring-pump), [STL im Repo](../hardware/pumpe/Peristaltic_Pump_V2.stl) | 1 | 3 |
| 20 | Pumpenschlauch Norprene/Tygon (**kein Silikon**) | 1 m | 10 |
| 21 | Kugellager 608, für Rotor und Wellenaufnahme | 4 | 4 |
| | **Zwischensumme** | | **17** |

> **Mechanische Anpassung an den AC-Motor:** Der V2-Kopf ist für die 5-mm-Welle
> des NEMA17 gezeichnet. Der AC-Synchronmotor (Pos. 6) hat eine andere Welle/
> Flanschform — Kopfaufnahme und Motorhalter müssen dazu passen (ggf. Adapter
> oder eine an den Motor angepasste Kopfvariante). Vor dem Druck die Welle des
> konkreten Motors messen.

Der Kopf sitzt auf der Welle des AC-Synchronmotors (Pos. 6). Der Autor gibt
**119 g PLA** an, 0,2 mm Schicht, 4 Wandungen, 30 Prozent Füllung, rund 3 h
Druckzeit — die 3 € sind also reines Filament. Wer nicht selbst druckt, liegt
bei einem gekauften Peristaltikkopf eher bei 25–50 €.

Die vier 608-Kugellager sitzen als Rollen im Rotor (drei) und in der
Wellenaufnahme (eines).

Der Schlauch ist das eigentliche Verschleißteil der Pumpe — Wechselintervall
typisch 500–1000 Betriebsstunden, Ersatz gleich mitbestellen.

## 5. Hydraulik

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 22 | Saug-/Druckschlauch, säurebeständig | 2 m | 8 |
| 23 | Impfventil (Rückschlagventil) für den Einspritzpunkt | 1 | 12 |
| 24 | Fußventil mit Ansaugfilter für den Kanister | 1 | 10 |
| 25 | Sondenhalter / Messzelle im Bypass | 1 | 15 |
| | **Zwischensumme** | | **45** |

## 6. Kalibrierung und Pflege

| Pos | Teil | Menge | ca. € |
|---|---|---:|---:|
| 26 | Pufferlösung pH 7,00 und pH 4,00 | je 1 | 10 |
| 27 | KCl-Aufbewahrungslösung für die Sonde | 1 | 8 |
| | **Zwischensumme** | | **18** |

Puffer altern nach dem Öffnen. Zum Kalibrieren immer aus einem sauberen
Gefäß arbeiten und die Portion danach verwerfen — nie zurück in die Flasche.

---

## Summe

| Gruppe | ca. € |
|---|---:|
| Steuerung und Messkette | 113 |
| Platine und Kleinteile | 21 |
| Gehäuse und Montage | 20 |
| Pumpe | 17 |
| Hydraulik | 45 |
| Kalibrierung und Pflege | 18 |
| **Gesamt** | **234** |

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
| Strom | Dauerbetrieb ~1,5 W, AC-Motor (8 W) nur sekundenweise | ~4 / Jahr |

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

* Die **Vorsicherung in der Phase** (Netzseite), vor TSP-05 und Relais.
* Das **Impfventil** am Einspritzpunkt — ohne das drückt Poolwasser in die
  Dosierleitung zurück.
* Der **Stützkondensator an der 5-V-Schiene** und ein **ausreichend großes
  5-V-Netzteil**. Ein knappes Netzteil bricht ein, sobald das WLAN sendet oder
  das Relais anzieht, und das äußert sich als sporadischer Reset mitten in
  einer Dosierung.
* Der **10 kΩ Pulldown am Relais-Signal `S`** — hält die Pumpe beim Booten aus.
* Beide **Pufferlösungen**. Eine Ein-Punkt-Kalibrierung kennt die Steilheit
  der Sonde nicht, und genau deren Alterung ist das, was driftet.
