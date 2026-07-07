#include "ChordsProcessor.h"
#include "ChordsEditor.h"

ChordsProcessor::ChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    rollSeries(); // never an empty screen: chords out of the box
}

bool ChordsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
// Chord state (message thread)

void ChordsProcessor::rollSeries()
{
    if (! series.empty())
    {
        history.push_front (series);
        trimHistory();
    }

    series.clear();
    for (int i = 0; i < seriesLength; ++i)
        series.push_back (chords::roll (rng, simplify, useSevenths));

    updateLoop();
    ++revision;
}

void ChordsProcessor::setSeriesLength (int newLength)
{
    seriesLength = juce::jlimit (1, 8, newLength);

    // Growing rolls fresh chords into the new slots; shrinking truncates.
    // Existing chords stay - the selector never silently rerolls your loop.
    while ((int) series.size() > seriesLength)
        series.pop_back();
    while ((int) series.size() < seriesLength)
        series.push_back (chords::roll (rng, simplify, useSevenths));

    updateLoop();
    ++revision;
}

void ChordsProcessor::recallRoll (int index)
{
    if (index < 0 || index >= (int) history.size())
        return;

    // Non-destructive: the recalled roll stays in history; the outgoing
    // series joins it up front, so nothing is ever lost by recalling.
    auto recalled = history[(size_t) index];
    if (! series.empty())
        history.push_front (series);
    series = std::move (recalled);
    seriesLength = juce::jlimit (1, 8, (int) series.size());
    trimHistory();
    updateLoop();
    ++revision;
}

void ChordsProcessor::trimHistory()
{
    int total = 0;
    for (auto& roll : history)
        total += (int) roll.size();

    while (total > 1000 && history.size() > 1)
    {
        total -= (int) history.back().size();
        history.pop_back();
    }
}

void ChordsProcessor::updateLoop()
{
    std::vector<PlayChord> fresh;
    fresh.reserve (series.size());
    for (const auto& c : series)
    {
        PlayChord pc;
        for (auto note : chords::midiNotes (c))
            if (pc.count < 8 && note >= 0 && note <= 127)
                pc.notes[pc.count++] = note;
        fresh.push_back (pc);
    }

    {
        const juce::ScopedLock sl (loopLock);
        nextLoop = std::move (fresh);
    }
    loopDirty.store (true);
}

//==============================================================================
// Loop playback (audio thread)

bool ChordsProcessor::copyLoopIfDirty()
{
    if (! loopDirty.load())
        return false;
    const juce::ScopedTryLock sl (loopLock);
    if (! sl.isLocked())
        return false;
    currentLoop = nextLoop;
    loopDirty.store (false);
    return true;
}

void ChordsProcessor::stopSoundingNotes (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = 0; i < soundingCount; ++i)
        midi.addEvent (juce::MidiMessage::noteOff (1, soundingNotes[i]), sampleOffset);
    soundingCount = 0;
}

void ChordsProcessor::prepareToPlay (double sampleRate, int)
{
    wasPlaying = false;
    soundingCount = 0;
    samplesIntoChord = 0;
    chordIdx = 0;

    // Internal synth chain, as in Scale Shifter: soft attack, high sustain,
    // long release so tails ring well into the following chords.
    for (auto& v : voices)
    {
        v.amp.setSampleRate (sampleRate);
        v.amp.setParameters ({ 0.012f, 0.25f, 0.80f, 2.20f });
        v.amp.reset();
        v.bright.setSampleRate (sampleRate);
        v.bright.setParameters ({ 0.002f, 0.50f, 0.25f, 1.00f });
        v.bright.reset();
        v.phase = v.phase2 = v.phase3 = 0.0;
        v.note = -1;
    }
    nextVoice = 0;

    // Different L/R delay times give the voices a stereo field.
    delayLineL.assign ((size_t) juce::jmax (1, (int) (sampleRate * 0.32)), 0.0f);
    delayLineR.assign ((size_t) juce::jmax (1, (int) (sampleRate * 0.48)), 0.0f);
    delayPosL = delayPosR = 0;

    reverb.setSampleRate (sampleRate);
    juce::Reverb::Parameters rv;
    rv.roomSize = 0.70f;
    rv.damping = 0.35f;
    rv.wetLevel = 0.33f;
    rv.dryLevel = 0.85f;
    rv.width = 1.0f;
    reverb.setParameters (rv);
    reverb.reset();
}

void ChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    juce::MidiBuffer localMidi;
    const bool isPlaying = playing.load();

    if (isPlaying && ! wasPlaying)
    {
        // Transport start: from the top, with the freshest series.
        chordIdx = 0;
        samplesIntoChord = 0;
        copyLoopIfDirty();
    }
    if (! isPlaying && wasPlaying)
    {
        stopSoundingNotes (localMidi, 0);
        localMidi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    }
    wasPlaying = isPlaying;

    if (isPlaying)
    {
        const double bpmNow = juce::jlimit (30.0, 300.0, (double) bpm.load());
        const auto barsNow = (double) juce::jlimit (1, 4, barsPerChord.load());
        const auto chordLen = juce::jmax ((juce::int64) 1,
            (juce::int64) (sampleRate * 60.0 / bpmNow * 4.0 * barsNow)); // 4/4 only

        int offset = 0;
        while (offset < numSamples && ! (currentLoop.empty() && ! loopDirty.load()))
        {
            if (samplesIntoChord == 0)
            {
                // A wrap back to chord 1 is the moment a mid-play roll takes
                // over: the finished pass played to the end, the new one is
                // adopted here.
                if (chordIdx == 0)
                    copyLoopIfDirty();
                if (currentLoop.empty())
                    break;
                if (chordIdx >= (int) currentLoop.size())
                    chordIdx = 0;

                stopSoundingNotes (localMidi, offset); // offs, then ons, same sample
                const auto& pc = currentLoop[(size_t) chordIdx];
                for (int i = 0; i < pc.count; ++i)
                {
                    localMidi.addEvent (juce::MidiMessage::noteOn (1, pc.notes[i], 0.72f), offset);
                    soundingNotes[soundingCount++] = pc.notes[i];
                }
            }

            const auto chunk = (int) juce::jmin ((juce::int64) (numSamples - offset),
                                                 chordLen - samplesIntoChord);
            offset += juce::jmax (1, chunk);
            samplesIntoChord += juce::jmax (1, chunk);

            if (samplesIntoChord >= chordLen)
            {
                samplesIntoChord = 0;
                chordIdx = (chordIdx + 1) % juce::jmax (1, (int) currentLoop.size());
            }
        }

        // While a mid-play roll waits for the loop to wrap, the cards show
        // the NEW series but the OLD one is still sounding - so no card
        // highlight until the swap (the UI never lies).
        playingChord.store (currentLoop.empty() || loopDirty.load() ? -1 : chordIdx);
        chordProgress.store ((float) ((double) samplesIntoChord / (double) chordLen));
    }
    else
    {
        playingChord.store (-1);
        chordProgress.store (0.0f);
    }

    // Output: the family synth renders in-process; otherwise mirror the MIDI
    // to the chosen device (try-lock: device swaps never stall audio).
    if (synthOn.load())
    {
        renderSynth (buffer, localMidi);
    }
    else if (! localMidi.isEmpty())
    {
        const juce::ScopedTryLock sl (midiOutLock);
        if (sl.isLocked() && midiOutput != nullptr)
            midiOutput->sendBlockOfMessages (localMidi, juce::Time::getMillisecondCounter() + 1.0, sampleRate);
    }
}

//==============================================================================
// Output routing (as in Scale Shifter's standalone OUT chooser)

void ChordsProcessor::setMidiOutputDevice (const juce::String& identifier)
{
    auto fresh = identifier.isEmpty() ? nullptr : juce::MidiOutput::openDevice (identifier);
    if (fresh != nullptr)
        fresh->startBackgroundThread();

    const juce::ScopedLock sl (midiOutLock);
    if (midiOutput != nullptr) // silence anything still ringing on the old device
        for (int ch = 1; ch <= 16; ++ch)
            midiOutput->sendMessageNow (juce::MidiMessage::allNotesOff (ch));
    std::swap (midiOutput, fresh);
    midiOutputId = midiOutput != nullptr ? identifier : juce::String();
}

juce::String ChordsProcessor::getMidiOutputId() const
{
    const juce::ScopedLock sl (midiOutLock);
    return midiOutputId;
}

void ChordsProcessor::setStandaloneOutput (const juce::String& choice)
{
    if (choice.startsWith ("synth") || choice.isEmpty())
    {
        const auto flavour = choice.fromFirstOccurrenceOf (":", false, false);
        synthVoice.store (flavour == "sine" ? 1 : flavour == "saw" ? 2 : flavour == "strings" ? 3 : 0);
        synthOn.store (true);
        setMidiOutputDevice ({});
    }
    else
    {
        synthOn.store (false);
        setMidiOutputDevice (choice);
    }
}

juce::String ChordsProcessor::getStandaloneOutput() const
{
    if (! synthOn.load())
        return getMidiOutputId();
    switch (synthVoice.load())
    {
        case 1:  return "synth:sine";
        case 2:  return "synth:saw";
        case 3:  return "synth:strings";
        default: return "synth";
    }
}

//==============================================================================
// Internal synth - ported from Scale Shifter (spec 7.1: the synth "as-is").
// Polyphonic additive voices into stereo delay + reverb; velocity opens the
// brightness envelope (upper partials), not just the volume.

void ChordsProcessor::renderSynth (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const double sampleRate = getSampleRate();
    const int numSamples = buffer.getNumSamples();
    if (sampleRate <= 0.0 || numSamples <= 0 || buffer.getNumChannels() < 1)
        return;

    auto* left = buffer.getWritePointer (0);
    const float master = juce::Decibels::decibelsToGain (synthVolDb.load());

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        for (auto& v : voices)
        {
            if (! v.amp.isActive())
                continue;
            // Four voice flavours share the oscillator trio. Warm Pad: a
            // dominant equal pair ~10 cents apart for the audible slow
            // phasing, plus sub and fixed harmonics. Pure Sine: one clean
            // oscillator. Soft Saw: one band-limited saw. Strings: three
            // detuned soft saws with a slow bow-in. Normalized so eight
            // ringing voices stay clear of the limiter.
            const int flavour = synthVoice.load();
            const double inc1 = juce::MathConstants<double>::twoPi * v.freq / sampleRate;
            const double inc2 = inc1 * 1.0055;
            const double inc3 = inc1 * 0.9965;
            const int harmonics = juce::jlimit (3, 9, (int) (sampleRate * 0.35 / juce::jmax (20.0, v.freq)));
            auto saw = [harmonics] (double p) // band-limited-ish, gentle top
            {
                float acc = 0.0f;
                for (int k = 1; k <= harmonics; ++k)
                    acc += (float) std::sin (k * p) / (float) k;
                return acc * 0.62f;
            };
            for (int n = pos; n < end; ++n)
            {
                const float env = v.amp.getNextSample();
                const float shimmer = 0.14f * v.velocity * v.bright.getNextSample();
                float s;
                switch (flavour)
                {
                    case 1: // Pure Sine
                        s = 0.9f * (float) std::sin (v.phase);
                        break;
                    case 2: // Soft Saw
                        s = 0.75f * saw (v.phase);
                        break;
                    case 3: // Strings
                        s = 0.42f * (saw (v.phase) + saw (v.phase2) + saw (v.phase3));
                        break;
                    default: // Warm Pad
                        s = (0.44f * ((float) std::sin (v.phase) + (float) std::sin (v.phase2))
                           + 0.16f * (float) std::sin (v.phase3)
                           + 0.28f * (float) std::sin (0.5 * v.phase)
                           + (0.22f + shimmer) * (float) std::sin (2.0 * v.phase)
                           + 0.08f * (float) std::sin (3.0 * v.phase)) / 1.55f;
                        break;
                }
                left[n] += 0.16f * master * v.gain * env * s;
                v.phase += inc1;
                v.phase2 += inc2;
                v.phase3 += inc3;
            }
            if (v.note >= 0)
                v.heldSamples += end - pos;
            if (v.phase > juce::MathConstants<double>::twoPi * 1024.0)
            {
                v.phase = std::fmod (v.phase, juce::MathConstants<double>::twoPi);
                v.phase2 = std::fmod (v.phase2, juce::MathConstants<double>::twoPi);
                v.phase3 = std::fmod (v.phase3, juce::MathConstants<double>::twoPi);
            }
        }
        pos = end;
    };

    for (const auto metadata : midi)
    {
        renderTo (juce::jlimit (0, numSamples, metadata.samplePosition));
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            // Prefer an idle voice; otherwise round-robin steal, so releases
            // ring out under the following notes.
            SynthVoice* voice = nullptr;
            for (auto& v : voices)
                if (! v.amp.isActive())
                    { voice = &v; break; }
            if (voice == nullptr)
            {
                voice = &voices[(size_t) nextVoice];
                nextVoice = (nextVoice + 1) % (int) voices.size();
            }
            voice->freq = juce::MidiMessage::getMidiNoteInHertz (msg.getNoteNumber());
            voice->velocity = msg.getFloatVelocity();
            // A sine has no timbre to spend velocity on, so spend it on
            // dynamics: a power curve gives soft notes a real pianissimo.
            voice->gain = 0.06f + 0.94f * std::pow (msg.getFloatVelocity(), 1.7f);
            voice->note = msg.getNoteNumber();
            voice->heldSamples = 0;
            {
                auto params = voice->amp.getParameters();
                switch (synthVoice.load())
                {
                    case 1:  params.attack = 0.006f; break; // sine: clean
                    case 2:  params.attack = 0.003f; break; // saw: plucky
                    case 3:  params.attack = 0.280f; break; // strings: bow in
                    default: params.attack = 0.012f; break; // warm pad
                }
                voice->amp.setParameters (params);
            }
            voice->amp.noteOn();
            voice->bright.noteOn();
        }
        else if (msg.isNoteOff() || msg.isAllNotesOff())
        {
            // Release scales with how long the note was held: staccato notes
            // get a snappy tail, pads ring long. Pure Sine keeps a longer
            // floor - without it, its clean tails vanish and the polyphony
            // is inaudible.
            const float releaseFloor = synthVoice.load() == 1 ? 0.65f : 0.12f;
            for (auto& v : voices)
                if (v.note >= 0 && (msg.isAllNotesOff() || v.note == msg.getNoteNumber()))
                {
                    auto params = v.amp.getParameters();
                    params.release = juce::jlimit (releaseFloor, 2.2f,
                                                   1.1f * (float) v.heldSamples / (float) sampleRate);
                    v.amp.setParameters (params);
                    v.amp.noteOff();
                    v.bright.noteOff();
                    v.note = -1;
                }
        }
    }
    renderTo (numSamples);

    // Stereo delay: different L/R times widen the mono voice.
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int n = 0; n < numSamples; ++n)
    {
        const float dry = left[n];
        auto tap = [&] (std::vector<float>& line, int& p) -> float
        {
            const float delayed = line[(size_t) p];
            line[(size_t) p] = dry + delayed * 0.35f;
            p = (p + 1) % (int) line.size();
            return delayed;
        };
        left[n] = dry + 0.30f * tap (delayLineL, delayPosL);
        if (right != nullptr)
            right[n] = dry + 0.30f * tap (delayLineR, delayPosR);
    }

    if (right != nullptr)
        reverb.processStereo (left, right, numSamples);
    else
        reverb.processMono (left, numSamples);

    // Soft safety limiter: transparent at normal levels, rounds off the
    // peaks when eight ringing voices + delay + reverb stack up.
    float peak = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        left[n] = std::tanh (left[n]);
        peak = juce::jmax (peak, std::abs (left[n]));
        if (right != nullptr)
        {
            right[n] = std::tanh (right[n]);
            peak = juce::jmax (peak, std::abs (right[n]));
        }
    }
    synthPeak.store (peak);
}

//==============================================================================
// State

void ChordsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("ChordsState");
    state.setProperty ("seriesLength", seriesLength, nullptr);
    state.setProperty ("useSevenths", useSevenths, nullptr);
    state.setProperty ("simplify", simplify, nullptr);
    state.setProperty ("uiWidth", lastUIWidth, nullptr);
    state.setProperty ("uiHeight", lastUIHeight, nullptr);
    state.setProperty ("bpm", (double) bpm.load(), nullptr);
    state.setProperty ("barsPerChord", barsPerChord.load(), nullptr);
    state.setProperty ("output", getStandaloneOutput(), nullptr);
    state.setProperty ("synthVol", (double) synthVolDb.load(), nullptr);

    juce::ValueTree seriesTree ("Series");
    for (const auto& c : series)
    {
        juce::ValueTree chord ("Chord");
        chord.setProperty ("root", c.root, nullptr);
        chord.setProperty ("quality", (int) c.quality, nullptr);
        chord.setProperty ("seventh", (int) c.seventh, nullptr);
        seriesTree.appendChild (chord, nullptr);
    }
    state.appendChild (seriesTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChordsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType ("ChordsState"))
        return;

    seriesLength = juce::jlimit (1, 8, (int) state.getProperty ("seriesLength", 4));
    useSevenths  = state.getProperty ("useSevenths", false);
    simplify     = state.getProperty ("simplify", true);
    lastUIWidth  = juce::jlimit (560, 4000, (int) state.getProperty ("uiWidth", 900));
    lastUIHeight = juce::jlimit (380, 3000, (int) state.getProperty ("uiHeight", 560));
    bpm.store (juce::jlimit (30.0f, 300.0f, (float) (double) state.getProperty ("bpm", 90.0)));
    barsPerChord.store (juce::jlimit (1, 4, (int) state.getProperty ("barsPerChord", 1)));
    synthVolDb.store (juce::jlimit (-24.0f, 6.0f, (float) (double) state.getProperty ("synthVol", 0.0)));
    setStandaloneOutput (state.getProperty ("output", "synth").toString());

    auto seriesTree = state.getChildWithName ("Series");
    std::vector<chords::Chord> restored;
    for (auto chord : seriesTree)
    {
        chords::Chord c;
        c.root    = chord.getProperty ("root", "C").toString();
        c.quality = (chords::Quality) juce::jlimit (0, 3, (int) chord.getProperty ("quality", 0));
        c.seventh = (chords::Seventh) juce::jlimit (0, 3, (int) chord.getProperty ("seventh", 0));
        restored.push_back (c);
    }

    if (! restored.empty())
    {
        series = std::move (restored);
        setSeriesLength (seriesLength); // reconcile if length and series disagree
    }

    updateLoop();
    ++revision;
}

juce::AudioProcessorEditor* ChordsProcessor::createEditor()
{
    return new ChordsEditor (*this);
}

// The standalone wrapper needs this factory.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordsProcessor();
}
