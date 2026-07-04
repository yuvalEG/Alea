#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// Milestone 1: a minimal MIDI-effect processor that emits one fixed note
// (C3, MIDI 60) per beat while the host transport is playing. Proves the
// plugin loads and its MIDI output reaches the host, end to end.
class AleaAudioProcessor : public juce::AudioProcessor
{
public:
    AleaAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override                     { return false; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return true; }
    bool isMidiEffect() const override                  { return true; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override   {}

private:
    void sendAllNotesOff (juce::MidiBuffer& midi, int sampleOffset);

    // Spec display convention: middle C = C3 = MIDI note 60. Channel fixed to 1.
    static constexpr int midiChannel   = 1;
    static constexpr int testNote      = 60;
    static constexpr juce::uint8 testVelocity = 100;
    static constexpr double gateInBeats = 0.5;

    int currentNote = -1;        // note currently sounding, -1 = none
    double noteOffPpq = 0.0;     // beat position where the current note ends
    std::int64_t lastBeat = std::numeric_limits<std::int64_t>::min();
    double lastPpqEnd = -1.0;    // beat position at the end of the previous block
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AleaAudioProcessor)
};
