#!/usr/bin/env python3
"""Konvertiert ein beliebiges Video in ein Spiel-Video (Theora, ohne Ton) und
extrahiert optional den Ton als Vorbis-Datei (Stereo, 44,1 kHz).

Aufruf:
  python3 video_konvertieren.py QUELLE.mp4 mission/apol_01.ogg [--start SEK] [--dauer SEK]
                                [--ton AUDIO_ZIELNAME] [--daten PFAD] [--ziel CUSTOM_ORDNER]

Aufloesung und Bildrate werden vom Original uebernommen (ffprobe).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HIER, "..", ".."))


def standard_ziel():
    """Custom-Ordner: $BARIS_CUSTOM, sonst der macOS-Standardort."""
    return os.environ.get("BARIS_CUSTOM") or os.path.expanduser(
        "~/Library/Application Support/Race Into Space/custom")


def werkzeuge_pruefen():
    """Stellt sicher, dass ffmpeg/ffprobe vorhanden sind und Theora/Vorbis koennen."""
    for tool in ("ffmpeg", "ffprobe"):
        if shutil.which(tool) is None:
            sys.exit("%s nicht gefunden. Auf dem Mac installieren mit: brew install ffmpeg" % tool)
    out = subprocess.run(["ffmpeg", "-hide_banner", "-encoders"], capture_output=True, text=True).stdout
    if "libtheora" not in out:
        sys.exit("Dein ffmpeg wurde ohne libtheora gebaut, kann also kein Theora-Video schreiben.\n"
                 "Auf dem Mac hilft normalerweise die Homebrew-Version: brew install ffmpeg\n"
                 "(pruefen mit: ffmpeg -encoders | grep theora)")
    if "libvorbis" not in out and " vorbis " not in out:
        print("Hinweis: Dein ffmpeg hat keinen Vorbis-Encoder; Ton-Export (--ton) wird nicht gehen.")


def sonde(pfad):
    """Liest Video-Eigenschaften mit ffprobe: (breite, hoehe, fps als Bruch, dauer)."""
    r = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                        "stream=width,height,r_frame_rate:format=duration", "-of", "json", pfad],
                       capture_output=True, text=True)
    try:
        j = json.loads(r.stdout)
        s = j["streams"][0]
        return s["width"], s["height"], s["r_frame_rate"], float(j["format"]["duration"])
    except (KeyError, IndexError, ValueError):
        sys.exit("Konnte %s nicht als Video lesen." % pfad)


def dauer_lesen(pfad):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration",
                        "-of", "csv=p=0", pfad], capture_output=True, text=True)
    try:
        return float(r.stdout.strip())
    except ValueError:
        return None


def ffmpeg(args):
    r = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"] + args,
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("ffmpeg ist fehlgeschlagen:\n" + r.stderr)


def main():
    ap = argparse.ArgumentParser(description="Konvertiert ein Video fuer Race into Space.")
    ap.add_argument("quelle", help="Eigenes Video (mp4, mov, ...)")
    ap.add_argument("ziel", help="Zielpfad unter data/video, z.B. mission/apol_01.ogg")
    ap.add_argument("--start", type=float, default=0, help="Startzeit in der Quelle (Sekunden)")
    ap.add_argument("--dauer", type=float, default=None, help="Laenge des Ausschnitts (Sekunden)")
    ap.add_argument("--ton", metavar="AUDIO_ZIELNAME",
                    help="Ton zusaetzlich als Vorbis nach audio/mission/NAME.ogg schreiben "
                         "(oder mit Unterordner, z.B. news/sov_000)")
    ap.add_argument("--daten", default=os.path.join(REPO, "data"), help="Pfad zum data-Ordner")
    ap.add_argument("--ziel-ordner", "--ziel-custom", dest="ziel_dir", default=standard_ziel(),
                    help="Custom-Ordner (Standard: $BARIS_CUSTOM oder macOS-Standardort)")
    a = ap.parse_args()

    werkzeuge_pruefen()
    if not os.path.isfile(a.quelle):
        sys.exit("Quelldatei nicht gefunden: %s" % a.quelle)

    rel = a.ziel[len("video/"):] if a.ziel.startswith("video/") else a.ziel
    if not rel.lower().endswith(".ogg"):
        rel += ".ogg"
    original = os.path.join(a.daten, "video", rel)
    if not os.path.isfile(original):
        sys.exit("Originalvideo nicht gefunden: %s\n(Zielpfad z.B. mission/apol_01.ogg; "
                 "Datenordner ggf. mit --daten angeben)" % original)
    breite, hoehe, fps, orig_dauer = sonde(original)
    print("Original: %dx%d, %s fps, %.1f s" % (breite, hoehe, fps, orig_dauer))

    # Bild: mittig auf das Zielseitenverhaeltnis zuschneiden, skalieren, Bildrate setzen
    asp = "%d/%d" % (breite, hoehe)
    filt = ("crop='if(gt(iw/ih,{a}),ih*{a},iw)':'if(gt(iw/ih,{a}),ih,iw/{a})',"
            "scale={w}:{h}:flags=lanczos,fps={fps},format=yuv420p").format(
                a=asp, w=breite, h=hoehe, fps=fps)
    zeit = ["-ss", str(a.start)] if a.start else []
    dauer = ["-t", str(a.dauer)] if a.dauer else []

    ziel_video = os.path.join(a.ziel_dir, "video", rel)
    os.makedirs(os.path.dirname(ziel_video), exist_ok=True)
    ffmpeg(zeit + dauer + ["-i", a.quelle, "-vf", filt, "-an", "-c:v", "libtheora", "-q:v", "7",
                           ziel_video])
    neu_dauer = dauer_lesen(ziel_video)
    print("Video geschrieben: %s (%.1f s)" % (ziel_video, neu_dauer or 0))
    if neu_dauer and (neu_dauer > orig_dauer * 1.5 or neu_dauer < orig_dauer / 1.5):
        print("WARNUNG: Neue Laenge (%.1f s) weicht stark vom Original (%.1f s) ab. "
              "Ton und Bild der Mission passen dann evtl. nicht mehr zusammen (--dauer pruefen)."
              % (neu_dauer, orig_dauer))

    if a.ton:
        name = a.ton[:-4] if a.ton.lower().endswith(".ogg") else a.ton
        if "/" not in name:
            name = "mission/" + name
        ziel_ton = os.path.join(a.ziel_dir, "audio", name + ".ogg")
        os.makedirs(os.path.dirname(ziel_ton), exist_ok=True)
        ffmpeg(zeit + dauer + ["-i", a.quelle, "-vn", "-c:a", "libvorbis", "-ar", "44100",
                               "-ac", "2", "-q:a", "4", ziel_ton])
        print("Ton geschrieben: %s" % ziel_ton)


if __name__ == "__main__":
    main()
