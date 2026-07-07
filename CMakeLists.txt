cmake_minimum_required(VERSION 3.22)

project(Alea VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download JUCE at configure time, pinned to a fixed release for reproducible builds.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# CLAP support: JUCE has no native CLAP target; this wrapper adds one on top
# of the existing processor (https://github.com/free-audio/clap-juce-extensions).
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        16e9d4ca7b1e86c76e04584b2c08e85a764bcda8) # pinned for reproducible builds
FetchContent_MakeAvailable(clap-juce-extensions)

# Embedded UI assets (header wordmark + UI font, Space Grotesk, SIL OFL)
juce_add_binary_data(AleaAssets SOURCES
    Assets/logo.png
    Assets/fonts/SpaceGrotesk-Medium.ttf)

# App/plugin icon: picked up automatically once Assets/icon.png exists.
set(ALEA_ICON_ARGS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png")
    set(ALEA_ICON_ARGS ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png")
endif()

# AU exists only on Apple platforms; plugin auto-copy needs admin rights on
# Windows, so it stays a macOS convenience.
if(APPLE)
    set(ALEA_FORMATS VST3 AU Standalone)
    set(ALEA_COPY_AFTER_BUILD TRUE)
else()
    set(ALEA_FORMATS VST3 Standalone)
    set(ALEA_COPY_AFTER_BUILD FALSE)
endif()

juce_add_plugin(Alea
    PRODUCT_NAME "Alea Scale Shifter"
    COMPANY_NAME "Alea Audio"
    BUNDLE_ID com.alea-audio.alea
    PLUGIN_MANUFACTURER_CODE Alau
    PLUGIN_CODE Alea
    ${ALEA_ICON_ARGS}
    # Classified as an instrument (not a "MIDI effect"): Ableton Live refuses
    # VST3s with no audio output and has no slot for third-party MIDI effects,
    # so like other MIDI generators Alea is an instrument with a silent output.
    FORMATS ${ALEA_FORMATS}
    IS_SYNTH TRUE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD ${ALEA_COPY_AFTER_BUILD})

target_sources(Alea PRIVATE
    Source/Params.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/UI.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Alea PUBLIC
    ALEA_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Alea
    PRIVATE
        AleaAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# CLAP build of the same plugin (for Bitwig, Reaper, and other CLAP hosts).
clap_juce_extensions_plugin(TARGET Alea
    CLAP_ID "com.alea-audio.alea"
    CLAP_FEATURES instrument "note-effect" generator)

# ---------------------------------------------------------------------------
# Alea Chord Randomizer: the second Alea product. Standalone-only until its
# M4 milestone brings plugin formats. Versioned independently of Scale
# Shifter; its release tags use the "chords-v" prefix.
set(CHORDS_VERSION 0.1.0)

set(CHORDS_ICON_ARGS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon-chords.png")
    set(CHORDS_ICON_ARGS ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon-chords.png")
endif()

# Same instrument-with-silent-output classification as Scale Shifter: hosts
# have no universal slot for third-party MIDI effects.
if(APPLE)
    set(CHORDS_FORMATS VST3 AU Standalone)
    set(CHORDS_COPY_AFTER_BUILD TRUE)
else()
    set(CHORDS_FORMATS VST3 Standalone)
    set(CHORDS_COPY_AFTER_BUILD FALSE)
endif()

juce_add_plugin(AleaChords
    PRODUCT_NAME "Alea Chord Randomizer"
    VERSION ${CHORDS_VERSION}
    COMPANY_NAME "Alea Audio"
    BUNDLE_ID com.alea-audio.chords
    PLUGIN_MANUFACTURER_CODE Alau
    PLUGIN_CODE Achd
    ${CHORDS_ICON_ARGS}
    FORMATS ${CHORDS_FORMATS}
    IS_SYNTH TRUE
    IS_MIDI_EFFECT FALSE
    # MIDI input is declared (and ignored): Live refuses to open VST3
    # instruments that expose no MIDI-in bus.
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD ${CHORDS_COPY_AFTER_BUILD})

target_sources(AleaChords PRIVATE
    Source/Chords/ChordEngine.cpp
    Source/Chords/ChordsProcessor.cpp
    Source/Chords/ChordsEditor.cpp)

target_compile_definitions(AleaChords PUBLIC
    CHORDS_VERSION="${CHORDS_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(AleaChords
    PRIVATE
        AleaAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# CLAP build of the Chord Randomizer (for Bitwig, Reaper, and other CLAP hosts).
clap_juce_extensions_plugin(TARGET AleaChords
    CLAP_ID "com.alea-audio.chords"
    CLAP_FEATURES instrument "note-effect" generator)

# Dev tool: renders the Chord Randomizer editor to a PNG (not shipped).
juce_add_console_app(ChordsUISnapshot PRODUCT_NAME "ChordsUISnapshot")
target_sources(ChordsUISnapshot PRIVATE
    Tools/ChordsUISnapshot.cpp
    Source/Chords/ChordEngine.cpp
    Source/Chords/ChordsProcessor.cpp
    Source/Chords/ChordsEditor.cpp)
target_compile_definitions(ChordsUISnapshot PRIVATE
    CHORDS_VERSION="${CHORDS_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1  # the tool pumps the message loop for posed shots
    JucePlugin_Name="Alea Chord Randomizer")
target_link_libraries(ChordsUISnapshot
    PRIVATE AleaAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Dev tool: renders the editor to a PNG for layout checks (not shipped).
juce_add_console_app(AleaUISnapshot PRODUCT_NAME "AleaUISnapshot")
target_sources(AleaUISnapshot PRIVATE
    Tools/UISnapshot.cpp
    Source/Params.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/UI.cpp
    Source/PluginEditor.cpp)
target_compile_definitions(AleaUISnapshot PRIVATE
    ALEA_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="Alea")
target_link_libraries(AleaUISnapshot
    PRIVATE AleaAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)
