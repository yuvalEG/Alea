#pragma once

#include "ChordsProcessor.h"
#include "../Hardware.h"

// Chord Randomizer UI, hardware faceplate edition (design handoff July 2026):
// Alea header with transport, a series row of backlit glass chord pads, the
// CHORDS and LOOP module plates, the LCD keyboard monitor and the engraved
// history plate. Built entirely from the family's shared hardware primitives
// (Hardware.h) so both Alea products stay visually identical.
class ChordsEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit ChordsEditor (ChordsProcessor&);
    ~ChordsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void refresh();   // pull processor state into the widgets
    void doRoll();
    void showMenu();
    void updateCardFonts();   // one shared fitted size for the whole series
    void buildOutputBox();
    void applyAutoGate();     // EVERY dims to 35% while AUTO is off (DS rule)

    // One big backlit glass pad per chord (design .hw-card): a dark purple
    // glass face with scanlines and gloss, the chord name lit in the scale
    // purple while sounding, a bottom progress strip, and a pin dot top-right.
    // The font size is set by the editor: every card in a series shares the
    // smallest fitted size, so C#Maj7 and A7 never render at different scales.
    // Clicking any card jumps the loop there.
    struct ChordCard : juce::Component
    {
        juce::String text;
        float fontSize = 40.0f;
        bool active = false;
        bool clickable = false;   // only while the loop plays
        bool incoming = false;    // pending swap: this chord arrives at the boundary (cyan)
        bool pinned = false;      // survives rolls
        float progress = 0.0f;
        std::function<void()> onPress, onPinToggle;
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::Rectangle<float> pinZone() const;
    };

    // The monitor: a glass LCD holding a real mini keyboard - white keys in
    // pale metal, the sounding chord's notes lit purple behind the scanlines.
    struct MonitorStrip : juce::Component
    {
        explicit MonitorStrip (ChordsProcessor& p) : proc (p) {}
        ChordsProcessor& proc;
        void paint (juce::Graphics&) override;
    };

    // Past rolls, newest at the right, grouped by roll, engraved into the
    // plate and fading with age. Scrollable (wheel, trackpad, drag, or the
    // edge page buttons); clicking a roll recalls it into the series row.
    // History holds ~1000 chords. The plate + title are painted by the editor.
    struct HistoryTicker : juce::Component
    {
        explicit HistoryTicker (ChordsProcessor& p) : proc (p) {}
        ChordsProcessor& proc;
        std::function<void (int)> onRecall;

        float scroll = 0.0f;        // 0 = pinned to the newest roll; grows into the past
        float maxScroll = 0.0f;     // measured during paint
        float dragStartScroll = 0.0f;
        int dragStartX = 0;
        int hoveredGroup = -1;

        // Hit rects from the last paint, in ticker coordinates.
        std::vector<std::pair<juce::Rectangle<float>, int>> groupRects;
        juce::Rectangle<float> olderButton, newerButton;

        void paint (juce::Graphics&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        int groupAt (juce::Point<float>) const;
        void scrollBy (float delta);
    };

    ChordsProcessor& chordsProc;
    const bool standalone;
    int seenRevision = -1;

    juce::Image logo;
    ui::TransportButton playButton;
    juce::TextButton menuButton,
                     rollButton { "Roll" },     // the hero key: flashes lit on a roll
                     panicButton { "Panic" };   // red legend, never a red fill
    // Backlit keys whose lit state crossfades: FREEZE (ice), CLICK (white),
    // AUTO (cyan - the automatic version of ROLL).
    ui::AnimatedButton freezeButton { "Freeze" }, clickButton { "Click" },
                       autoButton { "Auto" };
    juce::Slider tempoBox;                      // the glass BPM LCD (drag to set)
    // Control language (QA round 11): buttons act, toggles flip independent
    // options, segments pick one-of-N, dropdowns hold lists. Toggle labels are
    // sentence case; ALL-CAPS is for captions/titles/buttons.
    ui::AnimatedToggle simplifyToggle { "Simplify" },
                       susToggle { "Add sus" },
                       keyLockToggle { "Key lock" },
                       // Voicings (spec M5) live in LOOP - how it sounds, never what rolls.
                       smoothToggle { "Smooth" },
                       bassToggle { "Add bass note" };
    juce::ComboBox keyBox, scaleBox;            // key lock: tonic + scale type
    juce::Slider clickVolKnob;                  // click level, beside CLICK
    juce::HyperlinkButton helpLink;             // plugin only: routing help in the README
    // The dice rows share the family segmented switch; octave is multi-select
    // (a bitmask, never empty) - chords may land in several octaves at once.
    ui::SegmentedSelector lengthRow,            // NUMBER OF CHORDS 1-8
                          typeRow,              // CHORD TYPE: triads / 7ths / 9ths
                          everyRow,             // AUTO cadence: every 1 / 2 / 4 loops
                          barsRow, octaveRow,
                          voicingRow;           // VOICING: close / open spacing
    juce::ComboBox outputBox;
    void rebuildKeyBox();
    juce::Slider volKnob;
    juce::Array<juce::MidiDeviceInfo> devices;
    HistoryTicker ticker;
    MonitorStrip monitor;
    juce::OwnedArray<ChordCard> cards;

    juce::Rectangle<int> chordsPanel, loopPanel, monitorPanel, historyPanel; // module plates
    juce::Rectangle<int> meterRect;   // beside the knob when the synth is on
    float meterLevel = 0.0f;          // falling peak
    bool lastSynthOn = true;
    bool lastPlaying = false;
    bool lastPending = false;
    bool lastAutoOn = false;
    float rollLit = 0.0f;             // ROLL flashes lit on a roll (manual or auto)
    juce::uint64 lastSoundingHi = 0;
    juce::uint64 lastSounding = 0;
    int devicePollCountdown = 90;     // ~3s at 30 Hz: MIDI hotplug refresh

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsEditor)
};
