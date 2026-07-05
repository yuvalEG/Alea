#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Factory presets (spec 13.1) plus static (non-sweeping) patches, ending with
// the "Diatonic -> Dodecaphony" flagship.
namespace presets
{
    struct Factory
    {
        const char* category;             // section heading in the preset menu
        const char* name;                 // UTF-8
        int aNotes, bNotes;               // pitch-class bitmasks (bit 0 = C)
        int aRests, bRests;               // rest bitmasks (bit 0 = whole ... bit 4 = 1/16)
        int aOctMin, aOctMax, bOctMin, bOctMax;
        int aVelMin, aVelMax, bVelMin, bVelMax;
        int intervalMode, intervalSyncIdx;    // mode: 0 sync / 1 free / 2 random
        int lengthMode, lengthSyncIdx;
        int autoSweep;
        int durBarsIdx;                       // index into params::morphDurBarNames
        int morphMode, morphCurve;
    };

    const std::vector<Factory>& factory();

    // Resets every parameter to default, then applies the preset.
    void apply (juce::AudioProcessorValueTreeState&, const Factory&);
}
