add_executable(DubgefahrenTests
    TestHelpers.h
    test_DspMath.cpp
    test_SlotFields.cpp
    test_Oscillator.cpp
    test_Lfo.cpp
    test_Envelope.cpp
    test_SirenVoice.cpp)
target_link_libraries(DubgefahrenTests PRIVATE dg_engine Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(DubgefahrenTests)
