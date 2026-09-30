# UI-Umbau: Material und Zustandslicht – Design-Spezifikation

**Datum:** 2026-09-30
**Status:** Entwurf zur Freigabe
**Issue:** atze187/dubgefahren#15 (Voraussetzung für #11 und #14)

## 1. Ziel

Die Oberfläche soll hochwertiger wirken, ohne kitschig zu werden. Das Layout ist
übersichtlich und bleibt unverändert; geändert wird nur, wie die Elemente gezeichnet
werden.

Richtung: **Material als Basis** (dezente Verläufe, weiche Schatten, Regler mit Körper),
**Licht nur für Zustände** (playing, Fokus, Latch). Die Pads dürfen kräftig wirken,
alles andere bleibt dezent.

**Erfolgskriterien:**

- Layout, Größen, Bedienung, Parameter und Kit-Format sind unverändert.
- Alle Pad-Zustände (ruhend, spielt, Fokus, ausgewählt, gelatcht, leer, Sample fehlt)
  sind auf einen Blick unterscheidbar.
- Die gesamte UI nutzt eine eingebettete Schrift und sieht auf jedem Rechner gleich aus.
- Alles ist bei 0,75×, 1× und 2× Fenstergröße scharf und lesbar.
- Die UI läuft flüssig, auch wenn alle 16 Pads gleichzeitig spielen.
- Die bestehenden Tests und der Validator bleiben grün.

## 2. Umfang

**Enthalten:** Look-and-Feel, Zeichnen der Pads, Panels, Regler, Buttons, Auswahlboxen
und Schalter, eingebettete Schrift, Pad-Animation, zwei neue Build-Abhängigkeiten.

**Nicht enthalten:**

- Farbzuweisung pro Pad (#14). Dieser Umbau bereitet sie nur vor (Abschnitt 5).
- Wellenform-Anzeige (#11). Sie wird danach in der neuen Formensprache gebaut.
- Änderungen an Layout, Popup-Menüs, Dialogen, Engine oder Parametern.

## 3. Abhängigkeiten

- **melatonin_blur** (MIT) für Schlagschatten, Innenschatten und Glow. Einbindung per
  `FetchContent` mit festem Tag wie JUCE und Catch2, als JUCE-Modul in
  `dg_plugin_shared` gelinkt. Unter Windows wird keine Zusatzbibliothek benötigt
  (ohne Intel IPP greift die Fallback-Implementierung).
- **Inter** (SIL Open Font License 1.1) in den Schnitten Regular und SemiBold als TTF
  unter `plugin/assets/fonts/`, zusammen mit der Lizenzdatei `OFL.txt`. Einbettung
  über `juce_add_binary_data`.

Kein UI-Framework, keine Bild-Regler, kein WebView.

## 4. Aufbau

### 4.1 `DgLookAndFeel`

Bleibt die zentrale Stelle für den Look.

- Die Palette in `colours` wächst um: Verlaufsfarben für Panels (`panelTop`,
  `panelBottom`), Lichtkante (`highlight`), Schattenfarbe (`shadow`),
  Pad-Grundfarbe (`padBase`, Wert des heutigen `playing`-Grüns `0xff4cd07d`).
  `playing` entfällt; die Legende nutzt `padBase`.
- Neue Überschreibungen: `drawButtonBackground`, `drawComboBox`, `drawToggleButton`,
  `getTypefaceForFont`. `drawRotarySlider` wird neu gezeichnet (Abschnitt 6).
- Die Schrift wird einmal aus den Binärdaten geladen und für alle Fonts geliefert:
  fette Fonts erhalten Inter SemiBold, alle anderen Inter Regular. Bestehende
  `juce::FontOptions(..., juce::Font::bold)`-Aufrufe bleiben unverändert.

### 4.2 `plugin/ui/Surfaces.h/.cpp` (neu)

Gemeinsame Zeichenhelfer ohne Zustand:

- `drawPanel(g, bounds)`: Panel mit Verlauf, Lichtkante oben und weichem Schatten.
  Ersetzt die drei gleichen `fillRoundedRectangle`-Aufrufe in `SlotEditor`, `FxPanel`
  und `PerformancePanel`.
- `drawDivider(g, x, top, bottom)`: feine Doppellinie (dunkel plus hell) für die
  Gruppentrenner im FX-Panel.

Der Panel-Schatten ragt über die Panel-Fläche hinaus. Deshalb zeichnet der Editor
(`DubgefahrenEditor::paint` bzw. die `content_`-Komponente) die Schatten aller drei
Panels unter den Panels; die Panels selbst zeichnen nur Verlauf und Lichtkante.
`drawPanel` wird dafür in zwei Funktionen geteilt: `drawPanelShadow` und
`drawPanelBody`.

### 4.3 `plugin/ui/PadGlow.h` (neu)

Reine, header-only Funktion ohne JUCE-Abhängigkeit für die Helligkeitskurve der Pads
(Abschnitt 5.2), damit sie in `DubgefahrenPluginTests` ohne UI prüfbar ist.

### 4.4 `PadGrid`

- `Pad` erhält das Feld `baseColour_` (Standard `colours::padBase`) und einen Setter.
  Für #14 muss später nur dieser Setter aus dem Kit befüllt werden.
- `Pad` hält seine aktuelle Helligkeit (0…1) und zeichnet nur seinen Körper.
- `PadGrid::paint` zeichnet den Glow aller Pads auf der eigenen Fläche unter den
  Pads, weil er über die Pad-Fläche hinausragt.

## 5. Pads

### 5.1 Zustände

| Zustand | Aussehen |
|---|---|
| ruhend | dunkler Körper mit Verlauf, Lichtkante oben, Schimmer der Grundfarbe am unteren Rand, Schlagschatten |
| spielt | Körper in der Grundfarbe gefüllt (oben heller), Glow in der Grundfarbe nach außen, Text dunkel |
| Fokus | Ring in Akzentgelb mit gelbem Glow |
| ausgewählt | Ring dicker (2,5 px statt 1,5 px); ohne Fokus in neutralem Hellgrau |
| gelatcht | blauer Punkt oben rechts mit Glow |
| leer | vertieft (Innenschatten), gedämpfter Text, keine Grundfarbe, kein Schlagschatten |
| Sample fehlt | Name und Symbol in `warning` wie heute; Schimmer der Grundfarbe entfällt |

Fokus und Auswahl bleiben getrennte Signale: Farbe zeigt Fokus, Dicke zeigt Auswahl.
Ein Pad, das weder Fokus hat noch ausgewählt ist, hat keinen Ring, nur seine dunkle
Kontur.

### 5.2 Helligkeit

Die Helligkeit `b` (0…1) steuert die Füllung mit der Grundfarbe und die Deckkraft
des Glows.

- **Trigger** (Pad wird aktiv): `b = 1`.
- **Halten:** `b` fällt exponentiell auf den Haltewert 0,7 mit einer Zeitkonstante
  von 80 ms (nach etwa 250 ms ist der Haltewert praktisch erreicht).
- **Ende** (Pad wird inaktiv): `b` fällt linear in 200 ms auf 0.
- **Erneuter Trigger** während des Ausklingens: `b = 1`.

Ein erneuter Trigger, während das Pad durchgehend aktiv ist, ist in der UI nicht
sichtbar (`activeMask` ändert sich nicht) und löst kein neues Aufblitzen aus.

Schnittstelle (`PadGlow.h`):

```cpp
// Liefert die neue Helligkeit nach dtSeconds. active: Pad klingt gerade.
// wasActive: Zustand im vorigen Schritt.
float advancePadGlow(float brightness, bool active, bool wasActive, float dtSeconds);
```

Der vorhandene 30-Hz-Timer des Editors ruft über `PadGrid::setPadStates` die
Fortschreibung auf. Ein Pad und sein Glow-Bereich werden nur neu gezeichnet, wenn
sich `b` oder ein Zustand geändert hat; ruhende Pads verursachen kein Repaint.

Animiert wird nur die Deckkraft des Glows, nicht Radius oder Form, damit der
Schatten-Cache von melatonin_blur wirksam bleibt.

## 6. Regler, Panels, übrige Elemente (dezent)

**Regler**

- Spur: vertieft, dunkel.
- Wertebogen: `accent`, mit schwachem Glow. Bipolare Regler füllen weiter von der
  Nullposition aus.
- Körper: Kreis in der Mitte mit radialem Verlauf und kleinem Schlagschatten, darauf
  der helle Zeiger.
- Deaktiviert: Bogen in `textDim`, kein Glow.

**Panels:** leichter vertikaler Verlauf, Lichtkante oben, weicher Schlagschatten.
Gruppentrenner im FX-Panel als Doppellinie.

**Buttons und Auswahlboxen:** Verlauf und Lichtkante; gedrückt wirken sie eingelassen
(umgekehrter Verlauf, Innenschatten). Eingeschaltete Buttons sind in `accent` gefüllt.

**Schalter:** Zustand als kleine Leuchte links vom Text (an: `accent` mit Glow,
aus: dunkel vertieft) statt als Häkchen.

**Titel:** Inter SemiBold, erhöhte Laufweite, `accent`, ohne Glow.

**Fensterhintergrund:** kaum sichtbarer vertikaler Verlauf um `background`.

**Legende unter den Pads:** folgt den neuen Farben (`padBase`, `accent`, `latched`).

**Unverändert:** CPU-Anzeige und ihre Warnfarben, Popup-Menüs, Dialoge.

## 7. Risiken

Beide werden im Plan als frühe Schritte geprüft, bevor die Breite der UI umgebaut wird.

- **Schärfe bei skaliertem Fenster.** Die UI wird als Ganzes per
  `AffineTransform::scale` auf `content_` vergrößert. Zu prüfen ist, ob Schatten und
  Glow bei 0,75× und 2× scharf und korrekt positioniert sind.
- **Leistung bei 16 animierten Pads.** Falls das Zeichnen nicht flüssig ist, wird der
  Glow einmal pro Pad-Größe und Grundfarbe als Bild vorberechnet und nur mit
  wechselnder Deckkraft gezeichnet.

## 8. Tests und Abnahme

**Automatisch**

- `advancePadGlow`: Trigger setzt 1; Halten nähert sich 0,7 und unterschreitet es
  nicht; Ende erreicht nach 200 ms genau 0; erneuter Trigger während des Ausklingens
  setzt 1; Werte bleiben in 0…1.
- Die eingebettete Schrift wird geladen: Das Look-and-Feel liefert für normale und
  fette Fonts eine Inter-Schrift.
- Der Editor lässt sich bei 0,75×, 1× und 2× in ein Bild zeichnen, mit ruhenden,
  spielenden und leeren Pads. Geprüft wird nur der fehlerfreie Durchlauf, nicht das
  Aussehen.
- Alle bestehenden Tests und der Validator bleiben unverändert grün.

**Von Hand in Ableton**

- Alle Pad-Zustände aus Abschnitt 5.1 sind erkennbar und unterscheidbar.
- Aufblitzen beim Trigger und Ausklingen beim Ende sind sichtbar.
- Alles ist bei kleinster und größter Fenstergröße scharf und lesbar.
- Kein Ruckeln, wenn viele Pads gleichzeitig spielen.
