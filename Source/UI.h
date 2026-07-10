#pragma once

#include "PluginProcessor.h"
#include "Hardware.h"

// Scale-Shifter-specific components. Everything family-shared (palette, the
// hw:: faceplate drawing, LookAndFeel, transport, buttons, segments) lives in
// Hardware.h so the Chord Randomizer builds on the very same primitives.
namespace ui
{

juce::String noteName (int midiNote); // C3 = 60 display convention

// Four segments, each drawing its morph curve shape (no math notation);
// hovering names the curve.
class CurveSelector : public juce::Component,
                      public juce::TooltipClient
{
public:
    CurveSelector (juce::RangedAudioParameter&, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;
    void setAccent (juce::Colour c) { accent = c; repaint(); }

private:
    juce::Colour accent;
    int value = 0;
    std::array<float, 4> litAmt { { 0.0f, 0.0f, 0.0f, 0.0f } };
    FrameAnimator anim;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveSelector)
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
    // The octave is drawn starting at the scale's root (root D shows
    // D E F#... with C# last); keys keep their true black/white identity.
    juce::Rectangle<float> whiteKeyBounds (int slot) const;
    juce::Rectangle<float> blackKeyBounds (int whitesBefore) const;
    static constexpr float kKeybedPad = 5.0f; // dark margin the lit-key bloom spreads into

    AleaAudioProcessor& alea;
    int sourceIndex;
    juce::Colour accent;
    bool selected[12] {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, 12> attachments;
    std::atomic<float>* rootRaw = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboard)
};

// Rest toggle buttons styled like a mini keyboard (spec 9.2), one per rest
// duration. The rest currently "sounding" lights green on its originating
// scale's selector.
class RestSelector : public juce::Component
{
public:
    RestSelector (AleaAudioProcessor&, char scaleId, int sourceIndex, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    AleaAudioProcessor& alea;
    int sourceIndex;
    juce::Colour accent;
    bool selected[params::numRests] {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, params::numRests> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RestSelector)
};

// The morph position bar: click to jump, drag to scrub - also while
// auto-sweep runs (the sweep re-anchors at the dragged position).
// Right-click to bind a MIDI CC to Morph Position.
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
    float pctFromX (const juce::MouseEvent&) const;
    void showCcMenu();

    AleaAudioProcessor& alea;
    float value = 0.0f; // 0..100, mirrors the parameter
    bool dragging = false;
    bool scrubbing = false; // dragging while auto-sweep runs
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MorphBar)
};

// Output panel: monitoring - activity LED, playing note, bar/beat, an
// 88-key strip lighting the sounding note, and event history including
// rests (spec 9.1). In the standalone app it also hosts the output
// chooser: Internal Synth or any MIDI device.
class OutputPanel : public juce::Component
{
public:
    explicit OutputPanel (AleaAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AleaAudioProcessor& alea;
    std::unique_ptr<juce::ComboBox> outputBox;
    juce::Array<juce::MidiDeviceInfo> devices;
    juce::Slider volSlider;                    // internal synth volume (dB)
    juce::Slider transposeSlider;              // global output transpose
    juce::Label transposeField;                // double-click-editable "N st" readout
    std::unique_ptr<juce::ParameterAttachment> transposeFieldAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAttachment, transposeAttachment;
    bool lastSynthOn = false;
    float meterLevel = 0.0f;                   // falling peak for the output meter
    juce::Rectangle<int> meterRect;            // OUT meter, beside the level knob

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputPanel)
};

} // namespace ui
