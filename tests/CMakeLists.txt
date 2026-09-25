add_executable(DubgefahrenTests
    TestHelpers.h
    test_DspMath.cpp
    test_SlotFields.cpp)
target_link_libraries(DubgefahrenTests PRIVATE dg_engine Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(DubgefahrenTests)
