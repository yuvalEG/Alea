#pragma once

#include "PluginProcessor.h"

namespace ui
{

// Spec color scheme (section 9.4)
namespace colors
{
    const juce::Colour background { 0xff0a0a0f };
    const juce::Colour panel      { 0xff12121a };
    const juce::Colour control    { 0xff22222f };
    const juce::Colour border     { 0xff2a2a3a };
    const juce::Colour text       { 0xffe8e8f0 };
    const juce::Colour secondary  { 0xff8888a0 };
    const juce::Colour purple     { 0xff7c3aed };
    const juce::Colour cyan       { 0xff06b6d4 };
    const juce::Colour amber      { 0xfff59e0b };
    const juce::Colour green      { 0xff10b981 };
    const juce::Colour red        { 0xffef4444 };
    const juce::Colour playing    { 0xff22c55e };
}

juce::String noteName (int midiNote); // C3 = 60 display convention

// A row of mutually exclusive segments bound to a choice parameter.
class SegmentedSelector : public juce::Component
{
public:
    SegmentedSelector (juce::RangedAudioParameter&, const juce::StringArray& options, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::StringArray options;
    juce::Colour accent;
    int value = 0;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedSelector)
};

// One-octave keyboard; clicking a key toggles that pitch class (spec 9.2).
// The currently playing note lights green on its originating keyboard only.
class PianoKeyboard : public juce::Component
{
public:
    PianoKeyboard (AleaAudioProcessor&, char scaleId, int sourceIndex, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> whiteKeyBounds (int slot) const;
    juce::Rectangle<float> blackKeyBounds (int pc) const;

    AleaAudioProcessor& alea;
    int sourceIndex;
    juce::Colour accent;
    bool selected[12] {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, 12> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboard)
};

// Five rest toggle buttons styled like a mini keyboard (spec 9.2).
class RestSelector : public juce::Component
{
public:
    RestSelector (AleaAudioProcessor&, char scaleId, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Colour accent;
    bool selected[5] {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, 5> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RestSelector)
};

// The morph position bar: click to jump, drag to scrub; animates and is
// read-only while auto-sweep runs (manual scrub-during-sweep is a later
// milestone).
class MorphBar : public juce::Component
{
public:
    explicit MorphBar (AleaAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool sweepActive() const;
    void applyDrag (const juce::MouseEvent&);

    AleaAudioProcessor& alea;
    float value = 0.0f; // 0..100, mirrors the parameter
    bool dragging = false;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphBar)
};

// Output panel: activity LED, playing note, bar/beat, morph source,
// note history (last 50), Panic (spec 9.1).
class OutputPanel : public juce::Component
{
public:
    explicit OutputPanel (AleaAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AleaAudioProcessor& alea;
    juce::TextButton panicButton { "PANIC" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputPanel)
};

} // namespace ui
