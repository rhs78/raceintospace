# Krisensystem – Spezifikation (Etappe 2)

Stand: 21.11.2026 · Grundlage: `doc/fork/architektur.md`, Abschnitt 3

## Ziel

Bei bemannten Missionen greift der Spieler in kritischen Momenten ein, statt das Würfelergebnis
nur abzuspielen. Vorbilder: Mercury-Atlas 6 (Hitzeschild-Warnung), Gemini 8 (klemmende Steuerdüse),
Apollo 11 (Alarme 1201/1202), Apollo 13 (Explosion des Sauerstofftanks).

Grundsätze:

- **Echte Entscheidung unter Unsicherheit** – der Spieler kennt das Würfelergebnis nicht.
- **Wenige, gewichtige Dialoge** – kleine Störungen laufen wie bisher ohne Unterbrechung durch.
- **Datengetrieben** – Auslöser, Texte und Kosten stehen in `data/gamedata/decisions.json`.
- **Abschaltbar** – Option „Klassischer Missionsablauf“ stellt das Originalverhalten her.
- **Unbemannte Missionen** sind nicht betroffen.

## Auslöser

### A. Krise nach Fehlschlag

Einstiegspunkt: `mis_m.cpp`, nach `GetFailStat()` und vor `FailEval()`. Die Fehlerzeile aus
`fails.json` ist bekannt, ihre Folgen sind noch nicht angewendet.

Einteilung nach Fehlercode (`fail.code`):

| Klasse | Codes | Dialog |
|---|---|---|
| Lebensgefahr | 3, 5, 13, 16, 17, 22, 31, 33 | ja |
| Missionsziel gefährdet | 2, 4, 15, 26, 30 | ja |
| Störung | 0, 6, 7, 9, 12, 18, 19, 20, 23, 24, 25, 40 | nein (Originalablauf) |

### B. Alarm ohne Fehlschlag (Typ 1201)

Einstiegspunkt: nach der Sicherheitskarten-Prüfung, vor `if (PROBLEM)`.

- Nur bei Schritten, die in `decisions.json` als alarmfähig markiert sind (Start A, Andocken I, Mondlandung S, Wiedereintritt D).
- Bedingung: Würfelwert liegt im Band `|dice − safety| ≤ 3` (Wert in den Daten) und ein Zufallswurf (30 %) trifft.
- Der Alarm erscheint also sowohl kurz vor einem Erfolg als auch kurz vor einem Fehlschlag – der Spieler weiß nicht, welcher Fall vorliegt.

## Optionen

### Krise nach Fehlschlag

| Option | Wirkung |
|---|---|
| **Abbruch einleiten** | Abbruchwurf: Erfolgschance = Abbruchbasis des Schritts (Daten) + Sicherheit der Kapsel / 4 + Crew-Bonus. Erfolg: Mission endet als Teilfehlschlag, Crew überlebt; Rückweg über den vorhandenen `Alt`-Zweig, sonst Missionsende. Misserfolg: ursprüngliche Folge. |
| **Bodenteam arbeitet an einer Lösung** | Kosten in MB je nach Klasse (Daten, Vorschlag 5 / 10). Neuwurf mit Bonus = Crew-Bonus + Fortschritt der Hardware (Sicherheit − Basis) / 5. Erfolg: Schritt gilt als bestanden, Mission läuft weiter, Status „gerettet“. Misserfolg: ursprüngliche Folge. Höchstens einmal pro Mission. |
| **Weitermachen** (nur Klasse „Missionsziel gefährdet“) | Die Mission läuft weiter, aber alle folgenden Schritte erhalten einen Sicherheitsabzug (Vorschlag −10). Tritt später ein weiterer Fehlschlag auf, wird er eine Klasse schwerer eingestuft. |
| **Folgen hinnehmen** | Originalablauf über `FailEval()`. |

### Alarm

| Option | Wirkung |
|---|---|
| **GO** | Das gewürfelte Ergebnis gilt (Erfolg oder Fehlschlag mit anschließender Krise). |
| **ABBRUCH** | Sicherer Abbruch, Mission endet als Teilfehlschlag, kleiner Prestigeabzug. |

Als Hilfe zeigt der Dialog eine **Einschätzung des Flugdirektors** („GO empfohlen“ / „Abbruch
empfohlen“). Sie ist zu 70 % plus Crew-Bonus zuverlässig, aber nicht immer richtig.

## Prestige und Folgen

- Gerettete Crew nach einer Krise der Klasse „Lebensgefahr“: kleiner Prestigebonus (Vorschlag +2, „erfolgreicher Fehlschlag“ nach Apollo 13).
- Erfolgreicher Abbruch zählt nicht als Tod, Meilensteine der Mission werden nicht erreicht.
- Kosten des Bodenteams werden sofort vom Budget abgezogen; ist zu wenig Geld da, ist die Option gesperrt.

## Historische Szenarien

`decisions.json` ordnet Fehlergruppen (`MissionStep`, z. B. `F008`) und optional Fehlercodes einer
Überschrift und einem Text zu. Ohne passenden Eintrag erscheint ein allgemeiner Text.

| Szenario | Schritt(e) | Klasse |
|---|---|---|
| „Warnung: Hitzeschild möglicherweise gelöst“ (MA-6) | D Wiedereintritt | Alarm |
| „Steuerdüse klemmt, Kapsel rotiert“ (Gemini 8) | I Andocken, F Erdorbit-Aktivität | Lebensgefahr |
| „Programmalarm 1201“ (Apollo 11) | S Mondlandung | Alarm |
| „Explosion im Servicemodul“ (Apollo 13) | J TLI, O Mondtransfer | Lebensgefahr → Abbruch über Mondumrundung |

⚠ Zuordnung der Fehlergruppen zu den Schritten wird bei der Umsetzung gegen `fails.json` geprüft.

## Technische Umsetzung

- Neues Modul `src/game/mis_d.cpp/.h`: `CrisisResult OfferCrisis(...)`, `AlarmResult OfferAlarm(...)`.
- Dialog nach Vorlage `DrawMoonSelection` / `FailureMode` (`mis_c.cpp`), Bildschirm sichern und wiederherstellen.
- KI-Spieler: feste Regel ohne Dialog (Lebensgefahr → Bodenteam, falls bezahlbar, sonst Abbruch; Alarm → GO bei Sicherheit ≥ 75).
- Ergebnisse als neue `StepInfo`-Codes, damit Missionsbericht, Prestige und Wiederholung stimmen.
- Option in `options` (Konfigurationsdatei), Standard: Krisensystem an.
- Tests: Klasseneinteilung, Wahrscheinlichkeitsformeln, KI-Regel, Laden von `decisions.json`.
- Spielstand-Kompatibilität mit alten Ständen ist nicht erforderlich (Entscheidung vom 21.11.2026).

## Offene Punkte

- Sprache der Spieltexte: Englisch (entschieden 21.11.2026).
- Zahlenwerte (Band, Kosten, Abzüge) sind Startwerte und werden beim Probespielen justiert.

## Umsetzungsstand (21.11.2026)

- Umgesetzt in `src/game/mis_d.cpp/.h`, Einbindung in `MisCheck` (`mis_m.cpp`), Prestige in `AllotPrest` (`prest.cpp`).
- Parameter in `data/gamedata/decisions.json` (`params`, Abbruchbasis je Schrittbuchstabe, Texte, Szenarien).
- Ergebniscodes `StepInfo` 1960 (Abbruch), 1961 (Abbruch bei Lebensgefahr), 1962 (Alarm-Abbruch), 1963 (Weiterflug mit Risiko) – alle im Bereich „Teilfehlschlag“.
- Abweichung von der Spezifikation: Das Bodenteam erhält einen Abzug (`ground_penalty_life` 15, `ground_penalty_mission` 5), sonst wäre es gegenüber „Folgen hinnehmen“ immer überlegen.
- Abschalten: Zeile `classic_missions 1` in der Datei `config` im Spielstandverzeichnis; `game_style 1` schaltet ebenfalls ab.
- Getestet: Logik (Klassen, Wahrscheinlichkeiten, KI-Regel, Alarm-Auslöser, Laden der Daten). Nicht getestet: Dialog und Ablauf in einer echten Mission – Probespielen auf dem Mac erforderlich.

## Erweiterung: realistische Rettungssysteme (Entwurf 22.11.2026)

Der Abbruch hängt künftig vom **Rettungssystem der Kapsel** ab, nicht nur vom Missionsschritt.
Neues Katalogfeld je bemannter Kapsel/Lander: `abort_system`.

### Historische Grundlage

| Kapsel | Rettungssystem beim Start | Quelle |
|---|---|---|
| Mercury | Rettungsturm (Launch Escape System), getestet mit Little Joe; danach Abtrennen der Kapsel | Project Mercury (Wikipedia, verweist auf SP-4201) |
| Gemini | Schleudersitze (Titan II mit hypergolem Treibstoff, kleinerer Explosionsradius); in größerer Höhe Abtrennen der Kapsel | Project Gemini (Wikipedia) |
| Apollo | Rettungsturm, später Abbruch mit SPS-Triebwerk | Launch escape system (Wikipedia) |
| Wostok | Schleudersitz: in den ersten 40 s Ausschuss; 40–150 s Abschalten der Rakete, dann Ausschuss in geringerer Höhe; danach Abtrennen der Landekapsel | Vostok (spacecraft) (Wikipedia) |
| Woschod | **keine** Rettungsmöglichkeit in den ersten ca. 2,5 min (Schleudersitz ausgebaut, Rettungsturm nicht fertig) | Voskhod (spacecraft) (Wikipedia) |
| Sojus | Rettungsturm (SAS) | Launch escape system (Wikipedia) |
| Apollo-LM / LK | Abbruch der Landung durch Zünden der Aufstiegsstufe | ⚠ noch zu belegen |

⚠ Fiktive Programme des klassischen Szenarios (XMS-2, Lapot, Jupiter, Kvartet): Annahme
Schleudersitze (Minishuttles) bzw. Rettungsturm (Vierer-Kapseln).

### Spielregeln

| `abort_system` | Abbruch beim Start (Schritt A) | Folgen |
|---|---|---|
| `escape_tower` | „ABORT: ESCAPE TOWER“, hohe Basis (Vorschlag 70) | Crew unverletzt |
| `ejection_seats` | „ABORT: EJECTION SEATS“, mittlere Basis (Vorschlag 50) | Verletzungsrisiko je Crewmitglied (Vorschlag 25 %) |
| `none` | Option gesperrt: „NO ESCAPE SYSTEM“ | nur Bodenteam oder Folgen hinnehmen |

- Andere Schritte: Abbruch über das Abtrennen der Kapsel mit den bisherigen Basiswerten je Schritt.
- Mondlandung (Schritt S): „ABORT: ASCENT STAGE“ mit Basis aus dem Lander-Eintrag.
- Werte in `decisions.json` je System, Zuordnung im Hardware-Katalog.
- KI: Ist kein Abbruch möglich, wählt sie Bodenteam (falls bezahlbar) oder Folgen hinnehmen.
- Umsetzung nach Schritt 4b, weil beide den Hardware-Katalog ändern.
