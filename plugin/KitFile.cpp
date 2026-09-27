#include "plugin/KitFile.h"
#include "engine/SlotFields.h"

namespace dg {

namespace {
constexpr const char* kFormat = "dubgefahren-kit";
constexpr int kVersion = 1;
constexpr juce::int64 kMaxFileBytes = 1024 * 1024;

KitParseResult fail(const juce::String& message) { return { std::nullopt, message }; }

bool isNumber(const juce::var& v) { return v.isDouble() || v.isInt() || v.isInt64() || v.isBool(); }

juce::String slotLabel(int s) { return "Slot " + juce::String(s + 1); }
} // namespace

juce::String kitToJsonString(const Kit& kit)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("format", kFormat);
    root->setProperty("version", kVersion);

    juce::Array<juce::var> slots;
    for (int s = 0; s < kNumSlots; ++s)
    {
        auto* slot = new juce::DynamicObject();
        slot->setProperty("name", juce::String::fromUTF8(kit.names[static_cast<std::size_t>(s)].c_str()));
        auto* params = new juce::DynamicObject();
        for (int i = 0; i < kNumSlotFields; ++i)
        {
            const auto f = static_cast<SlotField>(i);
            params->setProperty(juce::Identifier(fieldSpec(f).key),
                                static_cast<double>(getSlotField(kit.slots[static_cast<std::size_t>(s)], f)));
        }
        slot->setProperty("params", juce::var(params));
        slots.add(juce::var(slot));
    }
    root->setProperty("slots", slots);
    return juce::JSON::toString(juce::var(root));
}

KitParseResult kitFromJsonString(const juce::String& text)
{
    juce::var root;
    if (juce::JSON::parse(text, root).failed() || !root.isObject())
        return fail("The file is not valid JSON.");
    if (root["format"].toString() != kFormat)
        return fail("The file is not a Dubgefahren kit.");
    if (!isNumber(root["version"]))
        return fail("The kit version is missing.");
    if (static_cast<int>(root["version"]) > kVersion)
        return fail("The kit was created with a newer version of Dubgefahren.");

    const auto* slots = root["slots"].getArray();
    if (slots == nullptr || slots->size() != kNumSlots)
        return fail("The kit must contain exactly 16 slots.");

    Kit kit;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const juce::var& slot = (*slots)[s];
        if (!slot.isObject())
            return fail(slotLabel(s) + " is invalid.");
        if (!slot["name"].isString())
            return fail(slotLabel(s) + " has no name.");
        const auto* params = slot["params"].getDynamicObject();
        if (params == nullptr)
            return fail(slotLabel(s) + " has no parameters.");

        SlotParams p = makeDefaultSlotParams();
        for (const auto& prop : params->getProperties())
        {
            const auto field = slotFieldFromKey(prop.name.toString().toStdString());
            if (!field)
                continue; // unbekannte Schlüssel (neuere Versionen) ignorieren
            if (!isNumber(prop.value))
                return fail(slotLabel(s) + ": value for \"" + prop.name.toString() + "\" is not a number.");
            setSlotField(p, *field, static_cast<float>(static_cast<double>(prop.value)));
        }
        kit.slots[static_cast<std::size_t>(s)] = p;
        kit.names[static_cast<std::size_t>(s)] = slot["name"].toString().substring(0, 32).toStdString();
    }
    return { kit, {} };
}

bool saveKitFile(const Kit& kit, const juce::File& file, juce::String& error)
{
    if (!file.getParentDirectory().createDirectory())
    {
        error = "Could not create folder: " + file.getParentDirectory().getFullPathName();
        return false;
    }
    if (!file.replaceWithText(kitToJsonString(kit), false, false, "\n"))
    {
        error = "Could not write file: " + file.getFullPathName();
        return false;
    }
    return true;
}

KitParseResult loadKitFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return fail("File not found: " + file.getFullPathName());
    if (file.getSize() > kMaxFileBytes)
        return fail("The file is too large for a kit.");
    return kitFromJsonString(file.loadFileAsString());
}

} // namespace dg
