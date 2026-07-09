#pragma once

#include "UI.h"

// Full plugin UI (spec section 9): Scale A/B panels, Timing, Morph, Output.
// Presets panel arrives with the preset milestone.
class AleaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit AleaAudioProcessorEditor (AleaAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void updateModeVisibility();
    void applyPresetAndMark (int index);
    void markPreset (int index); // -1 = nothing selected
    void layoutMain();
    void paintMain (juce::Graphics&);

    void setupSlider (juce::Slider&, const juce::String& paramID, juce::Colour accent, bool positionStyle = false);
    void setupCombo (juce::ComboBox&, const juce::String& paramID, const juce::StringArray& customLabels = {});

    AleaAudioProcessor& alea;

    // All UI lives in a fixed-size view; resizing scales its transform, so
    // the layout code stays in one coordinate system.
    struct MainView : juce::Component
    {
        explicit MainView (AleaAudioProcessorEditor& o) : owner (o) {}
        void paint (juce::Graphics& g) override { owner.paintMain (g); }
        AleaAudioProcessorEditor& owner;
    };
    MainView content { *this };
    int viewHeight = 0;

    // Panel geometry, recomputed on every resize (responsive layout: panels
    // stretch, controls keep their size).
    juce::Rectangle<int> presetsPanel, scaleAPanel, scaleBPanel,
                         timingPanel, morphPanel, outputPanel;

    // Scale panels
    ui::PianoKeyboard keyboardA, keyboardB;
    ui::RestSelector restsA, restsB;
    juce::Slider aOctMin, aOctMax, aVelMin, aVelMax;
    juce::Slider bOctMin, bOctMax, bVelMin, bVelMax;
    juce::ComboBox rootABox, rootBBox; // root pickers: rotate the pitch-class set

    // Timing panel
    ui::SegmentedSelector intervalMode, lengthMode;
    juce::Slider intervalSync, lengthSync; // discrete division sliders
    juce::Slider intervalFree, lengthFree;

    // Morph panel
    ui::MorphBar morphBar;
    juce::TextButton autoSweep { "AUTO-SWEEP" };
    ui::SegmentedSelector morphDurMode, morphMode;
    ui::CurveSelector morphCurve;
    juce::Slider morphDurBars, morphDurFree; // DURATION knob (sync bars / free seconds)
    juce::ComboBox morphDurUnit;             // free-mode unit (Seconds / Minutes)

    // Header
    ui::SegmentedSelector tempoSource;
    juce::Slider internalTempo;
    juce::TextButton menuButton;
    juce::TextButton freezeButton { "FREEZE" };
    juce::TextButton panicButton { "PANIC" };
    ui::TransportButton playButton;        // standalone transport (play / pause)
    juce::HyperlinkButton helpLink;         // plugin only: routing help in the README
    const bool standalone;

    // Output panel
    ui::OutputPanel output;

    // Presets panel: every preset is a bubble; the lit one is active
    std::vector<std::unique_ptr<juce::TextButton>> presetBtns;
    juce::TextButton savePreset { "Save" }, loadPreset { "Load" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<float> presetSnapshot; // param values after a preset settles; divergence clears the mark
    int snapshotCountdown = 0;         // ticks until snapshot capture - lets the host echo edits back first
    int shownPreset = -2;              // last mark painted; -2 forces a sync from the engine on first tick

    // Scale panels dim when the morph is fully on the other side
    float alphaA = 1.0f, alphaB = 1.0f;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sweepAttachment, freezeAttachment;

    juce::TooltipWindow tooltipWindow { this, 400 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AleaAudioProcessorEditor)
};
