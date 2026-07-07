#pragma once

#include "ChordsProcessor.h"

// Chord Randomizer UI (spec section 7): Alea header with transport, the
// series row of big chord cards, a controls row, and the history ticker.
// Deliberately shares the Scale Shifter design language - palette, Space
// Grotesk, header layout, OUT chooser, knob and meter.
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

    // One big chord name on a panel card - the heir of the old 75 pt label.
    // The font size is set by the editor: every card in a series shares the
    // smallest fitted size, so C#Maj7 and A7 never render at different scales.
    // During playback the sounding card lights up with a bar-progress strip,
    // and clicking any card jumps the loop there.
    struct ChordCard : juce::Component
    {
        juce::String text;
        float fontSize = 40.0f;
        bool active = false;
        bool clickable = false;   // only while the loop plays
        bool incoming = false;    // pending swap: this chord arrives at the boundary (amber)
        bool pinned = false;      // survives rolls
        float progress = 0.0f;
        std::function<void()> onPress, onPinToggle;
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::Rectangle<float> pinZone() const;
    };

    // Scale Shifter's 88-key monitor strip, single-color: the notes of the
    // sounding chord light up in playing green.
    struct MonitorStrip : juce::Component
    {
        explicit MonitorStrip (ChordsProcessor& p) : proc (p) {}
        ChordsProcessor& proc;
        void paint (juce::Graphics&) override;
    };

    // A row of labeled segments; used for series length (1-8), bars per
    // chord (1/2/4) and the octave picker. Same look as the family's mode
    // selectors. In multi mode segments toggle independently (a bitmask,
    // never empty) - the octave picker lets chords land in several octaves.
    struct SegmentRow : juce::Component
    {
        juce::StringArray labels;
        int selected = 0;                       // single mode: index into labels
        bool multi = false;
        int mask = 1;                           // multi mode: bit per label
        std::function<void (int)> onChange;     // index, or the new mask in multi mode
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    };

    // Past rolls, newest at the right, grouped by roll, fading with age.
    // Scrollable (wheel, trackpad, drag, or the edge page buttons); clicking
    // a roll recalls it into the series row. History holds ~1000 chords.
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
    int seenRevision = -1;

    juce::Image logo;
    juce::TextButton menuButton, rollButton { "ROLL" }, playButton { "PLAY" },
                     freezeButton { "FREEZE" }, panicButton { "PANIC" };
    juce::Slider tempoBox;
    juce::ToggleButton simplifyToggle { "Simplify Chords" },
                       susToggle { "Sus chords" },
                       keyLockToggle { "Key lock" },
                       autoRollToggle { "Auto roll after" };
    juce::ComboBox keyBox, scaleBox;            // key lock: tonic + scale type
    juce::TextButton clickButton { "CLICK" };   // metronome, next to the tempo
    juce::Slider clickVolKnob;                  // click level, beside CLICK
    juce::ComboBox autoRollBox;
    SegmentRow lengthRow, barsRow, octaveRow,
               extRow;                          // triads / 7ths / 9ths (a 9th presumes its 7th)
    juce::ComboBox outputBox;
    void rebuildKeyBox();
    juce::Slider volKnob;
    juce::Array<juce::MidiDeviceInfo> devices;
    HistoryTicker ticker;
    MonitorStrip monitor;
    juce::OwnedArray<ChordCard> cards;

    juce::Rectangle<int> dicePanel, loopPanel;   // titled control blocks
    juce::Rectangle<int> meterRect;   // beside the knob when the synth is on
    float meterLevel = 0.0f;          // falling peak
    bool lastSynthOn = true;
    bool lastPlaying = false;
    bool lastPending = false;
    juce::uint64 lastSoundingHi = 0;
    juce::uint64 lastSounding = 0;
    int devicePollCountdown = 90;     // ~3s at 30 Hz: MIDI hotplug refresh

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsEditor)
};
