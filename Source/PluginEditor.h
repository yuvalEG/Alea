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

    // Output panel
    ui::OutputPanel output;

    // Presets row
    juce::ComboBox presetBox;
    juce::TextButton savePreset { "Save" }, loadPreset { "Load" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<float> presetSnapshot; // param values right after a preset applied; divergence clears the mark

    // Scale panels dim when the morph is fully on the other side
    float alphaA = 1.0f, alphaB = 1.0f;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sweepAttachment, freezeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AleaAudioProcessorEditor)
};
