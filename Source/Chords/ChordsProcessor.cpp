#include "ChordsProcessor.h"
#include "ChordsEditor.h"
#include <algorithm>

ChordsProcessor::ChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    rollSeries(); // never an empty screen: chords out of the box
    startTimer (50); // message-thread side of the auto-roll handshake
}

void ChordsProcessor::timerCallback()
{
    if (autoRollPending.exchange (false) && playing.load())
    {
        autoRollInProgress = true;
        rollSeries();
        autoRollInProgress = false;
    }
}

chords::Chord ChordsProcessor::rollOne()
{
    chords::RollOptions opts;
    opts.simplified = simplify;
    opts.sevenths = useSevenths;
    opts.sus = susOn;
    opts.ninths = ninthsOn;
    opts.keyLock = keyLockOn;
    opts.scaleType = keyScale;
    opts.keyIndex = keyIndex;
    return chords::roll (rng, opts);
}

bool ChordsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
// Chord state (message thread)

void ChordsProcessor::markSeriesChange()
{
    // Only the FIRST change while a swap is pending snapshots the sounding
    // series - roll twice quickly and the audio is still on the original.
    if (playing.load())
    {
        if (! seriesSwapPending.load())
        {
            pendingOldSeries = series;
            seriesSwapPending.store (true);
        }
        // The latest change owns the swap: a manual roll after an auto one
        // must not be canceled by switching auto roll off.
        pendingFromAutoRoll = autoRollInProgress;
    }
}

void ChordsProcessor::rollSeries()
{
    markSeriesChange();

    if (! series.empty())
    {
        history.push_front (series);
        trimHistory();
    }

    // Pinned cards keep their chord; everything else rerolls.
    std::vector<chords::Chord> fresh;
    for (int i = 0; i < seriesLength; ++i)
        fresh.push_back (pinned[(size_t) i] && i < (int) series.size() ? series[(size_t) i]
                                                                       : rollOne());
    series = std::move (fresh);
    trimmedTail.clear();

    updateLoop();
    ++revision;
}

void ChordsProcessor::togglePin (int index)
{
    if (index < 0 || index >= 8)
        return;

    pinned[(size_t) index] = ! pinned[(size_t) index];

    // Pinning pins what you SEE: during a pending swap the sounding card
    // still shows the outgoing chord (auto roll may have already rolled
    // under it) - pinning that card writes the visible chord back into the
    // incoming series.
    if (pinned[(size_t) index] && playing.load() && seriesSwapPending.load()
        && index == playingChord.load()
        && index < (int) pendingOldSeries.size()
        && index < (int) series.size())
    {
        series[(size_t) index] = pendingOldSeries[(size_t) index];
        updateLoop();
    }

    ++revision;
}

void ChordsProcessor::revertPendingSwap()
{
    // Discard a still-unheard pending roll and restore the sounding series.
    // If that roll had pushed the old series into history, un-dup it.
    if (! history.empty() && history.front() == pendingOldSeries)
        history.pop_front();
    if (! pendingOldSeries.empty())
    {
        series = pendingOldSeries;
        seriesLength = juce::jlimit (1, 8, (int) series.size());
    }
    updateLoop();
    seriesSwapPending.store (false);
    ++revision;
}

void ChordsProcessor::handleStopped()
{
    // Pausing must not switch what you were practicing.
    autoRollPending.store (false);
    if (seriesSwapPending.load())
        revertPendingSwap();
}

void ChordsProcessor::cancelAutoRollSwap()
{
    // Auto roll switched off while its preview was pending: take it back.
    // Manual rolls keep their swap - only auto roll's own work is undone.
    autoRollPending.store (false);
    if (seriesSwapPending.load() && pendingFromAutoRoll)
        revertPendingSwap();
}

void ChordsProcessor::setSeriesLength (int newLength)
{
    markSeriesChange();
    seriesLength = juce::jlimit (1, 8, newLength);

    // Shrinking parks the trimmed chords; growing brings them back before
    // rolling anything new - the selector never silently rerolls your loop,
    // and 4 -> 3 -> 4 returns the same fourth chord.
    while ((int) series.size() > seriesLength)
    {
        trimmedTail.insert (trimmedTail.begin(), series.back());
        series.pop_back();
    }
    while ((int) series.size() < seriesLength)
    {
        if (! trimmedTail.empty())
        {
            series.push_back (trimmedTail.front());
            trimmedTail.erase (trimmedTail.begin());
        }
        else
            series.push_back (rollOne());
    }

    updateLoop();
    ++revision;
}

void ChordsProcessor::recallRoll (int index)
{
    if (index < 0 || index >= (int) history.size())
        return;

    markSeriesChange();

    // Non-destructive: the recalled roll stays in history; the outgoing
    // series joins it up front, so nothing is ever lost by recalling.
    auto recalled = history[(size_t) index];
    if (! series.empty())
        history.push_front (series);
    series = std::move (recalled);
    seriesLength = juce::jlimit (1, 8, (int) series.size());
    pinned.fill (false); // a recalled roll starts unpinned
    trimmedTail.clear();
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
    // Every checked octave sounds together - the voicing doubles across
    // them. The chords are this app's only dice; octaves are a choice.
    juce::Array<int> octaves;
    for (int o = 2; o <= 4; ++o)
        if ((octaveMask.load() >> (o - 2)) & 1)
            octaves.add (o);
    if (octaves.isEmpty())
        octaves.add (3);

    std::vector<PlayChord> fresh;
    fresh.reserve (series.size());
    for (const auto& c : series)
    {
        PlayChord pc;
        for (auto oct : octaves)
            for (auto note : chords::midiNotes (c, oct))
                if (pc.count < 16 && note >= 0 && note <= 127
                    && std::find (pc.notes, pc.notes + pc.count, note) == pc.notes + pc.count)
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
    seriesSwapPending.store (false); // the swap (if any) just landed
    return true;
}

void ChordsProcessor::stopSoundingNotes (juce::MidiBuffer& midi, int sampleOffset)
{
    for (int i = 0; i < soundingCount; ++i)
        midi.addEvent (juce::MidiMessage::noteOff (1, soundingNotes[i]), sampleOffset);
    soundingCount = 0;
    soundingBitsLo.store (0);
    soundingBitsHi.store (0);
}

void ChordsProcessor::prepareToPlay (double sampleRate, int)
{
    wasPlaying = false;
    soundingCount = 0;
    samplesIntoChord = 0;
    samplesIntoBeat = 0;
    chordIdx = 0;
    loopsCompleted = 0;
    clickCount = 0;
    clickEnv = 0.0f;

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
        // Transport start: from the top, with the freshest series. This is
        // a STOP transport by choice - holding a moment is FREEZE's job.
        chordIdx = 0;
        samplesIntoChord = 0;
        samplesIntoBeat = 0;
        loopsCompleted = 0;
        copyLoopIfDirty();
    }
    if (! isPlaying && wasPlaying)
    {
        stopSoundingNotes (localMidi, 0);
        localMidi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    }
    wasPlaying = isPlaying;

    // PANIC: immediate silence; the loop resumes at the next chord boundary.
    if (panicRequest.exchange (false))
    {
        stopSoundingNotes (localMidi, 0);
        localMidi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        clickEnv = 0.0f;
    }
    clickCount = 0;

    // A clicked card jumps the loop there on the spot.
    if (const int jump = jumpRequest.exchange (-1); jump >= 0 && isPlaying)
    {
        copyLoopIfDirty();
        if (! currentLoop.empty())
        {
            chordIdx = juce::jmin (jump, (int) currentLoop.size() - 1);
            samplesIntoChord = 0;
        }
    }

    if (isPlaying && frozen.load())
    {
        // FREEZE: time stops, the chord keeps sustaining. Nothing schedules,
        // nothing advances; unfreeze resumes exactly where it held.
    }
    else if (isPlaying)
    {
        const double bpmNow = juce::jlimit (30.0, 300.0, (double) bpm.load());
        const auto barsNow = (double) juce::jlimit (1, 4, barsPerChord.load());
        const auto beatLen = juce::jmax ((juce::int64) 1, (juce::int64) (sampleRate * 60.0 / bpmNow));
        const auto chordLen = juce::jmax ((juce::int64) 1, beatLen * 4 * (juce::int64) barsNow); // 4/4 only

        int offset = 0;
        while (offset < numSamples && ! (currentLoop.empty() && ! loopDirty.load()))
        {
            if (samplesIntoChord == 0)
            {
                // Every chord boundary adopts a pending roll or recall and
                // restarts it from its top - rolling mid-loop never waits
                // out the old pass (QA, July 7).
                if (copyLoopIfDirty())
                {
                    chordIdx = 0;
                    loopsCompleted = 0;
                }
                if (currentLoop.empty())
                    break;
                if (chordIdx >= (int) currentLoop.size())
                    chordIdx = 0;

                stopSoundingNotes (localMidi, offset); // offs, then ons, same sample
                const auto& pc = currentLoop[(size_t) chordIdx];
                juce::uint64 lo = 0, hi = 0;
                for (int i = 0; i < pc.count; ++i)
                {
                    localMidi.addEvent (juce::MidiMessage::noteOn (1, pc.notes[i], 0.72f), offset);
                    soundingNotes[soundingCount++] = pc.notes[i];
                    if (pc.notes[i] < 64) lo |= (juce::uint64) 1 << pc.notes[i];
                    else                  hi |= (juce::uint64) 1 << (pc.notes[i] - 64);
                }
                soundingBitsLo.store (lo);
                soundingBitsHi.store (hi);
                samplesIntoBeat = 0;
            }

            if (samplesIntoBeat >= beatLen)
                samplesIntoBeat = 0;
            if (samplesIntoBeat == 0 && metronomeOn.load() && clickCount < 32)
                clickEvents[clickCount++] = { offset, samplesIntoChord == 0 };

            auto chunk = (juce::int64) (numSamples - offset);
            chunk = juce::jmin (chunk, chordLen - samplesIntoChord);
            chunk = juce::jmin (chunk, beatLen - samplesIntoBeat);
            const int step = juce::jmax (1, (int) chunk);
            offset += step;
            samplesIntoChord += step;
            samplesIntoBeat += step;

            // Auto roll fires entering the last chord of the Nth pass. The
            // cards keep showing the sounding series until the wrap (the
            // editor uses pendingOldSeries), with the fresh one previewed
            // in amber - so the whole last chord doubles as a preview.
            if (autoRollOn.load() && ! autoRollPending.load() && ! loopDirty.load()
                && samplesIntoBeat == (juce::int64) step // a boundary just started this chunk
                && samplesIntoChord == (juce::int64) step
                && chordIdx == (int) currentLoop.size() - 1
                && loopsCompleted >= autoRollLoops.load() - 1)
                autoRollPending.store (true);

            if (samplesIntoChord >= chordLen)
            {
                samplesIntoChord = 0;
                if (++chordIdx >= (int) currentLoop.size())
                {
                    chordIdx = 0;
                    ++loopsCompleted;
                }
            }
        }

        // The UI suppresses the card highlight itself while a swap is
        // pending (and prints the sounding chord from pendingOldSeries,
        // indexed by this).
        playingChord.store (currentLoop.empty() ? -1 : chordIdx);
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

    // The click renders after the synth chain - a dry tick, no reverb tail.
    // It sounds in MIDI-device mode too: chords on the external instrument,
    // the pulse from the app.
    if (clickCount > 0 || clickEnv > 0.001f)
        renderClicks (buffer);
}

void ChordsProcessor::renderClicks (juce::AudioBuffer<float>& buffer)
{
    const double sampleRate = getSampleRate();
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const int numSamples = buffer.getNumSamples();

    int next = 0;
    for (int n = 0; n < numSamples; ++n)
    {
        if (next < clickCount && clickEvents[next].offset == n)
        {
            // Accented (chord-change) click sits a fifth above the beat click.
            clickPhase = 0.0;
            clickInc = juce::MathConstants<double>::twoPi * (clickEvents[next].accent ? 2093.0 : 1397.0) / sampleRate;
            clickEnv = 1.0f;
            clickGain = (clickEvents[next].accent ? 0.30f : 0.20f)
                        * juce::Decibels::decibelsToGain (clickVolDb.load());
            ++next;
        }
        if (clickEnv > 0.001f)
        {
            const float s = clickGain * clickEnv * (float) std::sin (clickPhase);
            left[n] += s;
            if (right != nullptr)
                right[n] += s;
            clickPhase += clickInc;
            clickEnv *= 0.9975f; // ~10 ms tick
        }
    }
    clickCount = 0;
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
    state.setProperty ("sus", susOn, nullptr);
    state.setProperty ("ninths", ninthsOn, nullptr);
    state.setProperty ("keyLock", keyLockOn, nullptr);
    state.setProperty ("keyScale", keyScale, nullptr);
    state.setProperty ("keyIndex", keyIndex, nullptr);
    state.setProperty ("uiWidth", lastUIWidth, nullptr);
    state.setProperty ("uiHeight", lastUIHeight, nullptr);
    state.setProperty ("bpm", (double) bpm.load(), nullptr);
    state.setProperty ("barsPerChord", barsPerChord.load(), nullptr);
    state.setProperty ("octaves", octaveMask.load(), nullptr);
    state.setProperty ("metronome", metronomeOn.load(), nullptr);
    state.setProperty ("clickVol", (double) clickVolDb.load(), nullptr);
    state.setProperty ("autoRoll", autoRollOn.load(), nullptr);
    state.setProperty ("autoRollLoops", autoRollLoops.load(), nullptr);
    state.setProperty ("output", getStandaloneOutput(), nullptr);
    state.setProperty ("synthVol", (double) synthVolDb.load(), nullptr);

    juce::ValueTree seriesTree ("Series");
    for (size_t i = 0; i < series.size(); ++i)
    {
        const auto& c = series[i];
        juce::ValueTree chord ("Chord");
        chord.setProperty ("root", c.root, nullptr);
        chord.setProperty ("quality", (int) c.quality, nullptr);
        chord.setProperty ("seventh", (int) c.seventh, nullptr);
        chord.setProperty ("ninth", c.ninth, nullptr);
        chord.setProperty ("pinned", i < 8 && pinned[i], nullptr);
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
    susOn        = state.getProperty ("sus", false);
    ninthsOn     = state.getProperty ("ninths", false);
    keyLockOn    = state.getProperty ("keyLock", false);
    keyScale     = juce::jlimit (0, 2, (int) state.getProperty ("keyScale", 0));
    keyIndex     = juce::jlimit (0, chords::keyNamesFor ((chords::ScaleType) keyScale).size() - 1,
                                 (int) state.getProperty ("keyIndex", 0));
    lastUIWidth  = juce::jlimit (560, 4000, (int) state.getProperty ("uiWidth", 900));
    lastUIHeight = juce::jlimit (380, 3000, (int) state.getProperty ("uiHeight", 560));
    bpm.store (juce::jlimit (30.0f, 300.0f, (float) (double) state.getProperty ("bpm", 90.0)));
    barsPerChord.store (juce::jlimit (1, 4, (int) state.getProperty ("barsPerChord", 1)));
    // "octave" (single) was the pre-multi-select property name.
    const int legacyMask = 1 << (juce::jlimit (2, 4, (int) state.getProperty ("octave", 3)) - 2);
    octaveMask.store (juce::jlimit (1, 7, (int) state.getProperty ("octaves", legacyMask)));
    metronomeOn.store (state.getProperty ("metronome", false));
    clickVolDb.store (juce::jlimit (-12.0f, 12.0f, (float) (double) state.getProperty ("clickVol", 0.0)));
    autoRollOn.store (state.getProperty ("autoRoll", false));
    autoRollLoops.store (juce::jlimit (1, 8, (int) state.getProperty ("autoRollLoops", 2)));
    synthVolDb.store (juce::jlimit (-24.0f, 6.0f, (float) (double) state.getProperty ("synthVol", 0.0)));
    setStandaloneOutput (state.getProperty ("output", "synth").toString());

    auto seriesTree = state.getChildWithName ("Series");
    std::vector<chords::Chord> restored;
    pinned.fill (false);
    for (auto chord : seriesTree)
    {
        chords::Chord c;
        c.root    = chord.getProperty ("root", "C").toString();
        c.quality = (chords::Quality) juce::jlimit (0, 5, (int) chord.getProperty ("quality", 0));
        c.seventh = (chords::Seventh) juce::jlimit (0, 3, (int) chord.getProperty ("seventh", 0));
        c.ninth   = chord.getProperty ("ninth", false);
        if (restored.size() < 8)
            pinned[restored.size()] = chord.getProperty ("pinned", false);
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
