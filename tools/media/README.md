# Medien-Werkzeuge fuer Race into Space

Drei kleine Python-Skripte, mit denen eigene Bilder und Videos ins Spiel kommen,
ohne die Originaldaten anzufassen. Alles landet im **custom-Ordner**, der die
Originaldaten ueberlagert.

| Skript | Zweck |
|---|---|
| `clip_katalog.py` | Erzeugt `doc/fork/clip-katalog.xlsx` (welcher Clip/welches Bild wird wo benutzt) |
| `bild_konvertieren.py` | Eigenes Bild -> Originalgroesse + Originalpalette (+ Transparenz) |
| `video_konvertieren.py` | Eigenes Video -> Theora 160x100 / 8 fps (+ optional Vorbis-Ton) |

## Installation (Mac)

```
brew install ffmpeg          # enthaelt Theora und Vorbis
pip3 install pillow openpyxl
```

Pruefen: `ffmpeg -encoders | grep -i theora` muss `libtheora` zeigen.

## Der custom-Ordner

Das Spiel sucht Ersatzdateien in dieser Reihenfolge:

1. `$BARIS_CUSTOM` (Umgebungsvariable, falls gesetzt)
2. macOS: `~/Library/Application Support/Race Into Space/custom`
3. `<Datenordner>/custom`

Der Ordner spiegelt den Aufbau von `data/`:

```
custom/images/main_menu.png
custom/video/mission/apol_01.ogg
custom/audio/mission/para_00.ogg
```

Die Skripte schreiben standardmaessig in Punkt 1 bzw. 2. Mit `--ziel-ordner PFAD`
kann man einen anderen Ordner waehlen. Den Datenordner (Originale) finden sie
automatisch im Repository (`data/`); sonst `--daten PFAD_ZU_data` angeben.

**Rueckgaengig machen:** die Datei im custom-Ordner loeschen. Dann nimmt das
Spiel wieder das Original. Die Originale in `data/` werden nie veraendert.

## 1. Katalog: `clip_katalog.py`

```
python3 tools/media/clip_katalog.py
```

Schreibt `doc/fork/clip-katalog.xlsx` mit drei Blaettern:

- **Missionsclips**: jeder Eintrag aus `seq.json` (Erfolg) und `fseq.json`
  (Fehlschlag) mit Schritt, Seite, Hardware, Fehlertext, Video- und Audiodateien.
  Die Spalte "Alle Dateien vorhanden" zeigt `nein`, wenn z. B. zu einem Clip
  keine Audiodatei existiert (das ist in den Originaldaten haeufig so).
- **Videodateien**: Aufloesung, fps, Dauer, Anzahl/Liste der Sequenzen, die das
  Video nutzen; Spalte "Ersetzt (custom)" zum Abhaken.
- **Bilder**: Groesse, Modus, Palettenfarben, Transparenz, Quelldateien, die den
  Dateinamen erwaehnen (Naeherung; "(?)" = nur ueber einen gekuerzten
  Namen gefunden, z. B. wenn der Name im Code zusammengebaut wird).

Optionen: `--daten`, `--quelle` (Quellcode, Standard `src/game`), `--ausgabe`.

### Schluessel der Sequenzen
`MissionIdSequence` = Variantenzahl, Clipanzahl, weiblich (0/1), dann der
Schluessel: Schrittbuchstabe + Seite (`U` USA / `S` UdSSR) + Hardware-ID
(z. B. `C2` = Apollo bzw. Sojus). Bei Fehlschlaegen folgt die Fehlernummer
(2 Ziffern), die Fehlergruppe ist `F` + Nummer.

## 2. Bilder: `bild_konvertieren.py`

Einzelnes Bild:

```
python3 tools/media/bild_konvertieren.py MEIN_BILD.png images/main_menu.png
```

Der Zielpfad ist relativ zu `data/` (die Tabelle im Katalog zeigt alle Pfade).
Das Skript liest die Originaldatei, skaliert dein Bild auf deren Groesse,
reduziert auf deren Palette, behaelt die Original-Transparenz und schreibt nach
`custom/images/...`. Neben dein Bild kommt `MEIN_BILD.vorschau.png` (RGB, mit
Schachbrett an transparenten Stellen) zur Kontrolle.

Optionen:

- `--modus fuellen|einpassen|strecken`
  - `fuellen`: Seitenverhaeltnis behalten, Bild fuellt alles, Rand wird mittig
    abgeschnitten (Standard fuer Bilder **ohne** Transparenz)
  - `einpassen`: Seitenverhaeltnis behalten, ganzes Bild sichtbar, Rest
    transparent (Standard fuer Bilder **mit** Transparenz)
  - `strecken`: verzerrt auf die Zielgroesse
- `--dither`: Floyd-Steinberg-Dithering (weichere Verlaeufe, aber Rauschen;
  Standard aus)
- `--freistellen [FARBE]`: Hintergrundfarbe transparent machen (Standard
  `#FF00FF`, Magenta). Gedacht fuer KI-Bilder, die keine echte Transparenz
  koennen: Sprite auf einfarbig magentafarbenen Hintergrund erzeugen lassen.
- `--toleranz N`: Farbabstand (RGB), bis zu dem Pixel als Hintergrund gelten
  (Standard 60). Bleiben rosa Raender, Wert erhoehen; werden Teile des Motivs
  durchsichtig, Wert senken. Die Magentafarbe sollte im Motiv nicht vorkommen.

Beispiel Sprite:

```
python3 tools/media/bild_konvertieren.py sprite_magenta.png images/aprog/aprog.0.1.png --freistellen
```

Transparente Pixel bekommen den transparenten Palettenindex des Originals
(Pixel mit Alpha < 128); die tRNS-Information des Originals bleibt erhalten.
Nur Originale mit Transparenz haben transparente Stellen im Ergebnis.

### Stapelmodus

```
python3 tools/media/bild_konvertieren.py --ordner EINGANG [--freistellen]
```

Alle Bilder in `EINGANG` werden konvertiert. Konvention: Dateiname = Zielpfad,
wobei `/` durch `__` ersetzt wird:

```
EINGANG/images__main_menu.png            -> images/main_menu.png
EINGANG/images__aprog__aprog.0.1.png     -> images/aprog/aprog.0.1.png
```

Dateien ohne `__` oder ohne passendes Original werden uebersprungen (mit Meldung).
`*.vorschau.png` werden ignoriert. `--modus`, `--dither`, `--freistellen`
gelten fuer alle Dateien.

## 3. Videos: `video_konvertieren.py`

```
python3 tools/media/video_konvertieren.py MEIN_FILM.mp4 mission/apol_01.ogg --start 12 --dauer 10
python3 tools/media/video_konvertieren.py MEIN_FILM.mp4 mission/apol_01.ogg --ton para_00
```

- Der Zielpfad ist relativ zu `data/video/` (`mission/`, `training/`, `news/`).
- Aufloesung und Bildrate kommen vom Original (Mission und Training: 160x100,
  8 fps; News: 312x106, 15 fps). Dein Video wird mittig auf das
  Seitenverhaeltnis zugeschnitten und skaliert.
- Ergebnis: Theora, yuv420p, **ohne Ton**, in `custom/video/...`.
- `--ton NAME` schreibt zusaetzlich den Ton als Vorbis (Stereo, 44,1 kHz) nach
  `custom/audio/mission/NAME.ogg` (mit Unterordner z. B. `news/sov_000`). Der
  Audioname eines Clips steht im Katalog (Spalte Audiodateien).
- Weicht die neue Dauer stark von der des Originals ab, gibt es eine Warnung.

Hat dein ffmpeg kein `libtheora`, bricht das Skript mit einer Erklaerung ab
(`brew install ffmpeg` loest das normalerweise).

## Bekannte Grenzen

- Nur Bilder mit vorhandenem Original koennen konvertiert werden.
- Bilder, die gemeinsam auf einem Bildschirm stehen, teilen sich die Palette;
  ein Ersatz mit nur 16-40 Palettenfarben kann daher flau aussehen.
- Die Dateizuordnung "Verwendet in" im Bilder-Blatt ist eine Textsuche und nur eine Naeherung.
