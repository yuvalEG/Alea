#include "MidiOut.h"
#include "Sound.h"

namespace alea
{

void MidiOutputTarget::setDevice (const juce::String& identifier)
{
    // Open BEFORE taking the lock: opening a device can block, and the audio
    // thread is trying this lock every block.
    auto fresh = identifier.isEmpty() ? nullptr : juce::MidiOutput::openDevice (identifier);
    if (fresh != nullptr)
        fresh->startBackgroundThread();

    const juce::ScopedLock sl (lock);
    if (output != nullptr) // silence anything still ringing on the old device
        for (int ch = 1; ch <= 16; ++ch)
            output->sendMessageNow (juce::MidiMessage::allNotesOff (ch));

    std::swap (output, fresh);
    id = output != nullptr ? identifier : juce::String();
}

juce::String MidiOutputTarget::deviceId() const
{
    const juce::ScopedLock sl (lock);
    return id;
}

void MidiOutputTarget::send (const juce::MidiBuffer& midi, double sampleRate)
{
    const juce::ScopedTryLock sl (lock);
    if (sl.isLocked() && output != nullptr)
        output->sendBlockOfMessages (midi, juce::Time::getMillisecondCounter() + 1.0, sampleRate);
}

void applyOutputChoice (const juce::String& choice,
                        std::atomic<bool>& synthOn,
                        std::atomic<int>& synthVoice,
                        MidiOutputTarget& midiOut,
                        bool standaloneLike)
{
    if (const int flavour = flavourFromChoice (choice); flavour >= 0)
    {
        synthVoice.store (flavour);
        synthOn.store (true);
        midiOut.setDevice ({});
        // Decode the piano HERE, on the message thread, rather than on the
        // first note - startVoice runs on the audio thread and the set is
        // ~51 MB of Ogg (see alea::prewarmPiano).
        if (flavour == piano)
            prewarmPiano();
    }
    else
    {
        // Empty = MIDI to the host (plugin); a device id = standalone MIDI
        // out, which only the standalone has. In a DAW the host owns routing.
        synthOn.store (false);
        midiOut.setDevice (standaloneLike ? choice : juce::String());
    }
}

juce::String currentOutputChoice (const std::atomic<bool>& synthOn,
                                  const std::atomic<int>& synthVoice,
                                  const MidiOutputTarget& midiOut)
{
    return synthOn.load() ? choiceForFlavour (synthVoice.load())
                          : midiOut.deviceId();
}

} // namespace alea
