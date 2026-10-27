#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <juce_core/juce_core.h>
#include "plugin/Config.h"

using namespace dg;

namespace {
struct TempDir
{
    juce::File dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("dgcfg", "", false);
    TempDir() { dir.createDirectory(); }
    ~TempDir() { dir.deleteRecursively(); }
};

void writeConfig(const juce::File& f, const juce::String& kitFolder)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("kitFolder", kitFolder);
    f.replaceWithText(juce::JSON::toString(juce::var(obj)));
}

void setTestEnv(const char* name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s(name, value.toRawUTF8());
#else
    setenv(name, value.toRawUTF8(), 1);
#endif
}
} // namespace

TEST_CASE("missing config uses and creates the default folder", "[config]")
{
    TempDir tmp;
    const auto def = tmp.dir.getChildFile("Default");
    const auto info = resolveKitFolder(tmp.dir.getChildFile("none.json"), def);
    CHECK(info.folder == def);
    CHECK(info.warning.isEmpty());
    CHECK(def.isDirectory());
}

TEST_CASE("valid config folder is used", "[config]")
{
    TempDir tmp;
    const auto kits = tmp.dir.getChildFile("MyKits");
    kits.createDirectory();
    const auto cfg = tmp.dir.getChildFile("cfg.json");
    writeConfig(cfg, kits.getFullPathName());
    const auto info = resolveKitFolder(cfg, tmp.dir.getChildFile("Default"));
    CHECK(info.folder == kits);
    CHECK(info.warning.isEmpty());
}

TEST_CASE("empty kitFolder falls back without warning", "[config]")
{
    TempDir tmp;
    const auto cfg = tmp.dir.getChildFile("cfg.json");
    writeConfig(cfg, "");
    const auto def = tmp.dir.getChildFile("Default");
    const auto info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isEmpty());
}

TEST_CASE("invalid config, missing folder and relative path fall back with a warning", "[config]")
{
    TempDir tmp;
    const auto def = tmp.dir.getChildFile("Default");
    const auto cfg = tmp.dir.getChildFile("cfg.json");

    cfg.replaceWithText("{ kaputt");
    auto info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isNotEmpty());
    CHECK(info.warning.contains("invalid"));

    writeConfig(cfg, tmp.dir.getChildFile("gibtsnicht").getFullPathName());
    info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isNotEmpty());

    writeConfig(cfg, "relativ\\ordner");
    info = resolveKitFolder(cfg, def);
    CHECK(info.folder == def);
    CHECK(info.warning.isNotEmpty());
}

TEST_CASE("environment variables are expanded", "[config]")
{
    TempDir tmp;
    const auto value = tmp.dir.getParentDirectory().getFullPathName();
    const auto sep = juce::File::getSeparatorString();
    setTestEnv("DG_TEST_DIR", value);

    CHECK(expandEnvironmentVariables("%DG_TEST_DIR%" + sep + "x") == value + sep + "x");
    CHECK(expandEnvironmentVariables("%DG_DOES_NOT_EXIST_42%" + sep + "x") == "%DG_DOES_NOT_EXIST_42%" + sep + "x");
    CHECK(expandEnvironmentVariables("a%%b") == "a%%b");
    CHECK(expandEnvironmentVariables("kein prozent") == "kein prozent");

    const auto cfg = tmp.dir.getChildFile("cfg.json");
    writeConfig(cfg, "%DG_TEST_DIR%" + sep + tmp.dir.getFileName());
    const auto info = resolveKitFolder(cfg, tmp.dir.getChildFile("Default"));
    CHECK(info.folder == tmp.dir);
    CHECK(info.warning.isEmpty());
}
