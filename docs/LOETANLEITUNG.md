# Lötanleitung — pH-Minus-Dosieranlage

Begleitend: [SCHALTPLAN.md](SCHALTPLAN.md) und [schaltplan.svg](schaltplan.svg).

Steuerung ist der **LilyGo T-Display S3 AMOLED** — er misst, regelt, treibt die
Pumpe und ist gleichzeitig Anzeige und Bedienteil. Ein separater ESP32-C3
kommt nicht mehr vor.

Display und Touch sind auf dem Board integriert: dafür ist **nichts zu löten**.
Verdrahtet werden nur Stromversorgung, ADS1115, TMC2209 und Motor.

Die Reihenfolge ist bewusst so gewählt, dass nach jedem Abschnitt geprüft
werden kann, bevor mehr Spannung ins Spiel kommt. **Bitte nicht vorgreifen** —
insbesondere darf der Motor erst dran, wenn VREF eingestellt ist.

---

## 0. Sicherheit zuerst

**Elektrisch**

* Nur mit **gezogenem Netzstecker** löten. Der 12-V-Zweig kann bei Kurzschluss
  mehrere Ampere liefern — das reicht für Brandspuren und geschmolzene Litze.
* Eine **2-A-Sicherung (träge)** in die 12-V-Zuleitung. Nicht optional.
* **Der Motorstecker wird niemals bei eingeschaltetem VMOT gezogen oder
  gesteckt.** Das zerstört den TMC2209 zuverlässig.
* Elkos richtig herum: Minusseite ist am Gehäuse markiert.

**Chemisch (später bei der Inbetriebnahme)**

* pH-Minus ist Säure. Schutzbrille und Handschuhe.
* pH-Minus **niemals** mit Chlorprodukten mischen — es entsteht Chlorgas.
* Der Säurebehälter steht **tiefer als die Pumpe**, damit bei einem
  Schlauchdefekt nichts nachlaufen kann.
* Am Einspritzpunkt gehört ein **Rückschlagventil**, damit kein Poolwasser
  in die Dosierleitung zurückdrückt.
* Der Schlauch muss säurebeständig sein (Norprene/Tygon für Chemie,
  **kein** Silikon — Silikon quillt und wird von Säure angegriffen).

---

## 1. Werkzeug und Material

**Werkzeug**

* Lötkolben mit temperaturgeregelter Spitze, 320–350 °C
* Lötzinn 0,7–1,0 mm (bleihaltig lötet sich für Handarbeit deutlich einfacher)
* Seitenschneider, Abisolierzange, Pinzette
* Multimeter (Durchgangsprüfer, DC-Spannung, Widerstand)
* Kleiner Keramik-/Kunststoffschraubendreher für das VREF-Poti

**Material**

Preise und die vollständige Liste inklusive Hydraulik und Chemie stehen in
[TEILELISTE.md](TEILELISTE.md).

| Pos | Teil | Menge |
|---|---|---|
| 1 | Lochrasterplatine 100 × 80 mm, RM 2,54 | 1 |
| 2 | Stift-/Buchsenleiste passend zum Header des S3 AMOLED | 1 Satz |
| 3 | Buchsenleiste 1×8 (für TMC2209) | 2 |
| 4 | Buchsenleiste 1×10 (für ADS1115) | 1 |
| 5 | Schraubklemme 2-polig, RM 5,0 (12 V, Motor) | 3 |
| 6 | Schraubklemme 3-polig, RM 3,5 (pH-Board, Reserve) | 2 |
| 7 | Elko 100 µF / 25 V, low ESR (**C1**) | 1 |
| 8 | Widerstand 10 kΩ (**R1**, EN-Pull-up) | 1 |
| 9 | Widerstand 10 kΩ (**R2**, Schutz vor A0) | 1 |
| 10 | Schottky-Diode SS34 oder 1N5819 (**D1**) | 1 |
| 11 | Kühlkörper für den TMC2209 | 1 |
| 12 | Litze 0,5 mm² rot/schwarz (Leistung) | je 1 m |
| 13 | Litze 0,25 mm² diverse Farben (Signal) | je 1 m |
| 14 | Schrumpfschlauch-Sortiment | 1 |
| 15 | Abstandsbolzen M3 + Gehäuse (IP54 empfohlen) | 1 Satz |
| 16 | I²C-Isolator ISO1540/ISO1541 als Modul | 1 |
| 17 | Isolierter DC-DC 5 → 9 V, 1 W (**B0509S-1W**) | 1 |
| 18 | Linearregler AMS1117-5.0 | 1 |
| 19 | Elko 10 µF und 22 µF, 2× 100 nF (Filter isolierte Seite) | 1 Satz |
| 20 | Ferritperle für die isolierte 5-V-Zuleitung | 1 |

Zusätzlich (siehe Abschnitt 12):

| Pos | Teil | Menge |
|---|---|---|
| 21 | Gehäuse mit Sichtfenster für das Display | 1 |

> **Der Buck-Converter muss mindestens 1 A liefern.** Das Displayboard zieht
> je nach Helligkeit 150–300 mA, dazu kommen pH-Board und Reserve. Ein kleiner
> 0,5-A-Buck bricht ein, sobald das Display hell wird und gleichzeitig der
> Motor anläuft.

> **Buchsenleisten statt Direktlöten.** TMC2209 und ADS1115 werden gesteckt,
> nicht eingelötet. Der TMC2209 ist ein Verschleißteil — und Auslöten von
> 16 Pins auf Lochraster endet meistens mit einer zerstörten Platine.
> Zum Displayboard führen ohnehin nur wenige Adern; die kommen an eine
> steckbare Verbindung, damit das Board für Reparaturen frei wird.

---

## 2. Baugruppe A — Platine vorbereiten

![Platine mit gesteckten Modulen](bilder/01-platine-bestueckt.jpg)

*Die bestückte Platine vor dem Verdrahten. Links oben das pH-Signalboard mit
BNC-Buchse und den beiden Trimmpotis, in der Mitte der ADS1115, rechts das
T-Display S3 AMOLED. Links unten die Treiber-Erweiterungskarte mit dem
TMC2209 und dem blauen Kühlkörper, daneben der Buck-Converter. TMC2209 und
ADS1115 stehen bewusst weit auseinander — der Treiber wird warm.*

1. Platine so ausrichten, dass später gilt: **12 V/Motor links, Signale rechts.**
   Das hält die Leistungsströme von der Messkette fern.
2. Die Buchsenleisten für TMC2209 und ADS1115 probeweise bestücken und die
   Position anzeichnen. Zwischen TMC2209 und ADS1115 mindestens 20 mm Abstand
   lassen — der Treiber wird warm. Das Displayboard sitzt nicht auf der
   Lochrasterplatine, sondern hinter dem Gehäusefenster und wird über eine
   steckbare Leitung angebunden.
3. Buchsenleisten löten: erst **je einen Eckpin** anlöten, Ausrichtung
   prüfen (Leiste muss plan aufliegen), dann die restlichen Pins.
4. Schraubklemmen einlöten:
   * KL1 (2-polig, RM 5,0): 12-V-Eingang
   * KL2 (2-polig, RM 5,0): Motor Spule 1
   * KL3 (2-polig, RM 5,0): Motor Spule 2
   * KL4 (3-polig, RM 3,5): pH-Board (V+, G, PO)
   * KL5 (3-polig, RM 3,5): Reserve
   * KL6 (6-polig oder Stiftleiste): Leitung zum Displayboard
     (5 V, GND, 3V3, STEP, DIR, EN, SDA, SCL — Aufteilung nach Platzangebot)
5. **GND-Sternpunkt** anlegen: ein kräftiger Lötpunkt (oder ein 2-poliger
   Lötstützpunkt) etwa mittig auf der Platine. Alle Massen laufen dorthin,
   nicht kreuz und quer von Modul zu Modul.

**Prüfen:** Durchgangsprüfer zwischen benachbarten Pins jeder Buchsenleiste —
darf **nirgends** piepen. Lötbrücken jetzt finden, nicht später.

---

## 3. Baugruppe B — Masse und 12-V-Zweig

1. Alle GND-Pins mit 0,5 mm² schwarz sternförmig an den GND-Sternpunkt:
   * KL1 Minus
   * TMC2209-Sockel `GND` (beide, Leistungs- und Logikseite)
   * KL6 `GND` (zum Displayboard)
   * ADS1115-Sockel `GND`
   * KL4 `G`
   * Buck `IN−` und `OUT−`
2. `+12 V` von KL1 auf den TMC2209-Sockel `VMOT` — kurze, dicke Leitung.
3. **C1 (100 µF)** direkt zwischen die Sockelpins `VMOT` und `GND` löten,
   so dicht wie möglich am Treiber. Plus an VMOT, Minus (markierte Seite)
   an GND. Ohne diesen Elko können Spannungsspitzen beim Motorstart den
   TMC2209 zerstören.
4. `+12 V` von KL1 zusätzlich zum Buck-Converter `IN+`.

**Prüfen:**
* Durchgang KL1-Minus ↔ Sternpunkt: piept.
* Durchgang KL1-Plus ↔ KL1-Minus: **piept nicht** (nur ein kurzes Aufladen
  von C1, dann muss es hochohmig werden).

---

## 4. Baugruppe C — Buck-Converter einstellen

**Dieser Schritt passiert isoliert, bevor der Buck an die Platine kommt.**

1. Buck-Modul (**min. 1 A**) **ohne** angeschlossene Last mit 12 V versorgen.
2. Ausgangsspannung messen und mit dem Trimmpoti auf **5,00 V** einstellen.
   Bei MP1584-Modulen sind das oft mehrere Umdrehungen — geduldig drehen und
   dabei messen.
3. 12 V abschalten, Poti mit einem Tropfen Nagellack sichern.
4. Erst jetzt: `OUT+` über **D1** (Schottky, Ring/Kathode Richtung Display)
   auf KL6 `5V`, `OUT−` an den Sternpunkt.
5. Vom Punkt hinter D1 zusätzlich eine Leitung zu KL4 `V+`
   — **aber noch nicht anschließen**, bis Abschnitt 8 (Messung am pH-Board)
   erledigt ist. Bis dahin die Ader isoliert beiseitelegen.

**Prüfen:** Nach dem Einschalten (nur Buck, Displayboard noch nicht
angeschlossen) muss an D1-Kathode gegen GND ca. **4,6–4,8 V** liegen
(5,0 V minus Diodenspannung).

Die 5 V gehen auf ein **`VBUS`**-Pad der linken Stiftleiste (dort gibt es
zwei davon, direkt über `GPIO16`). `VBUS` liegt board-intern parallel zur
5-V-Schiene des USB-C-Anschlusses — genau deshalb sitzt D1 in der Zuleitung.
Der Akkuanschluss bleibt frei.

---

## 5. Baugruppe D — Signalleitungen Displayboard ↔ TMC2209

Mit 0,25 mm² Litze, möglichst kurz und nicht parallel zu den Motorleitungen:

| Von | Nach | Farbvorschlag |
|---|---|---|
| S3 `GPIO11` | TMC2209 `STEP` | gelb |
| S3 `GPIO12` | TMC2209 `DIR` | grün |
| S3 `GPIO10` | TMC2209 `EN` | weiß |
| S3 `3V3` | TMC2209 `VIO` | rot dünn |

Alle benötigten Pins liegen auf der **linken Stiftleiste**, von oben nach
unten: `3V3 · 1 · 2 · 3 · 10 · 11 · 12 · 13 · 14 · 15 · GND · VBUS · VBUS · 16`.

> **Nicht an GPIO 2 und 3 gehen.** Die sehen auf der Leiste frei aus, hängen
> bei der Touch-Variante aber am CST816T. Wer sie belegt, verliert den Touch.

Dann die drei Ergänzungen am TMC2209-Sockel:

5. **R1 (10 kΩ)** von `EN` nach `VIO`. Damit ist der Treiber gesperrt,
   solange der ESP32 bootet oder im Reset hängt — ohne diesen Widerstand
   kann ein floatender EN-Pin den Motor unkontrolliert bestromen.
6. `MS1` **und** `MS2` mit einer Drahtbrücke auf `VIO` (3,3 V) legen.
   Ergibt 1/16 Microstepping = 3200 Schritte pro Umdrehung.
7. `SPREAD`, `DIAG`, `INDEX`, `PDN/UART` bleiben **offen** — nichts anlöten.
   `GPIO15` bleibt am S3 unbeschaltet, aber als UART-Reserve zugänglich.

**Prüfen:**
* `EN` ↔ `VIO`: ca. 10 kΩ.
* `MS1` ↔ `VIO` und `MS2` ↔ `VIO`: Durchgang.
* `STEP`, `DIR`, `EN` gegen GND: **kein** Durchgang.

---

## 6. Baugruppe E — isolierte Messseite

**Die gesamte Messkette liegt hinter einer galvanischen Trennstelle.** Das ist
keine Verfeinerung, sondern Voraussetzung: eine pH-Elektrode hat bis zu
250 MΩ Innenwiderstand, und der Ableitstrom des Netzteils sucht seinen Weg zur
Erde durch genau diese Elektrode. Gemessen an diesem Aufbau waren es **712 mV
Messspanne am Netzteil gegen 0,7 mV an einer Powerbank** — Faktor tausend, bei
sonst unverändertem Aufbau. Ohne Trennung ist die Anlage im Becken nicht
kalibrierbar. Hergang in [INBETRIEBNAHME.md](INBETRIEBNAHME.md).

### 6.1 Isolierte Versorgung

1. **Zweiten Massepunkt anlegen.** Ein eigener Lötstützpunkt, mit deutlichem
   Abstand zum Sternpunkt, am besten optisch als eigener Bereich markiert.
   Er heißt ab hier `GND iso`.
2. `B0509S` `+Vin` an die 5-V-Schiene (D1-Kathode), `−Vin` an den
   **Sternpunkt**. Das ist die letzte Verbindung zur netzbezogenen Seite.
3. `B0509S` `−Vout` an `GND iso`, `+Vout` an `AMS1117-5.0` `IN`.
   **10 µF direkt am Wandlerausgang.**
4. `AMS1117` `GND` an `GND iso`, `OUT` über die **Ferritperle** auf die
   isolierte 5-V-Schiene. **22 µF und 100 nF am Reglerausgang**, dazu je
   100 nF direkt an pH-Board und ADS1115.

> **Kein B0505S.** Ein ungeregelter 1-W-Wandler liefert bei den hier
> benötigten rund 25 mA — 12 % seiner Nennlast — eher 5,5 bis 6 V. Der
> ADS1115 verträgt maximal 5,5 V. Der Umweg über 9 V und den Linearregler
> kostet 50 Cent und nimmt diese Unsicherheit heraus.

### 6.2 Isolator

5. `ISO1540` `VCC1` an S3 `3V3`, `GND1` an den **Sternpunkt**.
6. `ISO1540` `SDA1` an S3 `GPIO13`, `SCL1` an S3 `GPIO14` — das ist der
   **zweite** I²C-Bus (`Wire1`). Der Touchcontroller des Displays hat seinen
   eigenen Bus auf GPIO2/3; der bleibt unangetastet.
7. `ISO1540` `VCC2` an die isolierte 5-V-Schiene, `GND2` an `GND iso`.
8. `ISO1540` `SDA2`/`SCL2` an ADS1115 `SDA`/`SCL`.

> **Pull-ups auf Seite 1 nicht vergessen.** Bisher hat den Bus allein das
> ADS1115-Breakout mit seinen 10 kΩ hochgezogen. Die sitzen jetzt hinter der
> Trennstelle. Bringt das Isolatormodul auf Seite 1 keine mit, gehören dort
> **je 4,7 kΩ von SDA und SCL nach 3,3 V** hin — sonst bleibt der Bus tot und
> es sieht aus wie ein defekter Isolator.

### 6.3 ADS1115

9. `VDD` des ADS1115-Sockels an die **isolierte 5-V-Schiene**, `GND` an
   `GND iso`.

   > Früher stand hier „nicht 5 V". Das galt, solange der ADS1115 direkt am
   > ESP32 hing — dessen GPIOs sind nicht 5-V-tolerant. Hinter dem Isolator
   > ist das gegenstandslos: die Pegelanpassung macht der ISO1540. pH-Board
   > und ADS1115 **müssen** auf derselben Schiene liegen, sonst arbeiten die
   > Pull-ups des Breakouts gegen einen anderen Pegel.
10. `ADDR` an `GND iso` (I²C-Adresse 0x48).

> Die **Qwiic-Buchse** des Displayboards scheidet mit der Trennung aus: sie
> führt Masse und 3,3 V der netzbezogenen Seite direkt heran und würde die
> Trennstelle überbrücken.
11. **R2 (10 kΩ)** von KL4 `PO` zum ADS1115-Sockel `A0`.
    Den Widerstand direkt an der Klemme anlöten und die Verbindung zu `A0`
    möglichst kurz halten. R2 liegt vollständig auf der isolierten Seite.
12. `A1`, `A2`, `A3` bleiben frei.

**Prüfen — und zwar am LEEREN Sockel, bevor der Chip hineinkommt:**

> **Diese Messung entscheidet über das Bauteil.** Der ADS1115 verträgt laut
> Datenblatt maximal 5,5 V an `VDD`. Liegen dort versehentlich die 12 V der
> Motorschiene, raucht er in dem Moment ab, in dem du ihn einsteckst — und ein
> IC, das geraucht hat, ist Schrott, auch wenn es danach noch antwortet.
>
> Also: Modul **draußen lassen**, Anlage einschalten, und am leeren Sockel
> messen:
>
> * `VDD` gegen `GND iso`: **5,0 V ± 0,1** vom AMS1117. Nicht 9 V, nicht 12 V.
> * Messspitzen tauschen: der Wert muss negativ werden. Bestätigt, dass
>   Versorgung und Masse nicht verpolt sind.
> * Anlage wieder ausschalten, dann erst das Modul stecken.
>
> Dieselbe Messung lohnt an jedem Sockel, in dem ein Halbleiter sitzt.

Danach, stromlos:
* **`GND iso` ↔ Sternpunkt: KEIN Durchgang.** Das ist die eine Messung, die
  über Erfolg oder Misserfolg des ganzen Umbaus entscheidet. Bleibt irgendwo
  eine Ader stehen, funktioniert alles wie vorher — nur die Störung ist
  wieder da, und man sucht sie lange. Dasselbe zwischen isolierter 5-V-Schiene
  und der netzbezogenen 5-V-Schiene.
* `VDD` ↔ `GND iso` am ADS-Sockel: **kein** Durchgang.
* KL4 `PO` ↔ ADS `A0`: ca. 10 kΩ.
* `SDA1` ↔ 3,3 V am Isolator: ca. 4,7–10 kΩ. Misst du „unendlich", fehlen die
  Pull-ups auf Seite 1 — siehe oben.

---

## 7. Erster Funktionstest — nur Logik, kein Motor, keine 12 V

1. **TMC2209-Modul noch NICHT stecken.** 12-V-Netzteil bleibt aus.
2. ADS1115 in den Sockel stecken, Displayboard über KL6 anschließen —
   aber **nur** GND, 3V3, SDA und SCL. Die 5-V-Ader bleibt vorerst ab.
3. Displayboard nur per **USB-C** mit dem PC verbinden.
4. Testsketch `tools/i2c_adc_test` flashen (siehe
   [INBETRIEBNAHME.md](INBETRIEBNAHME.md), Phase 1).
5. Im seriellen Monitor muss stehen: `gefunden: 0x48 <- sieht nach ADS1115 aus`.

Kommt hier nichts, liegt es fast immer an: SDA/SCL vertauscht, GND fehlt,
VDD fehlt, oder Pull-ups fehlen. Erst weitermachen, wenn 0x48 erscheint.

> Der Touchcontroller `0x15` liegt auf dem **anderen** I²C-Bus (GPIO2/3) und
> taucht in diesem Scan nicht auf. Das ist richtig so.

---

## 8. pH-Board messen und anschließen

**Erst jetzt** wird das pH-Board mit Spannung versorgt — und zwar zunächst
auf dem Tisch, nicht an der Platine.

1. Aufdruck des Boards lesen: 5 V oder 3,3–5 V?
2. Board provisorisch mit der passenden Spannung versorgen (Labornetzteil
   oder der bereits eingestellte Buck), `G` an GND.
3. pH-Sonde anstecken, in **pH-7-Pufferlösung** stellen, 2 Minuten warten.
4. `PO` gegen `G` messen und notieren: ____ V
5. Sonde spülen (destilliertes Wasser), in **pH-4-Pufferlösung**,
   2 Minuten warten, `PO` messen und notieren: ____ V
6. Auswerten:
   * Beide Werte ≤ 3,2 V → alles gut, `PO` direkt an KL4 anschließen.
   * Ein Wert > 3,2 V → Spannungsteiler ergänzen: 10 kΩ von `PO` nach
     `A0`-Knoten (das ist bereits R2) plus 20 kΩ von diesem Knoten nach GND.
     Damit landen 3,3 V-Eingangssignal ≈ 2,2 V am ADC. Die Kalibrierung
     rechnet den Faktor automatisch heraus.
7. Erst jetzt die in Abschnitt 4 beiseitegelegte `V+`-Ader an KL4 anschließen.
8. pH-Board und Sonde an KL4 verdrahten: `V+`, `G`, `PO`.

**Kabelführung:** Das BNC-Kabel der Sonde und die Leitung zum pH-Board
möglichst kurz halten und mit **mindestens 10 cm Abstand** zu den
Motorleitungen verlegen. Das Sondensignal ist hochohmig und fängt
Störungen aus den Motorleitungen sonst zuverlässig ein.

---

## 9. TMC2209 vorbereiten — VREF einstellen

**Bevor der Motor angeschlossen wird.**

1. Kühlkörper auf den Treiberchip kleben (Wärmeleitpad, nicht auf die
   Rückseite der Platine).
2. TMC2209-Modul in den Sockel stecken. **Motor noch nicht anschließen.**
   Auf die Einbaurichtung achten — die Pinbeschriftung des Moduls muss
   zur Beschriftung der Platine passen. Verkehrt herum gesteckt raucht der
   Treiber sofort ab.
3. 12 V einschalten.
4. VREF messen: Multimeter-Minus an GND, Plus an den **Schleifer des
   Trimmpotis** (bei vielen Modulen gibt es einen kleinen Testpunkt daneben).
   Kein metallischer Schraubendreher am Poti, während gemessen wird —
   ein Ausrutscher schließt VREF kurz.
5. **Nennstrom des Motors aus dem Datenblatt nehmen.** Nicht aus dem
   Spulenwiderstand ableiten — das geht um Faktoren daneben. Der hier
   verwendete NEMA17 ist mit **0,4 A pro Phase** angegeben, obwohl seine
   3,6 Ω nach einem deutlich stärkeren Motor aussehen.

6. Zielstrom auf **70–80 % des Nennstroms** legen, also 0,28–0,32 A.
   Die Formel hängt am Treiber:

   | Treiber | Formel | für 0,28–0,32 A |
   |---|---|---|
   | **A4988**, Rsense 0,1 Ω | `VREF = I × 8 × Rsense` | **0,224–0,256 V** |
   | **TMC2209**, Rsense 0,11 Ω | `I_RMS ≈ VREF × 1,77` | **0,16–0,18 V** |

   > Sense-Widerstände unterscheiden sich zwischen Herstellern. Steht auf
   > deinem Modul ein anderer Wert, gilt der.

   **Zu viel Strom ist kein Sicherheitspolster, sondern der direkte Weg zum
   überhitzten Motor.**

   > **Wenn die Formel nicht greift.** Sie setzt einen bekannten
   > Sense-Widerstand voraus — bei Modulen mit abweichender Strombelastbarkeit
   > (etwa „2,5 A"-Ausführungen) steht der oft nirgends. Und Datenblätter
   > billiger Motoren widersprechen sich: Das hier verwendete nennt 0,4 A,
   > während 3,6 Ω pro Wicklung eher auf die 1-A-Klasse deuten.
   >
   > Dann gilt die **Temperatur** als Maßstab. Verlustleistung beider
   > Wicklungen ist `P = 2 × I² × R`; für einen 42 × 34 mm NEMA17 sind 3–5 W
   > üblich. Praktisch: fünf Minuten laufen lassen und anfassen. Handwarm bis
   > deutlich warm ist gut, ab etwa 80 °C nimmt der Rotormagnet dauerhaft
   > Schaden.
   >
   > Vorgehen: VREF **von oben herunter** so weit senken, bis die Pumpe gerade
   > noch zuverlässig anläuft, dann rund 20 % Reserve draufgeben.
7. 12 V wieder ausschalten.

**Vorgehen später:** In Phase 2 den Strom in 0,05-V-Schritten erhöhen, bis
die Pumpe unter Last keine Schritte mehr verliert. Danach nicht weiter
erhöhen. Der Motor darf handwarm bis deutlich warm werden (ca. 60 °C sind
für Schrittmotoren normal), aber nicht so heiß, dass man ihn nicht
kurz anfassen kann.

---

## 10. Motor anschließen

1. **12 V ausgeschaltet.** Prüfen, nicht annehmen.
2. Spulenpaare messen — **nicht aus der Farbfolge raten**. Am vorliegenden
   Motor ergab die Messung:
   * rot ↔ blau: **3,6 Ω** → Spule 1
   * grün ↔ schwarz: **3,6 Ω** → Spule 2
   * Adern verschiedener Spulen: **∞**

   Bei einem anderen Motor selbst messen. Ohne Messgerät: zwei Adern
   kurzschließen und die Welle drehen — wird sie schwergängig, ist es ein Paar.
3. Spule 1 (rot/blau) an KL2, Spule 2 (grün/schwarz) an KL3.
4. Von KL2/KL3 zu den TMC2209-Sockelpins:
   `KL2-1 → 1A`, `KL2-2 → 1B`, `KL3-1 → 2A`, `KL3-2 → 2B`.
5. Motorleitungen verdrillen (je Spulenpaar) — reduziert die Abstrahlung
   deutlich.

**Prüfen:** Durchgang zwischen KL2 und KL3 darf es nicht geben.

---

## 11. Abschließende Prüfliste vor dem ersten Volllauf

> **Zuerst, stromlos: `GND iso` gegen Sternpunkt auf Durchgang prüfen.**
> Es darf keiner bestehen. Diese Messung steht bewusst an erster Stelle —
> sie ist in einer Minute gemacht und erspart tagelange Fehlersuche an einer
> Messung, die scheinbar grundlos rauscht.

Alles abhaken, bevor 12 V dauerhaft anliegen:

- [ ] Sichtprüfung mit Lupe: keine Lötbrücken, keine kalten Lötstellen
- [ ] KL1 Plus ↔ Minus: kein Kurzschluss
- [ ] 3,3-V-Netz ↔ GND: kein Kurzschluss
- [ ] 5-V-Netz ↔ GND: kein Kurzschluss
- [ ] 12-V-Netz ↔ 5-V-Netz: kein Durchgang
- [ ] Alle GND am Sternpunkt, Durchgang zu jedem Modul
- [ ] C1 richtig gepolt, direkt am TMC2209
- [ ] R1 (EN-Pull-up) sitzt
- [ ] MS1 und MS2 auf VIO
- [ ] Sicherung 2 A träge in der 12-V-Zuleitung
- [ ] Buck-Ausgang auf 5,0 V eingestellt und nachgemessen
- [ ] D1 richtig gepolt (Ring zeigt zum ESP32)
- [ ] TMC2209 richtig herum gesteckt
- [ ] VREF passend zum **Datenblatt-Nennstrom** eingestellt (70–80 %)
- [ ] Motorstecker fest, Spulen korrekt zugeordnet
- [ ] Kühlkörper auf dem TMC2209
- [ ] Sondenkabel getrennt von den Motorleitungen verlegt
- [ ] Firmware geflasht
- [ ] Nichts an GPIO 2 oder 3 angeschlossen (Touch!)
- [ ] 5 V liegen auf `VBUS`, nicht auf `3V3`
- [ ] Buck-Converter für mindestens 1 A ausgelegt
- [ ] Spannung am Displayboard unter Last gemessen (> 4,7 V bei laufendem Motor)

**Einschaltreihenfolge:** immer erst USB/5 V (Logik), dann 12 V.
**Ausschaltreihenfolge:** erst 12 V, dann Logik.

---

## 12. Displayboard einbauen

![Fertig verdrahteter Aufbau](bilder/02-aufbau-verdrahtet.jpg)

*Endzustand: Leistungsseite links (12 V, Treiber, Motorkabel), Messkette
rechts oben. Das Displayboard ist im Gehäuse fixiert, die Motorleitung kreuzt
die Platine mit Abstand zum I²C-Bus.*

Am Board selbst wird nichts gelötet — Display und Touch sind integriert. Es
geht nur um Befestigung und die Anbindung über KL6.

1. **Ausschnitt im Gehäusedeckel** anfertigen: sichtbare Fläche 536 × 240 px
   auf 1,91 Zoll, also rund 43 × 19 mm. Etwas Rand einplanen, das Glas endet
   nicht bündig mit der Anzeige.
2. Board mit Abstandsbolzen M3 hinter dem Fenster befestigen. **Nicht** auf
   die Rückseite drücken, dort liegen Bauteile.
3. Verbindung zur Lochrasterplatine über KL6 stecken:
   `5 V` (hinter D1), `GND`, `3V3`, `STEP`, `DIR`, `EN`, `SDA`, `SCL`.
4. Die Leitung so verlegen, dass sie **nicht parallel zu den Motorleitungen**
   läuft — sie führt sowohl den I²C-Bus als auch die Schrittimpulse.
5. Den USB-C-Anschluss zugänglich lassen: er ist der Weg für Firmware und
   serielle Konsole, wenn WLAN oder Display einmal nicht mitspielen.

**Prüfen:** Mit eingeschaltetem Display und laufendem Motor die 5-V-Spannung
am Displayboard messen. Fällt sie unter 4,7 V, ist der Buck zu klein.

**Montageort:** Das AMOLED ist nicht für Dauerfeuchte gebaut. Im Technikraum
gehört es in ein Gehäuse mit Sichtfenster, nicht offen an die Wand.

---

## 13. Mechanik und Hydraulik

![Pumpenkopf von außen](bilder/03-pumpenkopf.jpg)

*Der Pumpenkopf von außen auf dem Gehäuse. Im Rotor sitzen drei Rollenlager,
in der Mitte das 608er Kugellager der verstärkten Wellenaufnahme. Unten die
beiden Schlauchenden — Saug- und Druckseite.*

* Peristaltikkopf auf die NEMA17-Welle: Wellendurchmesser 5 mm prüfen,
  Madenschraube auf die Abflachung setzen. Verwendet wird das 3D-Druckmodell
  [V2 Peristaltic Pump](https://makerworld.com/de/models/2225892-v2-peristaltic-pump-water-pump-measuring-pump)
  von MakerWorld; die STL liegt unter
  [../hardware/pumpe/](../hardware/pumpe/) im Repo. Die V2 hat die
  **verstärkte Wellenaufnahme** — dort liegt das volle Pumpenmoment an, und
  eine ausgeleierte Aufnahme fördert zu wenig, ohne dass es die Firmware
  merken kann. Begründung in [../hardware/README.md](../hardware/README.md).
* Die Pumpe **oberhalb** des Säurebehälters montieren.
* Saugseite: Schlauch mit Fußventil und Ansaugfilter im Kanister.
* Druckseite: Impfventil (Rückschlagventil) im Bypass **nach** dem Filter
  und **nach** der Wärmepumpe/Heizung, mit möglichst gutem Abstand
  zur pH-Sonde — sonst misst die Sonde die frische Säure statt des
  Poolwassers und die Regelung schwingt.
* Sonde selbst: in einer Messzelle im Bypass oder mit Sondenhalter im
  Rücklauf, immer **vor** dem Einspritzpunkt.
* Schlauch als Verschleißteil betrachten: Wechselintervall notieren
  (typisch 500–1000 Betriebsstunden) und regelmäßig auf Risse prüfen.

---

## 14. Wenn etwas nicht funktioniert

| Symptom | Wahrscheinliche Ursache |
|---|---|
| ADS1115 wird nicht gefunden | SDA/SCL vertauscht, GND fehlt, Pull-ups fehlen, ADDR offen |
| Messwert springt stark | Sondenkabel zu lang/zu nah an Motorleitungen, GND nicht sternförmig |
| Messwert driftet langsam | Sonde alt oder ausgetrocknet, Kalibrierung fällig |
| Motor brummt, dreht nicht | eine Spule falsch zugeordnet — Paare erneut durchmessen |
| Motor läuft rau, verliert Schritte | VREF zu niedrig oder Schrittrate zu hoch |
| TMC2209 sehr heiß | VREF zu hoch, Kühlkörper fehlt |
| ESP startet neu, wenn der Motor anläuft | C1 fehlt/zu klein, Buck zu schwach, GND-Schleife |
| 3200 Schritte ≠ 1 Umdrehung | MS1/MS2 nicht korrekt auf VIO |
| Firmware meldet dauerhaft „Sensorfehler" | pH-Board unversorgt, PO nicht angeschlossen, Spannung außerhalb 0,03–3,25 V |
| Display startet neu, wenn der Motor anläuft | Buck zu klein oder 5-V-Leitung zu dünn |
| Touch reagiert schlecht, seit der ADS1115 dran ist | ADS versehentlich auf dem Touchbus (GPIO2/3) statt auf GPIO13/14 |
| Motor läuft ruckelig, Menge stimmt aber | normal: die Bildausgabe unterbricht die Schrittausgabe kurz |
| Kein Bild, Konsole meldet „Display init failed" | Boardvariante oder Board-Einstellungen falsch |
| ADS1115 wird heiß oder raucht | Falsche Spannung an `VDD` (12 V statt 3,3 V) oder Versorgung verpolt — Chip ersetzen, Ursache vorher finden |
| `Spannung unplausibel`, ADC roh = 0 | `A0` liegt auf GND statt auf `PO`, oder das pH-Board hat keine Versorgung |
