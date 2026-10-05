#pragma once
#include "engine/FxParams.h"

namespace dg {

// Die drei Performance-Knobs (je 0 bis 1). Sie sind ein Aufschlag auf die Einzelwerte der Effekte:
// bei 0 ändert sich nichts, die Einzelparameter selbst werden nie verändert.
struct MacroParams
{
    float space = 0.0f;       // Delay und Reverb weiter und tiefer
    float grit = 0.0f;        // Sättigung, Wow, dunklere Echos, Phaser
    float throwAmount = 0.0f; // Dub-Wurf: mehr FX-Send, Delay bis zur Selbstoszillation
};

// Wirksame Effektwerte: Einzelwerte plus Aufschläge, jeweils begrenzt. Ein Einzelwert, der schon über
// der Obergrenze eines Knobs liegt, wird nicht abgesenkt.
FxParams applyMacros(const FxParams& base, const MacroParams& m);

// Aufschlag (0 bis 1), den Throw zum FX-Send jedes Slots addiert.
float throwSendBoost(const MacroParams& m);

} // namespace dg
