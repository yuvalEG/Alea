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
        const int triad      = m ({ 0, 4, 7 });
        const int chromatic  = 0xfff;

        return std::vector<Factory> {
            //                       name                              aNotes    bNotes     aRest bRest  aOct   bOct   aVel     bVel     int len dur mode curve
            { "Major \xe2\x86\x92 Minor",                              major,    minor,     0,    0,     3, 5,  3, 5,  80, 110, 80, 110, 4,  3,  3,  2,   0 },
            { "Pentatonic Drift",                                      majPent,  minPent,   0,    0,     3, 5,  4, 6,  70, 100, 70, 100, 3,  2,  4,  1,   2 },
            { "Sparse \xe2\x86\x92 Dense",                             m ({0,7}), chromatic, 0x07, 0,    2, 4,  3, 6,  60, 90,  90, 120, 3,  3,  4,  0,   1 },
            { "Octave Climb",                                          triad,    triad,     0,    0,     1, 2,  6, 7,  80, 110, 80, 110, 3,  2,  5,  2,   3 },
            { "Diatonic \xe2\x86\x92 Dodecaphony",                     major,    chromatic, 0,    0,     3, 5,  2, 6,  75, 105, 60, 120, 4,  3,  5,  0,   0 },
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

    set ("intervalSync", (float) f.intervalSyncIdx);
    set ("lengthSync",   (float) f.lengthSyncIdx);
    set ("morphDurBars", (float) f.durBarsIdx);
    set ("morphMode",    (float) f.morphMode);
    set ("morphCurve",   (float) f.morphCurve);
    set ("morphPos",     0.0f);
    set ("autoSweep",    1.0f);
}

} // namespace presets
