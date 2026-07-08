#include "Sound.h"
#include "PianoData.h"
#include <mutex>

namespace alea
{

//==============================================================================
// The flavour table - the single source of truth for both apps' OUT menus,
// state strings and the engine.

const std::array<FlavourInfo, numFlavours>& flavourTable()
{
    static const std::array<FlavourInfo, numFlavours> table { {
        { warmPad,  groupSynth,   "synth",          "Warm Pad"  },
        { pureSine, groupClean,   "synth:sine",     "Pure Sine" },
        { softSaw,  groupSynth,   "synth:saw",      "Soft Saw"  },
        { strings,  groupSynth,   "synth:strings",  "Strings"   },
        { ePiano,   groupSynth,   "synth:epiano",   "E-Piano"   },
        { organ,    groupSynth,   "synth:organ",    "Organ"     },
        { pluck,    groupSynth,   "synth:pluck",    "Pluck"     },
        { bells,    groupSynth,   "synth:bells",    "Bells"     },
        { triangle, groupClean,   "synth:triangle", "Triangle"  },
        { glass,    groupClean,   "synth:glass",    "Glass"     },
        { piano,    groupSampled, "piano",          "Piano"     },
    } };
    return table;
}

const char* groupName (int group)
{
    switch (group)
    {
        case groupSampled: return "SAMPLED";
        case groupClean:   return "CLEAN";
        default:           return "SYNTH";
    }
}

int flavourFromChoice (const juce::String& choice)
{
    for (const auto& f : flavourTable())
        if (choice == f.choice)
            return f.flavour;
    return -1;
}

juce::String choiceForFlavour (int flavour)
{
    for (const auto& f : flavourTable())
        if (f.flavour == flavour)
            return f.choice;
    return "synth";
}

//==============================================================================
// The sampled piano: Salamander Grand Piano by Alexander Holm (CC BY 3.0),
// one velocity layer trimmed to short mono zones every minor third. Decoded
// once per process and shared - DAWs open many instances.

namespace
{
    struct PianoZone
    {
        int rootNote = 60;
        double sampleRate = 48000.0;
        juce::AudioBuffer<float> data; // mono
    };

    const std::vector<PianoZone>& pianoZones()
    {
        static std::vector<PianoZone> zones;
        static std::once_flag once;
        std::call_once (once, []
        {
            juce::OggVorbisAudioFormat ogg;
            for (int i = 0; i < PianoData::namedResourceListSize; ++i)
            {
                const char* name = PianoData::namedResourceList[i]; // "p024_ogg"
                int size = 0;
                const char* data = PianoData::getNamedResource (name, size);
                const auto root = juce::String (name).retainCharacters ("0123456789").getIntValue();
                if (data == nullptr || size <= 0 || root <= 0 || root > 127)
                    continue;

                std::unique_ptr<juce::AudioFormatReader> reader (
                    ogg.createReaderFor (new juce::MemoryInputStream (data, (size_t) size, false), true));
                if (reader == nullptr)
                    continue;

                PianoZone zone;
                zone.rootNote = root;
                zone.sampleRate = reader->sampleRate;
                zone.data.setSize (1, (int) reader->lengthInSamples);
                reader->read (&zone.data, 0, (int) reader->lengthInSamples, 0, true, false);
                zones.push_back (std::move (zone));
            }
            std::sort (zones.begin(), zones.end(),
                       [] (const PianoZone& a, const PianoZone& b) { return a.rootNote < b.rootNote; });
        });
        return zones;
    }

    // How much of a voice feeds the stereo delay. The original four send
    // everything (their sound was tuned with it); clean tones stay nearly
    // dry so echoes never blur the pitch; the piano's room is reverb only.
    float delaySendFor (int flavour)
    {
        switch (flavour)
        {
            case pureSine: case triangle: case glass: return 0.25f;
            case piano:                               return 0.0f;
            case organ:                               return 0.35f;
            case ePiano: case bells:                  return 0.8f;
            default:                                  return 1.0f;
        }
    }
}

//==============================================================================
void SoundEngine::prepare (double sampleRate)
{
    sr = sampleRate;

    for (auto& v : voices)
    {
        v.amp.setSampleRate (sampleRate);
        v.amp.setParameters ({ 0.012f, 0.25f, 0.80f, 2.20f });
        v.amp.reset();
        v.bright.setSampleRate (sampleRate);
        v.bright.setParameters ({ 0.002f, 0.50f, 0.25f, 1.00f });
        v.bright.reset();
        v.phase = v.phase2 = v.phase3 = v.trem = 0.0;
        v.note = -1;
        v.sample = nullptr;
        // Karplus-Strong line, sized for the lowest reachable pitch.
        v.ks.assign ((size_t) juce::jmax (1024, (int) (sampleRate / 8.0)), 0.0f);
    }
    nextVoice = 0;

    // Different L/R delay times give the mono voices a stereo field.
    delayLineL.assign ((size_t) juce::jmax (1, (int) (sampleRate * 0.32)), 0.0f);
    delayLineR.assign ((size_t) juce::jmax (1, (int) (sampleRate * 0.48)), 0.0f);
    delayPosL = delayPosR = 0;
    dryBus.assign (8192, 0.0f);
    sendBus.assign (8192, 0.0f);

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

void SoundEngine::startVoice (Voice& v, int note, float velocity, int newFlavour)
{
    v.freq = juce::MidiMessage::getMidiNoteInHertz (note);
    v.velocity = velocity;
    // A sine has no timbre to spend velocity on, so spend it on dynamics:
    // a power curve gives soft notes a real pianissimo.
    v.gain = 0.06f + 0.94f * std::pow (velocity, 1.7f);
    v.note = note;
    v.heldSamples = 0;
    v.flavour = newFlavour;
    v.phase = v.phase2 = v.phase3 = v.trem = 0.0;
    v.sample = nullptr;

    auto amp = v.amp.getParameters();
    auto bright = v.bright.getParameters();
    amp.decay = 0.25f; amp.sustain = 0.80f;
    bright = { 0.002f, 0.50f, 0.25f, 1.00f };

    switch (newFlavour)
    {
        case pureSine: amp.attack = 0.006f; break;                      // clean
        case softSaw:  amp.attack = 0.003f; break;                      // plucky
        case strings:  amp.attack = 0.280f; break;                      // bow in
        case triangle: amp.attack = 0.006f; break;
        case glass:    amp.attack = 0.008f; break;
        case organ:    amp.attack = 0.004f; amp.sustain = 1.0f;  amp.decay = 0.05f; break;
        case ePiano:   amp.attack = 0.002f; amp.sustain = 0.35f; amp.decay = 1.2f;
                       bright = { 0.001f, 0.45f, 0.03f, 0.50f }; break; // the tine dies fast
        case bells:    amp.attack = 0.001f; amp.sustain = 0.0f;  amp.decay = 3.5f;
                       bright = { 0.001f, 0.90f, 0.0f, 0.60f }; break;  // uppers die first
        case pluck:    amp.attack = 0.001f; amp.sustain = 1.0f;  amp.decay = 0.1f; break;
        case piano:    amp.attack = 0.001f; amp.sustain = 1.0f;  amp.decay = 0.1f; break;
        default:       amp.attack = 0.012f; break;                      // warm pad
    }
    v.amp.setParameters (amp);
    v.bright.setParameters (bright);

    if (newFlavour == pluck)
    {
        // Karplus-Strong: a noise burst rings through a damped delay line
        // of the note's period. Velocity brightens the pluck (soft hits
        // excite with duller noise).
        v.ksLen = juce::jlimit (2, (int) v.ks.size() - 1, (int) (sr / v.freq));
        v.ksPos = 0;
        juce::Random rng ((juce::int64) note * 7919 + (juce::int64) (velocity * 127.0f));
        const float bright01 = 0.20f + 0.75f * velocity;
        float lp = 0.0f;
        for (int i = 0; i < v.ksLen; ++i)
        {
            lp += bright01 * (rng.nextFloat() * 2.0f - 1.0f - lp);
            v.ks[(size_t) i] = lp;
        }
        // Per-period loss tuned for a string-like ring: longer on low
        // notes, snappier up high.
        const double t60 = juce::jlimit (0.8, 3.0, 3.0 * 250.0 / v.freq + 0.6);
        v.ksDamp = (float) std::pow (0.001, 1.0 / (t60 * v.freq));
    }
    else if (newFlavour == piano)
    {
        const auto& zones = pianoZones();
        const PianoZone* best = nullptr;
        for (const auto& z : zones)
            if (best == nullptr || std::abs (z.rootNote - note) < std::abs (best->rootNote - note))
                best = &z;
        if (best != nullptr)
        {
            v.sample = &best->data;
            v.samplePos = 0.0;
            v.sampleStep = std::pow (2.0, (note - best->rootNote) / 12.0) * best->sampleRate / sr;
        }
    }

    v.amp.noteOn();
    v.bright.noteOn();
}

void SoundEngine::releaseVoice (Voice& v, bool)
{
    auto params = v.amp.getParameters();
    switch (v.flavour)
    {
        case organ:  params.release = 0.09f; break;  // the stop closes
        case piano:  params.release = 0.18f; break;  // the damper falls
        case pluck:  params.release = 0.25f; break;  // the string is muted
        case bells:  params.release = 0.35f; break;  // choked
        default:
        {
            // Release scales with how long the note was held: staccato
            // notes get a snappy tail, pads ring long. The clean tones
            // keep a higher floor - without it their tails vanish and
            // the polyphony is inaudible.
            const bool clean = v.flavour == pureSine || v.flavour == triangle || v.flavour == glass;
            const float releaseFloor = v.flavour == pureSine ? 0.65f : clean ? 0.50f : 0.12f;
            params.release = juce::jlimit (releaseFloor, 2.2f,
                                           1.1f * (float) v.heldSamples / (float) sr);
            break;
        }
    }
    v.amp.setParameters (params);
    v.amp.noteOff();
    v.bright.noteOff();
    v.note = -1;
}

float SoundEngine::renderVoiceSample (Voice& v)
{
    const double inc1 = juce::MathConstants<double>::twoPi * v.freq / sr;

    // Band-limited-ish saw with a gentle top, shared by two flavours.
    const int harmonics = juce::jlimit (3, 9, (int) (sr * 0.35 / juce::jmax (20.0, v.freq)));
    auto saw = [harmonics] (double p)
    {
        float acc = 0.0f;
        for (int k = 1; k <= harmonics; ++k)
            acc += (float) std::sin (k * p) / (float) k;
        return acc * 0.62f;
    };

    float s = 0.0f;
    double advance1 = inc1, advance2 = inc1 * 1.0055, advance3 = inc1 * 0.9965;

    switch (v.flavour)
    {
        case pureSine:
            s = 0.9f * (float) std::sin (v.phase);
            break;

        case softSaw:
            s = 0.75f * saw (v.phase);
            break;

        case strings:
            s = 0.42f * (saw (v.phase) + saw (v.phase2) + saw (v.phase3));
            break;

        case triangle:
        {
            // Odd partials at 1/k^2 - clear, a shade brighter than sine.
            float acc = (float) std::sin (v.phase);
            if (harmonics >= 3) acc -= (float) std::sin (3.0 * v.phase) / 9.0f;
            if (harmonics >= 5) acc += (float) std::sin (5.0 * v.phase) / 25.0f;
            if (harmonics >= 7) acc -= (float) std::sin (7.0 * v.phase) / 49.0f;
            s = 0.85f * acc;
            break;
        }

        case glass:
        {
            // Crystalline: pure octaves above a sine, zero detune.
            const float b = v.bright.getNextSample();
            float acc = 0.80f * (float) std::sin (v.phase);
            if (harmonics >= 4)  acc += 0.20f * b * (float) std::sin (4.0 * v.phase);
            if (harmonics >= 8)  acc += 0.05f * b * b * (float) std::sin (8.0 * v.phase);
            s = acc;
            break;
        }

        case ePiano:
        {
            // Tine piano: a sine body, a fast-dying inharmonic tine whose
            // level rides velocity, and a slow amplitude tremolo.
            const float tine = v.bright.getNextSample() * (0.15f + 0.55f * v.velocity * v.velocity);
            const float tremolo = 1.0f + 0.12f * (float) std::sin (v.trem);
            s = tremolo * (0.80f * (float) std::sin (v.phase)
                           + 0.12f * (float) std::sin (2.0 * v.phase)
                           + tine * (float) std::sin (3.58 * v.phase));
            v.trem += juce::MathConstants<double>::twoPi * 4.6 / sr;
            break;
        }

        case organ:
        {
            // Drawbars: 16' 8' 4' 2-2/3' 2', full sustain, a whisper of
            // vibrato so held chords stay alive.
            const double vib = 1.0 + 0.0035 * std::sin (v.trem);
            advance1 = inc1 * vib;
            s = 0.55f * (0.55f * (float) std::sin (v.phase)
                         + 0.30f * (float) std::sin (0.5 * v.phase)
                         + 0.28f * (float) std::sin (2.0 * v.phase)
                         + 0.18f * (float) std::sin (3.0 * v.phase)
                         + 0.14f * (float) std::sin (4.0 * v.phase));
            v.trem += juce::MathConstants<double>::twoPi * 6.0 / sr;
            break;
        }

        case bells:
        {
            // Inharmonic partials with the uppers dying first - a struck
            // bar rather than a cathedral bell.
            const float b = v.bright.getNextSample();
            float acc = (float) std::sin (v.phase);
            acc += b * (0.50f * (float) std::sin (2.74 * v.phase)
                        + 0.24f * (float) std::sin (5.43 * v.phase));
            acc += b * b * 0.10f * (float) std::sin (8.90 * v.phase);
            s = 0.80f * acc;
            break;
        }

        case pluck:
        {
            const int next = (v.ksPos + 1) % juce::jmax (1, v.ksLen);
            s = v.ks[(size_t) v.ksPos];
            // Muting a released string damps it far faster than letting it ring.
            const float damp = v.note >= 0 ? v.ksDamp : v.ksDamp * 0.9985f;
            v.ks[(size_t) v.ksPos] = damp * 0.5f * (v.ks[(size_t) v.ksPos] + v.ks[(size_t) next]);
            v.ksPos = next;
            s *= 0.95f;
            break;
        }

        case piano:
        {
            if (v.sample == nullptr || v.samplePos >= (double) (v.sample->getNumSamples() - 2))
            {
                v.amp.reset(); // the recording ran out - the voice is done
                return 0.0f;
            }
            const auto* data = v.sample->getReadPointer (0);
            const int i = (int) v.samplePos;
            const float frac = (float) (v.samplePos - i);
            s = 1.35f * (data[i] + frac * (data[i + 1] - data[i]));
            v.samplePos += v.sampleStep;
            break;
        }

        default: // Warm Pad: the dominant detuned pair, sub, fixed harmonics
        {
            const float shimmer = 0.14f * v.velocity * v.bright.getNextSample();
            s = (0.44f * ((float) std::sin (v.phase) + (float) std::sin (v.phase2))
               + 0.16f * (float) std::sin (v.phase3)
               + 0.28f * (float) std::sin (0.5 * v.phase)
               + (0.22f + shimmer) * (float) std::sin (2.0 * v.phase)
               + 0.08f * (float) std::sin (3.0 * v.phase)) / 1.55f;
            break;
        }
    }

    v.phase += advance1;
    v.phase2 += advance2;
    v.phase3 += advance3;
    if (v.phase > juce::MathConstants<double>::twoPi * 1024.0)
    {
        v.phase = std::fmod (v.phase, juce::MathConstants<double>::twoPi);
        v.phase2 = std::fmod (v.phase2, juce::MathConstants<double>::twoPi);
        v.phase3 = std::fmod (v.phase3, juce::MathConstants<double>::twoPi);
    }
    return s;
}

void SoundEngine::render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                          int flavour, float master)
{
    const int numSamples = buffer.getNumSamples();
    if (sr <= 0.0 || numSamples <= 0 || buffer.getNumChannels() < 1)
        return;

    if ((int) dryBus.size() < numSamples)
    {
        dryBus.resize ((size_t) numSamples);
        sendBus.resize ((size_t) numSamples);
    }
    std::fill (dryBus.begin(), dryBus.begin() + numSamples, 0.0f);
    std::fill (sendBus.begin(), sendBus.begin() + numSamples, 0.0f);

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        for (auto& v : voices)
        {
            if (! v.amp.isActive())
                continue;
            const float send = delaySendFor (v.flavour);
            for (int n = pos; n < end; ++n)
            {
                const float env = v.amp.getNextSample();
                const float c = 0.16f * master * v.gain * env * renderVoiceSample (v);
                dryBus[(size_t) n] += c;
                sendBus[(size_t) n] += c * send;
            }
            if (v.note >= 0)
                v.heldSamples += end - pos;
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
            Voice* voice = nullptr;
            for (auto& v : voices)
                if (! v.amp.isActive())
                    { voice = &v; break; }
            if (voice == nullptr)
            {
                voice = &voices[(size_t) nextVoice];
                nextVoice = (nextVoice + 1) % (int) voices.size();
            }
            startVoice (*voice, msg.getNoteNumber(), msg.getFloatVelocity(), flavour);
        }
        else if (msg.isNoteOff() || msg.isAllNotesOff())
        {
            for (auto& v : voices)
                if (v.note >= 0 && (msg.isAllNotesOff() || v.note == msg.getNoteNumber()))
                    releaseVoice (v, msg.isAllNotesOff());
        }
    }
    renderTo (numSamples);

    // Stereo delay: different L/R times widen the mono voices. Each
    // flavour decides how much of itself it sends.
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int n = 0; n < numSamples; ++n)
    {
        const float dry = dryBus[(size_t) n];
        const float send = sendBus[(size_t) n];
        auto tap = [&] (std::vector<float>& line, int& p) -> float
        {
            const float delayed = line[(size_t) p];
            line[(size_t) p] = send + delayed * 0.35f;
            p = (p + 1) % (int) line.size();
            return delayed;
        };
        left[n] += dry + 0.30f * tap (delayLineL, delayPosL);
        if (right != nullptr)
            right[n] += dry + 0.30f * tap (delayLineR, delayPosR);
    }

    if (right != nullptr)
        reverb.processStereo (left, right, numSamples);
    else
        reverb.processMono (left, numSamples);

    // Soft safety limiter: transparent at normal levels, rounds off the
    // peaks when many ringing voices + delay + reverb stack up.
    float blockPeak = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        left[n] = std::tanh (left[n]);
        blockPeak = juce::jmax (blockPeak, std::abs (left[n]));
        if (right != nullptr)
        {
            right[n] = std::tanh (right[n]);
            blockPeak = juce::jmax (blockPeak, std::abs (right[n]));
        }
    }
    peak = blockPeak;
}

} // namespace alea
