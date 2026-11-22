# Fork „Race into Space“ – Architekturanalyse (Etappe 0)

Stand: 21.11.2026, Basis: `raceintospace/raceintospace` Commit `1568aaa` (nach Release 2.0.0)
Ziel des Forks: frühere Startjahre, realistischere US-/UdSSR-Programme (Mercury bis ISS),
interplanetare Missionen, interaktive Missionsentscheidungen, epochengerechte Kontrollzentren
und historisches Filmmaterial. Zielplattform: macOS auf Apple Silicon (Mac mini).

Pfade beziehen sich auf `src/game/`, sofern nicht anders angegeben.

---

## 1. Ausgangslage

| Merkmal | Befund |
|---|---|
| Sprache / Umfang | C++, ca. 58.000 Zeilen, CMake + vcpkg |
| Build Linux | erfolgreich (System-Bibliotheken), `game_test` ohne Fehler |
| Build macOS | Presets `macos-vcpkg` / `macos-vcpkg-release`, benötigt Homebrew `sdl` + `sdl12-compat` – auf dem Mac mini noch zu prüfen |
| Grafik | SDL-1.2-API (auf macOS über `sdl12-compat` → SDL2), 320×200 Pixel, 256-Farben-Palette, Fenster 1280×800 per CPU-Skalierung |
| Video | Ogg/Theora, Missionsclips 160×100 bei 8 fps, Nachrichten 312×106 bei 15 fps; 537 Missionsclips, ca. 30 MB |
| Audio | Vorbis, separat zum Video gestartet, keine echte Synchronisation |
| Daten | großteils JSON: `mission.json`, `fails.json`, `seq.json`, `fseq.json`, `event.json`, `news.json`, `urast.json` (Startzustand), `hist.json` (historisches Hardwaremodell) |
| Spielstand | cereal-JSON, zlib-komprimiert; nur 3 Klassen versioniert (`Players` v1, `ReplayItem` v2, `INTERIMDATA` v1) |
| Tests | Boost.Test unter `test/`, keine Abdeckung von Grafik |

---

## 2. Harte Grenzen im Code

### 2.1 Zeitachse

- Jahr als `int8_t` „Jahre seit 1900“ (`data.h:864`), Obergrenze technisch 2027.
- Spielschleife `while (Data->Year < 78)` (`game_main.cpp:499`), Asserts `Year >= 57 && Year <= 78` (`data.h:903`, `legacy.h:406`).
- Kein zentraler Basiswert: `Year-57`, `Year-58`, `Year-53` an über 30 Stellen (Rundenzähler, Budgethistorie, Geheimdienst, Nachrichtenindex, Museum, Missionshistorie).
- Startjahr kommt aus `urast.json` (`"Year": 57`); `urast.json` wird beim Laden gegen `sizeof(Players)` geprüft (`game_main.cpp:234`), zusätzlich `BOOST_STATIC_ASSERT` auf Strukturgrößen (`data.h:1415ff`).
- Arrays mit Zeitbezug: `BudgetHistory[40]` (Index `Year-53`), `Events[100]`, `MAX_NEWS_ITEMS 84`, `eCount ≤ 42` (= exakt 21 Jahre), `PastIntel[30]`, `History[100]`.
- Bei Start 1955 entsteht in `budget.cpp:231` ein negativer Index.
- Startfenster Mars/Jupiter/Saturn als feste Jahreslisten (`mission_util.cpp:349-373`), keine Fenster nach 1977.
- Spielende: Mondlandung oder Herbst 1977 (`game_main.cpp:598, 731`).
- Jahresanzeige mit fest verdrahtetem „19“-Präfix an ca. 10 Stellen.

### 2.2 Hardware und Programme

- Je Kategorie ein festes Array `Equipment[7]` (`data.h:639-659`): Sonden 3 belegt, Raketen 5, Kapseln/LM 7, Zusatzgeräte 6.
- Identität = Arrayposition, abgeleitet aus `Equipment::ID[1]` (`'0'+index`). Dieser Index steckt in Spielständen, Missionsdaten, Bilddateinamen (`rdfull.but.(plr*32+hw*8+unit)`), Animationsschlüsseln (`Mev.Name` aus Schrittbuchstabe + ID → `seq.json`/`fseq.json`/`fails.json`) und Hilfetext-IDs.
- **Freie Plätze sind praktisch nicht nutzbar:**
  - Raketen: kodiert als `Index+1`, `+4` für Zusatzbooster (Werte 1–7) – Platz 5 und 6 kollidieren mit dieser Kodierung.
  - Sonden: Index 4 ist im Missionsslot für das Docking-Modul reserviert (`vab.cpp:908-958`).
- Programmlogik fest im Code: Crewgröße per `switch` (`hardware.cpp:34`), Missionseignung je Kapsel (`crew.cpp:731-1050`), Zuordnung Kapsel ↔ Raumhafengebäude (`port.cpp:426`), LM-Paarung (`vab.cpp:1549`), Technologietransfer je Platz handgeschrieben (`rdplex.cpp:1518-2098`, Duplikat in `aipur.cpp`).
- Genau 5 crewfähige Programme (`Crew[6][8][5]`, `Prog` 1–5 = Kapsel 0–4), Prestigekategorien 13–17 fest daran gekoppelt.
- KI (`aimis.cpp`, `aimast.cpp`, `aipur.cpp`) arbeitet mit festen Indizes und Programmcodes.

**Folge:** Zusätzliche reale Programme (Redstone, Jupiter-C, Vanguard, Atlas, Saturn IB, Skylab …) sind ohne Umbau des Hardwaremodells nicht einbaubar – auch nicht für Ausbaustufe 1.

### 2.3 Missionssystem

- 62 Missionstypen in `mission.json`; Schrittfolge als Buchstabencode (`Code`, z. B. Mars `ABZ~1eVW!`), Verzweigungen über `Alt[36]`/`AltD[36]`.
- `missSteps.json` ist nur die Flugbahnzeichnung im Planungsbildschirm (2-stellige IDs, also max. 99 Missionen).
- Zufallswürfe werden **bei der Missionsvorbereitung** erzeugt (`mc2.cpp:493-499`), nicht beim Schritt.
- Schrittschleife `MisCheck` (`mis_m.cpp:123-580`): Fehlschlag, wenn `dice > safety`; Fehlerzeile aus `fails.json` per `GetFailStat`; Folgen ausschließlich in `FailEval` (`mis_m.cpp:783-1312`, ca. 25 Fehlercodes).
- Auswahlliste der Missionen fest auf `56 + plr` begrenzt (`future.cpp:828, 855`); weitere feste Missionsnummern-Bereiche an ca. 10 Stellen.
- Planetenmissionen: Auflösung verzögert über Saisonzähler und einen einzigen Sicherheitswurf pro Saison (`start.cpp:740-845`), keine Orbiter/Lander.
- Prestige: 28 feste Kategorien (`MAXIMUM_PRESTIGE_NUM`), 8 Meilensteine, 2 freie Meilenstein-Plätze.
- PBEM-Spiele spielen Missionen aus gespeicherten `Mev`-Daten nach – Entscheidungen müssen dort mitgespeichert werden.

### 2.4 Grafik, Video, macOS

- Gesamte Oberfläche zeichnet in einen 320×200-8-Bit-Puffer mit einer globalen Palette; Überblendungen sind Palettenoperationen (155 Aufrufe von `FadeIn`/`FadeOut`, ca. 200 Stellen mit `legacyScreen()`).
- Kein Truecolor-Pfad, PNG-Lader akzeptiert nur Palettenbilder (`src/display/image.cpp:129-149`).
- Video als SDL-1.2-YUV-Overlay mit fester Größe 160×100 (`graphics.cpp:177`), Bildrate per Ganzzahldivision (`mmfile.cpp:250`, falsch bei 29,97 fps).
- Kontrollzentrum: ein Hintergrundbild je Seite (`control.img.0/1.png`), keine Epochen.
- macOS-Paket: SDL2 wird von Hand ins Bundle kopiert, kein `NSHighResolutionCapable`, Ad-hoc-Signatur.

---

## 3. Einstiegspunkte für die geplanten Features

| Feature | Einstiegspunkt | Bemerkung |
|---|---|---|
| Krisenentscheidung bei Fehlschlag | `mis_m.cpp` zwischen `GetFailStat` (Z. 401) und `FailEval` (Z. 411) | Fehlerzeile bekannt, Folgen noch nicht angewendet. Optionen: Abbruch über `Alt`-Zweig, Weiterflug mit Risiko, Ressourceneinsatz mit Neuwurf (Mechanik der Codes 9/19 existiert) |
| Warnung ohne Fehlschlag (1201-Alarm) | nach Z. 339, vor `if (PROBLEM)` | Knapper Wurf über der Sicherheit → GO/NO-GO |
| Entscheidungen bei Langzeitflügen | Dauerschleife `mis_m.cpp:200-231` | je Dauerstufe „verlängern oder zurückkehren“ – Grundlage für Stationen |
| Dialog-Vorlage | `DrawMoonSelection` (`mis_c.cpp:1086`), `FailureMode` (`mis_c.cpp:801`) | vorhandene Mehrfachauswahl während einer Mission |
| Startjahr wählbar | nach `NewGamePreferences()` (`game_main.cpp:265`), vor `InitData()` | erfordert parametrisierte Zeitachse |
| Epochen-Kontrollzentrum | `DrawControl` (`mc.cpp:93-102`), Videorechteck `mis_c.cpp:422-434` | Auswahl nach `Data->Year` / Programm |
| Historische Filme | Clip-Auswahl `PlaySequence` (`mis_c.cpp:129-495`), Dateisuche `fs.cpp:233` | HD-Paket mit gleichen Dateinamen, Rückfall auf Originalclips |
| Startfenster | `MissionTimingOk` (`mission_util.cpp:325-376`) | durch Tabelle oder Berechnung über synodische Periode ersetzen |

---

## 4. Gefundene Fehler im Originalcode

Kandidaten für Pull Requests an das Originalprojekt (behebt man ohnehin in Etappe 1):

- `Astros::serialize` liest `Race` nur bei `version > 0`, es gibt aber kein `CEREAL_CLASS_VERSION(Astros, …)` – das Feld wird nie gespeichert (`data.h:512`).
- `ARCHIVE_VECTOR`/`ARCHIVE_ARRAY` (`serialize.h`) lesen über das Vektorende hinaus, wenn die Datei weniger Elemente enthält.
- `endgame.cpp:630-634`: `yr` bleibt bei Jahr > 77 uninitialisiert.
- `rdplex.cpp:892`: Assert prüft den Wertebereich der Kapseln statt der aktuellen Kategorie.
- `rdplex.cpp:976`: Raketenindex wird mit `MANNED_HW_FOUR_MAN_CAPSULE` verglichen.
- `hardef.cpp:789`: liest `Misc[ROCKET_HW_BOOSTERS]`.
- `mmfile.cpp:250`: Bildrate per Ganzzahldivision.
- `news.cpp:309`: Nachrichtenindex ohne Bereichsprüfung.

⚠ Diese Liste stammt aus der Code-Lektüre; jeder Punkt wird vor einer Korrektur durch einen Test bestätigt.

---

## 5. Überarbeiteter Etappenplan

Die Analyse ändert die Reihenfolge gegenüber dem ersten Vorschlag: Der Hardware-Katalog muss **vor** den Mercury-Inhalten kommen, das Krisensystem kann dagegen früh und unabhängig von der Grafik umgesetzt werden.

Aufwand: S = bis 1 Tag, M = einige Tage, L = eine Woche und mehr (jeweils Agentenarbeit plus Ihre Tests auf dem Mac).

| Etappe | Inhalt | Aufwand | Spielstand-kompatibel |
|---|---|---|---|
| 1 | **Sicherheitsnetz:** Charakterisierungstests (Hardwaretypen, Raketenkodierung, VAB-Zusammenbau, Laden von `urast.json`/`hist.json`, Speichern/Laden); Kodierungen zentralisieren (`EncodeRocket`, Payload-Slot, Programm ↔ Kapsel); Fehler aus Abschnitt 4 beheben | M | ja |
| 2 | **Krisensystem** im bestehenden Grafikstil: neues Modul `mis_d.cpp`, `decisions.json`, Szenarien Apollo 13, Gemini 8, 1201-Alarm, MA-6-Hitzeschild; KI-Regel; Option „klassischer Modus“ | M | ja (Entscheidung in `StepInfo`-Codes) |
| 3 | **Zeitachse parametrisieren:** `StartYear`/`EndYear` im Spielstand (`Players` v2), alle Basiswerte ersetzen, Arrays vergrößern mit Versionsmigration, Jahresanzeige vierstellig, Startfenster berechnet, Spielende konfigurierbar (auch offenes Ende) | M–L | Migration |
| 4 | **Hardware-Katalog** `hardware.json`: zuerst beschreibend für die vorhandenen 21 Einträge, dann Regeln aus dem Code in Daten, dann dynamische Arrays und Programm-IDs in Missionen und Crews | L | Migration (ein Versionssprung) |
| 5 | **Ausbaustufe 1 – USA bis Ende Mercury:** Start 1955 wählbar; Redstone, Jupiter-C, Vanguard, Atlas; Explorer/Pioneer/Vanguard-Satelliten; alle geflogenen und nicht geflogenen Mercury-Missionen; Nachrichten und Ereignisse 1955–56; recherchierte Daten mit Quellen | M–L (inhaltslastig) | neues Spiel |
| 6 | **SDL2-Port:** Fenster/Renderer/Textur, Tastatur über `SDL_TEXTINPUT`, YUV-Texturen, Wegfall von `sdl12-compat` und `SDLMain.m`, Retina | M | ja |
| 7 | **Truecolor-Ebene und Kontrollzentren:** zusätzliche Bildebene unter der 320×200-Oberfläche, RGB-PNG-Lader, Kontrollzentrum je Epoche mit Bildschirm-Slots aus JSON (Mercury Control Center, MCC Houston) | L | ja |
| 8 | **Videopipeline:** dynamische Clipgröße, korrekte Bildraten, Zeitsteuerung über Zeitstempel, eingebetteter Ton, optionales HD-Paket `data/video-hd/`; Werkzeug zum Schneiden/Umwandeln historischer Aufnahmen | M | ja |
| 9 | **Interplanetare Missionen:** Vorbeiflug/Orbiter/Lander/Probenrückführung, Ankunftsschritte als echte Missionsschritte, neue Prestigekategorien | L | Migration |
| 10 | Gemini/Apollo vertiefen, Apollo Applications, Skylab, Raumstationen mit Versorgung | L | – |
| 11 | Sowjetische Seite gleichwertig | L | – |

Etappen 1–2 liefern früh eine spielbare Verbesserung, ohne das Datenmodell anzufassen.
Etappen 3–4 sind der eigentliche Umbau und Voraussetzung für alle Inhaltserweiterungen.
Etappen 6–8 (Grafik/Video) sind unabhängig von 3–5 und können parallel laufen.

---

## 6. Hauptrisiken

- **Animations- und Fehlerschlüssel** hängen an Hardware-IDs (`Mev.Name` → `seq.json`, `fseq.json`, `fails.json`). Jede neue Hardware braucht Rückfall-Zuordnungen auf vorhandene Clips.
- **Spielstände:** alte BARIS-Spielstände (`legacy.cpp`) und PBEM-Spiele müssen über die Versionsmigration erhalten bleiben – oder wir erklären sie im Fork bewusst für inkompatibel (Entscheidung offen).
- **Bildschirm-Wiederherstellung:** Pop-ups sichern den 320×200-Puffer; Truecolor-Inhalte darunter werden dabei nicht erfasst.
- **Videogröße:** HD-Material für alle Missionsschritte liegt im Bereich mehrerer hundert MB bis 1 GB – nicht ins Repository, sondern als lokales Paket.
- **Rechte am Filmmaterial:** NASA-Aufnahmen überwiegend gemeinfrei; ⚠ sowjetisches Material rechtlich unklar, nur private Nutzung.
- **KI:** neue Hardware und Missionen ignoriert die KI, bis sie rollenbasiert umgebaut ist (Etappe 4, Schritt KI).
- **Abstand zum Originalprojekt:** je tiefer der Umbau, desto schwieriger die Übernahme späterer Upstream-Änderungen. Etappe 1 und die Fehlerkorrekturen sollten daher als Pull Request ans Originalprojekt gehen.

---

## 7. Offene Entscheidungen

1. Reihenfolge: Krisensystem (Etappe 2) vor dem großen Umbau – einverstanden?
2. Kompatibilität alter Spielstände (BARIS-Originalformat, PBEM) erhalten oder aufgeben? Aufgeben spart in Etappen 3–4 spürbar Aufwand.
3. Fehlerkorrekturen aus Etappe 1 als Pull Request an das Originalprojekt anbieten?
4. Bei Spielende: Mondlandung weiterhin als Sieg, oder Punktewertung bis zum gewählten Endjahr?

---

## 8. Etappe 3 – Umsetzungsstand (Zeitachse)

- `Players` (Version 2) enthält `StartYear` und `EndYear` (Jahre seit 1900). Hilfen: `TurnIndex()`, `YearIndex()`, `BudgetIndex()`, `TotalTurns()`, `IsFirstTurn()`, `BeforeFirstIntel()`, `IsLastTurn()`.
- Standardwerte stehen in `data/gamedata/urast.json` (`"StartYear": 57`, `"EndYear": 63`) – **einzige Stelle für das Spielende**. Das Startjahr (1955/56/57) wird im Einstellungsbildschirm über den Knopf „START YEAR“ (Taste Y) gewählt; `MainLoop` setzt `Year = StartYear` vor `InitData()`.
- Arrays: `MAX_GAME_YEARS 48`, `MAX_GAME_TURNS 96`; `BudgetHistory[52]` (4 Leerplätze davor), `PastIntel[48]`, `Events[192]`, `MAX_NEWS_PER_PLAYER 96`, `MAX_MISSION_COUNT 300`, `Count`/`eCount` und `PrestType.Index` sind 16 Bit. Alte Spielstände werden nicht mehr geladen (Legacy-Loader meldet „falsches Dateiformat“).
- Ereigniskarten: `FillEventCards()` (`timeline.cpp`) behält die sechs Perioden des Originals (2, 8, 8, 12, 16, Rest) ab Spielbeginn; die letzte Periode wird auf die benötigte Kartenzahl gestreckt bzw. abgeschnitten. Sind alle erlaubten Pools erschöpft, werden sie neu gemischt.
- Spielende: nach dem Herbst von `EndYear` ohne Mondlandung gewinnt das Land mit mehr Prestigepunkten (Summe `Prestige[i].Points`), Siegerfluss `NewEnd(win, 0, false)` ohne HISTORY/MOON EVA; Gleichstand -> `SpecialEnd()`.
- Startfenster: `MarsWindows`/`JupiterWindows`/`SaturnWindows` (synodische Periode, Epoche, Fensterlänge). Bis einschließlich 1963 identisch mit der alten Tabelle, danach Abweichungen (siehe `test/game/timeline_test.cpp`).
- Nachrichtentexte: `news.json` beginnt 1957; frühere Jahre nutzen den 1957-Block (`NewsYearIndex`).

