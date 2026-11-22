# Hardware-Katalog – Spezifikation (Etappe 4)

Stand: 22.11.2026 · Grundlagen: `architektur.md` (Abschnitt 2.2), Analysebericht Hardware (Etappe 0),
`recherche-mercury.md` · Status: **Entwurf zur Freigabe**

---

## 1. Ziel

Hardware und Programme werden **datengetrieben**. Statt vier fester Arrays mit je 7 Plätzen
(Sonden, Raketen, Kapseln/LM, Zusatzgeräte) beschreibt eine Katalogdatei beliebig viele Einträge
je Seite. Damit werden für Ausbaustufe 1 (Etappe 5) möglich:

- Raketen: Redstone, Jupiter-C/Juno I, Vanguard, Juno II, Thor-Able, Thor-Agena, Atlas (Mercury-Atlas), Atlas-Agena, Scout
- Satelliten/Sonden: Explorer, Vanguard, Pioneer, Ranger, Mariner
- Kapsel: Mercury mit realen Grenzen (z. B. Dauer), Grundlage für die nicht geflogenen Mercury-Missionen

**Nicht** Teil von Etappe 4: die historischen Inhalte selbst (Etappe 5), neue Grafiken, neue Missionstypen.

**Abnahmekriterium:** Nach Etappe 4 ist das Spiel im Szenario „Klassisch“ im Verhalten identisch
mit dem heutigen Stand – nachgewiesen durch die Tests aus Etappe 1 –, aber vollständig aus dem
Katalog gespeist. Ein zusätzlicher Testeintrag (z. B. eine fiktive 6. Rakete) muss sich
ohne Codeänderung erforschen, kaufen, zusammenbauen und starten lassen.

## 2. Ausgangslage (Kurzfassung)

| Problem | Wo |
|---|---|
| Identität = Arrayposition, abgeleitet aus `Equipment::ID[1]` | überall, ca. 40 Dateien |
| Raketen-Kodierung `Index+1`, `+4` für Booster | Missionen, Spielstand, Anzeigen |
| Sonden-Slot 4 = Docking-Modul | `vab.cpp`, `mc2.cpp` u. a. |
| Regeln fest im Code: Crewgröße, Missionseignung, LM-Paarung, Technologietransfer, Kicker | `hardware.cpp`, `crew.cpp`, `vab.cpp`, `rdplex.cpp`, `aipur.cpp` |
| genau 5 crewfähige Programme, an Raumhafengebäude und Prestigekategorien 13–17 gekoppelt | `port.cpp`, `ast*.cpp`, `prest.cpp` |
| Bild- und Animationsnamen aus Indexarithmetik (`rdfull.but.(plr*32+hw*8+unit)`, `Mev.Name` aus ID) | `place.cpp`, `mc2.cpp`, `seq.json`, `fseq.json`, `fails.json` |
| Bildschirme mit fester Zahl von Schaltflächen (5 Kapseln, 4 Kategorien) | `crew.cpp`, `rdplex.cpp`, `vab.cpp`, `ast4.cpp` |
| KI mit festen Indizes und Skripten | `aimis.cpp`, `aimast.cpp`, `aipur.cpp` |

## 3. Datenmodell

### 3.1 Katalogdatei `data/gamedata/hardware.json`

Ein Eintrag je Programm und Seite. Feldgruppen:

| Gruppe | Felder | Zweck |
|---|---|---|
| Identität | `id` (z. B. `"US.REDSTONE"`), `side` (`US`/`SV`), `category` (`probe`, `rocket`, `manned`, `misc`), `role`, `name`, `type_label` | stabile ID statt Arrayposition |
| Rolle | `role`: `orbital_probe`, `lunar_probe`, `interplanetary_probe`, `launcher`, `booster`, `capsule`, `minishuttle`, `lander`, `kicker`, `eva`, `docking_module`, `photo_recon` | ersetzt Indexabfragen im Code |
| Physik | `unit_weight`, `max_payload`, `crew`, `duration_level` (A–F), `reach` (`suborbital`, `orbital`, `translunar`, `interplanetary`) | Zusammenbau und Missionseignung |
| Fähigkeiten | `can_dock`, `direct_ascent`, `accepts_boosters`, `consumed_on_flight`, `compatible_landers`, `compatible_kickers`, `min_mission` | ersetzt Sonderregeln in `crew.cpp`, `vab.cpp` |
| Verfügbarkeit | `available_from` (Jahr/Saison), `requires` (Liste von IDs oder Meilensteinen), `obsolete_from` | historische Abfolge |
| Werte | `init_cost`, `unit_cost`, `rd_cost`, `base_safety`, `max_rd`, `max_safety`, `delay` | heute in `urast.json`/`hist.json` |
| Technologietransfer | `tech_transfer`: Liste `{from, factor}` | ersetzt handgeschriebene Formeln in `rdplex.cpp`/`aipur.cpp` |
| Prestige | `prestige_first` (Kategorie bei Erstflug, z. B. „One-Person“) | ersetzt `12 + Prog` |
| Darstellung | `anim_id` (2-stelliger Code für `seq.json`/`fseq.json`/`fails.json`, z. B. `"R0"`), `images` (Forschung, Start, VAB-Sprite, Missionsabzeichen-Basis, Geheimdienst), `help_id` | entkoppelt Grafik/Animation von der Position |
| KI | `ai_tier`, `ai_role_priority` | rollenbasierte KI |
| Klassisch | `legacy {cat, idx}` | Zuordnung zu den 21 Originaleinträgen (für Tests und vorhandene Grafiken) |

**Animationen und Grafiken:** Neue Einträge verweisen per `anim_id`/`images` auf vorhandene
Clips und Bilder (z. B. Redstone nutzt vorerst die Atlas-Clips). Eigene Clips/Bilder können später
über den Custom-Ordner oder neue Einträge in `seq.json` ergänzt werden.

**Beispiel** (Werte nur zur Veranschaulichung, die echten Werte folgen in Etappe 5):

```json
{
  "id": "US.REDSTONE",
  "side": "US",
  "category": "rocket",
  "role": "launcher",
  "name": "REDSTONE",
  "reach": "suborbital",
  "max_payload": 500,
  "accepts_boosters": false,
  "available_from": { "year": 55, "season": 0 },
  "requires": [],
  "init_cost": 12, "unit_cost": 2, "rd_cost": 1,
  "base_safety": 40, "max_rd": 85, "max_safety": 95,
  "tech_transfer": [],
  "anim_id": "R0",
  "images": { "rd": "rd/rdfull.but.0.png", "liftoff": "liftoff.abz.USR0.png" }
}
```

### 3.2 Szenarien `data/gamedata/scenarios/*.json`

Ein Szenario legt fest, welche Katalogeinträge im Spiel sind, und enthält Start- und
Endjahr sowie den Startzustand (bisher `urast.json`/`hist.json`).

| Szenario | Inhalt |
|---|---|
| `classic.json` | die 21 Originaleinträge je Seite, Start 1957 – entspricht dem heutigen Spiel |
| `historical.json` | ab Etappe 5: reale US-Programme 1955–1963, UdSSR vorerst klassisch |

Auswahl beim Spielstart (Einstellungsbildschirm), neben dem Startjahr.

### 3.3 Spielzustand

- `Equipment` je Spieler als **dynamische Liste** statt `Probe[7]`, `Rocket[7]`, `Manned[7]`, `Misc[7]`;
  jedes Element trägt seinen Katalogindex.
- Alle Tabellen, die heute an den Arrays hängen (`Buy`, `Spend`, `IntelHardwareTable`, Crew-Tabellen),
  werden über den Katalogindex adressiert.
- Missionen speichern Katalogindizes je Slot (`Capsule`, `Kicker`, `LM`, `Probe`, `DockingModule`,
  `Launcher`) und einen getrennten Booster-Schalter – keine `+1`/`+4`/„4 = DM“-Kodierungen mehr.
- Astronautenprogramme: jedes Katalogelement mit `crew > 0` ist ein Programm; Crews werden
  je Programm gespeichert (nicht mehr fest 5).
- Spielstandversion wird erhöht; alte Spielstände werden nicht übernommen (Entscheidung vom 21.11.2026).

### 3.4 Sitzplatz- und Größengrenzen

- Missionsschritte (`Mev.Name`) werden aus Schrittbuchstabe + Seite + `anim_id` gebildet –
  Animationsschlüssel bleiben dadurch kompatibel.
- Crewgröße pro Programm bis 7 (für spätere Stationen), Anzahl Programme pro Seite unbegrenzt.

## 4. Benutzeroberfläche

Bildschirme mit fester Schaltflächenzahl erhalten eine **Blätterfunktion** im vorhandenen Stil
(Pfeile wie in den Forschungs- und Kaufbildschirmen):

| Bildschirm | Änderung |
|---|---|
| Forschung / Kauf (`rdplex.cpp`) | vor/zurück über alle Einträge der Kategorie (heute schon vorhanden, aber mit festen Grenzen) |
| Zusammenbau VAB/VIB (`vab.cpp`) | Liste der Nutzlastkombinationen ohne 7er-Grenze, Raketenwahl mit Blättern |
| Kapselwahl bei Missionsplanung (`crew.cpp`) | Liste statt 5 fester Schaltflächen, gefiltert nach Missionseignung |
| Astronautenkomplex (`ast*.cpp`) | Programmliste mit Blättern |
| Raumhafen (`port.cpp`) | die bisherigen 5 Programmgebäude bleiben für die ersten 5 Programme; weitere über eine Liste im Astronautenkomplex |
| Hardware-Historie, Museum, Geheimdienst | Einträge ohne Grafik zeigen eine Ersatzgrafik |

Größere grafische Überarbeitungen der Bildschirme folgen erst mit Etappe 7.

## 5. Umsetzungsschritte

Jeder Schritt endet mit grünem Build, allen Tests und einem spielbaren Stand.

| Schritt | Inhalt | Aufwand |
|---|---|---|
| 4a | **Katalog lesen, nur beschreibend:** `hardware.json` mit den 21 Originaleinträgen je Seite, Loader, Tests „Katalog = heutige Werte“; Code liest Namen, Typtexte, Crewgröße, Hilfe-IDs und Bildnamen aus dem Katalog | M |
| 4b | **Regeln in Daten:** `HardwareType`, `CrewSize`, Missionseignung (`crew.cpp`), LM-/Kicker-Paarung (`vab.cpp`), Technologietransfer (eine Funktion statt zwei Kopien), Prestige-Zuordnung, Namensvergleiche (`"DO"`, Mondläufer) | M–L |
| 4c | **Dynamischer Spielzustand:** Listen statt Arrays, Spielstandversion, Startzustand aus Szenario statt `urast.json`/`hist.json`-Abschnitten | L |
| 4d | **Missionen auf Katalogindizes:** `MissionType.Hard[]`, `PastInfo`, alle Dekodierstellen (über die Hilfsfunktionen aus Etappe 1) | L |
| 4e | **Programme und Crews dynamisch, Blätter-UI** in Forschung, Kauf, VAB, Kapselwahl, Astronautenkomplex | L |
| 4f | **KI rollenbasiert, Szenario-Auswahl, Ersatzgrafiken**; Abschlusstest mit fiktivem Zusatzeintrag | M |

Agenten-Aufteilung: 4a und die Tests zuerst (ein Agent), danach 4b–4e nacheinander (sie berühren
dieselben Dateien), 4f kann teilweise parallel zu 4e laufen. Prüfung und Zusammenführung jeweils durch mich.

## 6. Tests

- **Goldene Tests:** Für das Szenario „Klassisch“ liefern alle bisherigen Funktionen
  (`HardwareType`, `CrewSize`, VAB-Kombinationen je Mission, Kosten, Technologietransfer,
  Crewgrößen, Prestige-Zuordnung) dieselben Ergebnisse wie vor dem Umbau.
  Referenzwerte werden **vor** Schritt 4b aus dem heutigen Code erzeugt.
- **Katalogtests:** Pflichtfelder, eindeutige IDs, gültige Verweise (`requires`, `compatible_*`, `anim_id`).
- **Erweiterungstest:** fiktive Zusatzrakete und Zusatzsonde im Testszenario – Erforschung, Kauf,
  Zusammenbau, Missionsablauf mit Animation über `anim_id`.
- **Spielstand:** Speichern/Laden mit mehr als 7 Einträgen je Kategorie.
- Auf dem Mac durch Sie: ein klassisches Spiel über mehrere Jahre, Vergleich mit dem Original.

## 7. Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| Animations- und Fehlerschlüssel hängen an der ID | `anim_id` im Katalog, Schlüsselbildung unverändert |
| Viele Stellen lesen Arrays direkt (ca. 40 Dateien) | Zugriff nur noch über Hilfsfunktionen; Schritt 4c erst nach 4a/4b |
| KI verhält sich anders | goldene Tests für KI-Kaufentscheidungen im klassischen Szenario; neue Einträge zunächst nur für Spieler |
| Bildschirme mit fester Geometrie | Blättern statt Umgestalten; Neugestaltung erst Etappe 7 |
| PBEM (E-Mail-Spiel) | Datenformat ändert sich ohnehin; nach Etappe 4 testen oder als nicht unterstützt kennzeichnen |
| Spielgewichte (Gewicht/Nutzlast) sind abstrakte Einheiten, nicht kg | Umrechnung für reale Werte in Etappe 5 festlegen (Vorschlag dort) |

Gesamtaufwand: groß – der umfangreichste Umbau des Projekts.

## 8. Entscheidungen

1. **Szenario-Konzept** „Klassisch“ / „Historisch“, unabhängig vom Startjahr wählbar – *Empfehlung: ja.*
   Alternative: Startjahr 1955/56 bedeutet automatisch historische Hardware.
2. **Fiktive Original-Programme** (Nova, XMS-2-Minishuttle, Jupiter-Kapsel, Kicker A–C) im historischen
   Szenario weglassen und durch reale ersetzen (Saturn IB/V, Agena, Centaur usw. in späteren Ausbaustufen) –
   *Empfehlung: ja, im klassischen Szenario bleiben sie.*
3. **UdSSR in Ausbaustufe 1** mit klassischer Hardware – *Empfehlung: ja* (bereits so geplant).
4. **Blätter-UI** im Originalstil jetzt, Neugestaltung erst mit Etappe 7 – *Empfehlung: ja.*
5. **PBEM** (E-Mail-Spiel) nach dem Umbau weiter unterstützen oder deaktivieren?
   *Empfehlung: vorerst deaktivieren*, spart Test- und Anpassungsaufwand.
