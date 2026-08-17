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

    // Whether the MONITOR must withhold the notes. Pulled out of the editor so
    // it can be tested directly: it is decided from the card that is SOUNDING,
    // and getting that wrong shows the answer to a chord you are meant to be
    // naming - which is the whole exercise, silently broken.
    static bool monitorHidden (bool earMode, int soundingCard,
                               const std::array<bool, 8>& revealed);

    // Whether a reveal is still telling the truth. A reveal is about ONE chord
    // on ONE card, so it lives exactly as long as that card still shows that
    // chord - and not one moment longer.
    //
    // This replaced two attempts at timing the expiry (on the series serial,
    // then also on a pending swap landing). Both were guesses at WHEN the
    // content changes, and each missed a case: the serial moves when a roll
    // fires but the cards flip a chord later, and expiring on the landing
    // killed a deliberate pre-reveal that had nothing to do with the swap.
    // Comparing the chord itself cannot miss a case, and cannot leak: a
    // reveal survives only while the answer it gave is still the right one.
    static bool revealStillValid (const juce::String& revealedChord,
                                  const juce::String& shownChord);

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
        bool hidden = false;      // ear workout (M6): three dots instead of the name
        float progress = 0.0f;
        std::function<void()> onPress, onPinToggle, onReveal;
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::Rectangle<float> pinZone() const;
        // The hidden indicator IS the reveal button, which is what lets the
        // card body go on jumping the loop (spec M6). Three hit zones, one
        // meaning each: pin dot, indicator, card body.
        juce::Rectangle<float> revealZone() const;
    };

    // The monitor: a glass LCD holding a real mini keyboard - white keys in
    // pale metal, the sounding chord's notes lit purple behind the scanlines.
    struct MonitorStrip : juce::Component
    {
        explicit MonitorStrip (ChordsProcessor& p) : proc (p) {}
        ChordsProcessor& proc;
        bool suppressed = false;  // ear workout: keys stay dark until the sounding card is revealed
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
    // LEVEL is internal-synth chrome and comes and goes with it (lastSynthOn).
    // VELOCITY is always shown: the velocity it sets rides the MIDI going to
    // the host and to a MIDI device too, so hiding it would hide a control
    // that is still having an effect.
    juce::Slider volKnob, velKnob;
    juce::Array<juce::MidiDeviceInfo> devices;
    HistoryTicker ticker;
    MonitorStrip monitor;
    juce::OwnedArray<ChordCard> cards;

    // Ear workout (spec M6). Reveals live here, not in the processor: they are
    // a property of what you are looking at, not of what is playing. Each one
    // is derived from revealedChord below on every refresh, so it ends by
    // itself the moment its card shows something else.
    std::array<bool, 8> revealed { };
    // WHICH chord each reveal was for. A reveal is only valid while the card
    // still shows this exact text, which is what makes the lifetime immune to
    // when the series changes underneath it.
    std::array<juce::String, 8> revealedChord;
    void setEarMode (bool on);

    juce::Rectangle<int> chordsPanel, loopPanel, monitorPanel, historyPanel; // module plates
    juce::Rectangle<int> meterRect;   // beside the knob when the synth is on
    float meterLevel = 0.0f;          // falling peak
    bool lastSynthOn = true;
    bool lastPlaying = false;
    bool lastPending = false;
    bool lastAutoOn = false;
    float rollLit = 0.0f;             // ROLL flashes lit on a roll (manual or auto)
    int seenRollSerial = 0;           // last processor roll the key flashed for
    juce::uint64 lastSoundingHi = 0;
    juce::uint64 lastSounding = 0;
    int devicePollCountdown = 90;     // ~3s at 30 Hz: MIDI hotplug refresh

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsEditor)
};
