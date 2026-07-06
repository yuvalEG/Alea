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

private:
    void timerCallback() override;
    void updateModeVisibility();
    void applyPresetAndMark (int index);
    void markPreset (int index); // -1 = nothing selected

    void setupSlider (juce::Slider&, const juce::String& paramID, juce::Colour accent, bool positionStyle = false);
    void setupCombo (juce::ComboBox&, const juce::String& paramID, const juce::StringArray& customLabels = {});

    AleaAudioProcessor& alea;

    // Scale panels
    ui::PianoKeyboard keyboardA, keyboardB;
    ui::RestSelector restsA, restsB;
    juce::Slider aOctMin, aOctMax, aVelMin, aVelMax;
    juce::Slider bOctMin, bOctMax, bVelMin, bVelMax;

    // Timing panel
    ui::SegmentedSelector intervalMode, lengthMode;
    juce::ComboBox intervalSync, lengthSync;
    juce::Slider intervalFree, lengthFree;

    // Morph panel
    ui::MorphBar morphBar;
    juce::TextButton autoSweep { "AUTO-SWEEP" };
    ui::SegmentedSelector morphDurMode, morphMode;
    ui::CurveSelector morphCurve;
    juce::ComboBox morphDurBars, morphDurUnit;
    juce::Slider morphDurFree;

    // Header
    ui::SegmentedSelector tempoSource;
    juce::Slider internalTempo;
    juce::TextButton menuButton;
    juce::TextButton freezeButton { "FREEZE" };
    juce::TextButton panicButton { "PANIC" };
    juce::TextButton playButton { "PLAY" }; // standalone transport
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
