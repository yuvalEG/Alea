cmake_minimum_required(VERSION 3.22)

project(Alea VERSION 0.3.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download JUCE at configure time, pinned to a fixed release for reproducible builds.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# CLAP support: JUCE has no native CLAP target; this wrapper adds one on top
# of the existing processor (https://github.com/free-audio/clap-juce-extensions).
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        c1a5ad025f95d01e03267857fa8276ebeed16500) # pinned for reproducible builds; first commits with JUCE 9 support
FetchContent_MakeAvailable(clap-juce-extensions)

# Embedded UI assets (header wordmark + UI font, Space Grotesk, SIL OFL)
juce_add_binary_data(AleaAssets SOURCES
    Assets/logo.png
    Assets/fonts/SpaceGrotesk-Medium.ttf)

# Embedded piano multisample (Assets/piano/p<midi>_<soft|hard>.ogg, mono, two
# velocity layers every minor third): Upright Piano KW, a Kawai upright,
# CC0 public domain, prepared by Tools/SamplePrep.cpp. Both products embed it.
file(GLOB ALEA_PIANO_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/piano/*.ogg")
if(NOT ALEA_PIANO_SAMPLES)
    # Bootstrap: build SamplePrep, generate Assets/piano, re-configure. The
    # placeholder keeps the target alive (the engine skips non-sample names).
    message(WARNING "Assets/piano is empty - piano flavour will be silent until SamplePrep runs.")
    set(ALEA_PIANO_SAMPLES "${CMAKE_CURRENT_SOURCE_DIR}/Assets/logo.png")
endif()
juce_add_binary_data(AleaPianoSamples
    NAMESPACE PianoData
    HEADER_NAME PianoData.h
    SOURCES ${ALEA_PIANO_SAMPLES})

# Dev tool: prepares the embedded piano samples from raw flacs (not shipped).
juce_add_console_app(SamplePrep PRODUCT_NAME "SamplePrep")
target_sources(SamplePrep PRIVATE Tools/SamplePrep.cpp)
target_compile_definitions(SamplePrep PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_FLAC=1)
target_link_libraries(SamplePrep
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# App/plugin icon: picked up automatically once Assets/icon.png exists.
set(ALEA_ICON_ARGS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png")
    set(ALEA_ICON_ARGS ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png")
endif()

# Everything both products configure the same way. Only the version define
# differs, so it is the one argument: a change here reaches both plugins,
# which is the whole point of them being one codebase.
function(alea_configure_plugin target version_define)
    target_compile_definitions(${target} PUBLIC
        ${version_define}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(${target}
        PRIVATE
            AleaAssets
            AleaPianoSamples
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

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
    Source/Hardware.cpp
    Source/Sound.cpp
    Source/MidiOut.cpp
    Source/Shifter/Params.cpp
    Source/Shifter/Presets.cpp
    Source/Shifter/PluginProcessor.cpp
    Source/Shifter/UI.cpp
    Source/Shifter/PluginEditor.cpp)

alea_configure_plugin(Alea ALEA_VERSION="${PROJECT_VERSION}")

# CLAP build of the same plugin (for Bitwig, Reaper, and other CLAP hosts).
clap_juce_extensions_plugin(TARGET Alea
    CLAP_ID "com.alea-audio.alea"
    CLAP_FEATURES instrument "note-effect" generator)

# ---------------------------------------------------------------------------
# Alea Chord Randomizer: the second Alea product. Versioned independently of
# Scale Shifter; its release tags use the "chords-v" prefix.
set(CHORDS_VERSION 0.4.1)

set(CHORDS_ICON_ARGS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon-chords.png")
    set(CHORDS_ICON_ARGS ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon-chords.png")
endif()

# Same instrument-with-silent-output classification as Scale Shifter: hosts
# have no universal slot for third-party MIDI effects, and the same
# ALEA_FORMATS / ALEA_COPY_AFTER_BUILD computed once above.

juce_add_plugin(AleaChords
    PRODUCT_NAME "Alea Chord Randomizer"
    VERSION ${CHORDS_VERSION}
    COMPANY_NAME "Alea Audio"
    BUNDLE_ID com.alea-audio.chords
    PLUGIN_MANUFACTURER_CODE Alau
    PLUGIN_CODE Achd
    ${CHORDS_ICON_ARGS}
    FORMATS ${ALEA_FORMATS}
    IS_SYNTH TRUE
    IS_MIDI_EFFECT FALSE
    # MIDI input is declared (and ignored): Live refuses to open VST3
    # instruments that expose no MIDI-in bus.
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD ${ALEA_COPY_AFTER_BUILD})

target_sources(AleaChords PRIVATE
    Source/Hardware.cpp
    Source/Sound.cpp
    Source/MidiOut.cpp
    Source/Chords/ChordEngine.cpp
    Source/Chords/ChordsProcessor.cpp
    Source/Chords/ChordsEditor.cpp)

alea_configure_plugin(AleaChords CHORDS_VERSION="${CHORDS_VERSION}")

# CLAP build of the Chord Randomizer (for Bitwig, Reaper, and other CLAP hosts).
clap_juce_extensions_plugin(TARGET AleaChords
    CLAP_ID "com.alea-audio.chords"
    CLAP_FEATURES instrument "note-effect" generator)

# Dev tool: renders the Chord Randomizer editor to a PNG (not shipped).
juce_add_console_app(ChordsUISnapshot PRODUCT_NAME "ChordsUISnapshot")
target_sources(ChordsUISnapshot PRIVATE
    Tools/ChordsUISnapshot.cpp
    Source/Hardware.cpp
    Source/Sound.cpp
    Source/MidiOut.cpp
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
    PRIVATE AleaAssets AleaPianoSamples juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Dev tool: renders the editor to a PNG for layout checks (not shipped).
juce_add_console_app(AleaUISnapshot PRODUCT_NAME "AleaUISnapshot")
target_sources(AleaUISnapshot PRIVATE
    Tools/UISnapshot.cpp
    Source/Hardware.cpp
    Source/Sound.cpp
    Source/MidiOut.cpp
    Source/Shifter/Params.cpp
    Source/Shifter/Presets.cpp
    Source/Shifter/PluginProcessor.cpp
    Source/Shifter/UI.cpp
    Source/Shifter/PluginEditor.cpp)
target_compile_definitions(AleaUISnapshot PRIVATE
    ALEA_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1  # the tool pumps the message loop for posed shots
    JucePlugin_Name="Alea")
target_link_libraries(AleaUISnapshot
    PRIVATE AleaAssets AleaPianoSamples juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# ---------------------------------------------------------------------------
# Regression checks. Both dev tools carry real pass/fail suites behind flags -
# easy to miss, since the tools are named for the screenshots they also take -
# so ctest is the single command that runs every one of them, and CI fails the
# build on a regression:
#
#   cmake --build build && ctest --test-dir build --output-on-failure
#
# All four are offline and deterministic. AleaUISnapshot --updatetest also
# accepts a "live" argument that queries GitHub, deliberately NOT wired here:
# a check that can fail because a network is down is not a regression check.
enable_testing()

add_test(NAME update_tag_selection    COMMAND AleaUISnapshot --updatetest)
add_test(NAME preset_switch_morph     COMMAND AleaUISnapshot --morphtest)
add_test(NAME free_timing_and_sources COMMAND AleaUISnapshot --freetempotest)
add_test(NAME chord_vocabulary        COMMAND ChordsUISnapshot --vocab)
add_test(NAME pending_swap_history    COMMAND ChordsUISnapshot --swaptest)
add_test(NAME store_poses_chords      COMMAND ChordsUISnapshot --posetest)
add_test(NAME store_poses_shifter     COMMAND AleaUISnapshot --posetest)
add_test(NAME piano_velocity_layers  COMMAND AleaUISnapshot --pianotest)
add_test(NAME chords_velocity         COMMAND ChordsUISnapshot --veltest)
add_test(NAME ear_monitor_hiding     COMMAND ChordsUISnapshot --monitortest)

# The chord audit walks ~250k voicings; the rest are quick.
set_tests_properties(chord_vocabulary PROPERTIES TIMEOUT 900)
