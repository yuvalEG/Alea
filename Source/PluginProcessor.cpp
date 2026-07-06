#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

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
    pTranspose    = raw ("transpose");    pSynthVol      = raw ("synthVol");

    // Fresh instances boot into the first factory preset, marked as selected,
    // so there's music (and a lit bubble) before any knob is touched. Saved
    // sessions overwrite this via setStateInformation right after.
    presets::apply (apvts, presets::factory()[0]);
    currentPreset.store (0);

    // Standalone makes sound out of the box; plugins default to pure MIDI.
    synthOn.store (wrapperType == wrapperType_Standalone);
}

void AleaAudioProcessor::cacheScaleRefs (char scale, ScaleRefs& refs)
{
    for (int pc = 0; pc < 12; ++pc)
        refs.notes[pc] = raw (params::noteId (scale, pc).toRawUTF8());
    for (int r = 0; r < params::numRests; ++r)
        refs.rests[r] = raw (params::restId (scale, r).toRawUTF8());

    const auto s = juce::String::charToString (scale);
    refs.octMin = raw ((s + "OctMin").toRawUTF8());
    refs.octMax = raw ((s + "OctMax").toRawUTF8());
    refs.velMin = raw ((s + "VelMin").toRawUTF8());
    refs.velMax = raw ((s + "VelMax").toRawUTF8());
    refs.root   = raw ((s + "Root").toRawUTF8());
}

void AleaAudioProcessor::readSnapshot (const ScaleRefs& refs, ScaleSnapshot& snap) const
{
    snap.numPitchClasses = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (refs.notes[pc]->load() > 0.5f)
            snap.pitchClasses[snap.numPitchClasses++] = pc;

    snap.numRests = 0;
    for (int r = 0; r < params::numRests; ++r)
        if (refs.rests[r]->load() > 0.5f)
            snap.rests[snap.numRests++] = r;

    snap.octMin = (int) refs.octMin->load();
    snap.octMax = juce::jmax ((int) refs.octMax->load(), snap.octMin); // Max auto-clamps >= Min (spec 4.1)
    snap.velMin = juce::jmax (1, (int) refs.velMin->load());           // never emit velocity 0 (spec 4.1)
    snap.velMax = juce::jmax ((int) refs.velMax->load(), snap.velMin);
    snap.root   = (int) refs.root->load();
}

bool AleaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AleaAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentNote = -1;
    lastPpqEnd = -1.0;
    internalPpq = 0.0;
    wasPlaying = false;

    // Internal synth chain: a warm pad voicing. Soft attack, high sustain,
    // long release so tails ring well into the following notes.
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

    // Different L/R delay times give the single sine voice a stereo field.
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

void AleaAudioProcessor::sendAllNotesOff (juce::MidiBuffer& midi, int sampleOffset)
{
    if (currentNote >= 0)
    {
        midi.addEvent (juce::MidiMessage::noteOff (currentNoteChannel, currentNote), sampleOffset);
        currentNote = -1;
    }
    midi.addEvent (juce::MidiMessage::allNotesOff (channelA), sampleOffset);
    midi.addEvent (juce::MidiMessage::allNotesOff (channelB), sampleOffset);
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

void AleaAudioProcessor::setMidiOutputDevice (const juce::String& identifier)
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

juce::String AleaAudioProcessor::getMidiOutputId() const
{
    const juce::ScopedLock sl (midiOutLock);
    return midiOutputId;
}

void AleaAudioProcessor::setStandaloneOutput (const juce::String& choice)
{
    if (choice == "synth")
    {
        synthOn.store (true);
        setMidiOutputDevice ({});
    }
    else
    {
        synthOn.store (false);
        // Device output only exists in the standalone; in a DAW the host
        // owns MIDI routing.
        setMidiOutputDevice (wrapperType == wrapperType_Standalone ? choice : juce::String());
    }
}

juce::String AleaAudioProcessor::getStandaloneOutput() const
{
    return synthOn.load() ? "synth" : getMidiOutputId();
}

// Polyphonic additive voices following our own note stream, into stereo
// delay + reverb. Velocity opens the brightness envelope (upper partials),
// not just the volume.
void AleaAudioProcessor::renderSynth (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi)
{
    const double sampleRate = getSampleRate();
    const int numSamples = buffer.getNumSamples();
    if (sampleRate <= 0.0 || numSamples <= 0 || buffer.getNumChannels() < 1)
        return;

    auto* left = buffer.getWritePointer (0);
    const float master = juce::Decibels::decibelsToGain (pSynthVol->load());

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        for (auto& v : voices)
        {
            if (! v.amp.isActive())
                continue;
            // A dominant equal pair ~10 cents apart gives the audible slow
            // phasing (waves breathing in and out); a third, quieter osc
            // detuned the other way plus sub and fixed 2nd/3rd fill the tone.
            // Normalized so eight ringing voices stay clear of the limiter.
            const double inc1 = juce::MathConstants<double>::twoPi * v.freq / sampleRate;
            const double inc2 = inc1 * 1.0055;
            const double inc3 = inc1 * 0.9965;
            for (int n = pos; n < end; ++n)
            {
                const float env = v.amp.getNextSample();
                const float shimmer = 0.14f * v.velocity * v.bright.getNextSample();
                const float s = (0.44f * ((float) std::sin (v.phase) + (float) std::sin (v.phase2))
                               + 0.16f * (float) std::sin (v.phase3)
                               + 0.28f * (float) std::sin (0.5 * v.phase)
                               + (0.22f + shimmer) * (float) std::sin (2.0 * v.phase)
                               + 0.08f * (float) std::sin (3.0 * v.phase)) / 1.55f;
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
            voice->amp.noteOn();
            voice->bright.noteOn();
        }
        else if (msg.isNoteOff() || msg.isAllNotesOff())
        {
            // Release scales with how long the note was held: staccato notes
            // get a snappy tail, pads ring long.
            for (auto& v : voices)
                if (v.note >= 0 && (msg.isAllNotesOff() || v.note == msg.getNoteNumber()))
                {
                    auto params = v.amp.getParameters();
                    params.release = juce::jlimit (0.12f, 2.2f,
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

void AleaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    generateBlock (buffer, midi);

    // The built-in synth renders in any wrapper when chosen (the MIDI still
    // flows to the host alongside). The device mirror is standalone-only,
    // with a try-lock so device swaps never stall the audio thread.
    if (synthOn.load())
    {
        renderSynth (buffer, midi);
    }
    else if (wrapperType == wrapperType_Standalone && ! midi.isEmpty())
    {
        const juce::ScopedTryLock sl (midiOutLock);
        if (sl.isLocked() && midiOutput != nullptr)
            midiOutput->sendBlockOfMessages (midi, juce::Time::getMillisecondCounter() + 1.0, getSampleRate());
    }
}

void AleaAudioProcessor::generateBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    // Standalone has no host: PLAY/STOP drives the transport and the clock
    // is always the internal one (spec section 10).
    const bool standalone = wrapperType == wrapperType_Standalone;
    const bool isPlaying = standalone ? standaloneTransport.load()
                                      : position.hasValue() && position->getIsPlaying();
    hostIsPlaying.store (isPlaying);

    const bool freeRun = standalone || (int) pTempoSource->load() == 1;
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

        // Sweep toggled off while stopped: park the morph at the position
        // the bar was showing (same rule as during playback).
        const bool sweepOnNow = pAutoSweep->load() > 0.5f;
        if (! sweepOnNow && lastSweepOn)
        {
            if (auto* p = apvts.getParameter ("morphPos"))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) morphPercent.load()));
        }
        else if (sweepOnNow && ! lastSweepOn)
        {
            morphPercent.store (pMorphPos->load()); // bar shows the parked position while stopped
        }
        lastSweepOn = sweepOnNow;

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

    // Engaging auto-sweep mid-play resumes the journey from wherever the
    // morph currently sits (inverse-curve anchoring, same math as scrubbing).
    // Disengaging parks the morph where the sweep left it - toggling the
    // sweep never teleports the position in either direction.
    const bool sweepOn = pAutoSweep->load() > 0.5f;
    if (sweepOn && ! lastSweepOn)
    {
        const double legPpq = sweepLegPpq (bpm);
        const double pos = pMorphPos->load() / 100.0;
        // A completed One-Shot re-engages from A - resuming at 100% would
        // pin the morph at B and the button would appear dead. Anything
        // mid-journey resumes exactly where it parked.
        if (legPpq <= 1.0e-9 || ((int) pMorphMode->load() == params::oneShot && pos >= 0.999))
            sweepAnchorPpq = ppqStart;
        else
            sweepAnchorPpq = ppqStart - inverseCurve (pos, (int) pMorphCurve->load()) * legPpq;
    }
    else if (! sweepOn && lastSweepOn)
    {
        if (auto* p = apvts.getParameter ("morphPos"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) morphPercent.load()));
    }
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

    // Shrinking the interval must pull an already-scheduled event closer:
    // dragging the Free slider through large values (or leaving a long Sync
    // division) schedules a far-future note that a later, smaller setting
    // never revisits - the "silence after slider drag" stall. Random mode is
    // exempt (re-rolling here would burn rng draws and break determinism).
    {
        const int im = (int) pIntervalMode->load();
        if (im != params::random)
        {
            const double curInterval = im == params::free
                ? juce::jmax (0.001, (double) pIntervalFree->load() * bpm / 60.0)
                : params::divisionBars[(size_t) pIntervalSync->load()] * 4.0;
            nextEventPpq = juce::jmin (nextEventPpq, juce::jmax (ppqStart, restEndPpq) + curInterval);
        }
    }

    morphPercent.store (morphAt (ppqStart, bpm) * 100.0);

    // Freeze: hold the sounding note indefinitely - no note-offs, no new
    // events. Time (and morph) keeps moving; unfreezing re-anchors cleanly.
    if (pFreeze->load() > 0.5f)
    {
        // With short notes the stream is usually between notes when Freeze
        // lands - grab the last note and hold it, so Freeze freezes
        // something audible. Freezing during a rest freezes the rest itself:
        // silence held is still the current moment.
        if (! wasFrozen && currentNote < 0 && lastNote.load() >= 0 && activeRest.load() < 0)
        {
            const int held = lastNote.load();
            const auto vel = (juce::uint8) juce::jlimit (1, 127, activeVelocity.load());
            midi.addEvent (juce::MidiMessage::noteOn (channelA, held, vel), 0);
            currentNote = held;
            currentNoteChannel = channelA;
            activeNote.store (held);
        }
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
            midi.addEvent (juce::MidiMessage::noteOff (currentNoteChannel, currentNote), ppqToOffset (offAt));
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
            const int restSource = &src == &snapA ? 0 : 1;
            restEndPpq = eventPpq + params::restBars[(size_t) slot] * 4.0;
            nextEventPpq = restEndPpq;
            activeRest.store (slot);
            activeRestSource.store (restSource);
            history[(size_t) (historyCount.load() % historyCapacity)].store (0x200 | (restSource << 8) | slot);
            historyCount.fetch_add (1);
            continue;
        }

        // Octave and velocity ranges interpolate between A and B by m (spec 5.2).
        const int octMin = (int) std::lround (snapA.octMin + (snapB.octMin - snapA.octMin) * m);
        const int octMax = juce::jmax (octMin, (int) std::lround (snapA.octMax + (snapB.octMax - snapA.octMax) * m));
        const int velMin = (int) std::lround (snapA.velMin + (snapB.velMin - snapA.velMin) * m);
        const int velMax = juce::jmax (velMin, (int) std::lround (snapA.velMax + (snapB.velMax - snapA.velMax) * m));

        const int octave = octMin + rng.nextInt (octMax - octMin + 1);
        // The octave span starts at the scale's root: root A + octave 3
        // plays A3..G#4. C3 = 60 convention (spec 5.3), plus global transpose.
        const int pcOffset = ((src.pitchClasses[pick] - src.root) % 12 + 12) % 12;
        const int note = 24 + 12 * octave + src.root + pcOffset
                       + (int) pTranspose->load();

        nextEventPpq = eventPpq + intervalPpqAt (bpm);

        if (note < 0 || note > 127) // out of range: drop, never wrap (spec 5.3)
            continue;

        // Origin scale (spec 5.4): the pool it was drawn from, except shared
        // notes follow morph position. Decides highlight color AND channel.
        const int pc = src.pitchClasses[pick];
        auto contains = [pc] (const ScaleSnapshot& s)
        {
            for (int i = 0; i < s.numPitchClasses; ++i)
                if (s.pitchClasses[i] == pc) return true;
            return false;
        };
        const bool inBoth = contains (snapA) && contains (snapB);
        const int source = inBoth ? (m < 0.5 ? 0 : 1) : (&src == &snapA ? 0 : 1);
        const int channel = source == 1 ? channelB : channelA;

        if (currentNote >= 0) // monophonic: off before on (spec principle 4)
            midi.addEvent (juce::MidiMessage::noteOff (currentNoteChannel, currentNote), offset);

        const auto velocity = (juce::uint8) juce::jlimit (1, 127, velMin + rng.nextInt (velMax - velMin + 1));
        midi.addEvent (juce::MidiMessage::noteOn (channel, note, velocity), offset);
        currentNote = note;
        currentNoteChannel = channel;
        noteOffPpq = eventPpq + lengthPpqAt (bpm); // gate runs from the note's start

        notesSent.fetch_add (1);
        lastNote.store (note);
        activeSource.store (source);
        activeNote.store (note);
        activeSourcePc.store (pc); // pre-transpose: the key to light on the scale keyboards
        activeVelocity.store ((int) velocity);
        activeRest.store (-1);
        history[(size_t) (historyCount.load() % historyCapacity)].store (note | (source << 8) | ((int) velocity << 10));
        historyCount.fetch_add (1);
    }
}

void AleaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 2, nullptr);
    state.setProperty ("morphCC", morphCC.load(), nullptr);
    state.setProperty ("standaloneOutput", getStandaloneOutput(), nullptr);
    state.setProperty ("currentPreset", currentPreset.load(), nullptr);
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

            // States saved before presets tracked selection have no
            // currentPreset property: treat them as a fresh boot rather
            // than showing an unmarked mystery patch.
            const auto presetProp = apvts.state.getProperty ("currentPreset");
            if (presetProp.isVoid())
            {
                presets::apply (apvts, presets::factory()[0]);
                currentPreset.store (0);
            }
            else
            {
                currentPreset.store ((int) presetProp);
            }

            // Restore the remembered output (device opening needs the
            // message thread; skip anywhere else).
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                setStandaloneOutput (apvts.state.getProperty ("standaloneOutput",
                    wrapperType == wrapperType_Standalone ? "synth" : "").toString());
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
