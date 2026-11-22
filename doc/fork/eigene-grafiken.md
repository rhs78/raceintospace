# Eigene Grafiken und Missionsvideos

Stand: 22.11.2026 · Werkzeuge: `tools/media/` · Übersicht aller Dateien: `doc/fork/clip-katalog.xlsx`

Diese Anleitung beschreibt, wie Sie Grafiken (z. B. mit Google Gemini) und Missionsvideos erstellen
und ins Spiel bringen, ohne Originaldateien zu überschreiben.

---

## 1. Grundprinzip

1. **Hochaufgelöstes Master-Bild erzeugen** (Gemini) und **aufbewahren**.
2. Master mit `tools/media/bild_konvertieren.py` auf das Spielformat umrechnen
   (Größe, 256-Farben-Palette des Originals, Transparenz).
3. Ergebnis landet im **Custom-Ordner**; das Spiel nimmt es dort automatisch vor dem Original.
4. Rückgängig: Datei im Custom-Ordner löschen.

**Warum Master aufbewahren?** Das Spiel arbeitet heute mit 320×200 Bildpunkten und 256 Farben.
Mit Etappe 7 (Echtfarben-Ebene) können dieselben Master-Bilder in hoher Auflösung verwendet werden –
dann muss nichts neu generiert werden.

Empfohlene Ablage auf dem Mac (außerhalb des Repositorys):

```
~/RIS-Grafik/
  master/      Original-Ausgaben von Gemini, Dateiname = Zielpfad, z. B. images__main_menu.png
  referenz/    NASA-Fotos als Vorlagen
  prompts.md   verwendete Prompts (zur Wiederholung und für einheitlichen Stil)
```

## 2. Custom-Ordner

Das Spiel sucht Ersatzdateien in dieser Reihenfolge:

| Priorität | Ort |
|---|---|
| 1 | Ordner aus Umgebungsvariable `BARIS_CUSTOM` |
| 2 | `~/Library/Application Support/Race Into Space/custom` (macOS) |
| 3 | `<Spieldaten>/custom` (im App-Paket: `raceintospace.app/Contents/Resources/custom`) |

Empfohlen ist Ort 2: kein Neubau nötig, Spiel einfach neu starten. Der Ordner spiegelt den
Aufbau des Datenordners:

```
custom/images/main_menu.png
custom/images/control.img.0.png
custom/video/mission/apol_01.ogg
custom/audio/mission/<name>.ogg
```

## 3. Technische Vorgaben

### 3.1 Bildgrößen im Spiel und empfohlene Master-Größe

| Kategorie | Dateien (Beispiele) | Spielgröße | Seitenverhältnis | Transparenz | Gemini-Format | Master |
|---|---|---|---|---|---|---|
| Vollbild-Hintergründe | `main_menu.png`, `port/usa_port.dat.0.png` | 320×200 | 16:10 | nein | 16:9, Rand wird beschnitten | ≥ 1600×1000 |
| Vollbild 4:3 | `control.img.0/1.png` (Kontrollzentrum), `first.img.*`, `hospital.*`, `preferences.png` | 320×240 | 4:3 | nein | 4:3 | ≥ 1600×1200 |
| Nachrichtenbilder | `news/news.cdr.usa.*` | 320×110 | ca. 3:1 | nein | 16:9, oben/unten beschnitten | ≥ 1600×550 |
| Forschung/Hardware | `rd/rdfull.but.*` | 104×77 | ca. 4:3 | nein | 4:3 | ≥ 800×600 |
| Start-/Hardwaresprites | `liftoff.abz.*` | 100×80 | 5:4 | ja | 4:3 oder 1:1 | ≥ 800×640 |
| Geheimdienst | `intel/intel.but.*` | 157×100 | ca. 16:10 | ja | 16:9 | ≥ 800×500 |
| Astronautenprogramme | `aprog/aprog.*` | 80×50 | 16:10 | ja | 16:9 | ≥ 640×400 |
| Gebäude Raumhafen | `port/usa_port.dat.*` | je nach Gebäude | – | ja | passend | ≥ 8× Spielgröße |
| Missionsabzeichen | `patches/patches.but.*` | ca. 20–35 px | ca. 1:1 | ja | 1:1 | ≥ 512×512 |
| Gesichter | `faces/faces.but.*` | 18×15 | – | nein | – | nicht sinnvoll |

Genaue Maße jeder Datei: Tabellenblatt „Bilder“ in `clip-katalog.xlsx`.

### 3.2 Farben (256-Farben-Palette)

Das Bild wird auf die Palette des **Originalbilds** umgerechnet, weil Schrift, Rahmen und Schaltflächen
auf demselben Bildschirm feste Palettenfarben nutzen. Daraus folgen Regeln für die Gestaltung:

- **Klare Flächen, deutliche Konturen**, wenig feine Farbverläufe – Verläufe werden zu Stufen.
- **Farbstimmung nahe am Original** wählen (Original vorher ansehen), dann passen die Palettenfarben gut.
- **Keine Schrift im Bild** – Gemini erzeugt oft fehlerhafte Buchstaben, und das Spiel schreibt Texte selbst.
- **Wichtiges mittig** – beim Zuschneiden auf das Zielformat gehen Ränder verloren.
- Bei Bildern mit Himmel/Weltraum: Sternfelder sparsam, sie werden bei 320×200 zu Rauschen.
- Option `--dither` testen, wenn Verläufe zu stufig wirken.

### 3.3 Transparenz (Sprites)

Gemini kann keine echte Transparenz erzeugen. Deshalb:

- Objekt auf **einfarbigem Magenta-Hintergrund (#FF00FF)** erzeugen lassen,
  ohne Schatten, ohne Bodenfläche, ohne Spiegelung.
- Beim Konvertieren `--freistellen` verwenden: Magenta wird transparent.
- Objekt sollte kein Magenta/Pink enthalten.

### 3.4 Videos

| Ordner | Größe | Bilder/s | Ton |
|---|---|---|---|
| `video/mission` | 160×100 | 8 | getrennt in `audio/mission` |
| `video/news`, `video/training` | siehe Blatt „Videodateien“ | | |

- Welche Szene welchen Clip nutzt: Blatt „Missionsclips“ im Katalog. Viele Clips werden
  mehrfach verwendet – ein Ersatz wirkt an allen Stellen.
- Umwandeln: `tools/media/video_konvertieren.py` (ffmpeg mit Theora erforderlich).
- Länge möglichst nahe am Original halten, der Ton läuft unabhängig vom Bild.
- Quellen für historisches Material: NASA (meist gemeinfrei), National Archives, archive.org.
  ⚠ Sowjetisches Material: Rechtelage unklar, nur private Nutzung.
- Hochaufgelöste Videos mit Ton im Clip folgen mit Etappe 8; Masterdateien deshalb ebenfalls aufbewahren.

## 4. Prompts für Gemini

### 4.1 Stilvorgabe (allen Prompts voranstellen)

Damit alle Bilder einheitlich wirken, einen festen Stilblock verwenden und nicht variieren:

```
Style: detailed painted illustration in the look of an early-1990s computer strategy game,
semi-realistic, muted period colors, strong clear shapes and outlines, soft but limited
shading, no film grain, no lens effects, no text, no letters, no logos, no watermark.
Historically accurate to the year {JAHR}. Content must stay inside the central 80 % of the frame.
```

### 4.2 Vorlage

```
{STILBLOCK}
Subject: {MOTIV}
Setting: {ORT, ZEIT, TAGESZEIT}
Camera: {BLICKWINKEL}
Aspect ratio: {FORMAT}
{nur Sprites:} Isolated object on a flat solid magenta (#FF00FF) background,
no shadow, no ground, no reflection, object fully visible with margin.
```

Referenzfotos (NASA) können Sie dem Prompt als Bild beifügen: „Use the attached photo as reference
for layout and equipment, but repaint it in the given style.“

### 4.3 Beispiele

**Kontrollzentrum Mercury (`control.img.0.png`, 4:3)**
```
{STILBLOCK mit JAHR=1962}
Subject: Mercury Control Center at Cape Canaveral during a manned orbital flight:
rows of consoles with analog gauges and small CRT screens, a large world map wall display
with the orbital ground track, flight controllers in white short-sleeve shirts and ties.
Camera: elevated view from the back of the room toward the map wall.
Aspect ratio: 4:3
```
Hinweis: Das Spiel blendet Video, Uhr und kleine Monitorbilder an festen Positionen ein.
Vor dem Erstellen das Original ansehen und die Raumaufteilung grob übernehmen,
sonst liegen Einblendungen über wichtigen Bildteilen. Die freie Anordnung der Einblendungen
kommt mit Etappe 7.

**Hauptmenü (`main_menu.png`, 16:10)**
```
{STILBLOCK mit JAHR=1957}
Subject: a Redstone rocket on its launch stand at dusk, gantry beside it, floodlights,
small group of engineers in the foreground silhouette.
Camera: low angle, rocket slightly left of center.
Aspect ratio: 16:9
```

**Hardware-Sprite Mercury-Redstone (`liftoff.abz.USR0.png` o. ä., transparent)**
```
{STILBLOCK mit JAHR=1961}
Subject: Mercury-Redstone launch vehicle with Mercury capsule and launch escape tower,
side view, upright, entire vehicle visible.
Isolated object on a flat solid magenta (#FF00FF) background, no shadow, no ground.
Aspect ratio: 1:1
```
⚠ Welches Sprite welche Hardware zeigt, steht im Katalog (Blatt „Bilder“, Spalte „verwendet in“);
die Zuordnung der Dateinamen zu Programmen ist im Originalspiel nicht überall offensichtlich.

**Nachrichtenbild (`news/news.cdr.usa.*`, sehr breit)**
```
{STILBLOCK mit JAHR=1958}
Subject: press conference at the Pentagon announcing Explorer 1, three men holding up a
full-size satellite model above their heads, photographers in front.
Camera: wide shot, all people in the horizontal middle band of the image.
Aspect ratio: 16:9
```

## 5. Ablauf je Bild

1. Original ansehen (Katalog → Pfad; Datei in `data/images/`).
2. Prompt aus Vorlage erstellen, in `prompts.md` notieren.
3. Gemini-Ausgabe als `master/<zielpfad mit __>.png` speichern.
4. Konvertieren, z. B.:
   ```bash
   python3 tools/media/bild_konvertieren.py ~/RIS-Grafik/master/images__control.img.0.png images/control.img.0.png
   ```
5. Vorschau `*.vorschau.png` prüfen; bei Bedarf `--dither` oder anderen Zuschnitt (`--modus`) testen.
6. Spiel neu starten, Bildschirm prüfen.
7. Im Katalog Spalte „Ersetzt (custom)“ abhaken.

## 6. Rechte

- Selbst generierte Bilder: für private Nutzung unproblematisch.
- NASA-Fotos als Referenz: meist gemeinfrei (Werke der US-Regierung); Ausnahmen bei Fotos von Agenturen/Firmen.
- Keine Logos oder Markenzeichen Dritter in Bilder übernehmen.
- Der Fork steht unter GPL; eigene Grafiken im Custom-Ordner gehören nicht zum Repository und
  werden nicht veröffentlicht (`data/custom/` ist von Git ausgenommen).

## 7. Grenzen bis zur Grafik-Modernisierung

| Grenze | aufgehoben mit |
|---|---|
| 320×200 Bildpunkte, 256 Farben, Palette des Originals | Etappe 7 (Echtfarben-Ebene) |
| feste Positionen für Video und Einblendungen im Kontrollzentrum | Etappe 7 |
| Missionsvideos 160×100, 8 Bilder/s, Ton getrennt | Etappe 8 |
| ein Kontrollzentrum pro Seite (keine Epochen) | Etappe 7 |
