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
    # Classified as an instrument (not a "MIDI effect"): Ableton Live refuses
    # VST3s with no audio output and has no slot for third-party MIDI effects,
    # so like other MIDI generators Alea is an instrument with a silent output.
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    IS_MIDI_EFFECT FALSE
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
