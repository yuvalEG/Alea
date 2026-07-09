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
    const juce::Colour ice        { 0xff9bdcf0 }; // FREEZE active - icy, distinct from scale-B cyan
}

// Hardware faceplate drawing (the skeuomorphic reskin, July 8 2026). Shared
// free functions so both the editor's paintMain and the individual component
// paints render the same brushed-gunmetal language. The CSS handoff's layered
// gradients/shadows are translated to juce::Graphics fills here.
namespace hw
{
    const juce::Colour metalLine { 0xff0b0c0e }; // the near-black seam between parts
    const juce::Colour led        { 0xff10b981 }; // the single "selected/lit" emerald

    // Brushed gunmetal. isPlate = a recessed module plate (flatter, bordered);
    // otherwise the raised faceplate slab (broad sheen sweep + top highlight).
    void brushedMetal (juce::Graphics&, juce::Rectangle<float> r, float radius, bool isPlate);

    // A single pan-head screw; slotDeg rotates the slot per instance.
    void screw (juce::Graphics&, juce::Point<float> centre, float slotDeg, float size = 10.0f);

    // A recessed inset well (segmented tracks, fader/slider grooves, keybeds).
    void insetWell (juce::Graphics&, juce::Rectangle<float> r, float radius);

    // Glass LCD screen base: dark phosphor glass + inner wash + gloss + bezel.
    // Draw the glowing content on top, THEN call lcdScanlines so the horizontal
    // CRT lines sit above the readout (the readout reads as behind the glass).
    void lcd (juce::Graphics&, juce::Rectangle<float> r, juce::Colour phosphor);
    void lcdScanlines (juce::Graphics&, juce::Rectangle<float> r);

    // Segmented vertical output meter (12 cells bottom-up; amber near top, red at clip).
    void meter (juce::Graphics&, juce::Rectangle<float> r, float level01);

    // A backlit push-button face. lit = the coloured/LED state (black legend);
    // returns the colour the caller should draw the legend in.
    juce::Colour button (juce::Graphics&, juce::Rectangle<float> r, bool lit,
                         juce::Colour ledColour, bool over, bool down);

    // The physical knob (value-arc ring + ticks + metal cap + pointer),
    // translated 1:1 from the CSS .hw-knob layers. pos is 0..1; bipolar grows
    // the lit arc from 12 o'clock. Shared by the LookAndFeel and the gallery.
    void knob (juce::Graphics&, juce::Rectangle<float> bounds, float pos,
               juce::Colour accent, bool bipolar);

    // Real soft glow behind a filled shape (juce::DropShadow of the shape's
    // alpha, in the accent colour). The one true glow - use it everywhere a
    // CSS box-shadow/bloom was faked. blur is the Gaussian radius in px.
    void dropGlow (juce::Graphics&, const juce::Path& filledShape, juce::Colour colour, int blur);

    // Soft outward glow around a rounded rect (no box-shadow in JUCE).
    void glowRoundedRect (juce::Graphics&, juce::Rectangle<float> r, float radius,
                          juce::Colour colour, float strength = 1.0f);
    // Phosphor-glow text (drawn several times at low alpha, then crisp).
    void glowText (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification, juce::Colour colour);
}

// Standalone transport: drawn play triangle / pause bars plus a label -
// prominent, green, and the icon tells the truth (pausing holds the clock).
// Shared design language with Alea Chord Randomizer's transport.
class TransportButton : public juce::Button
{
public:
    TransportButton() : juce::Button ("transport") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

juce::String noteName (int midiNote); // C3 = 60 display convention

// A row of mutually exclusive segments bound to a choice parameter. The
// hovered segment's tooltip (when provided) names/explains that option.
class SegmentedSelector : public juce::Component,
                          public juce::TooltipClient
{
public:
    SegmentedSelector (juce::RangedAudioParameter&, const juce::StringArray& options, juce::Colour accent,
                       const juce::StringArray& tooltips = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

private:
    juce::StringArray options, tooltips;
    juce::Colour accent;
    int value = 0;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedSelector)
};

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

private:
    juce::Colour accent;
    int value = 0;
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAttachment, transposeAttachment;
    bool lastSynthOn = false;
    float meterLevel = 0.0f;                   // falling peak for the output meter
    juce::Rectangle<int> meterRect;            // OUT meter, beside the level knob

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputPanel)
};

} // namespace ui
