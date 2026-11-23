# Vorschau Szenario „Historisch“ (nur USA, 1955–1963) – Spielwerte

Stand: 23.11.2026 · Status: **Vorschlag zur Freigabe** (nichts davon wird vom Spiel geladen)
Datenbasis: `recherche-mercury.md` (Tags [P]/[W]/[M], ⚠ = unsicher), Katalogmodell `hardware-katalog.md`,
Startwerte-Format aus `data/gamedata/scenarios/classic.json`.
Maschinenlesbarer Entwurf: `data/gamedata/drafts/historical_us_preview.json`
(Katalogeinträge im Schema von `hardware.json` + Szenarioblock im Schema von `classic.json`).

Kennzeichnung in diesem Dokument: **⚠ Schätzung** = von mir geschätzt, keine Quelle. Neue Fakten dieser Runde
stammen aus englischsprachigen Wikipedia-Seiten (als [W] markiert, Seitenname genannt; Wikipedia zitiert NASA/NSSDCA)
und aus der Zählung der Wikipedia-Startlisten (eigene Zählung, ⚠).

---

## 1. Ergebnis in Kürze

| Kategorie | Historisches Szenario (USA) | Anzahl |
|---|---|---|
| Sonden/Satelliten | Explorer, Vanguard (Satellit), Pioneer, Ranger, Mariner | 5 (klassisch 3) |
| Raketen | Redstone, Juno I (Jupiter-C), Vanguard, Atlas D, Atlas-Agena | 5 (klassisch 5) |
| Kapseln | Mercury | 1 (klassisch 7) |
| Zusatzgeräte | nur `PHOTO_RECON` als technischer Platzhalter (siehe 7.) | 1 (klassisch 6) |
| Optional | Thor-Able, Juno II (zusätzliche Raketen, ohne neue Mechanik einsetzbar) | +2 |

Die UdSSR bleibt klassisch (`SV.*` unverändert aus `classic.json`).
Start 1955, Ende 1963 (`start_year` 55, `end_year` 63).

---

## 2. Skalierungsregeln

### 2.1 Gewicht und Nutzlast (abstrakte Einheiten)

**Regel:** 1 Spieleinheit ≈ 2,7 kg (Anker: Mercury-Kapsel im Orbit ≈ 1.350 kg = 500 Einheiten = Nutzlast Mercury-Atlas ≈ 1.360 kg ⚠ [W Atlas_LV-3B, zitiert Astronautix]).
`unit_weight` = Masse des Objekts im Flug (Mercury: ohne Fluchtturm, der nach ca. 2 min abgeworfen wird; Raumschiffmasse beim Start mit Turm 1.832–1.964 kg [P SP-4012 Tab. 2-30…2-35]).
`max_payload` einer Rakete = Masse, die sie auf ihre Bahn bringt (Orbit; bei Redstone: ballistischer Scheitel).

Prüfung, ob die lineare Skala für die kleinen Satelliten taugt:

| Gegenstand | real | Einheiten | Bemerkung |
|---|---|---|---|
| Explorer 1 | 13,97 kg [W Explorer_1] | 5 | Spiel: 6 (16 kg-Klasse, deckt Explorer 1/3) |
| Vanguard 1 / 2 / 3 | 1,47 / 9,8 / 22,7 kg [W Vanguard_(rocket)] | 0,5 / 3,6 / 8,4 | Spiel: 4 (Auslegung 11,3 kg [P SP-4012]) |
| Juno I Nutzlast | 18 kg [P SP-4012 Tab. 1-51]; 11 kg [W] ⚠ | 7 | Spiel: 7 |
| Pioneer 4 / 0 / 5 | 6,08 / 38 (83,8 lb) / 43,2 kg [W Pioneer_4, Pioneer_0, Pioneer_5] | 2 / 14 / 16 | Spiel: 16 |
| Mariner 2 | 203,6 kg [W Mariner_2] | 75 | Spiel: 75 |
| Ranger 1–2 / 3–5 | 306,2 / 330 kg [W Ranger_1, Ranger_3] | 113 / 122 | Spiel: 122 |
| Atlas-Agena B LEO | 2.627 kg [P SP-4012] | 973 | Spiel: 970 |

Ergebnis: Die lineare Skala funktioniert. Ganzzahlen genügen für alles ab ca. 4 kg; Gegenstände unter ca. 3 kg (Vanguard 1) lassen sich
nicht abbilden und werden durch die Auslegungsklasse ersetzt. Eine nichtlineare Abbildung wäre nicht möglich, weil die VAB-Regel
(`Vehicle::thrust() >= Summe der Gewichte`) additiv ist.

Prüfung gegen die VAB-Regel (Rakete `max_payload` ≥ Nutzlastgewicht; Kicker entfallen), Ergebnis aus dem Entwurfsskript:

| Rakete | max_payload (≈ kg) | trägt (nach Gewicht) | Einschränkung durch `reach` |
|---|---|---|---|
| Redstone | 500 (1.350) | alles | nur suborbitale Missionen (Mercury-Kapsel 500 passt exakt) |
| Juno I | 7 (19) | Explorer (6), Vanguard-Satellit (4) | – |
| Vanguard | 4 (11) | nur Vanguard-Satellit (4) | – |
| Atlas D | 500 (1.350) | Mercury (500, exakt), Explorer, Vanguard-Sat., Pioneer, Ranger, Mariner | orbital: nur Orbitalmissionen (kein Mond/Planet) |
| Atlas-Agena | 970 (2.619) | alles | translunar: Mond/Venus |
| (opt.) Thor-Able | 45 (122) | Explorer, Vanguard-Sat., Pioneer (16) | translunar |
| (opt.) Juno II | 17 (46) | Explorer, Vanguard-Sat., Pioneer (16) | orbital (Pioneer 3/4 hatten nur ca. 6 kg Mondbahnnutzlast, daher bewusst nicht translunar) |

Hinweis: Der klassische Code leitet „Mond/Planet möglich“ derzeit aus dem Raketenindex ab (`Vehicle::translunar()`: nur die Einstufen-Rakete
Atlas ist ausgeschlossen). Das Feld `reach` bei Raketen (hier `suborbital`/`orbital`/`translunar`) ist im Katalog schon vorgesehen
(`hardware-katalog.md`, Abschnitt 3.1), wird aber für Raketen noch nicht ausgewertet (offene Frage 1).

### 2.2 Kosten (MB, Spielgeld)

Es gibt keine belastbaren Einzelkosten für die meisten Posten (`recherche-mercury.md`, Abschnitt 5: „Einzelstückkosten: nicht ermittelt“).
Deshalb **relative Skalierung mit Ankern**:

| Anker | Historisch | Klassisch | Folge |
|---|---|---|---|
| Entwicklung (`init_cost`) | Mercury-Entwicklung McDonnell 40,2 Mio. USD [P SP-4201 App. F] | Mercury 20 MB | **1 MB ≈ 2 Mio. USD** (Entwicklung) |
| Stückkosten Kapsel/Sonde (`unit_cost`) | Mercury ca. 2,47 Mio. USD je Kapsel (49,354 Mio./20, abgeleitet) [P SP-4201 App. F] | Mercury 3 MB | **1 MB ≈ 0,85 Mio. USD** |
| Stückkosten Rakete | Atlas D ca. 5 Mio. USD ⚠ Schätzung (Atlas-Beschaffung 82,8 Mio. USD [P SP-4201 App. F], Stückzahl nicht geprüft); Juno II 2,9 Mio. USD (1958) [W Juno_II]; Redstone ca. 1,4 Mio. USD ⚠ (Mercury-Budget FY59/60: 6,49 + 4,48 Mio. für 8 Redstone [P SP-4012]) | Atlas 3 MB | **Atlas D = 4 MB ⇒ 1 MB ≈ 1,25 Mio. USD** (Raketen) |
| R&D-Kosten (`rd_cost`) | keine Quelle | klassisch 1–4 MB je Würfel | ⚠ Schätzung nach Klasse (Rakete 1–3, Sonde 1–3, Kapsel 1) |

Die Anker unterscheiden sich, weil das klassische Spiel Raketen im Verhältnis zu Kapseln billiger macht, als sie waren. Ich halte die
Spielmagnituden (Atlas 3–4 MB, Kapsel 3 MB) und skaliere innerhalb jeder Klasse nach historischen Verhältnissen. Alle Stückkosten außer
Mercury und Juno II sind damit ⚠ Schätzung.

### 2.3 Zuverlässigkeit

Spielmechanik (aus dem Code geprüft): `safety` = Startwert, jeder R&D-Würfel (Kosten `rd_cost` MB) addiert einen W6 (Mittel 3,5) bis `max_rd`;
`max_safety` ist die Obergrenze, die im Spielverlauf erreichbar wird. Klassische Raketen starten bei 10, Kapseln bei 5, Sonden bei 40.

Ableitung aus Startbilanzen, komprimiert in den spielbaren Bereich:

- Startwert `safety = round(5 + 25 · p_erst)` mit `p_erst` = Erfolgsquote der ersten (≤ 5) Starts → Bereich 5–30 (klassisch 5–10, daher leicht höher für bewährte Typen).
- `max_rd = round(76 + 14 · p_spät)` mit `p_spät` = Erfolgsquote der letzten (≤ 6) Starts → Bereich ca. 80–90 (klassisch 84–95).
- `max_safety = max_rd + 8` (höchstens 98).
- Sonden/Kapseln: Startwert und Grenzen nach Raumschiffbilanz, nicht nach Raketenbilanz (Raketenausfälle zählen bei der Rakete). Klassische Werte als Vergleichsanker.

| Rakete | Bilanz (Quelle) | p_erst | p_spät | Start | max_rd | max_safety |
|---|---|---|---|---|---|---|
| Redstone | Mercury-Redstone 5/6 [P SP-4201 App. D]; Redstone-R&D-Bilanz nicht ermittelt | 0,8 ⚠ Schätzung | 0,83 | 25 | 88 | 96 |
| Juno I | 3/6 [W Juno_I] (Explorer 1, 3, 4 Erfolg; 2, 5, Beacon Fehlschlag) | 0,5 | 0,5 | 18 | 83 | 91 |
| Vanguard | 3/11 [W Vanguard_(rocket)]; erste 5 Starts 1/5, letzte 6 2/6 (eigene Zählung der Startliste) | 0,2 | 0,33 | 10 | 81 | 89 |
| Atlas D | Atlas D 1959: 8 Erfolge in 13 Starts, 1961–62: 21/23 (eigene Zählung der Wikipedia-Startlisten, ⚠); Mercury-Atlas 7/9 [P SP-4201 App. D]; Gesamt Atlas D 103/135 [W SM-65D_Atlas] | 0,62 | 0,91 | 20 | 89 | 97 |
| Atlas-Agena | 21/28 (1961–65) [W Atlas-Agena]; Früh-Bilanz (Ranger 1–3, Mariner 1) schwach, Spätbilanz ⚠ Schätzung 0,85 | 0,43 ⚠ | 0,85 ⚠ | 16 | 88 | 96 |
| (opt.) Thor-Able | 16 Starts: 9 suborbital (7 Erfolge), 7 Orbit/Sonde (3 Erfolge) [W Thor-Able] | 0,43 | 0,5 ⚠ | 16 | 83 | 91 |
| (opt.) Juno II | 10 Starts, 3–4 Erfolge [P SP-4012; W Juno_II] | 0,4 | 0,45 | 15 | 82 | 90 |

Sonden und Kapsel (Spielwerte, Begründung):

| Posten | Start | max_rd | max_safety | Begründung |
|---|---|---|---|---|
| Explorer | 40 | 95 | 98 | wie klassisch Sputnik/Explorer: die Satelliten selbst funktionierten, die Ausfälle 1958 lagen bei der Rakete ([W] Explorer 2 und 5) |
| Vanguard-Satellit | 45 | 95 | 98 | alle drei Satelliten (Vanguard 1–3) arbeiteten, Kugelform einfach |
| Pioneer | 25 | 85 | 90 | keine volle Mondvorbeiflug-Mission 1958–60 (Pioneer 4 verfehlte Zielabstand, [W Pioneer_4]); Fehler überwiegend Träger, aber auch 3. Stufen/Elektronik |
| Ranger | 25 | 82 | 88 | Ranger 1–5 ohne vollen Erfolg ([W] Ranger_3; Block-III-Erfolge erst 1964, außerhalb des Zeitraums) |
| Mariner | 35 | 88 | 92 | Mariner 2 erster erfolgreicher Planetenvorbeiflug 14.12.1962 [W Mariner_2]; Mariner 1 ging durch Trägerfehler verloren |
| Mercury | 5 | 80 | 94 | sechs bemannte Flüge ohne Verlust (Raumschiff; Liberty Bell 7 Kapsel verloren); klassisch Mercury 5/78/92, Wostok 5/76/92 (`historical`-Satz) |

### 2.4 Missionsdauer (Mercury)

Reale Grenzen: MA-9 34 h 19 min 49 s [P SP-4201 App. D]; MA-10 (gestrichen) geplant als Dreitagesflug
(Ende 1962 noch als Eintagesflug geplant, Anfang 1963 tentativ ca. 3 Tage [W Mercury-Atlas_10; P SP-4201 S. 502]).
Vorschlag: `duration_days` 3 (klassisch 5), `duration_level` **B**, `max_plan_days` 2, `max_future_duration` 2 (Werte wie klassisch).
⚠ Unklar: die Zuordnung Stufe A–F ↔ Tage steht nirgends in den Daten. Mein Vorschlag liest A als „bis ca. 1,5 Tage“ (MA-9),
B als „bis ca. 3 Tage“ (MA-10-Plan). Da die klassische Mercury-Planungsgrenze schon bei Stufe B liegt, ändert sich die Mechanik nicht;
die Grenze wird nur als „real“ gelesen (offene Frage 4).

### 2.5 Verfügbarkeit (`available_from`) und Voraussetzungen (`requires`)

Format `{"year": JJ, "season": 0|1}` (0 = erste Jahreshälfte ⚠ Annahme). Diese Felder werden vom Loader noch nicht gelesen.
`requires` = Programm muss gestartet sein (Annahme, offene Frage 5).

| Posten | ab | obsolet ab | requires | Begründung |
|---|---|---|---|---|
| Redstone | 55/0 | – | – | Armee-Rakete, Erstflug 20.08.1953 [P SP-4012] |
| Vanguard (Rakete, Satellit) | 55/1 | 60/0 | – | Programm 1955 [M]; letzter Start 18.09.1959, Programmende 1959 [W] |
| Juno I | 57/1 | 60/0 | Redstone | Jupiter-C Erstflug 20.09.1956 [W]; Satellitenauftrag an die Armee nach Sputnik (Nov. 1957 [M]); letzter Start 23.10.1959 [W] |
| Explorer | 57/1 | – | – | Explorer 1 am 01.02.1958 UTC [W] |
| Pioneer | 58/0 | – | – | Pioneer 0 am 17.08.1958 [W] |
| Atlas D | 58/1 | – | – | SCORE (Atlas B) 18.12.1958, Atlas D erster Flug April 1959 [W SM-65D_Atlas] |
| Mercury | 58/1 | – | – | McDonnell-Vertrag 06.02.1959 [P SP-4012] |
| Ranger | 59/1 | – | – | Programmbeginn 1959 [M] ⚠; Erstflug 23.08.1961 [W] |
| Atlas-Agena | 60/0 | – | Atlas D | Atlas-Agena A 26.02.1960, Agena B 12.07.1961 [W] |
| Mariner | 61/0 | – | Ranger | Mariner R aus Ranger-Plattform abgeleitet ([W] Mariner_2 ⚠), Erstflug 22.07.1962 |
| (opt.) Thor-Able | 58/0 | 61/0 | – | RTV-Test 23.04.1958 [P SP-4012]; letzter Flug 01.04.1960 [W] |
| (opt.) Juno II | 58/1 | 62/0 | Juno I | Erstflug 06.12.1958 [W]; letzter Start 25.05.1961 [W] |

---

## 3. Posten im Detail (reale Fakten und Spielwerte)

Spielwerte: S = Start-Sicherheit, I = `init_cost`, U = `unit_cost`, G = `unit_weight`/`max_pay`, R = `rd_cost`, MaxRD, MaxS = `max_safety`.

### 3.1 Raketen

| Posten | Reale Fakten (Quelle) | S | I | U | G/max_pay | R | MaxRD | MaxS | reach |
|---|---|---|---|---|---|---|---|---|---|
| **Redstone** (auch Mercury-Redstone) | Nutzlast 1.814 kg ballistisch bis 189 km, Startmasse 29.931 kg [P SP-4012 Tab. 1-59]; 6 Mercury-Starts, 5 Erfolge [P SP-4201 App. D]; 25 Cape-Starts 1955–58 [W] | 25 | 12 | 1 | 500 | 1 | 88 | 96 | suborbital |
| **Juno I** (Jupiter-C) | 18 kg LEO [P SP-4012] (11 kg [W Juno_I]); 6 Starts, 3 Erfolge [W]; Jupiter-C 3/3 suborbital [W Jupiter-C] | 18 | 16 | 2 | 7 | 2 | 83 | 91 | orbital |
| **Vanguard** | 11,3 kg [P SP-4012]; 11 Starts, 3 Erfolge [W Vanguard_(rocket)] (⚠ je nach Zählung 12 Starts) | 10 | 22 | 2 | 4 | 2 | 81 | 89 | orbital |
| **Atlas D** (Mercury-Atlas) | ca. 1.360 kg LEO ⚠ [W Atlas_LV-3B]; Mercury-Atlas 9 Starts, 7 Erfolge [P SP-4201 App. D]; Atlas D insgesamt 135 Starts, 103 Erfolge [W SM-65D_Atlas] | 20 | 24 | 4 | 500 | 2 | 89 | 97 | orbital |
| **Atlas-Agena (B)** | 2.627 kg zu 555 km, 340 kg Fluchtbahn, 204 kg Mars/Venus [P SP-4012]; 28 Starts, 21 Erfolge (1961–65) [W] | 16 | 46 | 8 | 970 | 3 | 88 | 96 | translunar |
| (opt.) **Thor-Able** | 122 kg zu 850 km [P SP-4012]; 16 Starts, 10 Erfolge [W Thor-Able] | 16 | 22 | 3 | 45 | 2 | 83 | 91 | translunar |
| (opt.) **Juno II** | 41 kg [W] / 45 kg [P SP-4012]; 10 Starts, 3–4 Erfolge; 2,9 Mio. USD/Start 1958 [W Juno_II] | 15 | 14 | 3 | 17 | 2 | 82 | 90 | orbital |

Atlas-Agena ist nicht in der Wunschliste, aber **notwendig**: Ranger und Mariner flogen auf Atlas-Agena B, und die einstufige Atlas D
kann im Spielcode nicht translunar sein. Sie ist als eigene Rakete modelliert (nicht als Kicker), weil Atlas D als Mercury-Atlas ohne Agena flog.

### 3.2 Sonden und Satelliten

| Posten | Rolle / Typ / Ziel | Reale Fakten (Quelle) | G | S | I | U | R | MaxRD | MaxS | reach |
|---|---|---|---|---|---|---|---|---|---|---|
| **Explorer** | `orbital_probe`, satellite, Erde, research | Explorer 1: 13,97 kg, Erfolg 01.02.1958 [W Explorer_1]; Explorer 1, 3, 4 Erfolg, 2 und 5 Fehlschlag [W] | 6 | 40 | 6 | 1 | 1 | 95 | 98 | orbital |
| **Vanguard-Satellit** | `orbital_probe`, satellite, Erde, research | Vanguard 1/2/3: 1,47/9,8/22,7 kg [W Vanguard_(rocket)]; Auslegung 11,3 kg [P SP-4012] | 4 | 45 | 4 | 1 | 1 | 95 | 98 | orbital |
| **Pioneer** | `interplanetary_probe`, flyby, Mond | Pioneer 0/1/2 ca. 38 kg; Pioneer 4 6,08 kg; Pioneer 5 43,2 kg [W]; 9 Mondversuche 1958–60 ohne vollen Erfolg; Atlas-Able 0/4 [P SP-4012] | 16 | 25 | 20 | 3 | 2 | 85 | 90 | translunar |
| **Ranger** | `lunar_probe`, impact, Mond | 306–330 kg [W]; Block II mit Hartlander-Kapsel (Seismometer) [W Ranger_3]; Ranger 1–5 ohne vollen Erfolg [W] | 122 | 25 | 28 | 5 | 3 | 82 | 88 | translunar |
| **Mariner** | `interplanetary_probe`, flyby, Venus | Mariner 2: 203,6 kg, Vorbeiflug Venus 14.12.1962 [W Mariner_2]; Mariner 1 Fehlschlag 22.07.1962 [W] | 75 | 35 | 26 | 5 | 3 | 88 | 92 | interplanetary |

Rollenzuordnung (nach Spielmechanik): Mondvorbeiflug und Venusvorbeiflug nutzen klassisch beide die Rolle `interplanetary_probe`
(klassisch Ranger/Kosmos), Mondlandung (`LUNAR PROBE LANDING`) die Rolle `lunar_probe`. Deshalb Pioneer und Mariner als
`interplanetary_probe`, unterschieden über `targets` (`moon` bzw. `venus`), Ranger als `lunar_probe` (Hartlandung ≈ „Landung“ der Spielmission).
Pioneer als `lunar_probe` zu führen wäre die Alternative, kollidiert aber mit Ranger (offene Frage 2).

### 3.3 Kapsel

| Posten | Reale Fakten (Quelle) | G | S | I | U | R | MaxRD | MaxS | Sonstiges |
|---|---|---|---|---|---|---|---|---|---|
| **Mercury** | 1 Mann; 2,1 m breit, 1 m³ Kabine [P SP-4012]; McDonnell: Entwicklung 40,2 Mio., 20 Stück 49,35 Mio. USD [P SP-4201 App. F]; MA-9 34 h 19 min [P SP-4201 App. D]; MA-10 geplant ca. 3 Tage | 500 | 5 | 20 | 3 | 1 | 80 | 94 | `crew` 1, `abort_system` `escape_tower`, `duration_days` 3, `duration_level` B, `reach` orbital, `prestige_first` OnePerson |

Kosten bei Mercury entsprechen dem klassischen Wert (20/3) und sind zugleich der historische Anker (siehe 2.2).

### 3.4 Zusatzgeräte

Siehe Abschnitt 7: Kicker, EVA-Anzüge und Docking-Modul entfallen; `PHOTO_RECON` bleibt als Platzhalter.

---

## 4. Balance gegen die klassische UdSSR

Maß: **C70** = `init_cost` + (70 − Start)/3,5 · `rd_cost` (Kosten, um ein Programm per R&D auf 70 % zu bringen; der W6 liefert im Mittel 3,5 Punkte).
Summe der Posten, die für den Meilenstein nötig sind (Startkosten von Voraussetzungen zählen nur mit `init_cost`).

| Meilenstein | UdSSR klassisch (`basic`) | USA Vorschlag | Abweichung |
|---|---|---|---|
| Erster Orbitalsatellit | Sputnik + R-7: 72,9 | Explorer + Juno I (+ Redstone-Start): 72,3; Vanguard-Route 67,4 | −1 % / −8 % |
| Bemannter Orbitalflug | Wostok + R-7: 94,9 | Mercury + Atlas D: 91,1 | −4 % |
| Bemannter Suborbitalflug | (keiner billiger als 94,9) | Mercury + Redstone: 63,4 | US-Sonderweg, niedrigeres Prestige |
| Mondvorbeiflug | Kosmos + Proton: 178,3 | Pioneer + Atlas-Agena (+ Atlas-D-Start): 162,0 | −9 % |
| Mondlandung Sonde | Luna + Proton: 192,9 | Ranger + Atlas-Agena (+ Atlas-D-Start): 182,9 | −5 % |
| Venusvorbeiflug | Kosmos + Proton: 178,3 | Mariner + Atlas-Agena (+ Ranger-, Atlas-D-Start): 200,3 | +12 % |

(Im Satz `historical` sind die UdSSR-Werte um ca. 4 niedriger; Ergebnis analog.)
Alle Abweichungen liegen innerhalb von ±15 %, die USA sind bei den Frühmeilensteinen leicht günstiger (Armee-Hardware vorhanden) und
beim Venusvorbeiflug teurer (zwei Programme in Folge). Die Stückkosten der ersten Missionen liegen bei 3–13 MB gegenüber 4–18 MB klassisch.

Zuverlässigkeitsverlauf: US-Raketen starten bei 10–25 (klassisch 10) und enden bei MaxRD 81–89 (klassisch R-7 84/85, Titan 90); US-Sonden starten
bei 25–45 (klassisch 40), Mercury bei 5 mit MaxRD 80 (klassisch 78). Ausnahmen sind gewollt: Vanguard schlechter, Redstone besser als klassisch.

---

## 5. Darstellung: `anim_id`, Bilder, Typtext

Der Katalog erlaubt nur `anim_id` aus Kategoriebuchstabe + Ziffer; Mehrfachverwendung ist zulässig (Testeinträge teilen sich `P0`/`R0`).
Alle Bilder und Clips existieren bereits (US-Sätze `rdfull.but.N`, `liftoff.abz.US*`, Clips unter `P*`, `R*`, `C*`); keine neuen Grafiken.

| Posten | `anim_id` | Forschungsbild (rdfull.but) | Startbild | VAB-Sprite | help_id | `type_label` |
|---|---|---|---|---|---|---|
| Redstone, Juno I, Vanguard, Atlas D, Thor-Able, Juno II | R0 (Atlas-Clips: schlanke Einstufen-Rakete) | 8 | `liftoff.abz.USR0` | 0 | i207 | ROCKET |
| Atlas-Agena | R1 (Titan-Clips: zweistufig) | 9 | `liftoff.abz.USR1` | 1 | i209 | ROCKET |
| Explorer, Vanguard-Satellit | P0 | 0 | `liftoff.abz.USP0` | 9 | i201 | SATELLITE |
| Pioneer, Mariner | P1 (klassisch Ranger/Kosmos: Vorbeiflug) | 1 | `liftoff.abz.USP1` | 10 | i203 | PROBE |
| Ranger | P2 (klassisch Surveyor/Luna: Mondsonde) | 2 | `liftoff.abz.USP2` | 11 | i205 | PROBE |
| Mercury | C0 | 16 | `liftoff.abz.USC0` | 13 | i217 | CAPSULE |

Typtext: bestehende Werte (ROCKET, SATELLITE, PROBE, CAPSULE); die Namen sind kurz gehalten (`JUNO I`, `ATLAS D`, `ATLAS-AGENA`).
Eigene Texte wie „SUBORBITAL“ für Redstone wären möglich, die UI-Breite ist ungeprüft.
Die Hilfetexte (`help_id`) beschreiben dann die Klassik-Hardware; eigene Texte sind später zu ergänzen.

---

## 6. Wichtige Verhaltensfolgen (kurz)

- Mercury-Atlas: Atlas D trägt Mercury exakt (500/500). Zusatzgewicht ist nicht möglich: gewollt, weil es keine Kicker gibt.
- Redstone ist rein suborbital (`reach`), trotz `max_payload` 500: ohne Auswertung von `reach` könnte sie auch Orbitalmissionen (offene Frage 1).
- Vanguard-Rakete trägt nur den Vanguard-Satelliten, nicht Explorer.
- Die Sonden sind viel leichter als klassisch (6–122 statt 300–1.200): die Gewichtsprogression Atlas → Titan → Saturn entfällt, die Progression kommt aus `reach`, `available_from` und Kosten.

---

## 7. Klassische US-Posten: behalten oder entfernen

| Klassisch | Vorschlag | Begründung |
|---|---|---|
| EXPLORER | behalten (neu belegt) | Rolle `orbital_probe`, Werte neu |
| RANGER (Rolle `interplanetary_probe`) | wird **Pioneer/Mariner**; der Name Ranger geht an die Mondsonde | historisch: Ranger = Mondeinschlag |
| SURVEYOR | entfernen | erster Flug 1966, außerhalb |
| ATLAS | ersetzt durch Atlas D | |
| TITAN, SATURN, NOVA, BOOSTERS | entfernen | Titan II erst 1962 (Gemini), Saturn ab 1961 nur Entwicklung, Nova nie gebaut |
| MERCURY | behalten, Werte angepasst | |
| GEMINI, APOLLO, XMS2, JUPITER, EAGLE, CRICKET | entfernen | nach 1963 bzw. fiktiv (Entscheidung 2 in `hardware-katalog.md`, Abschnitt 8) |
| KICKER_A/B/C | entfernen | historisch Agena/Centaur, hier als Atlas-Agena-Rakete abgebildet; Kicker C (Probenkicker) fiktiv |
| EVA_SUITS | entfernen | erster US-Weltraumausstieg Juni 1965 (Gemini 4) ⚠ [M] |
| DOCKING | entfernen | erstes Docking 1966 (Gemini 8) ⚠ [M] |
| PHOTO_RECON | **behalten**, nicht erforschbar, Sicherheit 55 | technischer Platzhalter: `MISC_HW_PHOTO_RECON` ist im Code noch fest verdrahtet (mc2.cpp, mis_c.cpp, ast0.cpp). Spielerisch ohne Wirkung, da es nur für Mondlandungen mit Fotoaufklärung genutzt wird. |

Ergebnis: 5 Sonden, 5 Raketen, 1 Kapsel, 1 Zusatzgerät statt 3/5/7/6. Wegen der noch positionsabhängigen Stellen (Abschnitt „Noch positionsabhängig“ in
`hardware-katalog.md`: Missionen, VAB, `crew.cpp`, KI, Prestige-Tabellen) wird das Szenario erst nach 4d–4f lauffähig; bis dahin ist die Datei reiner Entwurf.

---

## 8. Offene Fragen an den Eigentümer

1. **Reichweite von Raketen:** Soll `reach` bei Raketen (`suborbital`/`orbital`/`translunar`) ausgewertet werden? Der Code kennt nur `Vehicle::translunar()` per Index (nur Atlas einstufig ausgeschlossen). Ohne Auswertung könnte Redstone Orbitalmissionen fliegen und Atlas D Mondmissionen.
2. **Rolle von Pioneer:** `interplanetary_probe` mit `targets: [moon]` (Vorschlag) oder `lunar_probe`? Zwei Programme gleicher Rolle setzen voraus, dass die Missionsauswahl nach `targets` filtert (Mondvorbeiflug nur Pioneer, Venus nur Mariner).
3. **Atlas-Agena als eigene Rakete** (Vorschlag) oder Kicker mit Gewicht? Kicker würde die klassischen Positionsstellen der Kicker-Regeln weiter brauchen.
4. **Dauerstufen A–F:** Welche Tage entsprechen den Stufen? Vorschlag: A ≤ ca. 1,5 Tage, B ≤ ca. 3 Tage; `duration` im Startwert 3 statt 5.
5. **`requires`:** bedeutet „Programm gestartet“ (Vorschlag) oder „erforscht bis X %“? Und was bedeutet Jahreshälfte `season` 0/1?
6. **Startbudget und Startjahr:** klassisch Budget 60 MB je Runde ab 1957. Für 1955 braucht das historische Szenario eine eigene Budgetkurve (NASA gab es erst ab FY1959; Mittel siehe `recherche-mercury.md`, Abschnitt 5). Kein Wert vorgeschlagen.
7. **Optionale Raketen** (Thor-Able, Juno II): aufnehmen? Thor-Able gibt der Mondsonde Pioneer einen Weg 1958–60 (sonst erst Atlas-Agena ab 1960); Juno II füllt die Lücke Juno I (7) ↔ Atlas D (500). Beide kosten Listenplätze (7 statt 5 Raketen).
8. **`basic` vs `historical` (Spielmodus):** der Entwurf setzt für die neuen US-Einträge identische Werte in beiden Sätzen. Soll `historical` das UdSSR-Verhalten (R-7 800, Wostok 700) auch für die USA abbilden?
9. **KI:** neue Einträge werden von der KI ignoriert (siehe `hardware-katalog.md`, Risiken). Soll die USA-Seite im Vorschau-Szenario nur für Menschen sein?
10. **Zufallsmodell** (`random` in `classic.json`): im Entwurf nicht enthalten.
11. **Stückkosten Raketen:** alle ⚠ Schätzung; falls bessere Quellen (SP-4012 Bd. 2 Beschaffungslisten) verfügbar sind, bitte nachreichen.

---

## 9. Quellen

Aus `recherche-mercury.md`: SP-4201 „This New Ocean“ (App. D Flight Data Summary, App. F Cost Summaries, S. 502 MA-10),
SP-4012 Historical Data Book Bd. 1+2 (Tab. 1-51, 1-59, 2-28 bis 2-35).

Neu in dieser Runde (englische Wikipedia, abgerufen als `index.php?title=…&action=raw`, Zweitquellen mit Zitat NASA/NSSDCA):
Atlas_LV-3B, SM-65D_Atlas, List_of_Atlas_launches (1957–1959 und 1960–1969; eigene Zählung der Erfolge je Jahr), Atlas-Able, Juno_I, Juno_II, Jupiter-C,
Vanguard_(rocket), Explorer_1, Vanguard_1, Pioneer_0, Pioneer_4, Pioneer_5, Ranger_1, Ranger_3, Mariner_1, Mariner_2, Thor-Able, Mercury-Redstone_Launch_Vehicle,
Mercury-Atlas_10, PGM-11_Redstone. NASA-Primärseiten (history.nasa.gov, nssdc.gsfc.nasa.gov) wurden in dieser Runde nicht direkt geprüft.

---

## 10. Entscheidungen des Eigentümers (23.11.2026)

| Nr. | Frage | Entscheidung |
|---|---|---|
| 1 | Reichweite (`reach`) der Raketen auswerten | ja |
| 2 | Pioneer und Mariner als `interplanetary_probe`, Missionsauswahl nach `targets` | ja |
| 3 | Atlas-Agena als eigene Rakete | ja |
| 4 | Dauerstufen laut Spiel: A = 1–2 Tage, B = 3–5 Tage, C = 6–7, D = 8–12, E = 13–16, F = 17–20 (`future.cpp`); MA-9 = A, MA-10 = B; Mercury höchstens B | ja |
| 5 | Thor-Able und Juno II aufnehmen | ja |
| 6 | Startbudget 1955–57 reduziert (Militärprogramme), ab 1958 NASA | ja, Werte siehe 11 |
| 7 | USA im historischen Szenario vorerst nur für menschliche Spieler | ja |
| 8 | `basic` = `historical` für neue US-Einträge, Zufallsmodell später | ja |

## 11. Budgetvorschlag 1955–1958 (zur Bestätigung bei der Umsetzung)

Mechanik des Spiels: Das Jahresbudget wird im Herbst ausgezahlt und verändert sich danach mit dem
Prestige (Obergrenze 180). Klassischer Startwert 1957: 60 MB für beide Seiten.

| Startjahr | Startbudget je Seite | Begründung |
|---|---|---|
| 1955 | 35 MB | nur Militärprogramme (Army/Navy/Air Force), Satellitenprogramm erst ab Juli 1955 |
| 1956 | 45 MB | Vanguard und Jupiter-C in Entwicklung |
| 1957 | 60 MB | klassischer Wert |

Zusätzlich ein Ereignis „Gründung der NASA“ (Okt. 1958) für die USA: einmalig +15 MB Budget
(analog zum vorhandenen Ereignis „+15“). Für die UdSSR gelten dieselben Startwerte (Fairness);
deren Budget entwickelt sich wie klassisch über das Prestige.
