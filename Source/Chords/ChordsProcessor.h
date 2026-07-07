#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "ChordEngine.h"
#include <deque>
#include <vector>

// Alea Chord Randomizer: rolls random chord series for the loop-and-improvise
// exercise (spec section 1). M1 is display-only - the processor exists to
// hold state (which the standalone wrapper persists across launches) and to
// keep the plugin architecture ready for playback (M2) and DAW formats (M4).
class ChordsProcessor : public juce::AudioProcessor
{
public:
    ChordsProcessor();

    // --- chord state (message thread only) ---
    int seriesLength = 4;                             // 1..8
    bool useSevenths = false;
    bool simplify = true;                             // guitar-friendly out of the box
    std::vector<chords::Chord> series;                // what the cards show
    std::deque<std::vector<chords::Chord>> history;   // past rolls, newest first

    void rollSeries();            // current series joins history, new one rolls
    void setSeriesLength (int);   // grows by rolling extras, shrinks by truncating
    void recallRoll (int index);  // copy a past roll back into the series

    // Bumped on every chord-state change; the editor polls it to stay in sync
    // (state can arrive from the wrapper before or after the editor exists).
    int revision = 0;

    int lastUIWidth = 900, lastUIHeight = 560;

    // --- AudioProcessor boilerplate (silent until M2) ---
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }
    const juce::String getName() const override            { return "Alea Chord Randomizer"; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    juce::Random rng;

    // History keeps roughly the last 1000 chords (the ticker scrolls),
    // trimmed in whole rolls.
    void trimHistory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsProcessor)
};
