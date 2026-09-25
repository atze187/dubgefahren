#pragma once
#include <optional>
#include <juce_core/juce_core.h>
#include "engine/Kit.h"

namespace dg {

constexpr const char* kKitExtension = ".dgkit";

struct KitParseResult
{
    std::optional<Kit> kit;
    juce::String error;
};

juce::String kitToJsonString(const Kit& kit);
KitParseResult kitFromJsonString(const juce::String& text);
bool saveKitFile(const Kit& kit, const juce::File& file, juce::String& error);
KitParseResult loadKitFile(const juce::File& file);

} // namespace dg
