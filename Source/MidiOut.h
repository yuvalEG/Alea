#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// The other half of "where does the output go": the standalone's MIDI-device
// output, shared by both products (the internal sounds live in Sound.h).
// Processor-agnostic, like the rest of the shared layer.
namespace alea
{

// A MIDI device the standalone mirrors its stream to. The device is opened and
// closed on the message thread while the audio thread keeps sending, so the two
// meet under a lock the audio thread only ever TRIES to take: a swap in flight
// costs one dropped block, never a stall.
class MidiOutputTarget
{
public:
    // Explicit: the non-copyable macro below declares a copy constructor, and
    // declaring any constructor suppresses the implicit default one.
    MidiOutputTarget() = default;

    // Message thread only. An empty identifier closes the current device.
    // Anything still ringing on the outgoing device is silenced first - without
    // that, switching away leaves held notes sounding on hardware forever.
    void setDevice (const juce::String& identifier);

    // The open device's identifier, or empty when the output is not a device.
    juce::String deviceId() const;

    // Audio thread. Mirrors a block to the open device, if there is one.
    void send (const juce::MidiBuffer&, double sampleRate);

private:
    mutable juce::CriticalSection lock;
    std::unique_ptr<juce::MidiOutput> output;
    juce::String id;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiOutputTarget)
};

// Where the notes go, applied and read back. Both products had their own
// identical copy of this pair, and that is precisely the shape the Aug 10 debt
// pass named as this codebase's main bug source: the piano prewarm had to be
// added to both by hand on Aug 17, and three earlier bugs were a fix landing
// in one product only.
//
// The atomics stay owned by each processor - they are read all over both
// editors - so these take references rather than moving the state. The rule
// lives in one place; the storage does not have to.
//
// `choice` is an internal flavour ("piano", "synth:organ"), a MIDI device
// identifier, or empty for MIDI to the host. Message thread only, because
// opening a device is.
void applyOutputChoice (const juce::String& choice,
                        std::atomic<bool>& synthOn,
                        std::atomic<int>& synthVoice,
                        MidiOutputTarget& midiOut,
                        bool standaloneLike);

juce::String currentOutputChoice (const std::atomic<bool>& synthOn,
                                  const std::atomic<int>& synthVoice,
                                  const MidiOutputTarget& midiOut);

} // namespace alea
