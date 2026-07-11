#include "ChordsProcessor.h"
#include "ChordsEditor.h"
#include <algorithm>

ChordsProcessor::ChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Standalone: sound with zero setup. Plugin: MIDI to the DAW by default,
    // with the synth as an option (the Scale Shifter AU lesson).
    synthOn.store (isStandaloneLike());
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
    rollSerial.fetch_add (1); // every roll presses the ROLL key in the UI
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
    // The chords are this app's only dice; voicing - octaves, spacing,
    // inversions, bass - is a choice, applied at the output stage (spec M5).
    chords::VoicingOptions vopts;
    vopts.octaveMask = octaveMask.load();
    vopts.smooth = smoothVoicing.load();
    vopts.open = openVoicing.load();
    vopts.bass = bassNote.load();

    std::vector<PlayChord> fresh;
    fresh.reserve (series.size());
    juce::Array<int> anchor; // threads each chord's voicing to the next for smooth voice-leading
    for (const auto& c : series)
    {
        PlayChord pc;
        for (auto note : chords::voice (c, vopts, anchor))
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

void ChordsProcessor::strikeChord (juce::MidiBuffer& midi, int sampleOffset)
{
    stopSoundingNotes (midi, sampleOffset); // offs, then ons, same sample
    if (currentLoop.empty())
        return;
    if (chordIdx >= (int) currentLoop.size())
        chordIdx = 0;
    const auto& pc = currentLoop[(size_t) chordIdx];
    juce::uint64 lo = 0, hi = 0;
    for (int i = 0; i < pc.count; ++i)
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, pc.notes[i], 0.72f), sampleOffset);
        soundingNotes[soundingCount++] = pc.notes[i];
        if (pc.notes[i] < 64) lo |= (juce::uint64) 1 << pc.notes[i];
        else                  hi |= (juce::uint64) 1 << (pc.notes[i] - 64);
    }
    soundingBitsLo.store (lo);
    soundingBitsHi.store (hi);
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

    // The shared family sound engine (synth flavours + sampled piano).
    sound.prepare (sampleRate);
}

void ChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    // In a DAW the host owns transport and tempo (spec M4).
    const bool hostMode = ! isStandaloneLike();
    double hostPpq = 0.0;
    if (hostMode)
    {
        bool hostPlaying = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                hostPlaying = pos->getIsPlaying();
                if (auto b = pos->getBpm())         bpm.store ((float) *b);
                if (auto p = pos->getPpqPosition()) hostPpq = *p;
            }
        playing.store (hostPlaying);
    }

    juce::MidiBuffer localMidi;
    const bool isPlaying = playing.load();

    if (isPlaying && ! wasPlaying)
    {
        // Transport start: from the top, with the freshest series. This is
        // a STOP transport by choice - holding a moment is FREEZE's job.
        // (Host mode strikes whatever chord the timeline says instead.)
        chordIdx = 0;
        samplesIntoChord = 0;
        samplesIntoBeat = 0;
        loopsCompleted = 0;
        needStrike = hostMode;
        copyLoopIfDirty();
    }
    if (! isPlaying && wasPlaying)
    {
        stopSoundingNotes (localMidi, 0);
        localMidi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        needStrike = false;
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

    // A clicked card jumps the loop there on the spot (standalone only -
    // in a DAW the timeline owns the position).
    if (const int jump = jumpRequest.exchange (-1); jump >= 0 && isPlaying && ! hostMode)
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
        // nothing advances; unfreeze resumes (host mode snaps back to the
        // timeline's chord).
    }
    else if (isPlaying && hostMode)
    {
        processHostBlock (localMidi, numSamples, sampleRate,
                          hostPpq, juce::jlimit (30.0, 300.0, (double) bpm.load()));
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

                strikeChord (localMidi, offset);
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

    // Output: the family synth renders in-process when chosen (in a DAW the
    // MIDI still flows to the host alongside, like Scale Shifter); the
    // device mirror is standalone-only (try-lock: device swaps never stall
    // audio).
    if (synthOn.load())
    {
        sound.render (buffer, localMidi, synthVoice.load(),
                      juce::Decibels::decibelsToGain (synthVolDb.load()));
        synthPeak.store (sound.lastPeak());
    }
    else if (! hostMode && ! localMidi.isEmpty())
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

    // The chords stream to the host as MIDI.
    if (hostMode)
    {
        midiMessages.clear();
        midiMessages.addEvents (localMidi, 0, numSamples, 0);
    }
}

// In a DAW the loop is locked to the host timeline: position, boundaries and
// beats all derive from ppq, so loop jumps and re-plays land identically.
void ChordsProcessor::processHostBlock (juce::MidiBuffer& localMidi, int numSamples, double sampleRate,
                                        double ppqStart, double bpmNow)
{
    const double spq = sampleRate * 60.0 / bpmNow;                                 // samples per quarter
    const double qpc = 4.0 * (double) juce::jlimit (1, 4, barsPerChord.load());    // quarters per chord (4/4)

    if (currentLoop.empty())
        copyLoopIfDirty();
    if (currentLoop.empty())
        return;

    constexpr double eps = 1.0e-4;
    double ppq = juce::jmax (0.0, ppqStart);
    int offset = 0;
    while (offset < numSamples)
    {
        const int size = juce::jmax (1, (int) currentLoop.size());
        const double passLen = qpc * (double) size;
        const double inPass = std::fmod (ppq, passLen);
        const int gridIdx = juce::jlimit (0, size - 1, (int) (inPass / qpc));

        if (gridIdx != chordIdx || needStrike)
        {
            // Wrapping to the pass top completes a loop; a pending roll or
            // re-voice is adopted at any chord boundary.
            if (gridIdx == 0 && chordIdx == size - 1 && ! needStrike)
                ++loopsCompleted;
            if (copyLoopIfDirty())
                loopsCompleted = 0;
            chordIdx = juce::jmin (gridIdx, (int) currentLoop.size() - 1);
            strikeChord (localMidi, offset);
            needStrike = false;

            // Auto roll: entering the last chord of the Nth pass.
            if (autoRollOn.load() && ! autoRollPending.load() && ! loopDirty.load()
                && chordIdx == (int) currentLoop.size() - 1
                && loopsCompleted >= autoRollLoops.load() - 1)
                autoRollPending.store (true);
        }

        // (No metronome in a DAW - hosts have their own click.)

        // Advance to the next event: beat, chord boundary, or block end.
        const double nextBeat = std::floor (ppq + eps) + 1.0;
        const double nextChord = ppq + (std::floor (inPass / qpc + eps) + 1.0) * qpc - inPass;
        const int chunk = juce::jlimit (1, numSamples - offset,
                                        (int) std::llround ((juce::jmin (nextBeat, nextChord) - ppq) * spq));
        offset += chunk;
        ppq += (double) chunk / spq;
    }

    playingChord.store (chordIdx);
    const double qpcPass = qpc * (double) juce::jmax (1, (int) currentLoop.size());
    chordProgress.store ((float) (std::fmod (std::fmod (ppq, qpcPass), qpc) / qpc));
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
    if (const int flavour = alea::flavourFromChoice (choice); flavour >= 0)
    {
        synthVoice.store (flavour);
        synthOn.store (true);
        setMidiOutputDevice ({});
    }
    else
    {
        // Empty = MIDI to the DAW (plugin); a device id = standalone MIDI out.
        synthOn.store (false);
        setMidiOutputDevice (isStandaloneLike() ? choice : juce::String());
    }
}

juce::String ChordsProcessor::getStandaloneOutput() const
{
    return synthOn.load() ? alea::choiceForFlavour (synthVoice.load())
                          : getMidiOutputId();
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
    state.setProperty ("bpm", (double) bpm.load(), nullptr);
    state.setProperty ("barsPerChord", barsPerChord.load(), nullptr);
    state.setProperty ("octaves", octaveMask.load(), nullptr);
    state.setProperty ("smoothVoicing", smoothVoicing.load(), nullptr);
    state.setProperty ("openVoicing", openVoicing.load(), nullptr);
    state.setProperty ("bassNote", bassNote.load(), nullptr);
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
    // (uiWidth/uiHeight in older states are deliberately ignored - window
    // size is not persisted; see the note in ChordsProcessor.h.)
    bpm.store (juce::jlimit (30.0f, 300.0f, (float) (double) state.getProperty ("bpm", 90.0)));
    barsPerChord.store (juce::jlimit (1, 4, (int) state.getProperty ("barsPerChord", 1)));
    // "octave" (single) was the pre-multi-select property name.
    const int legacyMask = 1 << (juce::jlimit (2, 4, (int) state.getProperty ("octave", 3)) - 2);
    octaveMask.store (juce::jlimit (1, 7, (int) state.getProperty ("octaves", legacyMask)));
    smoothVoicing.store (state.getProperty ("smoothVoicing", false));
    openVoicing.store (state.getProperty ("openVoicing", false));
    bassNote.store (state.getProperty ("bassNote", false));
    metronomeOn.store (state.getProperty ("metronome", false));
    clickVolDb.store (juce::jlimit (-12.0f, 12.0f, (float) (double) state.getProperty ("clickVol", 0.0)));
    autoRollOn.store (state.getProperty ("autoRoll", false));
    autoRollLoops.store (juce::jlimit (1, 8, (int) state.getProperty ("autoRollLoops", 2)));
    synthVolDb.store (juce::jlimit (-24.0f, 6.0f, (float) (double) state.getProperty ("synthVol", 0.0)));
    setStandaloneOutput (state.getProperty ("output",
        isStandaloneLike() ? "synth" : "").toString());

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
