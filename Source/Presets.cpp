#include "Presets.h"
#include "Params.h"

namespace presets
{

static int m (std::initializer_list<int> bits)
{
    int v = 0;
    for (int b : bits)
        v |= 1 << b;
    return v;
}

const std::vector<Factory>& factory()
{
    static const std::vector<Factory> list = []
    {
        const int major      = m ({ 0, 2, 4, 5, 7, 9, 11 });
        const int minor      = m ({ 0, 2, 3, 5, 7, 8, 10 });
        const int majPent    = m ({ 0, 2, 4, 7, 9 });
        const int minPent    = m ({ 0, 3, 5, 7, 10 });
        const int maj9       = m ({ 0, 2, 4, 7, 11 });
        const int wholeTone  = m ({ 0, 2, 4, 6, 8, 10 });
        const int triad      = m ({ 0, 4, 7 });
        const int chromatic  = 0xfff;

        // Fields: category name aNotes bNotes aRests bRests aOct bOct aVel bVel
        //         intMode intSync lenMode lenSync sweep durMode durBars durFree durUnit mode curve
        return std::vector<Factory> {
            { "Static", "Just an Arp",
              maj9, minPent, m ({ 3, 4 }), 0,  4, 6, 4, 6,  70, 115, 70, 115,
              0, 2,  0, 2,  0, 0, 3, 30.0f, 0, 0, 0 },
            { "Static", "Dice Roll",
              wholeTone, chromatic, m ({ 2 }), 0,  3, 5, 3, 5,  50, 120, 50, 120,
              2, 3,  2, 3,  0, 0, 3, 30.0f, 0, 0, 0 },

            { "Sweeps", "Major \xe2\x86\x92 Minor",
              major, minor, 0, 0,  3, 5, 3, 5,  80, 110, 80, 110,
              0, 4,  0, 3,  1, 0, 3, 30.0f, 0, 2, 0 },
            { "Sweeps", "Pentatonic Drift",
              majPent, minPent, 0, 0,  3, 5, 4, 6,  70, 100, 70, 100,
              0, 3,  0, 2,  1, 0, 4, 30.0f, 0, 1, 2 },
            { "Sweeps", "Sparse \xe2\x86\x92 Dense",
              m ({ 0, 7 }), chromatic, m ({ 0, 1, 2 }), 0,  2, 4, 3, 6,  60, 90, 90, 120,
              0, 3,  0, 3,  1, 0, 4, 30.0f, 0, 0, 1 },
            // Single-dimension sweeps: only velocity / only octave moves.
            { "Sweeps", "Soft \xe2\x86\x92 Loud",
              major, major, 0, 0,  3, 5, 3, 5,  25, 45, 105, 127,
              0, 3,  0, 3,  1, 0, 3, 30.0f, 0, 2, 0 },
            { "Sweeps", "Octave Climb",
              triad, triad, 0, 0,  1, 2, 6, 7,  80, 110, 80, 110,
              0, 3,  0, 2,  1, 0, 5, 30.0f, 0, 2, 3 },
            { "Sweeps", "Diatonic \xe2\x86\x92 Dodecaphony",
              major, chromatic, 0, 0,  3, 3, 3, 3,  75, 105, 60, 120,
              0, 5,  0, 6,  1, 1, 5, 3.0f, 1, 0, 0 }, // 3-minute journey, whole-bar notes
        };
    }();
    return list;
}

void apply (juce::AudioProcessorValueTreeState& apvts, const Factory& f)
{
    auto set = [&apvts] (const juce::String& id, float denormalized)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (denormalized));
    };

    // Start from a clean slate so presets are fully deterministic (spec 13.2
    // stores the whole patch).
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->getDefaultValue());

    auto setScale = [&set] (char scale, int notes, int rests, int octMin, int octMax, int velMin, int velMax)
    {
        for (int pc = 0; pc < 12; ++pc)
            set (params::noteId (scale, pc), (notes >> pc) & 1 ? 1.0f : 0.0f);
        for (int r = 0; r < 5; ++r)
            set (params::restId (scale, r), (rests >> r) & 1 ? 1.0f : 0.0f);

        const auto s = juce::String::charToString (scale);
        set (s + "OctMin", (float) octMin);
        set (s + "OctMax", (float) octMax);
        set (s + "VelMin", (float) velMin);
        set (s + "VelMax", (float) velMax);
    };

    setScale ('a', f.aNotes, f.aRests, f.aOctMin, f.aOctMax, f.aVelMin, f.aVelMax);
    setScale ('b', f.bNotes, f.bRests, f.bOctMin, f.bOctMax, f.bVelMin, f.bVelMax);

    set ("intervalMode", (float) f.intervalMode);
    set ("intervalSync", (float) f.intervalSyncIdx);
    set ("lengthMode",   (float) f.lengthMode);
    set ("lengthSync",   (float) f.lengthSyncIdx);
    set ("morphDurMode", (float) f.durMode);
    set ("morphDurBars", (float) f.durBarsIdx);
    set ("morphDurFree", f.durFree);
    set ("morphDurUnit", (float) f.durUnit);
    set ("morphMode",    (float) f.morphMode);
    set ("morphCurve",   (float) f.morphCurve);
    set ("morphPos",     0.0f);
    set ("autoSweep",    (float) f.autoSweep);
}

} // namespace presets
