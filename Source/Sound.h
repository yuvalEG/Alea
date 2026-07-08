#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// The Alea sound library - every internal sound the family can make, and
// the one engine that makes it. Shared by both products so a new flavour
// lands everywhere at once: the OUT menus, the state strings and the DSP
// all read from the same table below.
namespace alea
{

enum Flavour
{
    // Named, not ordered - the display order lives in the table, and only
    // the persisted "choice" strings need to stay stable (not these ints).
    warmPad = 0,
    softSaw,
    pureSine,   // Pure Sine and Triangle stay surgically clear (near-dry, no
    triangle,   // detune) even though they sit in the SYNTH group.
    piano,      // Salamander Grand Piano by Alexander Holm (CC BY 3.0).
    ePiano,
    organ,
    strings,
    pluck,
    bells,

    numFlavours
};

// Menu groups, in display order: pure synthesis, then emulated instruments.
enum FlavourGroup { groupSynth = 0, groupInstrument };

struct FlavourInfo
{
    int flavour;          // Flavour
    int group;            // FlavourGroup
    const char* choice;   // the persisted OUT state string
    const char* name;     // menu display name
};

const std::array<FlavourInfo, numFlavours>& flavourTable();
const char* groupName (int group);
int flavourFromChoice (const juce::String&);  // -1 if not an internal sound
juce::String choiceForFlavour (int flavour);

//==============================================================================
// Polyphonic engine: per-voice flavour (a menu switch never mutates notes
// already ringing), summed into a per-flavour delay send, then the family
// stereo delay + room reverb + tanh safety limiter.
class SoundEngine
{
public:
    SoundEngine() = default;

    void prepare (double sampleRate);

    // Renders (adds nothing - overwrites are the caller's business: the
    // buffer arrives cleared) and remembers the block peak for the meter.
    void render (juce::AudioBuffer<float>&, const juce::MidiBuffer&,
                 int flavour, float masterGain);

    float lastPeak() const { return peak; }

private:
    struct Voice
    {
        juce::ADSR amp, bright;
        double phase = 0.0, phase2 = 0.0, phase3 = 0.0, freq = 440.0;
        double trem = 0.0;              // e-piano tremolo / organ vibrato phase
        float gain = 0.0f, velocity = 0.0f;
        int note = -1;                  // -1 = released (may still ring)
        int heldSamples = 0;            // scales the release on note-off
        int flavour = 0;                // captured at strike

        // Karplus-Strong plucked string.
        std::vector<float> ks;
        int ksLen = 0, ksPos = 0;
        float ksDamp = 0.0f;

        // Sampler playback.
        const juce::AudioBuffer<float>* sample = nullptr;
        double samplePos = 0.0, sampleStep = 1.0;
    };

    void startVoice (Voice&, int note, float velocity, int flavour);
    void releaseVoice (Voice&, bool allNotesOff);
    float renderVoiceSample (Voice&);   // one mono sample, advances the voice

    std::array<Voice, 16> voices;
    int nextVoice = 0;
    double sr = 44100.0;

    std::vector<float> delayLineL, delayLineR;
    int delayPosL = 0, delayPosR = 0;
    std::vector<float> dryBus, sendBus; // per-flavour delay sends
    juce::Reverb reverb;
    float peak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundEngine)
};

} // namespace alea
