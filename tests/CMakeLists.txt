add_executable(DubgefahrenTests
    TestHelpers.h
    test_DspMath.cpp
    test_SlotFields.cpp
    test_Oscillator.cpp
    test_Lfo.cpp
    test_Envelope.cpp
    test_SirenVoice.cpp
    test_PadRouter.cpp
    test_FxUnits.cpp
    test_TapeDelay.cpp
    test_SpringReverb.cpp
    test_FxChain.cpp
    test_Kit.cpp
    test_Engine.cpp)
target_link_libraries(DubgefahrenTests PRIVATE dg_engine Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(DubgefahrenTests)

juce_add_console_app(DubgefahrenPluginTests PRODUCT_NAME "DubgefahrenPluginTests")
target_sources(DubgefahrenPluginTests PRIVATE plugin/test_PluginProcessor.cpp)
target_link_libraries(DubgefahrenPluginTests PRIVATE dg_plugin_shared Catch2::Catch2WithMain)
catch_discover_tests(DubgefahrenPluginTests)
