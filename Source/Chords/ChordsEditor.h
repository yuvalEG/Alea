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
    void updateCardFonts();   // one shared fitted size for the whole series

    // One big chord name on a panel card - the heir of the old 75 pt label.
    // The font size is set by the editor: every card in a series shares the
    // smallest fitted size, so C#Maj7 and A7 never render at different scales.
    struct ChordCard : juce::Component
    {
        juce::String text;
        float fontSize = 40.0f;
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
    juce::TextButton menuButton, rollButton { "ROLL" };
    juce::ToggleButton seventhToggle { "Use Seventh Chords" },
                       simplifyToggle { "Simplify Chords" };
    LengthSelector lengthSelector;
    HistoryTicker ticker;
    juce::OwnedArray<ChordCard> cards;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsEditor)
};
