#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// All parameter IDs, choice lists, and the parameter layout (spec section 4).
namespace params
{
    inline const juce::StringArray pitchClassNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    inline const juce::StringArray restNames { "2", "1", "1/2", "1/4", "1/8", "1/16" };
    constexpr std::array<double, 6> restBars { 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };
    constexpr int numRests = 6;

    // Note interval / length divisions, in bars (4/4 fixed in v1, so 1 bar =
    // 4 beats). Long to short, left to right, like the rest selectors.
    inline const juce::StringArray divisionNames { "4", "2", "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128" };
    constexpr std::array<double, 10> divisionBars { 4.0, 2.0, 1.0, 1.0/2, 1.0/4, 1.0/8, 1.0/16, 1.0/32, 1.0/64, 1.0/128 };

    // Fixed pool for Random interval/length mode (spec 4.2).
    inline const juce::StringArray randomPoolNames { "1/16", "1/8", "1/4", "1/2" };
    constexpr std::array<double, 4> randomPoolBars { 1.0/16, 1.0/8, 1.0/4, 1.0/2 };

    inline const juce::StringArray timingModes   { "Sync", "Free", "Random" };
    inline const juce::StringArray morphDurModes { "Sync", "Free" };
    inline const juce::StringArray morphDurBarNames { "1", "2", "4", "8", "16", "32", "64" };
    constexpr std::array<double, 7> morphDurBarValues { 1, 2, 4, 8, 16, 32, 64 };

    // Free-mode morph duration readout: seconds up to a minute, then minutes
    // (the knob's own auto-formatting unit).
    inline juce::String morphTimeString (float seconds, int = 0)
    {
        if (seconds < 60.0f)
            return juce::String ((int) std::lround (seconds)) + " s";
        const float m = seconds / 60.0f;
        return juce::String (m, m < 10.0f ? 1 : 0) + " min";
    }
    inline const juce::StringArray morphModes  { "One-Shot", "Loop", "Bounce" };
    inline const juce::StringArray morphCurves { "Linear", "Exponential", "S-Curve", "Logarithmic" };
    inline const juce::StringArray tempoSources { "Host", "Free-Run" };

    enum TimingMode { sync = 0, free = 1, random = 2 };
    enum MorphMode  { oneShot = 0, loop = 1, bounce = 2 };
    enum MorphCurve { linear = 0, exponential = 1, sCurve = 2, logarithmic = 3 };

    inline juce::String noteId (char scale, int pitchClass) { return juce::String::charToString (scale) + "Note" + juce::String (pitchClass); }
    inline juce::String restId (char scale, int rest)       { return juce::String::charToString (scale) + "Rest" + juce::String (rest); }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
