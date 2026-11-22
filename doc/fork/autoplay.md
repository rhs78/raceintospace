# Autoplay: automatischer Testlauf mit zwei KI-Spielern

Der Autoplay-Modus startet ein komplettes Spiel ohne Fenster-Eingaben: beide
Seiten werden von der KI gespielt, alle Wartepunkte der Oberfläche werden
automatisch beantwortet. So findet man Abstürze, Asserts und
Out-of-bounds-Zugriffe in Ereignissen, Missionen, KI und Zugschleife
automatisch (z. B. nach dem Umbau der Hardware auf dynamische Listen,
siehe `hardware-katalog.md`, Abschnitt 9).

Im normalen Spiel (ohne `--autoplay`) ändert sich nichts.

## Aufruf

    raceintospace --autoplay [--seed N] [--start-year 55|56|57]
                  [--model basic|historical|random] [--sandbox]
                  [--scenario classic|test_extra] [--years N]

| Option | Bedeutung |
| --- | --- |
| `--autoplay` | Autoplay-Modus (kein Intro, kein Menü, keine Wartezeiten, kein Ton) |
| `--seed N` | Startwert des Zufallsgenerators (Standard 1); gleicher Seed = gleicher Spielverlauf |
| `--start-year YY` | Startjahr 55, 56 oder 57 (Standard: das des Szenarios) |
| `--model` | Startmodell der Hardware: `basic`, `historical` oder `random` |
| `--sandbox` | Sandbox-Modus einschalten (wirkt nur auf menschliche Spieler, siehe unten) |
| `--scenario ID` | Szenario (`classic`, `test_extra`) |
| `--years N` | Spiel dauert N Jahre ab Startjahr (auch länger als das Szenario-Ende, max. 48); ohne Angabe bis Herbst 1963 |

Der Lauf gibt auf stdout Zeilen mit `[autoplay]` aus: Seed und Einstellungen,
pro Runde Jahr/Jahreszeit/Geld, jede Mission (Seite, Rampe, Missionscode,
Prestige), jedes Nachrichtenereignis und am Ende `END: ...` (Landung bzw.
Sieger nach Punkten) und `RESULT OK`. Exit-Code 0 bei sauberem Ende, sonst
Assert/Absturz (Exit-Code ungleich 0).

Unter Linux (ohne Display):

    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy BARIS_DATA=$PWD/data \
      BARIS_SAVE=/tmp/ris-save \
      ./build/src/game/raceintospace --autoplay --seed 5 --model random

`BARIS_SAVE` sollte auf ein Wegwerf-Verzeichnis zeigen (der Lauf schreibt
`roster.json`, `RECORDS.DAT` usw.).

Unter macOS (Mac mini; die Binärdatei liegt im App-Bundle, der SDL-Dummy-Treiber
unterdrückt das Fenster):

    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy BARIS_SAVE=/tmp/ris-save \
      build/debug/raceintospace.app/Contents/MacOS/raceintospace \
      --autoplay --seed 5

## Testmatrix: `tools/autoplay.sh`

    tools/autoplay.sh                       # ASan+UBSan-Build in build-asan, Matrix
    tools/autoplay.sh --no-asan             # normaler Debug-Build in build/
    tools/autoplay.sh --no-build --seeds 100 --sampled
    tools/autoplay.sh --no-build --binary build/debug/raceintospace.app/Contents/MacOS/raceintospace   # macOS

Standard: Seeds 1..40, je Seed alle 3 Startjahre x 3 Modelle (Sandbox
wechselt mit dem Seed), dazu pro Seed ein langes Spiel (`--years 20`, läuft
meist bis zur ersten Mondlandung) und das Szenario `test_extra`. Das sind
rund 400 Läufe, auf 4 Kernen mit ASan etwa 6 Minuten. Jeder Lauf hat ein
`timeout` (Standard 300 s, `--timeout`). Optionen: `--seeds N`, `--from S`,
`--sampled` (nur eine Kombination je Seed), `--jobs J`, `--results DIR`.

Ergebnis: Tabelle mit Lauf, Ergebnis (`OK`, `CRASH`, `HANG`, `UBSAN`,
`NOEND`), Exit-Code, Sekunden, letzter Runde und erster Fehlerzeile
(bei ASan mit Fundstelle). Logs je Lauf in
`autoplay-results/<Zeitstempel>/<lauf>/log.txt`. Einen Fehler
reproduziert man mit den Argumenten aus dem Namen des Laufs, z. B.
`s9-y55-basic-sb1-classic` = `--seed 9 --start-year 55 --model basic
--sandbox --scenario classic`. Exit-Code 1, wenn ein Lauf fehlschlug.

UBSan: `-fno-sanitize=alignment` ist gesetzt, weil `data.h` unter
`#pragma pack(1)` `std::string`/`std::vector` in Strukturen enthält (viel
Rauschen, unter x86/arm64 harmlos).

## Smoke-Test in CTest

    cmake --build build
    ctest --test-dir build -R autoplay_smoke --output-on-failure

Zwei Läufe (Seeds 1 und 2, classic, basic, Startjahr 57), ohne Sanitizer,
unter 2 Minuten (Debug-Build: wenige Sekunden). Das Spiel muss vorher gebaut
sein.

## Wie es funktioniert

Zentrale Stelle: `options.autoplay` wird in den untersten Ein-/Warte-Funktionen
geprüft, nicht in den einzelnen Bildschirmen.

* `game_main.cpp`: statt Intro und Hauptmenü ruft `game_main_impl()`
  `AutoplayNewGame()` (`autoplay.cpp`) auf. Das baut den Startzustand wie
  die Preferences (Szenario, Modell, Startjahr, Plr1/Plr2 = 2/3 = KI + USA/UdSSR,
  `AI[] = {1,1}`), setzt `srand(seed)` und ruft `MainLoop()`. In der Schleife
  wird pro Runde, Mission und Ereignis protokolliert.
* `sdlhelper.cpp`: `av_block()` wartet nie; `av_autoplay_tick()` drückt alle
  8 Aufrufe ENTER (auch aus `GetMouse_fast()`). ENTER ist die neutrale Taste
  ("weiter"/"OK") fast aller Bildschirme. `av_sync()` (Skalierung/Anzeige) und
  Überblendungen (`av_set_fading`) sind abgeschaltet, es gibt keinen
  Audio-Ausgang (`AnimSoundCheck()` meldet sofort "fertig") und keinen
  30-ms-Timer.
* `pace.cpp`: `idle_loop_secs()` (damit `delay()`, `idle_loop()`, Video-Bild-
  Takt) kehrt sofort zurück. Das Spiel hängt dadurch nicht von der Uhrzeit ab.
* `options.cpp`: Intro, Ton, Vollbild und 4x-Skalierung sind im Autoplay aus.

Sonderfälle:

* Die KI-Runde ruft im Autoplay `News()` statt `AIEvent()` auf. `News()` ist
  sonst nur für Menschen und löst das Ereignis genauso auf; so läuft auch der
  Nachrichten-Pfad (Ereignistext, Ereigniskarten) mit. Der Nachrichten-Bildschirm
  wird nach ein paar Durchläufen mit ENTER beendet.
* Das Krisen-Dialogfenster (`mis_d.cpp`) wird für die KI ohnehin übersprungen.
* Die Endbildschirme (`NewEnd`, `FinalScoreEnd`, `SpecialEnd`, Museum) laufen
  mit den ENTER-Tasten durch.
* Sandbox hat bei zwei KI-Spielern keine Wirkung (Geld- und Entlassungsregeln
  gelten nur für Menschen); die Option wird nur gesetzt und geloggt.
* Nicht abgedeckt (nur für menschliche Spieler): `Master()`, Vorbereitungs-
  Menüs (VAB, Einkauf, Astronautenzuteilung), `MisRev`, Missionsanzeige.
