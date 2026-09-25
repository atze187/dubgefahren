# Dubgefahren – Design-Spezifikation

**Datum:** 2026-09-25
**Status:** Entwurf zur Freigabe
**Typ:** VST3-Instrument (Windows x64), JUCE 8

## 1. Ziel und Kontext

Dubgefahren ist ein VST3-Instrument, das 16 unabhängig einstellbare Dubsirenen
bereitstellt und in Ableton Live (Session View) ein bisher genutztes Effekt-Drumrack
ersetzt.

**Einsatzumgebung:**

- Ableton Live, Session View, Live-Performance.
- Intech Studio Grid **BU16** triggert die 16 Pads über MIDI-Noten.
- **PO16** (Potis) regelt die gemeinsame Effektkette.
- **TEK2** (Endlos-Encoder) regelt die Performance-Regler.
- **PFB4** (Fader/Buttons) für Schalter wie Ziel Fokus/Alle, Panic, Latch-bei-Stopp.
- NI Maschine Jam wird parallel genutzt, hat aber keine plugin-spezifischen Anforderungen.

**Erfolgskriterien:**

- Das Plugin lädt in Ableton Live als Instrument und lässt sich mit dem BU16 ohne
  Umkonfiguration spielen (Noten 36–51).
- Jede Sirene ist per Oberfläche einstellbar; alle Parameter sind in Ableton
  automatisier- und mappbar.
- Mehrere Sirenen können gleichzeitig klingen; Choke-Gruppen funktionieren klickfrei.
- Zustand wird mit dem Ableton-Set gespeichert und korrekt wiederhergestellt.
- Kits lassen sich als Datei exportieren und importieren.
- Keine Aussetzer, kein Clipping bei extremen Einstellungen (z. B. Delay-Feedback 110 %).

## 2. Umfang

**Enthalten (Version 1):** alles in den Abschnitten 3–9.

**Nicht enthalten:** Sample-Wiedergabe (Architektur ist vorbereitet, siehe 3.3),
macOS, AU, CLAP, Standalone-App, eigenes MIDI-Learn, Velocity-Auswertung.

## 3. Architektur

### 3.1 Verzeichnisstruktur

```
dubgefahren/
├─ engine/           reines C++17/20, keine JUCE-Abhängigkeit, unit-getestet
│  ├─ SirenVoice     Oszillator + LFO + Pitch-Sweep + Amp-Hüllkurve (eine Stimme)
│  ├─ SoundSource    Schnittstelle „Klangquelle“ eines Slots
│  ├─ SlotParams     Parametersatz eines Slots
│  ├─ PadRouter      Note → Slot, Trigger-Modi, Choke-Gruppen, Fokus-Slot
│  ├─ FxChain        Drive → Filter → Tape-Delay → Federhall → Master
│  └─ Engine         verbindet alles; render(block, midi, transport)
├─ plugin/           JUCE-Hülle
│  ├─ Processor      Parameter (APVTS), MIDI, Engine-Aufruf, State
│  ├─ Editor         Oberfläche
│  ├─ KitFile        Kit-Export/-Import (JSON), Werks-Kit
│  └─ Config         Lesen der Plugin-Config
├─ tests/            Catch2: Engine-, Render-, State-Tests
├─ docs/
├─ CMakeLists.txt
├─ build.ps1
└─ README.md
```

### 3.2 Datenfluss

1. MIDI-Noten 36–51 gehen an den **PadRouter** (Note 36 = Slot 1 … Note 51 = Slot 16).
2. Der PadRouter entscheidet anhand von Trigger-Modus und Choke-Gruppe, welche Stimme
   startet, in den Release geht oder gechokt wird, und führt den Fokus-Slot.
3. Aktive Stimmen werden mit Lautstärke, Panorama und FX-Send pro Slot gemischt.
4. Der Mix läuft durch die **FxChain** zum Stereo-Ausgang.
5. Parameterwerte liest die Engine pro Audioblock aus einem einfachen Snapshot-Struct,
   den die JUCE-Hülle aus den APVTS-Parametern füllt. Die Engine kennt JUCE nicht.

### 3.3 Polyphonie und Erweiterbarkeit

- Genau **eine Stimme pro Slot**, also maximal 16 gleichzeitige Stimmen.
- Ein erneuter Trigger eines Slots startet dessen Stimme neu (Ausnahme Latch, siehe 5).
- Slots sprechen ausschließlich über die Schnittstelle `SoundSource` mit ihrer
  Klangquelle. Ein späterer `SamplePlayer` implementiert dieselbe Schnittstelle, ohne
  dass PadRouter oder FxChain geändert werden.

## 4. Sirenen-Stimme (Parameter pro Slot)

| Gruppe | Parameter | Bereich / Werte |
|---|---|---|
| Oszillator | Wellenform | Sinus, Dreieck, Sägezahn, Rechteck |
| | Pulsbreite | 5–95 % (nur Rechteck) |
| | Grundtonhöhe | 20 Hz–5 kHz, logarithmisch |
| LFO (moduliert Tonhöhe) | Form | Rechteck, Dreieck, Sägezahn auf, Sägezahn ab, Sample & Hold |
| | Rate | 0,05–40 Hz frei |
| | Sync | aus / an |
| | Rate (Sync) | 1/32 … 4 Takte |
| | Tiefe | 0–48 Halbtöne |
| Sweep (Pitch-Hüllkurve) | Betrag | −48 … +48 Halbtöne |
| | Zeit | 10 ms–10 s |
| Hüllkurve (Amp) | Attack | 0–5 s |
| | Release | 0–10 s |
| Trigger | Modus | Gate, Latch, One-Shot |
| | One-Shot-Länge | 50 ms–10 s |
| | Choke-Gruppe | keine, 1–4 |
| Mix | Lautstärke | −inf … +6 dB |
| | Panorama | L100 … R100 |
| | FX-Send | 0–100 % (Anteil in Delay und Hall; Drive und Filter wirken immer) |

Hinweis: Gegenüber der Diskussion (15 Parameter) wurden Pulsbreite sowie getrennte
Parameter für freie und synchronisierte LFO-Rate explizit aufgeführt; damit ergeben
sich 18 Parameter pro Slot, 288 Slot-Parameter insgesamt. Der Wert −60 dB bei der
Lautstärke bedeutet −inf (stumm).

**Klangliche Festlegungen:**

- Oszillatoren bandbegrenzt (PolyBLEP), Sinus direkt.
- Beim (Re-)Trigger startet der LFO mit Phase 0; Sample & Hold würfelt beim Trigger neu.
- Die Tonhöhe ergibt sich aus: Grundtonhöhe · 2^((LFO + Sweep + Performance-Offsets)/12),
  begrenzt auf 10 Hz–18 kHz.
- Sweep läuft vom Betrag linear (in Halbtönen) auf 0 über die Sweep-Zeit.
- Eine Stimme ist beendet, sobald der Release abgeschlossen ist; Delay- und Hall-Fahnen
  klingen in der FxChain weiter.

**Werks-Kit (16 Slots):**

| Slot | Name | Charakter |
|---|---|---|
| 1 | Classic | Rechteck-LFO, mittlere Rate, Gate |
| 2 | Wail | langsamer Dreieck-LFO, Latch |
| 3 | Trill | schneller Rechteck-LFO, kleine Tiefe |
| 4 | Laser | kurzer Sweep von oben nach unten, One-Shot |
| 5 | Riser | langer Sweep aufwärts, One-Shot |
| 6 | Faller | langer Sweep abwärts, One-Shot |
| 7 | Alarm | Rechteck-LFO, Quinte, Latch |
| 8 | UFO | Sample & Hold, Latch |
| 9 | Bleep | kurzer Sinus-Ton, One-Shot |
| 10 | Zap | sehr kurzer Sweep mit Sägezahn, One-Shot |
| 11 | Siren Up | Sägezahn-auf-LFO, Gate |
| 12 | Siren Down | Sägezahn-ab-LFO, Gate |
| 13 | Deep Wobble | tiefe Grundtonhöhe, langsamer Sinus-artiger Dreieck-LFO, Latch |
| 14 | Chirp | schneller Sägezahn-LFO, hohe Tonhöhe, Gate |
| 15 | Horn | Sägezahn-Oszillator, kein LFO, kurzer Sweep, Gate |
| 16 | Drop | langer Sweep abwärts um 48 Halbtöne, One-Shot |

Die Slots 4–6, 9, 10 und 16 liegen in Choke-Gruppe 1, alle anderen ohne Choke-Gruppe.
Die exakten Zahlenwerte werden in der Implementierung festgelegt und per Gehör abgestimmt;
die Charakterbeschreibung oben ist verbindlich.

## 5. Pads und Trigger-Logik

| Modus | Note-On | Note-Off |
|---|---|---|
| Gate | Start (Neustart falls aktiv) | Release |
| Latch | Start, wenn inaktiv oder im Release; Release, wenn aktiv gehalten | ignoriert |
| One-Shot | Start (Neustart falls aktiv); nach One-Shot-Länge automatisch Release | ignoriert |

- **Choke:** Startet eine Stimme mit Choke-Gruppe 1–4, erhalten alle anderen aktiven
  Stimmen derselben Gruppe einen Fade von 5 ms und werden danach beendet.
- **Fokus:** Das zuletzt per Note-On gestartete Pad bzw. das zuletzt in der Oberfläche
  angeklickte Pad ist der Fokus-Slot. Ein Note-On, das eine Latch-Stimme beendet,
  setzt ebenfalls den Fokus.
- **Velocity:** wird ignoriert.
- **Noten außerhalb 36–51** und sonstige MIDI-Nachrichten werden ignoriert.

**Latch bei Stopp** (globaler Parameter), ausgelöst genau einmal beim Wechsel des Host-
Transports von „läuft“ auf „gestoppt“:

| Wert | Verhalten |
|---|---|
| Weiterlaufen | Gelatchte Stimmen klingen weiter. |
| Ausklingen (Standard) | Gelatchte Stimmen gehen in ihren Release. |
| Sofort stoppen | Gelatchte Stimmen werden mit 5 ms Fade beendet. |

Gate-Stimmen erhalten vom Host Note-Offs; One-Shots laufen zu Ende.

**Panic** (globaler Parameter, Taster): beendet alle Stimmen mit 5 ms Fade, einschließlich
gelatchter Stimmen. Delay und Hall werden dabei nicht geleert.

## 6. Effektkette und globale Parameter

| Effekt | Parameter | Festlegungen |
|---|---|---|
| Drive | Drive | tanh-Sättigung, Pegelausgleich |
| Filter | Cutoff, Resonanz, Typ | SVF; Typ überblendet stufenlos LP → BP → HP; stabil bei maximaler Resonanz |
| Tape-Delay | Zeit, Feedback, Tone, Wow, Mix | Zeit tempo-synchron 1/16 … 1/1 inkl. punktiert/Triole; Feedback 0–110 % mit Soft-Clipper im Feedback-Weg; Tone = Filter im Feedback-Weg; Zeitänderungen gleiten (Tape-Pitch-Effekt) |
| Federhall | Decay, Tone, Mix | Allpass-Kette mit Dispersion |
| Master | Lautstärke | Limiter am Ausgang (Ceiling −0,3 dBFS) |

Der FX-Send pro Slot bestimmt den Anteil, der in Delay und Hall geht; der Rest läuft am
Delay/Hall vorbei direkt zum Master. Drive und Filter wirken auf den gesamten Mix.

**Performance-Regler** (für den TEK2):

| Parameter | Bereich | Mitte = neutral |
|---|---|---|
| Pitch-Offset | ±24 Halbtöne | 0 |
| LFO-Rate-Faktor | ×¼ … ×4 (logarithmisch) | ×1 |
| LFO-Tiefe-Offset | ±24 Halbtöne | 0 |
| Sweep-Offset | ±24 Halbtöne | 0 |
| Ziel | Fokus / Alle | Fokus |

Im Modus **Fokus** wirken die Offsets nur auf den Fokus-Slot; nicht-fokussierte Stimmen
behalten den Offset, den sie zum Zeitpunkt des Fokusverlusts hatten, bis sie enden.
Im Modus **Alle** wirken sie auf alle Stimmen. Werte werden über ca. 20 ms geglättet.

**Weitere globale Parameter:** Latch bei Stopp, Panic.

## 7. Oberfläche

Größe ca. 1000 × 640 px, skalierbar 75–200 %, dunkel, vektorbasiert gezeichnet.

```
┌──────────────────────────────────────────────────────────────────────┐
│ DUBGEFAHREN            Kit: [Werks-Kit ▾] [Import] [Export]   [PANIC]│
├──────────────────────┬───────────────────────────────────────────────┤
│  13   14   15   16   │ Slot 6 · "Laser-Zap"               [Umbenennen]│
│  9    10   11   12   │ OSZ   [Welle ▾] (Pitch) (PW)                    │
│  5   [6]   7    8    │ LFO   [Form ▾] (Rate) [Sync] (Tiefe)            │
│  1    2    3    4    │ SWEEP (Betrag) (Zeit)                           │
│                      │ AMP   (Attack) (Release)                        │
│  ● spielt  ◆ Fokus   │ TRIG  [Gate|Latch|One-Shot] (Länge) [Choke ▾]   │
│  ○ gelatcht          │ MIX   (Vol) (Pan) (FX-Send)                     │
├──────────────────────┴───────────────────────────────────────────────┤
│ DRIVE │ FILTER Cut Res Typ │ DELAY Zeit Fb Tone Wow Mix │ HALL Dec Tone Mix │ VOL │
├──────────────────────────────────────────────────────────────────────┤
│ PERFORMANCE (Pitch) (Rate) (Tiefe) (Sweep) [Ziel: Fokus|Alle] [Latch bei Stopp ▾] │
└──────────────────────────────────────────────────────────────────────┘
```

- Pad-Anordnung wie BU16: Pad 1 (Note 36) unten links.
- Pads zeigen live: spielt, gelatcht, Fokus.
- Linksklick: Slot auswählen und vorhören, solange gedrückt (Gate-Verhalten, unabhängig
  vom Trigger-Modus).
- Rechtsklick: Kopieren, Einfügen, Auf Werkseinstellung zurücksetzen, Umbenennen.
- Der Editor folgt dem Fokus-Slot; abschaltbar („Editor folgt Fokus“, UI-Einstellung).

## 8. Speichern, Kits und Config

**Plugin-State (Ableton-Set):** alle Parameter, Slot-Namen, UI-Einstellungen (Skalierung,
Editor folgt Fokus), Versionsfeld für Migration.

**Kit-Dateien:** Endung `.dgkit`, JSON, enthalten Format-Version und die 16 Slots
(Name + Slot-Parameter), keine Effekte. Import ersetzt nur die Slots. Ungültige oder
fremde Dateien werden abgelehnt, ohne den aktuellen Zustand zu ändern; die Oberfläche
zeigt eine Fehlermeldung.

**Werks-Kit:** fest im Plugin eingebaut, über das Kit-Menü jederzeit ladbar.

**Config-Datei:** `Dubgefahren.vst3\Contents\Resources\Dubgefahren.config.json`

```json
{
  "_hinweis": "kitFolder auf den gewünschten Ordner setzen. Umgebungsvariablen wie %USERPROFILE% sind erlaubt.",
  "kitFolder": ""
}
```

- Wird einmal beim Laden des Plugins gelesen, nie geschrieben.
- Umgebungsvariablen im Pfad werden expandiert.
- Fehlt die Datei, ist sie ungültig, ist `kitFolder` leer oder existiert der Ordner nicht,
  gilt der Standard `Dokumente\Dubgefahren\Kits`. Existiert dieser nicht, wird er
  angelegt. Bei ungültiger Config zeigt der Import-/Export-Dialog einen Hinweis.
- Die Datei ist für spätere Einträge (z. B. Sample-Ordner) erweiterbar.
- Bearbeiten erfordert Adminrechte, wenn das Plugin unter `Program Files` liegt.

## 9. Build, Tests, Fehlerbehandlung

**Build:**

- CMake ≥ 3.25, Visual Studio 2026 (MSVC), x64.
- JUCE 8 und Catch2 über `FetchContent`, Versionen gepinnt.
- Konfigurationen `Debug` und `Release`.
- Ergebnis: `Dubgefahren.vst3`. Automatisches Kopieren in den System-VST3-Ordner ist aus.
- Install-Schritt kopiert nach `C:\Program Files\Common Files\VST3\` (Adminrechte) und
  überschreibt eine vorhandene Config im Bundle nicht; eine Beispiel-Config wird nur
  angelegt, wenn keine existiert.
- `build.ps1` mit `configure`, `build`, `test`, `install`.
- README: Setup, Build, Ableton-Mapping-Anleitung, Config.

**Automatische Tests (Catch2, ohne DAW):**

- PadRouter: alle Trigger-Modi, Choke, Fokus, Latch bei Stopp (alle drei Werte), Panic.
- SirenVoice: Grundfrequenz (Nulldurchgänge), LFO-Rate und -Tiefe, Sweep-Endwert,
  Hüllkurvenende.
- FxChain: Ausgabe endlich und begrenzt bei 110 % Feedback und maximaler Resonanz über
  mindestens 30 s; keine NaN/Inf; keine Denormals.
- Engine-Offline-Render: Werks-Kit rendern; Pegel plausibel; Stille nach Panic (abzüglich
  Effektfahnen bei Mix 0); Choke klickfrei (Sample-Sprünge unter Schwelle).
- State: Speichern/Laden ergibt identischen Zustand; Kit-Round-Trip; beschädigte Kits
  werden abgelehnt; Config-Fallbacks (fehlend, ungültig, Ordner fehlt, Umgebungsvariable).
- Plugin-Validierung: Steinberg `validator` und pluginval (Strenge 5).

**Manueller Abnahmetest in Ableton** (Checkliste in der README): laden, BU16 triggern,
Trigger-Modi und Choke prüfen, PO16/TEK2/PFB4 mappen, Fokus/Alle, Latch bei Stopp,
Set speichern und neu laden, Kit exportieren und importieren.

**Fehlerbehandlung und Echtzeitregeln:**

- Im Audio-Thread keine Allokation, keine Locks, kein Datei-I/O.
- Kit-Import, Config-Lesen und Datei-Dialoge laufen im Message-Thread; Übergabe an den
  Audio-Thread über Parameter bzw. lock-freie Mechanismen.
- Beliebige Samplerates und Blockgrößen; Samplerate-Wechsel setzt DSP-Zustände zurück.
- Denormals werden unterdrückt (FTZ/DAZ im Audio-Callback).
