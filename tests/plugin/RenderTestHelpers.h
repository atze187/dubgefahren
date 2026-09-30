#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace dgtest {

inline juce::Image snapshot(juce::Component& c, float scale = 1.0f)
{
    return c.createComponentSnapshot(c.getLocalBounds(), true, scale);
}

// Schreibt das Bild nur, wenn DG_SNAPSHOT_DIR gesetzt ist (Sichtprüfung von Hand).
inline void savePng(const juce::Image& img, const juce::String& name)
{
    const auto dir = juce::SystemStats::getEnvironmentVariable("DG_SNAPSHOT_DIR", {});
    if (dir.isEmpty() || !img.isValid())
        return;
    const auto file = juce::File(dir).getChildFile(name + ".png");
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    if (auto out = file.createOutputStream())
        juce::PNGImageFormat().writeImageToStream(img, *out);
}

inline bool hasVisiblePixel(const juce::Image& img)
{
    for (int y = 0; y < img.getHeight(); ++y)
        for (int x = 0; x < img.getWidth(); ++x)
            if (img.getPixelAt(x, y).getAlpha() > 0)
                return true;
    return false;
}

} // namespace dgtest
