#!/usr/bin/env python3
"""Konvertiert ein beliebiges Bild in das Format, das Race into Space erwartet.

Das Spiel verlangt 8-Bit-Palettenbilder (PNG, Modus "P"). Bilder, die zusammen
auf einem Bildschirm erscheinen, teilen sich die Palette. Deshalb wird das neue
Bild immer auf die Palette des ORIGINALS reduziert.

Einzeln:  python3 bild_konvertieren.py MEIN_BILD.png images/main_menu.png
Stapel:   python3 bild_konvertieren.py --ordner EINGANG
          (Dateinamen wie das Ziel, aber mit '__' statt '/':
           images__main_menu.png  ->  images/main_menu.png)
"""
import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow fehlt. Bitte installieren: pip install pillow")

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HIER, "..", ".."))


def standard_ziel():
    """Custom-Ordner: $BARIS_CUSTOM, sonst der macOS-Standardort."""
    env = os.environ.get("BARIS_CUSTOM")
    if env:
        return env
    return os.path.expanduser("~/Library/Application Support/Race Into Space/custom")


def farbe_lesen(text):
    """'#FF00FF' oder 'FF00FF' -> (255, 0, 255)"""
    t = text.lstrip("#")
    if len(t) != 6:
        raise ValueError("Farbe muss im Format #RRGGBB angegeben werden: %s" % text)
    return tuple(int(t[i:i + 2], 16) for i in (0, 2, 4))


def transparente_indizes(original):
    """Palettenindizes, die im Original transparent sind (aus tRNS)."""
    t = original.info.get("transparency")
    if t is None:
        return set()
    if isinstance(t, int):
        return {t}
    return {i for i, alpha in enumerate(t) if alpha < 128}


def freistellen(bild, farbe, toleranz):
    """Setzt alle Pixel nahe der Hintergrundfarbe auf alpha=0 (RGB-Abstand <= toleranz)."""
    px = bild.load()
    grenze = toleranz * toleranz
    for y in range(bild.height):
        for x in range(bild.width):
            r, g, b, a = px[x, y]
            d = (r - farbe[0]) ** 2 + (g - farbe[1]) ** 2 + (b - farbe[2]) ** 2
            if d <= grenze:
                px[x, y] = (r, g, b, 0)
    return bild


def skalieren(bild, groesse, modus):
    """Skaliert RGBA-Bild auf 'groesse'. Alpha wird vormultipliziert behandelt,
    damit an transparenten Raendern keine Farbsaeume entstehen."""
    zb, zh = groesse
    bild = bild.convert("RGBa")  # vormultipliziert
    if modus == "strecken":
        out = bild.resize((zb, zh), Image.LANCZOS)
    else:
        faktor_b, faktor_h = zb / bild.width, zh / bild.height
        f = max(faktor_b, faktor_h) if modus == "fuellen" else min(faktor_b, faktor_h)
        nb, nh = max(1, round(bild.width * f)), max(1, round(bild.height * f))
        klein = bild.resize((nb, nh), Image.LANCZOS)
        out = Image.new("RGBa", (zb, zh), (0, 0, 0, 0))  # transparenter Untergrund
        # zentriert einsetzen; bei "fuellen" wird dabei ueberstehendes abgeschnitten
        out.paste(klein, ((zb - nb) // 2, (zh - nh) // 2))
    return out.convert("RGBA")


def palette_vorbereiten(original, transparent):
    """Baut ein Palettenbild nur aus den NICHT transparenten Originalfarben.

    Rueckgabe: (palettenbild, abbildung), wobei abbildung[i] der echte
    Originalindex zum Index i des Palettenbilds ist.
    """
    pal = original.getpalette() or []
    pal += [0] * (768 - len(pal))
    n = len(original.getpalette()) // 3
    erlaubt = [i for i in range(n) if i not in transparent] or [0]
    flach, abbildung = [], []
    for i in range(256):
        # nicht belegte Plaetze mit einer echten Farbe auffuellen (nicht mit Schwarz)
        orig_i = erlaubt[i] if i < len(erlaubt) else erlaubt[0]
        flach += pal[orig_i * 3:orig_i * 3 + 3]
        abbildung.append(orig_i)
    pbild = Image.new("P", (1, 1))
    pbild.putpalette(flach)
    return pbild, abbildung


def konvertieren(eingabe, ziel_rel, daten, ziel_dir, modus, dither, frei_farbe, toleranz):
    original_pfad = os.path.join(daten, ziel_rel)
    if not os.path.isfile(original_pfad):
        raise SystemExit("Original nicht gefunden: %s\n(Pfad relativ zum data-Ordner, z.B. "
                         "images/main_menu.png; Ordner ggf. mit --daten angeben)" % original_pfad)
    original = Image.open(original_pfad)
    if original.mode != "P":
        raise SystemExit("Das Original ist kein Palettenbild (Modus %s): %s" % (original.mode, ziel_rel))
    groesse = original.size
    transparent = transparente_indizes(original)

    bild = Image.open(eingabe).convert("RGBA")
    if frei_farbe is not None:
        freistellen(bild, frei_farbe, toleranz)
    hat_alpha = bild.getchannel("A").getextrema()[0] < 255

    if modus is None:  # automatische Wahl
        modus = "einpassen" if (transparent or hat_alpha) else "fuellen"

    bild = skalieren(bild, groesse, modus)
    alpha = bild.getchannel("A")

    # Quantisieren auf die Originalpalette (nur nicht transparente Farben)
    pbild, abbildung = palette_vorbereiten(original, transparent)
    rgb = Image.new("RGB", groesse, (0, 0, 0))
    rgb.paste(bild.convert("RGB"), mask=None)
    d = Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE
    q = rgb.quantize(palette=pbild, dither=d)
    # Indizes zurueck auf die Originalindizes abbilden
    daten_q = [abbildung[i] for i in q.tobytes()]
    ersatz = min(transparent) if transparent else None
    anzahl_transparent = 0
    if ersatz is not None:
        a = list(alpha.tobytes())
        for k, wert in enumerate(a):
            if wert < 128:
                daten_q[k] = ersatz
                anzahl_transparent += 1
    ausgabe = Image.new("P", groesse)
    ausgabe.putdata(daten_q)
    ausgabe.putpalette(original.getpalette())

    # Speichern (Original-tRNS beibehalten)
    ziel_datei = os.path.join(ziel_dir, ziel_rel)
    os.makedirs(os.path.dirname(ziel_datei), exist_ok=True)
    opts = {}
    if "transparency" in original.info:
        opts["transparency"] = original.info["transparency"]
    ausgabe.save(ziel_datei, **opts)

    # Vorschau als RGB (Transparenz auf Schachbrett) neben die Eingabedatei
    vorschau = ausgabe.convert("RGBA")
    if ersatz is not None:
        va = Image.new("L", groesse, 255)
        va.putdata([0 if i in transparent else 255 for i in daten_q])
        vorschau.putalpha(va)
    schach = Image.new("RGB", groesse)
    sp = schach.load()
    for y in range(groesse[1]):
        for x in range(groesse[0]):
            sp[x, y] = (200, 200, 200) if ((x // 8 + y // 8) % 2 == 0) else (150, 150, 150)
    schach.paste(vorschau, mask=vorschau.getchannel("A"))
    vorschau_pfad = os.path.splitext(eingabe)[0] + ".vorschau.png"
    schach.save(vorschau_pfad)

    benutzt = len(set(daten_q))
    print("OK: %s -> %s" % (eingabe, ziel_datei))
    print("    Groesse %dx%d, Modus '%s', %d von %d Palettenfarben benutzt, %d transparente Pixel"
          % (groesse[0], groesse[1], modus, benutzt, len(original.getpalette()) // 3,
             anzahl_transparent))
    print("    Vorschau: %s" % vorschau_pfad)


def main():
    ap = argparse.ArgumentParser(
        description="Konvertiert ein Bild passend zu einem Original aus data/images "
                    "(Originalgroesse, Originalpalette, Transparenz).")
    ap.add_argument("eingabe", nargs="?", help="Eigenes Bild (PNG/JPG/...)")
    ap.add_argument("ziel", nargs="?", help="Zielpfad relativ zu data/, z.B. images/main_menu.png")
    ap.add_argument("--ordner", help="Stapelmodus: Ordner mit Bildern, deren Dateiname dem Zielpfad "
                    "entspricht, wobei '/' durch '__' ersetzt ist (images__main_menu.png)")
    ap.add_argument("--daten", default=os.path.join(REPO, "data"), help="Pfad zum data-Ordner")
    ap.add_argument("--ziel-ordner", "--ziel", dest="ziel_dir", default=standard_ziel(),
                    help="Custom-Ordner (Standard: $BARIS_CUSTOM oder macOS-Standardort)")
    ap.add_argument("--modus", choices=["fuellen", "einpassen", "strecken"], default=None,
                    help="Skalierung. Standard: 'einpassen' bei Transparenz, sonst 'fuellen'")
    ap.add_argument("--dither", action="store_true", help="Floyd-Steinberg-Dithering verwenden")
    ap.add_argument("--freistellen", nargs="?", const="#FF00FF", default=None, metavar="FARBE",
                    help="Hintergrundfarbe (Standard #FF00FF, Magenta) transparent machen")
    ap.add_argument("--toleranz", type=float, default=60,
                    help="Toleranz fuer --freistellen (RGB-Abstand, Standard 60)")
    a = ap.parse_args()

    frei = farbe_lesen(a.freistellen) if a.freistellen else None
    if not os.path.isdir(a.daten):
        sys.exit("Datenordner nicht gefunden: %s (bitte --daten angeben)" % a.daten)

    if a.ordner:
        dateien = sorted(f for f in os.listdir(a.ordner)
                         if f.lower().endswith((".png", ".jpg", ".jpeg")) and ".vorschau." not in f)
        if not dateien:
            sys.exit("Keine Bilder in %s gefunden." % a.ordner)
        for f in dateien:
            if "__" not in f:
                print("UEBERSPRUNGEN (%s): Dateiname ohne '__' (erwartet z.B. images__main_menu.png)" % f)
                continue
            ziel = os.path.splitext(f)[0].replace("__", "/") + ".png"
            try:
                konvertieren(os.path.join(a.ordner, f), ziel, a.daten, a.ziel_dir,
                             a.modus, a.dither, frei, a.toleranz)
            except SystemExit as e:
                print("UEBERSPRUNGEN (%s): %s" % (f, e))
    else:
        if not a.eingabe or not a.ziel:
            ap.error("Bitte EINGABE und ZIEL angeben (oder --ordner nutzen).")
        konvertieren(a.eingabe, a.ziel, a.daten, a.ziel_dir, a.modus, a.dither, frei, a.toleranz)


if __name__ == "__main__":
    main()
