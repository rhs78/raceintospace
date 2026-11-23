# Grafik- und Videoanbindung neuer Hardware – Spezifikation

Stand: 23.11.2026 · Status: freigegeben · Umsetzung direkt nach Schritt 4d
(vorgezogener Teil von 4f „Ersatzgrafiken“)

## Ziel

Jede Hardware – auch neue, die im Originalspiel nicht existiert – bekommt ihre Grafiken und
Videos über **Dateinamen im Hardware-Katalog**. Fehlt eine Datei, erscheint ein neutrales
**Platzhalterbild** bzw. ein **Ersatzclip**. Eigene Dateien legt der Besitzer im Custom-Ordner ab
(`doc/fork/eigene-grafiken.md`), ohne Codeänderung.

## Bildarten

| Feld in `images` | Größe | Transparenz | Verwendung | heute |
|---|---|---|---|---|
| `rd` | 104×77 | nein | Forschung, Kauf | Dateiname ✔ |
| `liftoff` | 100×80 | ja | Startrampe, Missionsanzeige | Dateiname ✔ |
| `vab` | wie Sprite-Tabelle (`vtable.json`) | ja | Montagehalle | Index → Dateiname |
| `aprog` | 80×50 | ja | Astronautenkomplex | Index → Dateiname |
| `patch` | ca. 25×25 | ja | Missionsbericht, Museum | Index → Dateiname |
| `mhist` | klein | ja | Hardware-Historie | Index → Dateiname |
| `intel` | 157×100 | ja | Geheimdienst | Sprite-Koordinaten → Dateiname |

- Klassische Einträge verweisen auf die vorhandenen Dateien bzw. ausgeschnittene Einzelbilder –
  das Aussehen bleibt identisch.
- Fehlende Datei: Platzhalter je Bildart (`images/placeholder/<art>.png`, neutral, mit Kategorie-Symbol).

## Videos

- Videoschlüssel heute: Kategoriebuchstabe + eine Ziffer (`R0`, `C2` …), höchstens 10 je Kategorie,
  Teil von `Mev.Name` (10 Zeichen) und der Schlüssel in `seq.json`/`fseq.json`/`fails.json`.
- Neu: Katalogfeld `anim_id` darf eine **neue Kennung** sein; zusätzliches Feld `anim_fallback`
  (vorhandene Kennung, deren Clips verwendet werden, solange es keine eigenen gibt).
- Eigene Clips: neue Einträge in `seq.json`/`fseq.json` mit der neuen Kennung, Videodateien im Custom-Ordner.
- Die Längenbegrenzung von `Mev.Name` wird geprüft und bei Bedarf erweitert.

## Bedarfsliste für den Besitzer

Werkzeug `tools/media/grafik_bedarf.py` erzeugt `doc/fork/grafik-bedarf.xlsx`:
je Katalogeintrag und Bildart eine Zeile mit Zieldateiname, Größe, Transparenz,
Status (Original / Platzhalter / eigene Datei vorhanden) und einem Prompt-Vorschlag
nach dem Stilblock aus `eigene-grafiken.md`.

## Abnahme

- Klassisches Szenario sieht identisch aus (Bildnamen-Tests aus 4a angepasst, gleiche Pixel).
- Testszenario mit Zusatzrakete: alle Bildarten zeigen Platzhalter; eine Datei im Custom-Ordner ersetzt den Platzhalter.
- Autoplay-Matrix sauber.
