cmake_minimum_required(VERSION 3.22)

project(Alea VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download JUCE at configure time, pinned to a fixed release for reproducible builds.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Alea
    PRODUCT_NAME "Alea"
    COMPANY_NAME "Alea Audio"
    BUNDLE_ID com.alea-audio.alea
    PLUGIN_MANUFACTURER_CODE Alau
    PLUGIN_CODE Alea
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(Alea PRIVATE
    Source/PluginProcessor.cpp)

target_compile_definitions(Alea PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Alea
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
