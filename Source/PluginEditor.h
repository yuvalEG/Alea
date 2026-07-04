#pragma once

#include "PluginProcessor.h"

// Milestone 1 debug window: read-only view of what the processor sees
// (transport state, tempo, beat, notes sent). Not the real UI.
class AleaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit AleaAudioProcessorEditor (AleaAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override { repaint(); }

    AleaAudioProcessor& alea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AleaAudioProcessorEditor)
};
