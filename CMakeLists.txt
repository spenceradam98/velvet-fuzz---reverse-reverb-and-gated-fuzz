cmake_minimum_required(VERSION 3.22)

# macOS 11 (Big Sur) on Intel is the minimum; build universal (x86_64 + arm64).
# No -march flags anywhere: the DSP is scalar C++ and runs on any x86-64 (no AVX2 needed).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no VC++ redist needed
endif()
project(VelvetFuzz VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core engine (no JUCE dependency, unit-testable) ----
add_library(velvetfuzz_engine STATIC Source/engine/Engine.cpp Source/engine/Presets.cpp)
target_include_directories(velvetfuzz_engine PUBLIC Source/engine)
set_target_properties(velvetfuzz_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(VELVETFUZZ_BUILD_TESTS "Build engine tests" ON)
if(VELVETFUZZ_BUILD_TESTS)
    add_executable(velvetfuzz_tests tests/EngineTests.cpp)
    target_link_libraries(velvetfuzz_tests PRIVATE velvetfuzz_engine)
    enable_testing()
    add_test(NAME engine COMMAND velvetfuzz_tests)
endif()

option(VELVETFUZZ_BUILD_PLUGIN "Build the JUCE plugin" ON)
if(NOT VELVETFUZZ_BUILD_PLUGIN)
    return()
endif()

# ---- JUCE ----
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VELVETFUZZ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VELVETFUZZ_FORMATS AU)
endif()

juce_add_plugin(VelvetFuzz
    COMPANY_NAME "Velvet Fuzz Audio"
    PLUGIN_MANUFACTURER_CODE Vlvf
    PLUGIN_CODE Vfz1
    FORMATS ${VELVETFUZZ_FORMATS}
    PRODUCT_NAME "Velvet Fuzz"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(VelvetFuzz PRIVATE
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/plugin/Themes.cpp)

target_compile_definitions(VelvetFuzz PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VelvetFuzz
    PRIVATE
        velvetfuzz_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- UI screenshot tool (docs only; on Linux run under xvfb-run) ----
option(VELVETFUZZ_BUILD_SNAPSHOTS "Build the UI screenshot tool" OFF)
if(VELVETFUZZ_BUILD_SNAPSHOTS)
    juce_add_console_app(velvetfuzz_snapshots PRODUCT_NAME "Velvet Fuzz Snapshots")
    target_sources(velvetfuzz_snapshots PRIVATE tools/Snapshots.cpp
        Source/plugin/PluginProcessor.cpp Source/plugin/PluginEditor.cpp Source/plugin/Themes.cpp)
    target_compile_definitions(velvetfuzz_snapshots PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JucePlugin_Name="Velvet Fuzz" JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(velvetfuzz_snapshots PRIVATE velvetfuzz_engine juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
