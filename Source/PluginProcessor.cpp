#include "PluginProcessor.h"
#include "PluginEditor.h"

// A silent stereo output bus is required: Ableton Live (and some other hosts)
// won't open a VST3 that has no audio outputs, so Alea presents as an
// instrument that emits MIDI and outputs silence.
AleaAudioProcessor::AleaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AleaState", params::createLayout())
{
    cacheScaleRefs ('a', refA);
    cacheScaleRefs ('b', refB);

    pIntervalMode = raw ("intervalMode"); pIntervalSync = raw ("intervalSync"); pIntervalFree = raw ("intervalFree");
    pLengthMode   = raw ("lengthMode");   pLengthSync   = raw ("lengthSync");   pLengthFree   = raw ("lengthFree");
    pMorphPos     = raw ("morphPos");     pAutoSweep    = raw ("autoSweep");
    pMorphDurMode = raw ("morphDurMode"); pMorphDurBars = raw ("morphDurBars");
    pMorphDurFree = raw ("morphDurFree"); pMorphDurUnit = raw ("morphDurUnit");
    pMorphMode    = raw ("morphMode");    pMorphCurve   = raw ("morphCurve");
    pTempoSource  = raw ("tempoSource");  pInternalTempo = raw ("internalTempo");
    pFreeze       = raw ("freeze");
}

void AleaAudioProcessor::cacheScaleRefs (char scale, ScaleRefs& refs)
{
    for (int pc = 0; pc < 12; ++pc)
        refs.notes[pc] = raw (params::noteId (scale, pc).toRawUTF8());
    for (int r = 0; r < 5; ++r)
        refs.rests[r] = raw (params::restId (scale, r).toRawUTF8());

    const auto s = juce::String::charToString (scale);
    refs.octMin = raw ((s + "OctMin").toRawUTF8());
    refs.octMax = raw ((s + "OctMax").toRawUTF8());
    refs.velMin = raw ((s + "VelMin").toRawUTF8());
    refs.velMax = raw ((s + "VelMax").toRawUTF8());
}

void AleaAudioProcessor::readSnapshot (const ScaleRefs& refs, ScaleSnapshot& snap) const
{
    snap.numPitchClasses = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (refs.notes[pc]->load() > 0.5f)
            snap.pitchClasses[snap.numPitchClasses++] = pc;

    snap.numRests = 0;
    for (int r = 0; r < 5; ++r)
        if (refs.rests[r]->load() > 0.5f)
            snap.rests[snap.numRests++] = r;

    snap.octMin = (int) refs.octMin->load();
    snap.octMax = juce::jmax ((int) refs.octMax->load(), snap.octMin); // Max auto-clamps >= Min (spec 4.1)
    snap.velMin = juce::jmax (1, (int) refs.velMin->load());           // never emit velocity 0 (spec 4.1)
    snap.velMax = juce::jmax ((int) refs.velMax->load(), snap.velMin);
}

bool AleaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AleaAudioProcessor::prepareToPlay (double, int)
{
    currentNote = -1;
    lastPpqEnd = -1.0;
    internalPpq = 0.0;
    wasPlaying = false;
}

void AleaAudioProcessor::sendAllNotesOff (juce::MidiBuffer& midi, int sampleOffset)
{
    if (currentNote >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (midiChannel, currentNote), sampleOffset);
        currentNote = -1;
    }
    midi.addEvent (juce::MidiMessage::allNotesOff (midiChannel), sampleOffset);
    activeNote.store (-1);
}

void AleaAudioProcessor::resetSchedule (double ppq)
{
    nextEventPpq = ppq;   // first draw fires immediately
}

double AleaAudioProcessor::sweepLegPpq (double bpm) const
{
    if ((int) pMorphDurMode->load() == 0) // Sync: bars
        return params::morphDurBarValues[(size_t) pMorphDurBars->load()] * 4.0;

    const double seconds = pMorphDurFree->load() * ((int) pMorphDurUnit->load() == 1 ? 60.0 : 1.0);
    return seconds * bpm / 60.0;
}

// Inverse of the sweep curve: which leg fraction shows position p?
static double inverseCurve (double p, int curve)
{
    p = juce::jlimit (0.0, 1.0, p);
    switch (curve)
    {
        case params::exponential: return std::sqrt (p);
        case params::logarithmic: return 1.0 - std::sqrt (1.0 - p);
        case params::sCurve:
        {
            double t = p; // Newton iterations on t^2(3-2t) = p
            for (int i = 0; i < 5; ++i)
            {
                const double denom = 6.0 * t * (1.0 - t);
                if (std::abs (denom) < 1.0e-6)
                    break;
                t -= (t * t * (3.0 - 2.0 * t) - p) / denom;
                t = juce::jlimit (0.0, 1.0, t);
            }
            return t;
        }
        default: return p;
    }
}

// Morph position (0..1) as a pure function of the beat-clock position, so it
// is deterministic and re-syncs after host loop jumps (spec section 7).
double AleaAudioProcessor::morphAt (double ppq, double bpm) const
{
    if (pAutoSweep->load() < 0.5f)
        return pMorphPos->load() / 100.0;

    const double legPpq = sweepLegPpq (bpm);

    if (legPpq <= 1.0e-9)
        return 1.0; // zero duration = instantly at B

    // Legs elapsed since the sweep was engaged - not since bar 1, otherwise
    // enabling auto-sweep mid-song lands instantly at B in One-Shot mode.
    const double t = juce::jmax (0.0, ppq - sweepAnchorPpq) / legPpq;
    double leg = 0.0;

    switch ((int) pMorphMode->load())
    {
        case params::oneShot: leg = juce::jmin (t, 1.0); break;
        case params::loop:    leg = t - std::floor (t); break;
        case params::bounce:
        {
            const double ft = std::fmod (t, 2.0);
            leg = ft < 1.0 ? ft : 2.0 - ft;
            break;
        }
        default: break;
    }

    switch ((int) pMorphCurve->load())
    {
        case params::exponential: return leg * leg;
        case params::sCurve:      return leg * leg * (3.0 - 2.0 * leg);
        case params::logarithmic: return 1.0 - (1.0 - leg) * (1.0 - leg);
        default:                  return leg;
    }
}

double AleaAudioProcessor::intervalPpqAt (double bpm)
{
    switch ((int) pIntervalMode->load())
    {
        case params::free:   return juce::jmax (0.001, (double) pIntervalFree->load() * bpm / 60.0);
        case params::random:
        {
            const int i = rng.nextInt (4);
            lastRandomInterval.store (i);
            return params::randomPoolBars[(size_t) i] * 4.0;
        }
        default:             return params::divisionBars[(size_t) pIntervalSync->load()] * 4.0;
    }
}

double AleaAudioProcessor::lengthPpqAt (double bpm)
{
    switch ((int) pLengthMode->load())
    {
        case params::free:   return juce::jmax (0.001, (double) pLengthFree->load() * bpm / 60.0);
        case params::random:
        {
            const int i = rng.nextInt (4);
            lastRandomLength.store (i);
            return params::randomPoolBars[(size_t) i] * 4.0;
        }
        default:             return params::divisionBars[(size_t) pLengthSync->load()] * 4.0;
    }
}

void AleaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Incoming MIDI: CC learn + learned CC drives Morph Position (spec 7).
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (! msg.isController())
            continue;
        if (ccLearnArmed.exchange (false))
            morphCC.store (msg.getControllerNumber());
        if (msg.getControllerNumber() == morphCC.load())
            if (auto* p = apvts.getParameter ("morphPos"))
                p->setValueNotifyingHost ((float) msg.getControllerValue() / 127.0f);
    }

    midi.clear();

    if (panicRequested.exchange (false)) // spec 8: Panic sends All Notes Off immediately
        sendAllNotesOff (midi, 0);

    auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition()
                                              : juce::Optional<juce::AudioPlayHead::PositionInfo>();

    const bool isPlaying = position.hasValue() && position->getIsPlaying();
    hostIsPlaying.store (isPlaying);

    const bool freeRun = (int) pTempoSource->load() == 1;
    const double bpm = freeRun ? (double) pInternalTempo->load()
                               : (position.hasValue() ? position->getBpm().orFallback ((double) pInternalTempo->load())
                                                      : (double) pInternalTempo->load());
    hostBpm.store (bpm);

    if (! isPlaying)
    {
        if (wasPlaying)
            sendAllNotesOff (midi, 0);
        wasPlaying = false;
        lastFreeRun = freeRun;
        lastSweepOn = pAutoSweep->load() > 0.5f;
        activeRest.store (-1);
        return;
    }

    const double sampleRate = getSampleRate();
    const int numSamples = buffer.getNumSamples();
    if (sampleRate <= 0.0 || numSamples <= 0 || bpm <= 0.0)
        return;

    const double ppqPerSample = bpm / 60.0 / sampleRate;

    // Switching tempo source mid-play moves us between two different beat
    // clocks; without re-anchoring, the scheduler waits for a position the
    // new clock may never reach (= a note frozen forever).
    bool clockSwitched = false;
    if (freeRun != lastFreeRun)
    {
        clockSwitched = true;
        lastFreeRun = freeRun;
        if (freeRun)
            internalPpq = juce::jmax (0.0, lastPpqEnd); // carry the beat count over
    }

    // Beat-clock position: host timeline, or our own accumulator in Free-Run
    // (which deliberately ignores host jumps, spec section 7).
    double ppqStart;
    if (freeRun)
    {
        if (! wasPlaying)
            internalPpq = 0.0;
        ppqStart = internalPpq;
        internalPpq += numSamples * ppqPerSample;
    }
    else
    {
        ppqStart = position->getPpqPosition().orFallback (0.0);
    }

    const double ppqEnd = ppqStart + numSamples * ppqPerSample;

    // Host jump detection needs slack: our predicted block end and the
    // host's reported position disagree by tiny floating-point/rounding
    // amounts every block, and treating that jitter as a "jump backwards"
    // resets the scheduler every block (= runaway note bursts). Only a
    // discontinuity larger than one block is a real loop/locator jump.
    const double blockPpq = numSamples * ppqPerSample;
    const double jumpTolerance = juce::jmax (0.1, 2.0 * blockPpq);

    if (! wasPlaying || clockSwitched || (! freeRun && ppqStart < lastPpqEnd - jumpTolerance))
    {
        if (wasPlaying)
            sendAllNotesOff (midi, 0);
        resetSchedule (ppqStart);
        activeRest.store (-1);
        sweepAnchorPpq = ppqStart; // sweep restarts from play/loop start
    }
    else if (nextEventPpq < ppqStart - jumpTolerance)
    {
        // Jumped forward (locator/skip): re-anchor instead of burst-firing
        // every missed event to catch up.
        resetSchedule (ppqStart);
    }

    wasPlaying = true;
    lastPpqEnd = ppqEnd;
    hostPpq.store (ppqStart);

    // Engaging auto-sweep mid-play anchors the sweep here, so it always
    // starts its journey from A at the moment you turn it on.
    const bool sweepOn = pAutoSweep->load() > 0.5f;
    if (sweepOn && ! lastSweepOn)
        sweepAnchorPpq = ppqStart;
    lastSweepOn = sweepOn;

    // Scrub during auto-sweep (spec 4.3.2): re-anchor so the sweep sits at
    // the dragged position right now, then keeps travelling forward.
    const float scrub = scrubRequest.exchange (-1.0f);
    if (scrub >= 0.0f && sweepOn)
    {
        const double legPpq = sweepLegPpq (bpm);
        if (legPpq > 1.0e-9)
            sweepAnchorPpq = ppqStart - inverseCurve (scrub / 100.0, (int) pMorphCurve->load()) * legPpq;
    }

    morphPercent.store (morphAt (ppqStart, bpm) * 100.0);

    // Freeze: hold the sounding note indefinitely - no note-offs, no new
    // events. Time (and morph) keeps moving; unfreezing re-anchors cleanly.
    if (pFreeze->load() > 0.5f)
    {
        wasFrozen = true;
        return;
    }
    if (wasFrozen)
    {
        wasFrozen = false;
        resetSchedule (ppqStart);
        if (currentNote >= 0)
            noteOffPpq = ppqStart; // release the held note as the stream resumes
    }

    if (activeRest.load() >= 0 && ppqStart >= restEndPpq)
        activeRest.store (-1);

    ScaleSnapshot snapA, snapB;
    readSnapshot (refA, snapA);
    readSnapshot (refB, snapB);

    auto ppqToOffset = [&] (double ppq)
    {
        return juce::jlimit (0, numSamples - 1, (int) ((ppq - ppqStart) / ppqPerSample));
    };

    // Walk note-off and pick events through this block in beat order.
    for (int guard = 0; guard < 4096; ++guard)
    {
        const double offAt = currentNote >= 0 ? noteOffPpq : std::numeric_limits<double>::max();
        const double eventAt = juce::jmin (offAt, nextEventPpq);
        if (eventAt >= ppqEnd)
            break;

        if (offAt <= nextEventPpq)
        {
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, currentNote), ppqToOffset (offAt));
            currentNote = -1;
            activeNote.store (-1);
            continue;
        }

        // Pick-pool draw (spec 5.1/5.2): source scale by morph-weighted coin
        // flip, then a uniform outcome from that scale's notes + rests.
        const double eventPpq = nextEventPpq;
        const double m = morphAt (eventPpq, bpm);
        const int offset = ppqToOffset (eventPpq);
        const ScaleSnapshot& src = rng.nextDouble() < (1.0 - m) ? snapA : snapB;
        const int poolSize = src.numPitchClasses + src.numRests;

        if (poolSize == 0) // empty scale: continuous silence, keep running (spec 10)
        {
            nextEventPpq += intervalPpqAt (bpm);
            continue;
        }

        const int pick = rng.nextInt (poolSize);

        if (pick >= src.numPitchClasses) // a rest: silence for the rest's own duration
        {
            const int slot = src.rests[pick - src.numPitchClasses];
            restEndPpq = eventPpq + params::restBars[(size_t) slot] * 4.0;
            nextEventPpq = restEndPpq;
            activeRest.store (slot);
            activeRestSource.store (&src == &snapA ? 0 : 1);
            continue;
        }

        // Octave and velocity ranges interpolate between A and B by m (spec 5.2).
        const int octMin = (int) std::lround (snapA.octMin + (snapB.octMin - snapA.octMin) * m);
        const int octMax = juce::jmax (octMin, (int) std::lround (snapA.octMax + (snapB.octMax - snapA.octMax) * m));
        const int velMin = (int) std::lround (snapA.velMin + (snapB.velMin - snapA.velMin) * m);
        const int velMax = juce::jmax (velMin, (int) std::lround (snapA.velMax + (snapB.velMax - snapA.velMax) * m));

        const int octave = octMin + rng.nextInt (octMax - octMin + 1);
        const int note = 24 + 12 * octave + src.pitchClasses[pick]; // C3 = 60 convention (spec 5.3)

        nextEventPpq = eventPpq + intervalPpqAt (bpm);

        if (note < 0 || note > 127) // out of range: drop, never wrap (spec 5.3)
            continue;

        if (currentNote >= 0) // monophonic: off before on (spec principle 4)
            midi.addEvent (juce::MidiMessage::noteOff (midiChannel, currentNote), offset);

        const auto velocity = (juce::uint8) juce::jlimit (1, 127, velMin + rng.nextInt (velMax - velMin + 1));
        midi.addEvent (juce::MidiMessage::noteOn (midiChannel, note, velocity), offset);
        currentNote = note;
        noteOffPpq = eventPpq + lengthPpqAt (bpm); // gate runs from the note's start

        // Origin scale for the green highlight (spec 5.4): the pool it was
        // drawn from, except shared notes follow morph position.
        const int pc = src.pitchClasses[pick];
        auto contains = [pc] (const ScaleSnapshot& s)
        {
            for (int i = 0; i < s.numPitchClasses; ++i)
                if (s.pitchClasses[i] == pc) return true;
            return false;
        };
        const bool inBoth = contains (snapA) && contains (snapB);
        const int source = inBoth ? (m < 0.5 ? 0 : 1) : (&src == &snapA ? 0 : 1);

        notesSent.fetch_add (1);
        lastNote.store (note);
        activeSource.store (source);
        activeNote.store (note);
        activeRest.store (-1);
        history[(size_t) (historyCount.load() % historyCapacity)].store (note | (source << 8));
        historyCount.fetch_add (1);
    }
}

void AleaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 2, nullptr);
    state.setProperty ("morphCC", morphCC.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AleaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            morphCC.store ((int) apvts.state.getProperty ("morphCC", -1));
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
