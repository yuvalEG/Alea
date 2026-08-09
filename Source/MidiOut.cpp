#include "MidiOut.h"

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

} // namespace alea
