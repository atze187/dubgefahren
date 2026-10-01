cmake_minimum_required(VERSION 3.25)
project(Dubgefahren VERSION 0.5.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW ON)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.0
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE Catch2)
# Der Ordner muss wie das Modul heißen: JUCE liest den Modulnamen aus dem Ordnernamen.
FetchContent_Declare(melatonin_blur
    GIT_REPOSITORY https://github.com/sudara/melatonin_blur.git
    GIT_TAG v1.4
    GIT_SHALLOW ON
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/melatonin_blur)
FetchContent_MakeAvailable(melatonin_blur)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

enable_testing()
add_subdirectory(engine)
add_subdirectory(plugin)
add_subdirectory(tests)
