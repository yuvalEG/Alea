#pragma once

#include "ChordsProcessor.h"

// Chord Randomizer UI (spec section 7): Alea header, the series row of big
// chord cards, a controls row, and the history ticker. Deliberately shares
// the Scale Shifter design language - palette, Space Grotesk, header layout.
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

    // One big chord name on a panel card - the heir of the old 75 pt label.
    struct ChordCard : juce::Component
    {
        juce::String text;
        void paint (juce::Graphics&) override;
    };

    // Series length: eight segments, 1-8, styled like the mode selectors.
    struct LengthSelector : juce::Component
    {
        int value = 4;
        std::function<void (int)> onChange;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    };

    // Past rolls, newest at the right, grouped by roll, fading with age.
    struct HistoryTicker : juce::Component
    {
        explicit HistoryTicker (ChordsProcessor& p) : proc (p) {}
        ChordsProcessor& proc;
        void paint (juce::Graphics&) override;
    };

    ChordsProcessor& chordsProc;
    int seenRevision = -1;

    juce::Image logo;
    juce::TextButton menuButton, rollButton { "ROLL" };
    juce::ToggleButton seventhToggle { "Use Seventh Chords" },
                       simplifyToggle { "Simplify Chords" };
    LengthSelector lengthSelector;
    HistoryTicker ticker;
    juce::OwnedArray<ChordCard> cards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsEditor)
};
