#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// The Alea family's shared hardware faceplate language: palette, brushed-metal
// drawing, backlit controls, and the LookAndFeel both plugins install. This
// layer is processor-agnostic - it may not include any product header, so both
// Scale Shifter and Chord Randomizer (and the snapshot tools) compile it as-is.
namespace ui
{

// Spec color scheme (design tokens; identical across the family)
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
    const juce::Colour ice        { 0xff9bdcf0 }; // FREEZE active - icy, distinct from cyan's meaning
}

// Hardware faceplate drawing (the skeuomorphic reskin, July 8 2026). Shared
// free functions so every editor's paint and the individual component paints
// render the same brushed-gunmetal language. The CSS handoff's layered
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

    // Engraved text: a dark press below, the bright face on top. The engraved
    // look every plate title and caption on the metal wears.
    void engraved (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification, juce::Colour bright);

    // A module plate with its two corner screws and an engraved ALL-CAPS
    // title. tab (optional) draws the small colour light beside the title.
    void plate (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title,
                juce::Colour tab = juce::Colours::transparentBlack, float tabAlpha = 1.0f);

    // An engraved caption row label inside a plate (the design's .hw-sublabel).
    void sublabel (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification just = juce::Justification::centredLeft);

    // A small physical LED: dark metal dome when off, lit + bloom when on.
    void ledDot (juce::Graphics&, juce::Point<float> centre, float on01, juce::Colour colour,
                 float diameter = 10.0f);

    // A physical toggle switch (recessed track + metal knob); amt01 slides the
    // knob and crossfades the accent-lit track.
    void toggleSwitch (juce::Graphics&, juce::Rectangle<float> r, float amt01, juce::Colour accent);

    // The ambient glow the glass catches from its lit content (design: an
    // inset phosphor bloom hugging the screen edges). lcd() calls it; screens
    // that draw their own glass call it directly.
    void lcdAmbience (juce::Graphics&, juce::Rectangle<float> r, juce::Colour, float strength);

    // Glass LCD screen base: dark phosphor glass + inner wash + gloss + bezel.
    // Draw the glowing content on top, THEN call lcdScanlines so the horizontal
    // CRT lines sit above the readout (the readout reads as behind the glass).
    void lcd (juce::Graphics&, juce::Rectangle<float> r, juce::Colour phosphor);
    void lcdScanlines (juce::Graphics&, juce::Rectangle<float> r);

    // Segmented vertical output meter (12 cells bottom-up; amber near top, red at clip).
    void meter (juce::Graphics&, juce::Rectangle<float> r, float level01);

    // The monitor keybed (design Monitor88): a recessed dark bed holding pale
    // silver white keys (1px gaps, worn bottom lip) and stubby floating black
    // keys with a soft drop shadow. lit(note) returns 0..1: 0 = unlit, 1 = the
    // whole key face lights in the accent (with a 12px bloom spilling over its
    // neighbours), in between = a velocity slice rising from the key's bottom.
    // Identical in both products - only range, accent and lit() differ.
    void keybed (juce::Graphics&, juce::Rectangle<float> bed, int lowNote, int highNote,
                 const std::function<float (int)>& lit, juce::Colour accent);

    // A backlit push-button face. lit = the coloured/LED state (black legend);
    // returns the colour the caller should draw the legend in.
    juce::Colour button (juce::Graphics&, juce::Rectangle<float> r, float lit,
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
    // Phosphor-glow text: a real Gaussian halo of the glyphs, then crisp text.
    // Set the font on the Graphics BEFORE calling (the halo uses it).
    void glowText (juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification, juce::Colour colour, int blur = 7);

    // The LED bloom behind a lit key, drawn by the key's PARENT (a component's
    // own paint is clipped to its bounds, so an outer glow drawn inside reads
    // as a hard square - the one recurring glow bug). Call from the editor's
    // paint for every lit button; `key` supplies the bounds in parent coords.
    void keyBloom (juce::Graphics&, const juce::Component& key, juce::Colour colour,
                   float amount, float radius = 4.0f);
    // A button's current 0..1 backlight (AnimatedButton's crossfade, or the
    // plain toggle state) - what keyBloom wants as `amount`.
    float litAmount (const juce::Button&);
}

// Draw the ALEA wordmark, width-fit and vertically centred in `box`, with a
// clean OUTER drop shadow: the glyph footprint is punched out of the shadow so
// no shadow falls on the letters themselves (they read fully in front of it).
// The composited image is cached, so this is a single blit per paint.
void drawWordmark (juce::Graphics&, const juce::Image& logo, juce::Rectangle<int> box);

// The family LookAndFeel: Space Grotesk everywhere, hardware knobs, backlit
// keys, recessed combos, hw toggle switches, the glass BPM LCD (any linear
// slider with component ID "bpm"). Both editors install the same instance.
// Buttons with the "bloom" property set glow in their LED colour while lit.
juce::LookAndFeel& hardwareLookAndFeel();

// Runs a per-frame callback at 60Hz until it returns false, then stops itself.
// Cheap idle behaviour: the timer only runs while something is actually moving.
class FrameAnimator : private juce::Timer
{
public:
    std::function<bool()> onFrame;         // return true while still animating
    void go() { if (! isTimerRunning()) startTimerHz (60); }
private:
    void timerCallback() override { if (! onFrame || ! onFrame()) stopTimer(); }
};

// A backlit push-button whose lit state crossfades (super-fast) instead of
// snapping. It exposes the 0..1 lit amount via the "litAmt" component property
// so the shared LookAndFeel can draw the fade; non-animated buttons fall back
// to the toggle state.
class AnimatedButton : public juce::TextButton, private juce::Timer
{
public:
    using juce::TextButton::TextButton;

    // Fires on every backlight step. The editor hooks this to repaint the
    // area around the key, so the parent-drawn hw::keyBloom fades with it.
    std::function<void()> onLitChange;

protected:
    void buttonStateChanged() override
    {
        juce::TextButton::buttonStateChanged();
        const float target = getToggleState() ? 1.0f : 0.0f;
        if (! settled)                       // first sync: snap, don't animate on open
        {
            settled = true; litAmt = target;
            getProperties().set ("litAmt", litAmt);
            if (onLitChange) onLitChange();
        }
        else if (std::abs (target - litAmt) > 0.001f && ! isTimerRunning())
            startTimerHz (60);
    }
private:
    void timerCallback() override
    {
        const float target = getToggleState() ? 1.0f : 0.0f;
        litAmt += (target - litAmt) * 0.4f;  // ~80ms crossfade
        if (std::abs (target - litAmt) < 0.01f) { litAmt = target; stopTimer(); }
        getProperties().set ("litAmt", litAmt);
        repaint();
        if (onLitChange) onLitChange();
    }
    float litAmt = 0.0f;
    bool settled = false;
};

// A physical toggle switch whose knob/track crossfade instead of snapping -
// the ToggleButton twin of AnimatedButton (the LookAndFeel reads "litAmt").
class AnimatedToggle : public juce::ToggleButton, private juce::Timer
{
public:
    using juce::ToggleButton::ToggleButton;
protected:
    void buttonStateChanged() override
    {
        juce::ToggleButton::buttonStateChanged();
        const float target = getToggleState() ? 1.0f : 0.0f;
        if (! settled)                       // first sync: snap, don't animate on open
        {
            settled = true; litAmt = target;
            getProperties().set ("litAmt", litAmt);
        }
        else if (std::abs (target - litAmt) > 0.001f && ! isTimerRunning())
            startTimerHz (60);
    }
private:
    void timerCallback() override
    {
        const float target = getToggleState() ? 1.0f : 0.0f;
        litAmt += (target - litAmt) * 0.35f; // ~100ms slide
        if (std::abs (target - litAmt) < 0.01f) { litAmt = target; stopTimer(); }
        getProperties().set ("litAmt", litAmt);
        repaint();
    }
    float litAmt = 0.0f;
    bool settled = false;
};

// Standalone transport: a backlit green key with a drawn play triangle / stop
// square (icon paths, never a font glyph) - the icon tells the truth.
// Identical in both Alea products.
class TransportButton : public juce::Button
{
public:
    TransportButton() : juce::Button ("transport") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// A row of mutually exclusive segments. Two bindings share one look:
//  - parameter mode: bound to a choice parameter (the Scale Shifter panels);
//  - callback mode: onChange reports the clicked index (or the new bitmask in
//    multi mode), and the owner syncs state back via setSelected / setMask.
// The hovered segment's tooltip (when provided) names/explains that option.
class SegmentedSelector : public juce::Component,
                          public juce::TooltipClient
{
public:
    SegmentedSelector (juce::RangedAudioParameter&, const juce::StringArray& options, juce::Colour accent,
                       const juce::StringArray& tooltips = {});
    SegmentedSelector (const juce::StringArray& options, juce::Colour accent); // callback mode
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    // Callback mode only.
    std::function<void (int)> onChange;     // index - or the new mask in multi mode
    void setSelected (int index);           // sync from the owner (no callback)
    void setMask (int newMask);             // multi mode sync (no callback)
    // Multi mode: segments toggle independently as a bitmask. allowEmpty lets
    // the last lit segment go dark; otherwise one always stays on.
    void setMulti (bool allowEmpty);

    // Stack the options top-to-bottom instead of left-to-right.
    void setVertical (bool v) { vertical = v; repaint(); }
    // Swap the lit colour (e.g. active amber <-> inactive grey). The control
    // stays fully interactive; only its backlight changes.
    void setAccent (juce::Colour c) { accent = c; repaint(); }

private:
    void startLitAnim();
    void syncLit();                    // snap on the first owner sync, animate after
    bool advanceLit();                 // one animation frame; false when settled
    float targetFor (int i) const;

    juce::StringArray options, tooltips;
    juce::Colour accent;
    int value = 0;
    int mask = 1;
    bool multi = false, allowEmpty = false;
    bool settled = false;              // callback mode: has the owner synced once?
    bool vertical = false;
    std::vector<float> litAmt;         // per-segment 0..1 backlight, crossfaded
    FrameAnimator anim;
    std::unique_ptr<juce::ParameterAttachment> attachment; // parameter mode only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedSelector)
};

// The family About dialog shell: wordmark over a subtle gradient, a divider in
// the scale colours, the product's text below. Content comes from the caller.
void showAboutDialog (const juce::String& title, const juce::String& body,
                      float fontSize, int width, int height);

} // namespace ui
