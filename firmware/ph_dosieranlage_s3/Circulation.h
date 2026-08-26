// Circulation.h - Umwaelzung ueber eine Home-Assistant-Entitaet pruefen
//
// Bewusst KEIN zyklisches Polling: der Zustand wird erst abgefragt, wenn eine
// Dosierung unmittelbar bevorsteht - also nachdem alle anderen Bedingungen
// erfuellt sind. Fruehester Abstand zweier Abfragen ist damit die
// Durchmischungspause zwischen zwei Dosierungen (Standard 1800 s).
//
// Gefragt wird nach dem Ist-Zustand einer Entitaet (z.B. switch.poolpumpe)
// ueber GET /api/states/<entity>. Damit ist es egal, welches Geraet die Pumpe
// tatsaechlich schaltet, und Zeitplaene muessen nicht nachgebaut werden.
#pragma once

#include <Arduino.h>

enum CircState : uint8_t {
  CIRC_UNKNOWN = 0,   // noch nie erfolgreich abgefragt
  CIRC_ON,            // Pumpe laeuft
  CIRC_OFF,           // Pumpe steht
  CIRC_UNREACHABLE    // Home Assistant hat nicht oder unbrauchbar geantwortet
};

// Fragt Home Assistant ab, sofern noetig, und beantwortet: darf jetzt dosiert
// werden? Nutzt eine juengere Antwort aus dem Zwischenspeicher, statt bei
// jedem Aufruf erneut anzufragen.
bool circAllowsDosing();

CircState   circState();
const char *circStateText();
const char *circRawState();        // Zustandstext, wie HA ihn geliefert hat
uint32_t    circAgeS();            // Alter der letzten Antwort in Sekunden
uint32_t    circQueryCount();      // Zaehler, um das Abfrageaufkommen zu sehen

// Erzwingt bei der naechsten Pruefung eine frische Abfrage.
void circInvalidate();

// Einmalige Abfrage fuer Diagnose ("circ" in der Konsole / Webinterface).
bool circProbe(String &info);
