#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "ChordEngine.h"
#include <deque>
#include <vector>

// Alea Chord Randomizer: rolls random chord series for the loop-and-improvise
// exercise (spec section 1) and plays them in a loop (M2) - each chord a
// block voicing held for its bars, through the family synth or a MIDI device.
class ChordsProcessor : public juce::AudioProcessor,
                        private juce::Timer
{
public:
    ChordsProcessor();

    // --- chord state (message thread only) ---
    int seriesLength = 4;                             // 1..8
    bool useSevenths = false;
    bool simplify = true;                             // guitar-friendly out of the box
    bool susOn = false;                               // sus2/sus4 join the pool
    bool ninthsOn = false;                            // eligible chords may become 9ths
    bool keyLockOn = false;                           // diatonic rolls only
    int keyScale = 0;                                 // (int) chords::ScaleType
    int keyIndex = 0;                                 // into chords::keyNamesFor (keyScale)
    std::array<bool, 8> pinned {};                    // pinned cards survive rolls
    std::vector<chords::Chord> series;                // what the cards show
    std::deque<std::vector<chords::Chord>> history;   // past rolls, newest first

    void rollSeries();            // current series joins history; unpinned slots reroll
    void setSeriesLength (int);   // grows by rolling extras, shrinks by truncating
    void recallRoll (int index);  // copy a past roll back into the series (clears pins)
    void togglePin (int index);
    void updateLoop();            // re-voice the series for playback (e.g. octave change)
    void handleStopped();         // discard an unheard pending roll: pausing must not switch chords
    void cancelAutoRollSwap();    // auto roll turned off mid-preview: undo ITS pending roll only

    // Bumped on every chord-state change; the editor polls it to stay in sync
    // (state can arrive from the wrapper before or after the editor exists).
    int revision = 0;

    int lastUIWidth = 900, lastUIHeight = 560;

    // --- transport and loop (spec M2) ---
    std::atomic<bool>  playing { false };
    std::atomic<float> bpm { 90.0f };                 // 30..300
    std::atomic<int>   barsPerChord { 1 };            // 1, 2 or 4 (4/4 only)
    std::atomic<int>   octaveMask { 0b010 };          // bits for octaves 2/3/4; each chord lands in a random checked one
    std::atomic<int>   playingChord { -1 };           // series index sounding now, -1 = none
    std::atomic<float> chordProgress { 0.0f };        // 0..1 through the current chord
    std::atomic<bool>  metronomeOn { false };         // quarter-note click, accented on chord changes
    std::atomic<float> clickVolDb { 0.0f };           // -12..+12 dB on top of the base click level

    // Auto roll: after N completed loops of the series, roll fresh dice.
    // Triggered entering the last chord of the Nth pass, so the new series
    // lands exactly on the wrap.
    std::atomic<bool>  autoRollOn { false };
    std::atomic<int>   autoRollLoops { 2 };

    // A mid-play roll waits (at most one chord) for the next boundary; the
    // UI shows the old chord draining out while this is true. Re-voicing
    // (octave changes) also lands at the boundary but is NOT a swap - the
    // chords stay the same, so no "switching" theater.
    bool swapPending() const { return seriesSwapPending.load(); }

    // While a swap is pending, this is the series still sounding (message
    // thread only) - the UI prints the sounding chord from it.
    std::vector<chords::Chord> pendingOldSeries;

    // Performance controls (family header pattern): FREEZE holds the current
    // chord - time stops, the notes sustain - and PANIC silences everything.
    // Clicking a card mid-loop jumps to that chord on the spot.
    std::atomic<bool>  frozen { false };
    std::atomic<bool>  panicRequest { false };
    std::atomic<int>   jumpRequest { -1 };            // card index to jump to

    // Sounding notes for the monitor keyboard, as a 128-bit bitmask (octave
    // doubling can sound up to 12 notes at once).
    std::atomic<juce::uint64> soundingBitsLo { 0 }, soundingBitsHi { 0 };

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

    chords::Chord rollOne();      // one roll under the current toggles

    // Chords trimmed off by shrinking the series length; growing restores
    // them (A B C D -> 3 -> 4 gives D back, not a new roll). A fresh roll
    // or recall starts a new world and clears the memory.
    std::vector<chords::Chord> trimmedTail;

    // History keeps roughly the last 1000 chords (the ticker scrolls),
    // trimmed in whole rolls.
    void trimHistory();

    // --- loop scheduling (audio thread) ---
    // The message thread publishes the series as pre-voiced chords; the audio
    // thread adopts them at transport start and at each loop wrap, so a roll
    // during playback lets the current pass finish cleanly.
    struct PlayChord { int notes[16] = {}; int count = 0; }; // up to 4 notes x 3 octaves
    bool copyLoopIfDirty();                  // audio thread, try-lock
    void stopSoundingNotes (juce::MidiBuffer&, int sampleOffset);
    void markSeriesChange();                 // message thread, BEFORE mutating series

    juce::CriticalSection loopLock;
    std::vector<PlayChord> nextLoop;         // guarded by loopLock
    std::atomic<bool> loopDirty { false };
    std::atomic<bool> seriesSwapPending { false };

    // Who owns the pending swap (message thread): auto roll's swaps can be
    // canceled by switching auto roll off; manual rolls cannot.
    bool autoRollInProgress = false;
    bool pendingFromAutoRoll = false;
    void revertPendingSwap();

    std::vector<PlayChord> currentLoop;
    int chordIdx = 0;
    juce::int64 samplesIntoChord = 0;
    juce::int64 samplesIntoBeat = 0;
    int loopsCompleted = 0;
    bool wasPlaying = false;
    bool resumeRetrigger = false; // pause released the notes; resume re-strikes them
    int soundingNotes[16] = {};
    int soundingCount = 0;

    // Auto roll handshake: the audio thread raises the flag, the message
    // thread (timerCallback) rolls the dice.
    std::atomic<bool> autoRollPending { false };
    void timerCallback() override;

    // Metronome: click events collected while scheduling, rendered as a
    // short decaying sine after the synth (post reverb - a dry tick).
    struct ClickEvent { int offset; bool accent; };
    ClickEvent clickEvents[32];
    int clickCount = 0;
    double clickPhase = 0.0, clickInc = 0.0;
    float clickEnv = 0.0f, clickGain = 0.0f;
    void renderClicks (juce::AudioBuffer<float>&);

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
    std::array<SynthVoice, 16> voices; // room for a full three-octave doubling
    int nextVoice = 0;
    std::vector<float> delayLineL, delayLineR;
    int delayPosL = 0, delayPosR = 0;
    juce::Reverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordsProcessor)
};
