#include "Params.h"

namespace params
{

static void addScale (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                      char scale, const juce::String& prefix,
                      const std::array<bool, 12>& defaultNotes, int defaultRest)
{
    for (int pc = 0; pc < 12; ++pc)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { noteId (scale, pc), 1 },
            prefix + " " + pitchClassNames[pc], defaultNotes[(size_t) pc]));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { juce::String::charToString (scale) + "OctMin", 1 }, prefix + " Octave Min", 0, 8, 3));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { juce::String::charToString (scale) + "OctMax", 1 }, prefix + " Octave Max", 0, 8, 5));

    for (int r = 0; r < 5; ++r)
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { restId (scale, r), 1 },
            prefix + " Rest " + restNames[r], r == defaultRest));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { juce::String::charToString (scale) + "VelMin", 1 }, prefix + " Velocity Min", 0, 127, 80));
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { juce::String::charToString (scale) + "VelMax", 1 }, prefix + " Velocity Max", 0, 127, 110));
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Defaults mirror the spec's preset example: A = C major with a 1/4 rest,
    // B = C minor(ish) with no rests.
    addScale (layout, 'a', "A:", { true, false, true, false, true, true, false, true, false, true, false, true }, 2);
    addScale (layout, 'b', "B:", { true, false, true, true, false, true, false, true, true, false, true, false }, -1);

    auto choice = [] (const char* id, const char* name, const juce::StringArray& opts, int def)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, opts, def);
    };

    layout.add (choice ("intervalMode", "Interval Mode", timingModes, sync));
    layout.add (choice ("intervalSync", "Note Interval", divisionNames, 4)); // 1/4 bar = 1 beat
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "intervalFree", 1 }, "Interval (s)",
        juce::NormalisableRange<float> (0.05f, 10.0f, 0.0f, 0.4f), 0.5f));

    layout.add (choice ("lengthMode", "Length Mode", timingModes, sync));
    layout.add (choice ("lengthSync", "Note Length", divisionNames, 3)); // 1/8 bar
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lengthFree", 1 }, "Length (s)",
        juce::NormalisableRange<float> (0.05f, 10.0f, 0.0f, 0.4f), 0.25f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "morphPos", 1 }, "Morph Position",
        juce::NormalisableRange<float> (0.0f, 100.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "autoSweep", 1 }, "Auto-Sweep", false));
    layout.add (choice ("morphDurMode", "Morph Duration Mode", morphDurModes, 0));
    layout.add (choice ("morphDurBars", "Morph Duration (bars)", morphDurBarNames, 3)); // 8 bars
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "morphDurFree", 1 }, "Morph Duration (time)",
        juce::NormalisableRange<float> (0.0f, 600.0f), 30.0f));
    layout.add (choice ("morphDurUnit", "Morph Duration Unit", morphDurUnits, 0));
    layout.add (choice ("morphMode", "Morph Mode", morphModes, oneShot));
    layout.add (choice ("morphCurve", "Morph Curve", morphCurves, linear));

    layout.add (choice ("tempoSource", "Tempo Source", tempoSources, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "internalTempo", 1 }, "Internal Tempo",
        juce::NormalisableRange<float> (20.0f, 300.0f, 0.1f), 120.0f));

    return layout;
}

} // namespace params
