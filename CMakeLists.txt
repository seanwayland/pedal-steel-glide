cmake_minimum_required(VERSION 3.22)

project(PedalSteelGlide VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# --- JUCE ---------------------------------------------------------------------
# Pulled in with FetchContent so a fork just needs CMake + a compiler + the
# platform's usual audio-plugin build deps. No submodules to init.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# --- the plugin --------------------------------------------------------------
juce_add_plugin(PedalSteelGlide
    PLUGIN_NAME "Pedal Steel Glide"
    COMPANY_NAME "Wayland Audio"
    BUNDLE_ID "com.seanwayland.pedalsteelglide"
    PLUGIN_MANUFACTURER_CODE Wayl
    PLUGIN_CODE Psgl
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_SYNTH FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Pedal Steel Glide")

target_sources(PedalSteelGlide PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp)

# Keep JUCE lean - no curl / webview / splash. These must be set on EVERY
# target that compiles a JUCE module (the test links juce_core too), or that
# target's juce_core still pulls in libcurl at link time.
set(PSG_JUCE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_compile_definitions(PedalSteelGlide PUBLIC ${PSG_JUCE_DEFS})

target_link_libraries(PedalSteelGlide PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# --- tests ------------------------------------------------------------------
option(PSG_BUILD_TESTS "Build the GlideEngine test" ON)
if(PSG_BUILD_TESTS)
    add_executable(PsgTests tests/EngineTests.cpp)
    target_compile_definitions(PsgTests PRIVATE ${PSG_JUCE_DEFS})
    target_link_libraries(PsgTests PRIVATE juce::juce_audio_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_include_directories(PsgTests PRIVATE source)
    enable_testing()
    add_test(NAME PsgTests COMMAND PsgTests)
endif()
