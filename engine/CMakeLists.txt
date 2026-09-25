add_library(dg_engine STATIC
    DspMath.h DspMath.cpp
    SlotParams.h SlotParams.cpp SlotFields.h SlotFields.cpp
    Oscillator.h Oscillator.cpp
    Lfo.h Lfo.cpp
    Envelope.h Envelope.cpp
    SoundSource.h SirenVoice.h SirenVoice.cpp
    PadRouter.h PadRouter.cpp
    Drive.h SvFilter.h SvFilter.cpp Limiter.h
    FxParams.h FxParams.cpp TapeDelay.h TapeDelay.cpp
    SpringReverb.h SpringReverb.cpp
    FxChain.h FxChain.cpp
    Kit.h Kit.cpp
    Engine.h Engine.cpp)
target_include_directories(dg_engine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(dg_engine PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(dg_engine PRIVATE /W4 /permissive-)
endif()
