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

    void setupSlider (juce::Slider&, const juce::String& paramID, juce::Colour accent);
    void setupCombo (juce::ComboBox&, const juce::String& paramID);

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
    juce::ToggleButton autoSweep { "Auto-Sweep" };
    ui::SegmentedSelector morphDurMode;
    juce::ComboBox morphDurBars, morphDurUnit, morphMode, morphCurve;
    juce::Slider morphDurFree;

    // Header
    ui::SegmentedSelector tempoSource;
    juce::Slider internalTempo;

    // Output panel
    ui::OutputPanel output;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> sweepAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AleaAudioProcessorEditor)
};
