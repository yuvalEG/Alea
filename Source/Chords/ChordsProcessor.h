#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "ChordEngine.h"
#include <deque>
#include <vector>

// Alea Chord Randomizer: rolls random chord series for the loop-and-improvise
// exercise (spec section 1) and plays them in a loop (M2) - each chord a
// block voicing held for its bars, through the family synth or a MIDI device.
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

    // --- transport and loop (spec M2) ---
    std::atomic<bool>  playing { false };
    std::atomic<float> bpm { 90.0f };                 // 30..300
    std::atomic<int>   barsPerChord { 1 };            // 1, 2 or 4 (4/4 only)
    std::atomic<int>   playingChord { -1 };           // series index sounding now, -1 = none
    std::atomic<float> chordProgress { 0.0f };        // 0..1 through the current chord

    // Output choice, mirroring Scale Shifter's standalone OUT chooser:
    // built-in synth (default) or a MIDI device.
    std::atomic<bool>  synthOn { true };
    std::atomic<int>   synthVoice { 0 };              // 0 warm pad / 1 pure sine / 2 soft saw / 3 strings
    std::atomic<float> synthVolDb { 0.0f };
    std::atomic<float> synthPeak { 0.0f };            // post-limiter block peak, for the meter
    void setStandaloneOutput (const juce::String& choice); // "synth[:flavour]" or device identifier; message thread only
    juce::String getStandaloneOutput() const;

    // --- AudioProcessor ---
    void prepareToPlay (double, int) override;
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

    // --- loop scheduling (audio thread) ---
    // The message thread publishes the series as pre-voiced chords; the audio
    // thread adopts them at transport start and at each loop wrap, so a roll
    // during playback lets the current pass finish cleanly.
    struct PlayChord { int notes[8] = {}; int count = 0; };
    void updateLoop();                       // message thread: series -> nextLoop
    bool copyLoopIfDirty();                  // audio thread, try-lock
    void stopSoundingNotes (juce::MidiBuffer&, int sampleOffset);

    juce::CriticalSection loopLock;
    std::vector<PlayChord> nextLoop;         // guarded by loopLock
    std::atomic<bool> loopDirty { false };

    std::vector<PlayChord> currentLoop;
    int chordIdx = 0;
    juce::int64 samplesIntoChord = 0;
    bool wasPlaying = false;
    int soundingNotes[8] = {};
    int soundingCount = 0;

    // --- MIDI device output (standalone), as in Scale Shifter ---
    void setMidiOutputDevice (const juce::String& identifier);
    juce::String getMidiOutputId() const;
    mutable juce::CriticalSection midiOutLock;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::String midiOutputId;

    // --- internal synth, ported from Scale Shifter (spec 7.1: "as-is") ---
    void renderSynth (juce::AudioBuffer<float>&, const juce::MidiBuffer&);
    struct SynthVoice
    {
        juce::ADSR amp, bright;
        double phase = 0.0, phase2 = 0.0, phase3 = 0.0, freq = 440.0; // three detuned oscillators
        float gain = 0.0f, velocity = 0.0f;
        int note = -1;        // -1 = released (may still be ringing)
        int heldSamples = 0;  // how long the note was held - scales the release
    };
    std::array<SynthVoice, 8> voices;
    int nextVoice = 0;
    std::vector<float> delayLineL, delayLineR;
    int delayPosL = 0, delayPosR = 0;
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsProcessor)
};
