add_library(dg_engine STATIC
    DspMath.h DspMath.cpp
    SlotParams.h SlotParams.cpp SlotFields.h SlotFields.cpp
    Oscillator.h Oscillator.cpp
    Lfo.h Lfo.cpp
    Envelope.h Envelope.cpp
    SoundSource.h SampleData.h SampleRegion.h SampleInterp.h GrainStretch.h GrainStretch.cpp SirenVoice.h SirenVoice.cpp SamplePlayer.h SamplePlayer.cpp
    PadRouter.h PadRouter.cpp NoteMap.h NoteMap.cpp
    Saturator.h Saturator.cpp SvFilter.h SvFilter.cpp Limiter.h
    FxParams.h FxParams.cpp Macros.h Macros.cpp Phaser.h Phaser.cpp TapeDelay.h TapeDelay.cpp
    SpringReverb.h SpringReverb.cpp
    FxChain.h FxChain.cpp
    Kit.h Kit.cpp
    Engine.h Engine.cpp)
target_include_directories(dg_engine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(dg_engine PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(dg_engine PRIVATE /W4 /permissive-)
endif()
# The engine is linked into the plugin's shared library, so it must be position independent. Without -fPIC
# GCC assumes local binding, keeps values in registers across calls that go through the PLT, and the dynamic
# linker's lazy-binding stub clobbers them (segfault in SpringReverb::process on Linux).
set_target_properties(dg_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
