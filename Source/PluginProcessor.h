#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Params.h"

// Alea: generates a random monophonic MIDI note stream from Scale A,
// morphing toward Scale B (spec sections 5-8). Timing is driven by the host
// playhead (or an internal beat clock in Free-Run), sample-accurately.
class AleaAudioProcessor : public juce::AudioProcessor
{
public:
    AleaAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return JucePlugin_Name; }
    bool acceptsMidi() const override                   { return true; }
    bool producesMidi() const override                  { return true; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read-only status for the editor; written from the audio thread.
    std::atomic<bool>   hostIsPlaying { false };
    std::atomic<double> hostBpm  { 0.0 };
    std::atomic<double> hostPpq  { 0.0 };
    std::atomic<int>    notesSent { 0 };
    std::atomic<double> morphPercent { 0.0 };
    std::atomic<int>    lastNote { -1 };
    std::atomic<int>    activeNote { -1 };    // sounding right now (-1 = none)
    std::atomic<int>    activeSource { 0 };   // 0 = Scale A, 1 = Scale B (spec 5.4)
    std::atomic<int>    activeRest { -1 };    // rest slot (0-4) currently "sounding"
    std::atomic<int>    activeRestSource { 0 };
    std::atomic<int>    lastRandomInterval { -1 }; // pool index picked by Random mode
    std::atomic<int>    lastRandomLength { -1 };
    std::atomic<bool>   panicRequested { false };
    std::atomic<float>  scrubRequest { -1.0f };  // 0-100: re-anchor auto-sweep here
    std::atomic<bool>   ccLearnArmed { false };  // next incoming CC binds Morph Position
    std::atomic<int>    morphCC { -1 };          // learned controller number

    // Note history ring buffer (spec 9.1: last 50 notes). Entries pack
    // note | (source << 8); total written count in historyCount.
    static constexpr int historyCapacity = 64;
    std::array<std::atomic<int>, historyCapacity> history {};
    std::atomic<int> historyCount { 0 };

private:
    // Per-block copy of one scale's settings, cheap to read per event.
    struct ScaleSnapshot
    {
        int pitchClasses[12]; int numPitchClasses = 0;
        int rests[5];         int numRests = 0;
        int octMin = 0, octMax = 0, velMin = 1, velMax = 127;
    };

    // Cached raw-value pointers into the APVTS (audio-thread safe).
    struct ScaleRefs
    {
        std::atomic<float>* notes[12] {};
        std::atomic<float>* rests[5] {};
        std::atomic<float>* octMin {}; std::atomic<float>* octMax {};
        std::atomic<float>* velMin {}; std::atomic<float>* velMax {};
    };

    void cacheScaleRefs (char scale, ScaleRefs&);
    void readSnapshot (const ScaleRefs&, ScaleSnapshot&) const;

    double morphAt (double ppq, double bpm) const;      // 0..1
    double sweepLegPpq (double bpm) const;              // one A->B leg, in beats
    double intervalPpqAt (double bpm);                  // gap to next event, in beats
    double lengthPpqAt (double bpm);                    // gate length, in beats
    void sendAllNotesOff (juce::MidiBuffer&, int sampleOffset);
    void resetSchedule (double ppq);

    ScaleRefs refA, refB;
    std::atomic<float>* raw (const char* id) { return apvts.getRawParameterValue (id); }
    std::atomic<float> *pIntervalMode {}, *pIntervalSync {}, *pIntervalFree {},
                       *pLengthMode {}, *pLengthSync {}, *pLengthFree {},
                       *pMorphPos {}, *pAutoSweep {}, *pMorphDurMode {}, *pMorphDurBars {},
                       *pMorphDurFree {}, *pMorphDurUnit {}, *pMorphMode {}, *pMorphCurve {},
                       *pTempoSource {}, *pInternalTempo {}, *pFreeze {};

    juce::Random rng;

    static constexpr int midiChannel = 1;

    double nextEventPpq = 0.0;   // when the next pick-pool draw happens
    double noteOffPpq = 0.0;     // when the sounding note's gate ends
    int currentNote = -1;
    double internalPpq = 0.0;    // beat clock for Free-Run tempo mode
    double restEndPpq = 0.0;
    double lastPpqEnd = -1.0;
    bool wasPlaying = false;
    bool lastFreeRun = false;
    bool wasFrozen = false;
    double sweepAnchorPpq = 0.0; // beat position auto-sweep measures from
    bool lastSweepOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AleaAudioProcessor)
};
