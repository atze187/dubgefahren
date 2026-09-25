add_library(dg_engine STATIC
    DspMath.h DspMath.cpp
    SlotParams.h SlotParams.cpp SlotFields.h SlotFields.cpp)
target_include_directories(dg_engine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(dg_engine PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(dg_engine PRIVATE /W4 /permissive-)
endif()
