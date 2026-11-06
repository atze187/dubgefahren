# Flacher Control-Panel-Look für dubgefahren

Stand: 2026-11-05. Status: Entwurf, noch nicht freigegeben.

## Absicht

dubcycling hat einen neuen, flachen "Control-Panel"-Look bekommen (`E:\Repos\dubcycling\plugin\ui\PanelLook.h/.cpp`, Commit 15b2626). Er gefällt dem Nutzer besser als der heutige Look von dubgefahren. dubgefahren soll ihn komplett übernehmen, damit beide Plugins gleich aussehen. Es geht um Optik, nicht um Verhalten: Parameter, DSP, State und Bedienlogik bleiben unverändert.

## Entschieden (vom Nutzer)

- Der ganze flache Look wird übernommen, nicht nur einzelne Teile.
- Alle Drehregler werden zu horizontalen Segment-Slidern (LED-Segmente) mit LCD-Wert rechts daneben. Variante "flache Drehregler" wurde verworfen.
- Schrift: Monospace-Versalien statt Inter.

## Zielbild

- Fenster, Panels, Buttons: flache dunkle Flächen, 1-px-Kanten, kleine Eckenradien (ca. 2 px). Keine Verläufe, Lichtkanten oder Schlagschatten.
- Werteanzeigen: LCD-Stil (Bernstein auf Fast-Schwarz, umrandet).
- Regler: Segment-Slider, blau gefüllt (Akzent), Wert als LCD rechts.
- Filtertyp (LP/BP/HP): Dreifach-Schalter in der Art des GATE/1-SHOT-Schalters aus dubcycling, aktives Segment in Bernstein.
- Beschriftungen: Monospace, Versalien, gedämpftes Grau.
- Farbwerte als Ausgangspunkt aus dubcycling (`panel::kWindow`, `kStrip`, `kLcd`, `kEdge`, `kEdgeLight`, `kButton`, `kText`, `kDim`, `kAmber`, `kGreen`, `kWarn`, `kAccent`).
- Pads: flache Kacheln mit Kante. Belegte Pads bekommen einen Farbbalken am unteren Rand, ein getroffenes Pad füllt sich ganz. Die Pad-Farben aus dem Feature "Pad colours" (#14) bleiben die Quelle der Farben.

## Umfang

Betroffen sind alle sichtbaren UI-Teile in `plugin/ui` und `plugin/PluginEditor.*`: Look-and-Feel, Controls (`Knob`, `Choice`, `Toggle`), Live-, Performance- und Advanced-Panel, Slot-Editor, Pad-Grid, Wellenform, CPU-Meter, Settings-Dialog, Kopfzeile.

## Nicht-Ziele

- Keine Preset-Leiste aus dubcycling.
- Keine Änderung an Parametern, IDs, Wertebereichen, State oder DSP.
- Kein neues Layout über das hinaus, was der Wechsel von Drehreglern zu Slidern erzwingt.
- Die Skalierung des Fensters (Seitenverhältnis fest, Größenlimits relativ zur Basisgröße) bleibt erhalten.
- Die Basisgröße darf wachsen, wenn die Slider-Zeilen es brauchen, aber nur ein wenig und nicht übermäßig (Vorgabe des Nutzers). Zeilen werden trotzdem so dicht gesetzt, wie es lesbar bleibt.

## Anti-Ziele (wäre ein Fehlschlag, auch wenn es technisch läuft)

- Pads, die schlechter lesbar sind als heute (Zustand belegt, leer, getroffen, Farbe).
- Werte, die sich nicht mehr fein genug einstellen lassen (Doppelklick-Reset, Fein-Modus, Mausrad, falls heute vorhanden, müssen erhalten bleiben).
- Verlust der Hostautomation oder der MIDI-Learn-Anzeige.
- Text, der in Monospace-Versalien abgeschnitten wird.

## Randbedingungen

- Das Projekt baut auf Windows, macOS und Linux. Die Schriftwahl muss auf allen drei funktionieren.
- Es gibt bereits einen offenen Feature-Branch `feature/settings-midi-learn` (nicht gepusht) mit dem Settings-Dialog. Dieser Dialog ist Teil des Umfangs; der Look-Umbau sollte erst nach dem Merge dieses Branches auf `main` aufsetzen.
- VST3-Validierung (Steinberg-Validator und pluginval) muss weiter grün sein.

## Dem Builder überlassen

- Aufteilung in Schritte und Reihenfolge der Panels.
- Konkrete Maße der Segmente, Zeilenhöhen und Abstände. Referenz ist dubcycling (Segment 5 px, Lücke 1 px, Balkenhöhe 8 px).
- Wie Wertebereiche mit Skew (z. B. Cutoff) auf Segmente abgebildet werden.
- Reihenfolge und Anordnung der Slider-Zeilen in den Panels.
- Ob eine gemeinsame Basis mit dubcycling entsteht oder der Code kopiert wird.

## Meine Entscheidungen (zur Prüfung)

- **Schrift:** Wie dubcycling die System-Monospace-Schrift (`juce::Font::getDefaultMonospacedFontName()`), keine eingebettete Schrift. Folge: leicht unterschiedliche Optik je Betriebssystem, dafür kein Zusatz an Größe und Lizenz. Die eingebettete Inter-Schrift fällt weg, wenn sie nirgends mehr gebraucht wird.
- **Schlagschatten-Bibliothek:** `melatonin_blur` wird entfernt, sobald kein Code sie mehr nutzt.
- **Tests:** Es gibt keine UI-Tests im Umfang. Prüfung per Build, Validator, pluginval und Sichtprüfung im Host.
- **Reihenfolge:** Der Umbau beginnt erst, wenn `feature/settings-midi-learn` auf `main` gemergt ist. Der Settings-Dialog gehört dann zum Umfang.
- **Bipolare und gestufte Regler** (Pan, Tune, Stretch-Faktor): Segment-Slider mit Mittelmarke bei bipolaren Werten, gestufte Werte rasten auf Segmente. Auswahlfelder (`Choice`) werden flache Dropdowns, `Toggle` wird ein Schalter im Panel-Stil.
- **Pads:** Zustände leer, belegt, getroffen, gewählt und MIDI-Learn-Markierung bleiben unterscheidbar; die Zustände werden im Plan einzeln aufgelistet. MIDI-Learn meint das Pad-Raster im Settings-Dialog.
- **Wellenform, Regionen, CPU-Meter:** In die neue Palette umgefärbt und mit flachen Kanten versehen, aber nicht als LCD neu entworfen. Regionen- und Grain-Farben bleiben erhalten.
- **Aufräumen:** Wenn nach dem Umbau weder Inter noch `melatonin_blur` benutzt wird, werden Schrift, Lizenzdatei (`OFL.txt`) und die Abhängigkeit entfernt; Zip-Inhalt und Release-Notizen werden entsprechend geprüft.
- **Textbreite:** Beschriftungen werden mit gemessener Textbreite geprüft, nicht nur auf Windows geschätzt. Zeigt sich Abschneiden auf macOS oder Linux, wird gekürzt; manuelle Sichtprüfung dort ist Teil der Abnahme.
- **Dokument-Ablage:** Nicht committet, weil der Arbeitsbaum auf `feature/settings-midi-learn` mit einer offenen Änderung steht.
