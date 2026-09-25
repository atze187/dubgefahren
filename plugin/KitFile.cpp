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
        return fail(juce::String::fromUTF8("Die Datei ist kein gültiges JSON."));
    if (root["format"].toString() != kFormat)
        return fail("Die Datei ist kein Dubgefahren-Kit.");
    if (!isNumber(root["version"]))
        return fail("Die Kit-Version fehlt.");
    if (static_cast<int>(root["version"]) > kVersion)
        return fail("Das Kit stammt aus einer neueren Dubgefahren-Version.");

    const auto* slots = root["slots"].getArray();
    if (slots == nullptr || slots->size() != kNumSlots)
        return fail("Das Kit muss genau 16 Slots enthalten.");

    Kit kit;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const juce::var& slot = (*slots)[s];
        if (!slot.isObject())
            return fail(slotLabel(s) + juce::String::fromUTF8(" ist ungültig."));
        if (!slot["name"].isString())
            return fail(slotLabel(s) + " hat keinen Namen.");
        const auto* params = slot["params"].getDynamicObject();
        if (params == nullptr)
            return fail(slotLabel(s) + " hat keine Parameter.");

        SlotParams p = makeDefaultSlotParams();
        for (const auto& prop : params->getProperties())
        {
            const auto field = slotFieldFromKey(prop.name.toString().toStdString());
            if (!field)
                continue; // unbekannte Schlüssel (neuere Versionen) ignorieren
            if (!isNumber(prop.value))
                return fail(slotLabel(s) + juce::String::fromUTF8(": Wert für \"") + prop.name.toString() + juce::String::fromUTF8("\" ist keine Zahl."));
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
        error = "Ordner konnte nicht angelegt werden: " + file.getParentDirectory().getFullPathName();
        return false;
    }
    if (!file.replaceWithText(kitToJsonString(kit), false, false, "\n"))
    {
        error = "Datei konnte nicht geschrieben werden: " + file.getFullPathName();
        return false;
    }
    return true;
}

KitParseResult loadKitFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return fail("Datei nicht gefunden: " + file.getFullPathName());
    if (file.getSize() > kMaxFileBytes)
        return fail(juce::String::fromUTF8("Die Datei ist zu groß für ein Kit."));
    return kitFromJsonString(file.loadFileAsString());
}

} // namespace dg
