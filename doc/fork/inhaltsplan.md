# Inhaltsplan – Programme und Hardware je Ausbaustufe

Stand: 22.11.2026 · Status: **Entwurf zur Freigabe**
Fakten mit ⚠ sind aus Modellwissen und werden vor der Umsetzung gegen Primärquellen geprüft
(wie in `recherche-mercury.md`).

---

## 1. Gewünschte Programme und Zuordnung

| Programm | Seite | Zeitraum (historisch) | Kurzbeschreibung | Ausbaustufe |
|---|---|---|---|---|
| Redstone, Jupiter-C/Juno I | US | ab 1953 / 1956–58 | Kurzstreckenrakete, Basis für Explorer 1 und Mercury-Redstone | 1 |
| Vanguard (Rakete + Satellit) | US | 1957–59 | Navy-Programm, erster Startversuch Dez. 1957 fehlgeschlagen, Vanguard 1 im März 1958 | 1 |
| Juno II, Thor-Able, Thor-Agena, Atlas, Atlas-Agena, Scout | US | 1958–63 | weitere Träger für Satelliten, Sonden und Mercury | 1 |
| Explorer, Pioneer, Ranger, Mariner u. a. | US | 1958–63 | Satelliten und Sonden | 1 |
| Mercury inkl. nicht geflogener Missionen | US | 1958–63 | MR-5/6, MA-10 als spielbare Optionen | 1 |
| **X-15** | US | 1959–68 ⚠ | Raketenflugzeug, Abwurf von einer B-52, suborbitale Flüge; zwei Flüge über 100 km (1963) ⚠ | **1** |
| **X-20 Dyna-Soar** | US | 1957–63 ⚠ | Raumgleiter der Air Force auf Titan-Rakete, im Dez. 1963 gestrichen, nie geflogen ⚠ | **1** als Forschungsprogramm, Flüge ab Stufe 2 |
| Gemini, Titan II | US | 1962–66 | | 2 |
| **Manned Orbiting Laboratory (MOL)** | US | 1963–69 ⚠ | Air-Force-Station: Gemini-B-Kapsel mit Labor auf Titan III, ein unbemannter Testflug 1966, 1969 gestrichen ⚠ | **2** |
| Apollo, Saturn IB/V | US | 1961–75 | | 3 |
| **Apollo Applications Program (AAP)** | US | 1965–73 ⚠ | Nachfolgenutzung der Apollo-Hardware, daraus entstand Skylab ⚠ | **3** |
| **Skylab** | US | 1973–79 ⚠ | Raumstation aus einer Saturn-Oberstufe, drei Besatzungen 1973/74 ⚠ | **3** |
| Wostok, Woschod, Sojus, N1/L3 | SV | 1961–74 | sowjetische Seite (in Stufe 1 noch klassisch) | 2–3 |
| **Saljut 1–7** (inkl. Almaz) | SV | 1971–91 ⚠ | Raumstationen, teils militärisch (Almaz) ⚠ | **3** |
| **MiG-105 „Spiral“** | SV | 1965–78 ⚠ | sowjetischer Raumgleiter, nur atmosphärische Testflüge ⚠ | **3** |

## 2. Folgen für den Hardware-Katalog (Etappe 4c)

Damit diese Programme später ohne erneuten Umbau passen, nimmt Schritt 4c folgende Punkte
in das Datenmodell auf:

| Bedarf | Programme | Katalog |
|---|---|---|
| Raumgleiter mit Landung auf einer Piste, Fluggerät wird nicht verbraucht | X-15, Dyna-Soar, MiG-105 | Rolle `spaceplane`, `consumed_on_flight: false`, Lebensdauer in Flügen |
| Start ohne Rakete (Abwurf vom Trägerflugzeug) | X-15, MiG-105 (Testflüge) | `launch_mode: air_launch`, Trägerflugzeug als Infrastruktur statt Rakete |
| Start auf einer Rakete | Dyna-Soar | `launch_mode: rocket` (Standard) |
| Labor an einer Kapsel | MOL (Gemini B + Labor) | Rolle `lab_module`, Paarung wie beim Mondlander (`compatible_labs`) |
| Raumstation als dauerhaftes Objekt im Orbit | Skylab, Saljut, MOL | Rolle `station`, Lebensdauer, Crew-Plätze, Andockstellen; **Spielzustand „Objekte im Orbit“** |
| neue Missionsarten | alle | z. B. „Raketenflugzeug-Höhenflug“, „Raumgleiter-Orbitalflug“, „Station starten“, „Crew zur Station“, „Langzeitaufenthalt“ – Missionsdaten ab Etappe 5 |
| neue Prestige-Kategorien | z. B. erster geflügelter Raumflug, erste Raumstation | Prestige-Liste wird mit 4c erweiterbar |

Die **Station als dauerhaftes Objekt** ist der aufwendigste Teil (eigener Spielzustand,
Versorgung, Alterung). Er gehört inhaltlich zu Etappe 10; 4c legt nur die Datenstruktur an.

## 3. Ausbaustufe 1 nach dieser Ergänzung

Zeitraum 1955–1963, USA:

- Träger: Redstone, Jupiter-C/Juno I, Vanguard, Juno II, Thor-Able, Thor-Agena, Atlas, Atlas-Agena, Scout
- Satelliten und Sonden: Explorer, Vanguard, Pioneer, Ranger, Mariner (Venus)
- Bemannt: Mercury (inkl. MR-5/6, MA-10), **X-15** (Höhenflüge bis über 100 km)
- Forschung: **Dyna-Soar** (erforschbar; Flüge erst, wenn das Spiel über 1963 hinaus verlängert wird)
- UdSSR: klassische Hardware

## 4. Entscheidungen

1. Zuordnung der Programme zu den Ausbaustufen wie in Abschnitt 1?
2. X-15 bereits in Ausbaustufe 1? *Empfehlung: ja* (passt zeitlich, eigene Mechanik „Raketenflugzeug“ ist überschaubar).
3. Dyna-Soar in Stufe 1 nur als Forschungsprogramm, Flüge erst bei verlängertem Spiel? *Empfehlung: ja.*
