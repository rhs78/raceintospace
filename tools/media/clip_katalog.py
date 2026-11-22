#!/usr/bin/env python3
"""Erzeugt einen Excel-Katalog aller Missionsclips, Videos und Bilder.

Aufruf:  python3 clip_katalog.py [--daten PFAD_ZU_data] [--quelle PFAD_ZU_src]
                                 [--ausgabe doc/fork/clip-katalog.xlsx]
"""
import argparse
import json
import os
import re
import subprocess
import sys

try:
    from openpyxl import Workbook
    from openpyxl.styles import Alignment, Font, PatternFill
    from openpyxl.utils import get_column_letter
    from PIL import Image
except ImportError:
    sys.exit("Fehlende Pakete. Bitte installieren: pip install pillow openpyxl")

HIER = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HIER, "..", ".."))

# Schrittnamen: Buchstabe 'A' + Index (siehe S_Name in src/game/game_main.cpp)
SCHRITTE = [
    "START", "ORBITAL-EINSCHUSSBRENNEN", "HARDWARE-EINSCHALTEN", "WIEDEREINTRITT",
    "BERGUNG/LANDUNG", "ERDORBITALE AKTIVITAETEN", "ERD-ABBREMSBRENNEN", "ORBITALE EVA",
    "KOPPLUNG", "TRANS-LUNARE INJEKTION", "TRANS-ERD-INJEKTION", "MONDORBIT-EINSCHUSSBRENNEN",
    "MONDORBITALE AKTIVITAETEN", "ERD-KURSKORREKTUR", "MOND-KURSKORREKTUR", "MOND-EVA",
    "LANDEFAEHRE-AKTIVITAETEN", "MOND-ABBREMSBRENNEN", "MONDLANDUNG", "MONDSTART",
    "FOTOAUFKLAERUNG", "PLANETEN-ORBITBRENNEN", "PLANETARE AKTIVITAETEN", "ERD-RENDEZVOUS",
    "MOND-RENDEZVOUS", "TRANS-PLANETARE INJEKTION", "LANDEFAEHRE-SCHUBTEST", "DAUER",
    "GEMEINSAME DAUER", "WIEDEREINTRITTSKORRIDOR", "KURSKORREKTURBRENNEN",
    "KURSKORREKTURBRENNEN (2)", "ERD-ORBITEINSCHUSSBRENNEN",
]
# Sonderzeichen / Kleinbuchstaben (Spezialfaelle im Code)
SONDER = {"#": "START (Startsynchronisation)", "h": "LM-Aktivitaeten (Sonderfall)",
          "i": "LM-Aktivitaeten (Sonderfall)"}
SEITEN = {"U": "USA", "S": "UdSSR"}

# Besondere Schluessel ohne Schrittbuchstaben (siehe PlaySequence / mis_m.cpp)
SPEZIAL = {
    "DEFAULT": "Standardclip (Fallback)", "PAR": "Fallschirm", "FUN": "Spezial (UFUN/SFUN)",
    "TCP": "Training (CP)", "TLM": "Training (LM)", "TEV": "Training (EVA)",
    "TDO": "Training (Docking)", "TDU": "Training (Dauer)", "MOON": "Mond (HMOON)",
}


def lade_json(pfad):
    with open(pfad, encoding="utf-8") as f:
        return json.load(f)


def hardware_tabelle(daten):
    """Liefert {(Seite 'U'/'S', ID 'C2'): 'APOLLO'} aus dem Hardware-Katalog."""
    katalog = lade_json(os.path.join(daten, "gamedata", "hardware.json"))
    tab = {}
    for eintrag in katalog.get("hardware", []):
        # Testeintraege gehoeren nicht ins Spiel
        if eintrag.get("test_only"):
            continue
        seite = "U" if eintrag.get("side") == "US" else "S"
        kennung = eintrag.get("anim_id", "")
        if kennung:
            tab[(seite, kennung)] = eintrag.get("name", "")
    return tab


def schluessel_dekodieren(key, hw, ist_fehler):
    """Zerlegt den Schluessel (ohne die 3 Praefixzeichen) in Schritt, Seite, Hardware.

    Gibt (schrittname, seite, hardware, fehlernummer) zurueck.
    """
    fehler = ""
    k = key
    if ist_fehler and len(k) >= 2 and k[-2:].isdigit():
        fehler = k[-2:]
        k = k[:-2]
    if len(k) >= 2 and k[1] in SEITEN:
        buchstabe = k[0]
        if "A" <= buchstabe <= "Z":
            schritt = "%s (%s)" % (SCHRITTE[ord(buchstabe) - 65]
                                   if ord(buchstabe) - 65 < len(SCHRITTE) else "?", buchstabe)
        else:
            schritt = "%s (%s)" % (SONDER.get(buchstabe, "Sonderschritt"), buchstabe)
        seite = SEITEN[k[1]]
        kennung = k[2:4]
        name = hw.get((k[1], kennung), "")
        if kennung and not name:
            name = "? (%s)" % kennung
        elif name:
            name = "%s (%s)" % (name, kennung)
        return schritt, seite, name, fehler
    # Kein Schrittbuchstabe: Spezialschluessel wie UPAR, SFUN, DEFAULT
    seite = SEITEN.get(k[:1], "") if len(k) > 1 and k[:1] in SEITEN else ""
    rest = k[1:] if seite else k
    return SPEZIAL.get(rest, "Sonderschluessel (%s)" % k), seite, "", fehler


def dateien_indexieren(daten, unterordner):
    """{kleingeschriebener Dateistamm: [relative Pfade]} fuer .ogg in daten/<unterordner>/**"""
    idx = {}
    for wurzel, _, dateien in os.walk(os.path.join(daten, unterordner)):
        for d in dateien:
            if d.lower().endswith(".ogg"):
                rel = os.path.relpath(os.path.join(wurzel, d), daten).replace(os.sep, "/")
                idx.setdefault(os.path.splitext(d)[0].lower(), []).append(rel)
    return idx


def ffprobe(pfad):
    """Liest Breite, Hoehe, fps, Dauer. Fehler -> leere Werte."""
    try:
        out = subprocess.run(
            ["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
             "stream=width,height,r_frame_rate:format=duration", "-of", "json", pfad],
            capture_output=True, text=True, check=True).stdout
        j = json.loads(out)
        s = j["streams"][0]
        z, n = s["r_frame_rate"].split("/")
        return s["width"], s["height"], round(float(z) / float(n), 2), round(float(j["format"]["duration"]), 2)
    except Exception:
        return "", "", "", ""


def clips_lesen(daten, hw, vid_idx, aud_idx):
    zeilen = []
    nutzung = {}  # Videostamm -> Liste von Schluesseln
    for datei, fehler in (("seq.json", False), ("fseq.json", True)):
        eintraege = lade_json(os.path.join(daten, "gamedata", datei))["value0"]
        for e in eintraege:
            sid = e["MissionIdSequence"]
            if sid.startswith("XXXX"):   # Endmarker der Liste, kein echter Clip
                continue
            if sid[:3].isdigit():
                varianten, anzahl, weiblich, key = sid[0], int(sid[1]), sid[2], sid[3:]
                schritt, seite, hardware, fnr = schluessel_dekodieren(key, hw, fehler)
            else:   # Trainingsclips (UTCP, STLM, ...): ohne Zaehlerpraefix
                varianten, anzahl, weiblich, key = "", 1, "", sid
                schritt, seite, hardware, fnr = "Training (%s)" % sid[2:], SEITEN[sid[0]], "", ""
            gruppe = e.get("MissionStep", "")
            for v in e["video"][:anzahl]:
                if v != "NONE":
                    nutzung.setdefault(v.lower(), []).append(("F:" if fehler else "E:") + key)
            videos = [v for v in e["video"][:anzahl] if v != "NONE"]
            audios = [a for a in e["audio"][:anzahl] if a != "NONE"]
            vpfade = [(vid_idx.get(v.lower()) or ["FEHLT:" + v])[0] for v in videos]
            apfade = [(aud_idx.get(a.lower()) or ["FEHLT:" + a])[0] for a in audios]
            zeilen.append({
                "typ": "Fehlschlag" if fehler else "Erfolg", "sid": sid, "key": key,
                "varianten": varianten, "weiblich": "ja" if weiblich == "1" else "",
                "schritt": schritt, "seite": seite, "hw": hardware, "gruppe": gruppe,
                "fnr": fnr, "videos": vpfade, "audios": apfade,
            })
    return zeilen, nutzung


def fehlertexte(daten):
    """{(MissionStep, fail-Nummer als 2-stellig): [Texte]}"""
    fails = lade_json(os.path.join(daten, "gamedata", "fails.json"))["value0"]
    tab = {}
    for f in fails:
        tab.setdefault((f["MissionStep"], "%02d" % f["fail"]), []).append(f["text"])
    return tab


def sheet_formatieren(ws, breiten):
    kopf = PatternFill("solid", fgColor="DDDDDD")
    for c in ws[1]:
        c.font = Font(bold=True)
        c.fill = kopf
        c.alignment = Alignment(wrap_text=True, vertical="top")
    for i, b in enumerate(breiten, 1):
        ws.column_dimensions[get_column_letter(i)].width = b
    ws.freeze_panes = "A2"
    ws.auto_filter.ref = ws.dimensions
    ws.page_setup.orientation = "landscape"
    ws.page_setup.fitToWidth = 1
    ws.page_setup.fitToHeight = 0
    ws.sheet_properties.pageSetUpPr.fitToPage = True
    ws.print_title_rows = "1:1"


def kuerzen(liste, maximal=12):
    text = ", ".join(liste[:maximal])
    return text + ", ... (+%d)" % (len(liste) - maximal) if len(liste) > maximal else text


def bildreferenzen(quelle):
    """Liest alle Quelldateien einmal ein: {relativer Pfad: Text}"""
    texte = {}
    # Quellcode und Spieldaten (gamedata/*.json) durchsuchen
    ordner = [quelle, os.path.join(os.path.dirname(os.path.dirname(quelle)), "data", "gamedata")]
    for wurzel, _, dateien in (x for o in ordner for x in os.walk(o)):
        for d in dateien:
            if d.endswith((".cpp", ".h", ".c", ".hpp", ".json")):
                p = os.path.join(wurzel, d)
                try:
                    with open(p, encoding="utf-8", errors="ignore") as f:
                        texte[os.path.relpath(p, quelle)] = f.read()
                except OSError:
                    pass
    return texte


def main():
    ap = argparse.ArgumentParser(description="Erzeugt den Clip-/Bild-Katalog als Excel-Datei.")
    ap.add_argument("--daten", default=os.path.join(REPO, "data"), help="Pfad zum data-Ordner")
    ap.add_argument("--quelle", default=os.path.join(REPO, "src", "game"),
                    help="Pfad zum Quellcode (fuer Bildverweise)")
    ap.add_argument("--ausgabe", default=os.path.join(REPO, "doc", "fork", "clip-katalog.xlsx"),
                    help="Ausgabedatei (.xlsx)")
    a = ap.parse_args()

    hw = hardware_tabelle(a.daten)
    vid_idx = dateien_indexieren(a.daten, "video")
    aud_idx = dateien_indexieren(a.daten, "audio")
    zeilen, nutzung = clips_lesen(a.daten, hw, vid_idx, aud_idx)
    ftexte = fehlertexte(a.daten)

    wb = Workbook()
    # --- Blatt 1: Missionsclips ---
    ws = wb.active
    ws.title = "Missionsclips"
    ws.append(["Typ", "Sequenz-ID", "Schluessel", "Varianten", "Weiblich", "Schritt", "Seite",
               "Hardware/Programm", "Fehlergruppe", "Fehler-Nr.", "Fehlertext", "Videodateien",
               "Audiodateien", "Alle Dateien vorhanden"])
    for z in zeilen:
        text = " | ".join(ftexte.get((z["gruppe"], z["fnr"]), [])) if z["typ"] == "Fehlschlag" else ""
        fehlt = any(p.startswith("FEHLT:") for p in z["videos"] + z["audios"])
        ws.append([z["typ"], z["sid"], z["key"], z["varianten"], z["weiblich"], z["schritt"],
                   z["seite"], z["hw"], z["gruppe"], z["fnr"], text, "\n".join(z["videos"]),
                   "\n".join(z["audios"]), "nein" if fehlt else "ja"])
    sheet_formatieren(ws, [11, 14, 12, 9, 9, 34, 9, 20, 12, 9, 50, 28, 28, 12])
    for r in ws.iter_rows(min_row=2):
        for c in r:
            c.alignment = Alignment(wrap_text=True, vertical="top")

    # --- Blatt 2: Videodateien ---
    ws = wb.create_sheet("Videodateien")
    ws.append(["Ordner", "Datei", "Breite", "Hoehe", "fps", "Dauer (s)", "Anzahl Sequenzen",
               "Sequenzen (E: Erfolg, F: Fehlschlag)", "Ersetzt (custom)"])
    for wurzel, _, dateien in sorted(os.walk(os.path.join(a.daten, "video"))):
        for d in sorted(dateien):
            if not d.lower().endswith(".ogg"):
                continue
            w, h, fps, dauer = ffprobe(os.path.join(wurzel, d))
            ordner = os.path.relpath(wurzel, a.daten).replace(os.sep, "/")
            keys = nutzung.get(os.path.splitext(d)[0].lower(), [])
            ws.append([ordner, d, w, h, fps, dauer, len(keys), kuerzen(sorted(set(keys))), ""])
    sheet_formatieren(ws, [16, 22, 8, 8, 8, 10, 12, 70, 16])

    # --- Blatt 3: Bilder ---
    ws = wb.create_sheet("Bilder")
    ws.append(["Pfad", "Breite", "Hoehe", "Modus", "Palettenfarben", "Transparenz",
               "Verwendet in (src/game)", "Ersetzt (custom)"])
    texte = bildreferenzen(a.quelle)
    bilder = []
    for wurzel, _, dateien in os.walk(os.path.join(a.daten, "images")):
        for d in dateien:
            if d.lower().endswith(".png"):
                bilder.append(os.path.relpath(os.path.join(wurzel, d), a.daten).replace(os.sep, "/"))
    cache = {}
    for rel in sorted(bilder):
        with Image.open(os.path.join(a.daten, rel)) as im:
            breite, hoehe = im.size
            modus = im.mode
            pal = im.getpalette()
            farben = len(pal) // 3 if pal else ""
            trans = "ja" if "transparency" in im.info else "nein"
        # Dateinamen schrittweise kuerzen (hintere ".xyz"-Teile abschneiden) und im
        # Quelltext suchen; der laengste Treffer gewinnt (Naeherung, daher "(?)")
        stamm = rel[:-4]
        treffer = []
        while True:
            if stamm not in cache:
                cache[stamm] = sorted(f for f, t in texte.items() if stamm in t)
            if cache[stamm]:
                treffer = cache[stamm]
                if stamm != rel[:-4]:
                    treffer = [f + " (?)" for f in treffer]
                break
            if "." not in os.path.basename(stamm):
                break
            stamm = stamm.rsplit(".", 1)[0]
        ws.append([rel, breite, hoehe, modus, farben, trans, kuerzen(treffer, 6), ""])
    sheet_formatieren(ws, [38, 8, 8, 8, 10, 12, 50, 16])

    os.makedirs(os.path.dirname(a.ausgabe), exist_ok=True)
    wb.save(a.ausgabe)
    print("Katalog geschrieben: %s" % a.ausgabe)
    print("  %d Sequenzeintraege, %d Bilder" % (len(zeilen), len(bilder)))


if __name__ == "__main__":
    main()
