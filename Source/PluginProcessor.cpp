#include "PluginProcessor.h"
#include "PluginEditor.h"

// A silent stereo output bus is required: Ableton Live (and some other hosts)
// won't open a VST3 that has no audio outputs, so Alea presents as an
// instrument that emits MIDI and outputs silence.
AleaAudioProcessor::AleaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool AleaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AleaAudioProcessor::prepareToPlay (double, int)
{
    currentNote = -1;
    lastBeat = std::numeric_limits<std::int64_t>::min();
    lastPpqEnd = -1.0;
    wasPlaying = false;
}

void AleaAudioProcessor::releaseResources()
{
}

void AleaAudioProcessor::sendAllNotesOff (juce::MidiBuffer& midi, int sampleOffset)
{
    if (currentNote >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, currentNote), sampleOffset);
        currentNote = -1;
    }

    midi.addEvent (juce::MidiMessage::allNotesOff (midiChannel), sampleOffset);
}

void AleaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();

    auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();

    const bool isPlaying = position.hasValue() && position->getIsPlaying();

    hostIsPlaying.store (isPlaying);
    if (position.hasValue())
        hostBpm.store (position->getBpm().orFallback (0.0));

    if (! isPlaying)
    {
        // Transport just stopped (or paused): silence everything, once.
        if (wasPlaying)
            sendAllNotesOff (midi, 0);

        wasPlaying = false;
        return;
    }

    const double bpm        = position->getBpm().orFallback (120.0);
    const double ppqStart   = position->getPpqPosition().orFallback (0.0);
    const double sampleRate = getSampleRate();
    const int numSamples    = buffer.getNumSamples();

    if (sampleRate <= 0.0 || numSamples <= 0 || bpm <= 0.0)
        return;

    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const double ppqEnd = ppqStart + numSamples * ppqPerSample;

    // Loop wrap or jump backwards: kill the held note and rearm the beat counter.
    if (wasPlaying && ppqStart < lastPpqEnd)
    {
        sendAllNotesOff (midi, 0);
        lastBeat = std::numeric_limits<std::int64_t>::min();
    }

    if (! wasPlaying)
        lastBeat = std::numeric_limits<std::int64_t>::min();

    wasPlaying = true;
    lastPpqEnd = ppqEnd;
    hostPpq.store (ppqStart);

    auto ppqToOffset = [&] (double ppq)
    {
        auto offset = (int) ((ppq - ppqStart) / ppqPerSample);
        return juce::jlimit (0, numSamples - 1, offset);
    };

    // End the sounding note when its gate expires inside this block.
    if (currentNote >= 0 && noteOffPpq < ppqEnd)
    {
        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, currentNote), ppqToOffset (noteOffPpq));
        currentNote = -1;
    }

    // Fire one note on every integer beat inside [ppqStart, ppqEnd).
    for (auto beat = (std::int64_t) std::ceil (ppqStart - 1.0e-6); (double) beat < ppqEnd; ++beat)
    {
        if (beat <= lastBeat || beat < 0)
            continue;

        const int offset = ppqToOffset ((double) beat);

        // Monophonic guarantee: note-off before the next note-on.
        if (currentNote >= 0)
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, currentNote), offset);

        midi.addEvent (juce::MidiMessage::noteOn (midiChannel, testNote, testVelocity), offset);
        notesSent.fetch_add (1);
        currentNote = testNote;
        noteOffPpq = (double) beat + gateInBeats;
        lastBeat = beat;
    }
}

juce::AudioProcessorEditor* AleaAudioProcessor::createEditor()
{
    return new AleaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AleaAudioProcessor();
}
