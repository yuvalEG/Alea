#include "UI.h"

namespace ui
{

juce::String noteName (int midiNote)
{
    if (midiNote < 0)
        return "-";
    return params::pitchClassNames[midiNote % 12] + juce::String (midiNote / 12 - 2);
}

//==============================================================================
// Hardware faceplate drawing (the skeuomorphic reskin).

namespace hw
{
    // Baked finish (design handoff): brush 0 = clean satin, no grain;
    // sheen 1.9 = strong specular; glow 1.4 = brighter LED bloom.
    constexpr float kSheen = 1.9f, kGlow = 1.4f;

    // A real Gaussian glow behind a filled shape (JUCE_DRAWING_GUIDE
    // technique 2): rasterise the shape into an image, then juce::DropShadow
    // it in the accent colour. Not clipped to any control's bounds because
    // the caller draws it; soft, not a stroked outline.
    void dropGlow (juce::Graphics& g, const juce::Path& filledShape, juce::Colour colour, int blur)
    {
        const auto bounds = filledShape.getBounds().getSmallestIntegerContainer().expanded (blur + 2);
        if (bounds.isEmpty() || bounds.getWidth() > 4096 || bounds.getHeight() > 4096)
            return;
        juce::Image img (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
        {
            juce::Graphics ig (img);
            ig.setColour (juce::Colours::white);
            auto s = filledShape;
            s.applyTransform (juce::AffineTransform::translation ((float) -bounds.getX(), (float) -bounds.getY()));
            ig.fillPath (s);
        }
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::translation ((float) bounds.getX(), (float) bounds.getY()));
        juce::DropShadow (colour, blur, {}).drawForImage (g, img);
    }

    // Soft glow the JUCE way (no box-shadow / blur): stroke the shape a few
    // times growing outward at falling alpha (JUCE_DRAWING_GUIDE technique 1).
    void glowRoundedRect (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                          juce::Colour colour, float strength)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (colour.withAlpha (0.14f * (float) i * kGlow * strength));
            g.drawRoundedRectangle (r.expanded ((float) i * 1.6f), radius, (float) i * 1.4f);
        }
    }

    // Glowing text (LCD phosphor / lit note): a gentle halo, then crisp text
    // on top so it stays readable (a heavy multi-pass halo drowned the glyphs).
    void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification just, juce::Colour colour)
    {
        g.setColour (colour.withAlpha (0.16f));
        g.drawText (text, area.translated (1, 0), just);
        g.drawText (text, area.translated (-1, 0), just);
        g.drawText (text, area.translated (0, 1), just);
        g.drawText (text, area.translated (0, -1), just);
        g.setColour (colour);
        g.drawText (text, area, just);
    }

    void brushedMetal (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool isPlate)
    {
        if (isPlate)
        {
            // Flat satin vertical base (hsl(220 12% 17->12%)), hairline seam,
            // top inner highlight - no grain.
            juce::ColourGradient grad (juce::Colour (0xff262a30), r.getX(), r.getY(),
                                       juce::Colour (0xff191b1f), r.getX(), r.getBottom(), false);
            grad.addColour (0.46, juce::Colour (0xff21242a));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, radius);
            g.setColour (metalLine);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.11f));
            g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
        }
        else
        {
            // Raised slab: a broad anisotropic sheen sweep across the width
            // (the 97deg gradient), plus a pronounced top-left specular.
            juce::ColourGradient grad (juce::Colour (0xff191b20), r.getX(), r.getY(),
                                       juce::Colour (0xff191a1e), r.getRight(), r.getBottom(), false);
            grad.addColour (0.18, juce::Colour (0xff262a32));
            grad.addColour (0.34, juce::Colour (0xff333844)); // sheen peak
            grad.addColour (0.50, juce::Colour (0xff1a1c22));
            grad.addColour (0.68, juce::Colour (0xff2c313b));
            grad.addColour (0.84, juce::Colour (0xff1c1e24));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, radius);
            // Pronounced travelling specular (sheen 1.9), top-left origin.
            juce::ColourGradient spec (juce::Colours::white.withAlpha (0.16f * kSheen), r.getX() + r.getWidth() * 0.30f, r.getY() - r.getHeight() * 0.15f,
                                       juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.30f, r.getY() + r.getHeight() * 0.55f, true);
            g.setGradientFill (spec);
            g.fillRoundedRectangle (r, radius);
            // Raised edge: bright top seam, dark seam all round.
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawLine (r.getX() + radius, r.getY() + 1.5f, r.getRight() - radius, r.getY() + 1.5f, 2.0f);
            g.setColour (metalLine);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        }
    }

    void screw (juce::Graphics& g, juce::Point<float> centre, float slotDeg, float size)
    {
        const auto r = juce::Rectangle<float> (size, size).withCentre (centre);
        juce::ColourGradient body (juce::Colour (0xff3d4046), r.getX() + size * 0.4f, r.getY() + size * 0.34f,
                                   juce::Colour (0xff0d0e11), r.getRight(), r.getBottom(), true);
        body.addColour (0.58, juce::Colour (0xff202226));
        g.setGradientFill (body);
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (r.reduced (0.3f), 0.6f);
        // Slot.
        juce::Path slot;
        slot.addRoundedRectangle (centre.x - size * 0.29f, centre.y - 0.6f, size * 0.58f, 1.2f, 0.6f);
        slot.applyTransform (juce::AffineTransform::rotation (juce::degreesToRadians (slotDeg), centre.x, centre.y));
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillPath (slot);
    }

    void insetWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
    {
        juce::ColourGradient grad (juce::Colour (0xff101114), r.getX(), r.getY(),
                                   juce::Colour (0xff1a1b20), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, radius);
        // Inner top shadow reads as recessed.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine (r.getX() + radius, r.getY() + 0.8f, r.getRight() - radius, r.getY() + 0.8f, 1.4f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.drawLine (r.getX() + radius, r.getBottom() - 0.8f, r.getRight() - radius, r.getBottom() - 0.8f, 1.0f);
    }

    void lcd (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour phosphor)
    {
        juce::ColourGradient bg (phosphor.withMultipliedBrightness (0.16f).withMultipliedSaturation (1.4f), r.getCentreX(), r.getY(),
                                 juce::Colour (0xff06120d), r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 8.0f);
        // Faint inner phosphor wash (kept low so an idle screen reads dark, not lit).
        g.setColour (phosphor.withAlpha (0.05f));
        g.fillRoundedRectangle (r.reduced (2.0f), 6.0f);
        // Corner gloss (under the readout).
        juce::Path clip;
        clip.addRoundedRectangle (r, 8.0f);
        g.saveState();
        g.reduceClipRegion (clip);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(),
                                    juce::Colours::transparentWhite, r.getX() + r.getWidth() * 0.5f, r.getY() + r.getHeight() * 0.5f, false);
        g.setGradientFill (gloss);
        g.fillRect (r);
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }

    // The CRT scanlines - the TOP layer, drawn after the readout so the note
    // and LED read as sitting behind the glass (old-monitor look).
    void lcdScanlines (juce::Graphics& g, juce::Rectangle<float> r)
    {
        juce::Path clip;
        clip.addRoundedRectangle (r, 8.0f);
        g.saveState();
        g.reduceClipRegion (clip);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float y = r.getY(); y < r.getBottom(); y += 3.0f)
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
        g.restoreState();
    }

    void meter (juce::Graphics& g, juce::Rectangle<float> r, float level01)
    {
        g.setColour (juce::Colour (0xff0a0b0d));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        constexpr int segs = 12;
        const auto inner = r.reduced (3.0f);
        const float gap = 2.0f;
        const float segH = (inner.getHeight() - gap * (segs - 1)) / (float) segs;
        const int lit = juce::jlimit (0, segs, (int) std::ceil (level01 * segs));
        for (int i = 0; i < segs; ++i)
        {
            const float y = inner.getBottom() - (float) (i + 1) * segH - (float) i * gap;
            auto cell = juce::Rectangle<float> (inner.getX(), y, inner.getWidth(), segH);
            const bool on = i < lit;
            juce::Colour c = i >= segs - 1 ? colors::red
                           : i >= segs - 3 ? colors::amber
                                           : colors::playing;
            if (on)
            {
                g.setColour (c);
                g.fillRoundedRectangle (cell, 1.0f);
            }
            else
            {
                g.setColour (juce::Colour (0xff1e2733));
                g.fillRoundedRectangle (cell, 1.0f);
            }
        }
    }

    juce::Colour button (juce::Graphics& g, juce::Rectangle<float> r, bool lit,
                         juce::Colour ledColour, bool over, bool down)
    {
        if (lit)
        {
            // Backlit key: a clean top-lit coloured face (bright top -> mid ->
            // darker bottom) with a bright inner top highlight and a soft inner
            // core. No expanded outline (that read as a square stroke); the
            // outer bloom is added by the container via dropGlow where it fits.
            juce::ColourGradient grad (ledColour.brighter (0.55f), r.getX(), r.getY(),
                                       ledColour.darker (0.42f), r.getX(), r.getBottom(), false);
            grad.addColour (0.5, ledColour);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 4.0f);
            // Soft bright core (an inner glow, faded - not an outline).
            g.setColour (ledColour.brighter (0.7f).withAlpha (0.35f));
            g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.16f, r.getHeight() * 0.22f), 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawLine (r.getX() + 4.0f, r.getY() + 1.5f, r.getRight() - 4.0f, r.getY() + 1.5f, 1.4f);
            g.setColour (ledColour.darker (0.55f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            return juce::Colour (0xff07120d); // black legend
        }
        auto top = juce::Colour (0xff30323a), mid = juce::Colour (0xff23252b), bot = juce::Colour (0xff191a1e);
        if (down) { std::swap (top, bot); }
        juce::ColourGradient grad (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false);
        grad.addColour (0.46, mid);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (metalLine);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        if (! down)
        {
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
        }
        return over ? juce::Colour (0xffe8eaf1) : juce::Colour (0xffc0c4d0);
    }

    void knob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, juce::Colour accent, bool bipolar)
    {
        const auto c = bounds.getCentre();
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        // Ratios measured from the CSS .hw-knob layers (relative to the full
        // visual extent = the tick ring): cap 0.72, arc ring 0.69-0.88 thick,
        // ticks 0.88-0.94.
        const float capR  = R * 0.72f;
        const float arcMid = R * 0.785f;
        const float arcT   = R * 0.19f;
        auto ring = [&] (float rad) { return juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c); };
        const float a0 = juce::MathConstants<float>::pi * 1.25f;
        const float a1 = juce::MathConstants<float>::pi * 2.75f;
        const float av = a0 + juce::jlimit (0.0f, 1.0f, pos) * (a1 - a0);
        const float amid = (a0 + a1) * 0.5f;

        // Recessed well behind everything.
        juce::ColourGradient well (juce::Colour (0xff17181c), c.x, c.y,
                                   juce::Colour (0xff26282e), c.x, c.y + R, true);
        g.setGradientFill (well);
        g.fillEllipse (ring (R * 0.99f));

        // Value arc: a thick ring, dark unlit track + accent lit span.
        juce::Path track; track.addCentredArc (c.x, c.y, arcMid, arcMid, 0.0f, a0, a1, true);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (track, juce::PathStrokeType (arcT, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        juce::Path lit; lit.addCentredArc (c.x, c.y, arcMid, arcMid, 0.0f, bipolar ? amid : a0, av, true);
        g.setColour (accent.withAlpha (0.9f));
        g.strokePath (lit, juce::PathStrokeType (arcT, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (accent.brighter (0.45f));
        g.strokePath (lit, juce::PathStrokeType (arcT * 0.42f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // Tick ring, just outside the arc.
        g.setColour (juce::Colours::white.withAlpha (0.20f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (float) i / 10.0f * (a1 - a0);
            g.drawLine (c.x + std::sin (a) * R * 0.90f, c.y - std::cos (a) * R * 0.90f,
                        c.x + std::sin (a) * R * 0.955f, c.y - std::cos (a) * R * 0.955f, 1.0f);
        }

        // Metal cap: radial highlight biased to top (CSS 50%,30%).
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (ring (capR + 1.0f));
        juce::ColourGradient body (juce::Colour (0xff3a3d44), c.x, c.y - capR * 0.4f,
                                   juce::Colour (0xff101114), c.x, c.y + capR, true);
        body.addColour (0.42, juce::Colour (0xff26282d));
        body.addColour (0.72, juce::Colour (0xff191a1e));
        g.setGradientFill (body);
        g.fillEllipse (ring (capR));
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawEllipse (ring (capR).reduced (1.0f), 1.0f);
        // Domed centre.
        const float domR = capR * 0.5f;
        juce::ColourGradient dome (juce::Colour (0xff33363d), c.x, c.y - domR * 0.5f,
                                   juce::Colour (0xff16171b), c.x, c.y + domR, true);
        g.setGradientFill (dome);
        g.fillEllipse (ring (domR));

        // Pointer: an accent bar in the upper cap (CSS 9%..35% from top).
        juce::Path ptr;
        ptr.addRoundedRectangle (c.x - 1.5f, c.y - capR * 0.86f, 3.0f, capR * 0.5f, 1.5f);
        ptr.applyTransform (juce::AffineTransform::rotation (av, c.x, c.y));
        g.setColour (accent.brighter (0.2f));
        g.fillPath (ptr);
    }
} // namespace hw

//==============================================================================
SegmentedSelector::SegmentedSelector (juce::RangedAudioParameter& param,
                                      const juce::StringArray& opts, juce::Colour accentColour,
                                      const juce::StringArray& tips)
    : options (opts), tooltips (tips), accent (accentColour),
      attachment (param, [this] (float v) { value = (int) v; repaint(); })
{
    attachment.sendInitialUpdate();
}

juce::String SegmentedSelector::getTooltip()
{
    if (tooltips.isEmpty())
        return {};
    const int idx = juce::jlimit (0, tooltips.size() - 1,
                                  (int) ((float) getMouseXYRelative().x / ((float) getWidth() / (float) options.size())));
    return tooltips[idx];
}

void SegmentedSelector::paint (juce::Graphics& g)
{
    // Hardware segmented switch: a recessed inset track holding push-button
    // keys; the selected one is a backlit emerald key (the single lit colour).
    const auto bounds = getLocalBounds().toFloat();
    hw::insetWell (g, bounds, 6.0f);

    const float w = bounds.getWidth() / (float) options.size();
    for (int i = 0; i < options.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + w * (float) i, bounds.getY(), w, bounds.getHeight())
                       .reduced (3.0f, 3.0f);
        if (i == value) // soft emerald bloom behind the lit segment
        {
            juce::Path p;
            p.addRoundedRectangle (seg, 4.0f);
            hw::dropGlow (g, p, hw::led, 6);
        }
        const auto legend = hw::button (g, seg, i == value, hw::led, false, false);
        g.setColour (legend);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (options[i].toUpperCase(), seg, juce::Justification::centred);
    }
}

void SegmentedSelector::mouseDown (const juce::MouseEvent& e)
{
    const int idx = juce::jlimit (0, options.size() - 1,
                                  (int) (e.position.x / ((float) getWidth() / (float) options.size())));
    attachment.setValueAsCompleteGesture ((float) idx);
}

//==============================================================================
CurveSelector::CurveSelector (juce::RangedAudioParameter& param, juce::Colour accentColour)
    : accent (accentColour),
      attachment (param, [this] (float v) { value = (int) v; repaint(); })
{
    attachment.sendInitialUpdate();
}

void CurveSelector::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colors::control);
    g.fillRoundedRectangle (bounds, 5.0f);

    // Same shapes the engine applies (see morphAt's curve mapping).
    auto shape = [] (int curve, float t) -> float
    {
        switch (curve)
        {
            case params::exponential: return t * t;
            case params::sCurve:      return t * t * (3.0f - 2.0f * t);
            case params::logarithmic: return 1.0f - (1.0f - t) * (1.0f - t);
            default:                  return t;
        }
    };

    // Four hardware keys, each engraving its curve glyph; the selected one
    // lights emerald with a black glyph.
    const float w = bounds.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + w * (float) i, bounds.getY(), w, bounds.getHeight())
                       .reduced (3.0f, 2.0f);
        if (i == value)
        {
            juce::Path p;
            p.addRoundedRectangle (seg, 4.0f);
            hw::dropGlow (g, p, hw::led, 6);
        }
        const auto glyph = hw::button (g, seg, i == value, hw::led, false, false);

        const auto plot = seg.withSizeKeepingCentre (juce::jmin (34.0f, seg.getWidth() - 16.0f), seg.getHeight() - 14.0f);
        juce::Path path;
        for (int s = 0; s <= 20; ++s)
        {
            const float t = (float) s / 20.0f;
            const auto pt = juce::Point<float> (plot.getX() + plot.getWidth() * t,
                                                plot.getBottom() - plot.getHeight() * shape (i, t));
            if (s == 0)
                path.startNewSubPath (pt);
            else
                path.lineTo (pt);
        }
        g.setColour (glyph);
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void CurveSelector::mouseDown (const juce::MouseEvent& e)
{
    const int idx = juce::jlimit (0, 3, (int) (e.position.x / ((float) getWidth() / 4.0f)));
    attachment.setValueAsCompleteGesture ((float) idx);
}

juce::String CurveSelector::getTooltip()
{
    static const juce::StringArray tips {
        "Linear - steady pace",
        "Exponential - starts slow, ends fast",
        "S-Curve - eases in and out",
        "Logarithmic - starts fast, ends slow" };
    return tips[juce::jlimit (0, 3, (int) ((float) getMouseXYRelative().x / ((float) getWidth() / 4.0f)))];
}

//==============================================================================
namespace
{
    constexpr bool blackPc[12] = { false, true, false, true, false, false,
                                   true, false, true, false, true, false };
}

PianoKeyboard::PianoKeyboard (AleaAudioProcessor& p, char scaleId, int sourceIdx, juce::Colour accentColour)
    : alea (p), sourceIndex (sourceIdx), accent (accentColour)
{
    for (int pc = 0; pc < 12; ++pc)
    {
        auto* param = alea.apvts.getParameter (params::noteId (scaleId, pc));
        attachments[(size_t) pc] = std::make_unique<juce::ParameterAttachment> (
            *param, [this, pc] (float v) { selected[pc] = v > 0.5f; repaint(); });
        attachments[(size_t) pc]->sendInitialUpdate();
    }
    rootRaw = alea.apvts.getRawParameterValue (juce::String::charToString (scaleId) + "Root");
}

juce::Rectangle<float> PianoKeyboard::whiteKeyBounds (int slot) const
{
    const float w = (float) getWidth() / 7.0f;
    return { w * (float) slot, 0.0f, w, (float) getHeight() };
}

juce::Rectangle<float> PianoKeyboard::blackKeyBounds (int whitesBefore) const
{
    const float w = (float) getWidth() / 7.0f;
    const float bw = w * 0.62f;
    // Centered on the boundary after its preceding white key, clamped so a
    // black root at the very left stays fully visible.
    const float centre = juce::jlimit (bw * 0.5f, (float) getWidth() - bw * 0.5f, w * (float) whitesBefore);
    return { centre - bw * 0.5f, 0.0f, bw, (float) getHeight() * 0.6f };
}

void PianoKeyboard::paint (juce::Graphics& g)
{
    const int active = alea.activeNote.load();
    const bool mine = active >= 0 && alea.activeSource.load() == sourceIndex;
    const int activePc = active >= 0 ? alea.activeSourcePc.load() : -1;
    // Velocity fills a proportional slice of the key, bottom-up, always at
    // full green - like a little VU meter inside the key.
    const float velNorm = (float) alea.activeVelocity.load() / 127.0f;

    auto velocityFill = [&g, velNorm] (juce::Rectangle<float> key)
    {
        const auto fill = key.withTop (key.getBottom() - key.getHeight() * juce::jmax (0.12f, velNorm));
        // The fill's upper edge dissolves into the key underneath instead of
        // ending in a hard line.
        juce::ColourGradient grad (colors::playing.withAlpha (0.0f), fill.getX(), fill.getY(),
                                   colors::playing, fill.getX(), fill.getBottom(), false);
        grad.addColour (0.40, colors::playing.withAlpha (0.85f));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (fill, 3.0f);
    };

    const int root = rootRaw != nullptr ? (int) rootRaw->load() : 0;

    // Recessed keybed the keys sit in.
    hw::insetWell (g, getLocalBounds().toFloat(), 8.0f);

    // Bottom-rounded key shape (top edge sits square against the bed).
    auto keyPath = [] (juce::Rectangle<float> k, float r)
    {
        juce::Path p;
        p.addRoundedRectangle (k.getX(), k.getY(), k.getWidth(), k.getHeight(), r, r,
                               false, false, true, true);
        return p;
    };

    // Which slots are selected white / black keys (for geometry + the glow pass).
    auto whiteKeyRect = [&] (int slot) { return whiteKeyBounds (slot).reduced (1.0f); };

    // Soft accent bloom behind the lit keys - a real Gaussian glow (JUCE has
    // no box-shadow): render the lit key shapes into an image and drop-shadow
    // it in the accent colour. Subtle, not a hard outline.
    if (getWidth() > 0 && getHeight() > 0)
    {
        juce::Image mask (juce::Image::ARGB, getWidth(), getHeight(), true);
        {
            juce::Graphics mg (mask);
            mg.setColour (juce::Colours::white);
            int whiteSlot = 0, whitesSeen = 0;
            for (int i = 0; i < 12; ++i)
            {
                const int pc = (root + i) % 12;
                if (blackPc[pc])
                {
                    if (selected[i]) mg.fillPath (keyPath (blackKeyBounds (whitesSeen), 2.5f));
                }
                else
                {
                    if (selected[i]) mg.fillPath (keyPath (whiteKeyRect (whiteSlot), 3.0f));
                    ++whiteSlot;
                    ++whitesSeen;
                }
            }
        }
        juce::DropShadow (accent.withAlpha (0.85f), (int) (9.0f * hw::kGlow), {}).drawForImage (g, mask);
    }

    // White keys: dark grey when unselected (never white - scale keys), bright
    // accent + top-lit gradient when selected.
    int whiteSlotCounter = 0;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (root + i) % 12;
        if (blackPc[pc])
            continue;
        const auto key = whiteKeyRect (whiteSlotCounter++);
        const bool isPlayingKey = mine && i == activePc;
        const auto path = keyPath (key, 3.0f);

        if (selected[i])
        {
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.22f), key.getX(), key.getY(),
                                                     accent.darker (0.20f), key.getX(), key.getBottom(), false));
            g.fillPath (path);
            g.setColour (juce::Colours::white.withAlpha (0.22f)); // top highlight
            g.drawLine (key.getX() + 2.0f, key.getY() + 1.0f, key.getRight() - 2.0f, key.getY() + 1.0f, 1.2f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff272730), key.getX(), key.getY(),
                                                     juce::Colour (0xff1a1a20), key.getX(), key.getBottom(), false));
            g.fillPath (path);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawLine (key.getX() + 2.0f, key.getY() + 1.0f, key.getRight() - 2.0f, key.getY() + 1.0f, 1.0f);
        }
        if (isPlayingKey)
            velocityFill (key);

        g.setColour (selected[i] || isPlayingKey ? juce::Colours::black.withAlpha (0.8f)
                                                 : colors::secondary.withAlpha (0.75f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (params::pitchClassNames[pc],
                    key.withTop (key.getBottom() - 18.0f).toNearestInt(), juce::Justification::centred);
    }

    // Black keys: near-black when unselected, accent + top-lit when selected.
    int whitesBefore = 0;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (root + i) % 12;
        if (! blackPc[pc])
        {
            ++whitesBefore;
            continue;
        }
        const auto key = blackKeyBounds (whitesBefore);
        const bool isPlayingKey = mine && i == activePc;
        const auto path = keyPath (key, 2.5f);

        if (selected[i])
        {
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.22f), key.getX(), key.getY(),
                                                     accent.darker (0.25f), key.getX(), key.getBottom(), false));
            g.fillPath (path);
            g.setColour (juce::Colours::white.withAlpha (0.24f));
            g.drawLine (key.getX() + 2.0f, key.getY() + 1.0f, key.getRight() - 2.0f, key.getY() + 1.0f, 1.0f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b20), key.getX(), key.getY(),
                                                     juce::Colour (0xff0a0b0e), key.getX(), key.getBottom(), false));
            g.fillPath (path);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawLine (key.getX() + 2.0f, key.getY() + 1.0f, key.getRight() - 2.0f, key.getY() + 1.0f, 1.0f);
        }
        if (isPlayingKey)
            velocityFill (key);
    }
}

void PianoKeyboard::mouseDown (const juce::MouseEvent& e)
{
    auto toggle = [this] (int pc)
    {
        attachments[(size_t) pc]->setValueAsCompleteGesture (selected[pc] ? 0.0f : 1.0f);
    };

    const int root = rootRaw != nullptr ? (int) rootRaw->load() : 0;

    // Blacks first (they sit on top); clicks toggle the INTERVAL at that
    // slot, matching paint's geometry.
    int whitesBefore = 0;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (root + i) % 12;
        if (! blackPc[pc])
        {
            ++whitesBefore;
            continue;
        }
        if (blackKeyBounds (whitesBefore).contains (e.position))
            return toggle (i);
    }

    int whiteSlotCounter = 0;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (root + i) % 12;
        if (blackPc[pc])
            continue;
        if (whiteKeyBounds (whiteSlotCounter++).contains (e.position))
            return toggle (i);
    }
}

//==============================================================================
RestSelector::RestSelector (AleaAudioProcessor& p, char scaleId, int sourceIdx, juce::Colour accentColour)
    : alea (p), sourceIndex (sourceIdx), accent (accentColour)
{
    for (int r = 0; r < params::numRests; ++r)
    {
        auto* param = p.apvts.getParameter (params::restId (scaleId, r));
        attachments[(size_t) r] = std::make_unique<juce::ParameterAttachment> (
            *param, [this, r] (float v) { selected[r] = v > 0.5f; repaint(); });
        attachments[(size_t) r]->sendInitialUpdate();
    }
}

void RestSelector::paint (juce::Graphics& g)
{
    const int active = alea.activeRest.load();
    const bool mine = active >= 0 && alea.activeRestSource.load() == sourceIndex;

    const float w = (float) getWidth() / (float) params::numRests;
    for (int r = 0; r < params::numRests; ++r)
    {
        const bool resting = mine && r == active;
        const auto cell = juce::Rectangle<float> (w * (float) r, 0.0f, w, (float) getHeight()).reduced (2.0f);
        const auto fill = resting ? colors::playing : selected[r] ? accent : colors::control;
        if (resting || selected[r])
            g.setGradientFill (juce::ColourGradient (fill.brighter (0.12f), cell.getX(), cell.getY(),
                                                     fill.darker (0.18f), cell.getX(), cell.getBottom(), false));
        else
            g.setColour (fill);
        g.fillRoundedRectangle (cell, 4.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (cell, 4.0f, 1.0f);
        g.setColour (selected[r] || resting ? juce::Colours::black.withAlpha (0.75f) : colors::secondary);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (params::restNames[r], cell, juce::Justification::centred);
    }
}

void RestSelector::mouseDown (const juce::MouseEvent& e)
{
    const int r = juce::jlimit (0, params::numRests - 1,
                                (int) (e.position.x / ((float) getWidth() / (float) params::numRests)));
    attachments[(size_t) r]->setValueAsCompleteGesture (selected[r] ? 0.0f : 1.0f);
}

//==============================================================================
MorphBar::MorphBar (AleaAudioProcessor& p)
    : alea (p),
      attachment (*p.apvts.getParameter ("morphPos"),
                  [this] (float v) { value = v; repaint(); })
{
    attachment.sendInitialUpdate();
}

bool MorphBar::sweepActive() const
{
    return alea.apvts.getRawParameterValue ("autoSweep")->load() > 0.5f;
}

void MorphBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool sweep = sweepActive();
    const float pct = juce::jlimit (0.0f, 100.0f, sweep ? (float) alea.morphPercent.load() : value);

    g.setColour (colors::control);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Fill is a purple-to-cyan gradient across the whole travel, clipped to
    // the current position, so the color literally shows where between the
    // two scales you are.
    const float fillW = bounds.getWidth() * pct / 100.0f;
    if (fillW > 0.5f)
    {
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (bounds.getX(), bounds.getY(), fillW, bounds.getHeight()).toNearestInt());
        g.setGradientFill (juce::ColourGradient::horizontal (colors::purple, bounds.getX(),
                                                             colors::cyan, bounds.getRight()));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.restoreState();
    }

    // Position marker
    g.setColour (colors::amber);
    g.fillRect (juce::Rectangle<float> (bounds.getX() + fillW - 1.5f, bounds.getY(), 3.0f, bounds.getHeight()));

    g.setColour (colors::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    g.setColour (colors::text);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (juce::String (pct, 1) + "%", bounds, juce::Justification::centred);

    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.setColour (pct < 1.0f ? colors::purple : colors::text);
    g.drawText ("A", bounds.reduced (10.0f, 0.0f), juce::Justification::centredLeft);
    g.setColour (pct > 99.0f ? colors::text : colors::cyan);
    g.drawText ("B", bounds.reduced (10.0f, 0.0f), juce::Justification::centredRight);
}

float MorphBar::pctFromX (const juce::MouseEvent& e) const
{
    return juce::jlimit (0.0f, 100.0f, 100.0f * e.position.x / (float) getWidth());
}

void MorphBar::applyDrag (const juce::MouseEvent& e)
{
    attachment.setValueAsPartOfGesture (pctFromX (e));
}

void MorphBar::showCcMenu()
{
    juce::PopupMenu menu;
    const int cc = alea.morphCC.load();

    if (alea.ccLearnArmed.load())
        menu.addItem (1, "Waiting for a CC... (move a knob on your controller)", false);
    else
        menu.addItem ("Learn MIDI CC", [this] { alea.ccLearnArmed.store (true); });

    if (cc >= 0)
        menu.addItem ("Forget CC " + juce::String (cc), [this] { alea.morphCC.store (-1); });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void MorphBar::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return showCcMenu();

    dragging = true;
    scrubbing = sweepActive();

    if (scrubbing)
        alea.scrubRequest.store (pctFromX (e)); // re-anchors the running sweep
    else
    {
        attachment.beginGesture();
        applyDrag (e);
    }
}

void MorphBar::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    if (scrubbing)
        alea.scrubRequest.store (pctFromX (e));
    else
        applyDrag (e);
}

void MorphBar::mouseUp (const juce::MouseEvent&)
{
    if (dragging && ! scrubbing)
        attachment.endGesture();
    dragging = false;
    scrubbing = false;
}

//==============================================================================
void TransportButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat();
    const bool on = getToggleState();
    g.setColour (on ? colors::green : colors::green.withAlpha (over ? 0.24f : 0.16f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (on ? colors::green : colors::green.withAlpha (0.75f));
    g.drawRoundedRectangle (b.reduced (0.6f), 5.0f, 1.2f);

    g.setColour (on ? juce::Colours::black : colors::green.brighter (0.45f));
    const float cy = b.getCentreY();
    const float ix = 15.0f;
    if (on) // stop square - holding a moment is FREEZE's job
        g.fillRoundedRectangle (ix - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.5f);
    else
    {
        juce::Path p;
        p.addTriangle (ix - 5.0f, cy - 6.5f, ix - 5.0f, cy + 6.5f, ix + 6.5f, cy);
        g.fillPath (p);
    }
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (on ? "STOP" : "PLAY", (int) ix + 11, 0, getWidth() - (int) ix - 14, getHeight(),
                juce::Justification::centredLeft);
}

//==============================================================================
OutputPanel::OutputPanel (AleaAudioProcessor& p) : alea (p)
{
    const bool standalone = alea.wrapperType == juce::AudioProcessor::wrapperType_Standalone;

    outputBox = std::make_unique<juce::ComboBox>();
    outputBox->setColour (juce::ComboBox::backgroundColourId, colors::control);
    outputBox->setColour (juce::ComboBox::textColourId, colors::text);
    outputBox->setColour (juce::ComboBox::outlineColourId, colors::border);
    outputBox->setColour (juce::ComboBox::arrowColourId, colors::secondary);

    const auto current = alea.getStandaloneOutput();

    // The sound list comes from the shared flavour table (Source/Sound.h),
    // grouped by section: SYNTH / INSTRUMENT, then MIDI. Flavour ids are
    // 1 + alea::Flavour; "MIDI to DAW" is 50; devices from 100.
    if (! standalone)
        outputBox->addItem ("MIDI to DAW", 50); // plugin default, listed first

    for (int group : { alea::groupSynth, alea::groupInstrument })
    {
        outputBox->addSectionHeading (alea::groupName (group));
        for (const auto& f : alea::flavourTable())
            if (f.group == group)
                outputBox->addItem (f.name, 1 + f.flavour);
    }

    if (standalone)
    {
        // Standalone: the internal sounds (Warm Pad default) or any MIDI device.
        devices = juce::MidiOutput::getAvailableDevices();
        if (! devices.isEmpty())
            outputBox->addSectionHeading ("MIDI");
        for (int i = 0; i < devices.size(); ++i)
            outputBox->addItem (devices[i].name, 100 + i);

        outputBox->setSelectedId (1 + alea::warmPad, juce::dontSendNotification);
        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].identifier == current)
                outputBox->setSelectedId (100 + i, juce::dontSendNotification);
    }
    else
    {
        // Plugin: pure MIDI by default; the synth makes hosts that can't
        // route plugin MIDI (AU in Live/Logic) hear Alea directly.
        outputBox->setSelectedId (50, juce::dontSendNotification);
    }

    outputBox->onChange = [this]
    {
        const int id = outputBox->getSelectedId();
        if (id >= 1 && id <= alea::numFlavours)
            alea.setStandaloneOutput (alea::choiceForFlavour (id - 1));
        else if (id == 50)
            alea.setStandaloneOutput ({});
        else if (id >= 100 && id - 100 < devices.size())
            alea.setStandaloneOutput (devices[id - 100].identifier);
    };

    if (const int flavour = alea::flavourFromChoice (current); flavour >= 0 && alea.synthOn.load())
        outputBox->setSelectedId (1 + flavour, juce::dontSendNotification);

    addAndMakeVisible (*outputBox);

    // Synth volume knob + vertical meter sit beside the chooser when the
    // internal synth is the output.
    volSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volSlider.setPopupDisplayEnabled (true, true, this);
    volSlider.setColour (juce::Slider::rotarySliderFillColourId, colors::green.withAlpha (0.85f));
    volSlider.setColour (juce::Slider::rotarySliderOutlineColourId, colors::control);
    volSlider.setColour (juce::Slider::thumbColourId, colors::text);
    addChildComponent (volSlider);
    volAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        alea.apvts, "synthVol", volSlider);

    // Global transpose: an output transform, so it lives in OUTPUT. A bipolar
    // knob (its 12 o'clock is 0 semitones, the default).
    transposeSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    transposeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    transposeSlider.setColour (juce::Slider::rotarySliderFillColourId, colors::green.withAlpha (0.85f));
    transposeSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (transposeSlider);
    transposeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        alea.apvts, "transpose", transposeSlider);
}

void OutputPanel::resized()
{
    // One compact top row (saves vertical space): the sound chooser fills the
    // left, then TRANSPOSE knob, LEVEL knob, OUT meter to the right.
    if (outputBox == nullptr)
        return;
    const bool synth = alea.synthOn.load();
    const int ky = 4, ks = 44;

    int rx = getWidth();
    if (synth)
    {
        meterRect = juce::Rectangle<int> (rx - 14, ky + 2, 12, ks - 4);
        rx -= 14 + 8;
        volSlider.setBounds (rx - ks, ky, ks, ks);
        rx -= ks + 10;
    }
    transposeSlider.setBounds (rx - ks, ky, ks, ks);
    rx -= ks + 12;
    volSlider.setVisible (synth);
    outputBox->setBounds (0, ky + (ks - 26) / 2, rx, 26);
}

void OutputPanel::paint (juce::Graphics& g)
{
    const int active = alea.activeNote.load();
    const int rest   = alea.activeRest.load();
    const int last   = alea.lastNote.load();
    const double ppq = alea.hostPpq.load();
    const bool playing = alea.hostIsPlaying.load();

    // Output-mode changes relayout the top row (combo shrinks for vol+meter).
    if (alea.synthOn.load() != lastSynthOn)
    {
        lastSynthOn = alea.synthOn.load();
        resized();
    }

    // Knob-row captions + values (TRANSPOSE always; OUT meter + LEVEL with
    // the synth). Knob bounds come from resized().
    auto knobCaption = [&] (juce::Rectangle<int> kb, const juce::String& cap, const juce::String& val)
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (cap, kb.getX() - 12, kb.getBottom() - 2, kb.getWidth() + 24, 13, juce::Justification::centred);
        g.setColour (colors::text);
        g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
        g.drawText (val, kb.getX() - 12, kb.getBottom() + 10, kb.getWidth() + 24, 14, juce::Justification::centred);
    };
    if (outputBox != nullptr)
    {
        const int st = (int) std::lround (alea.apvts.getRawParameterValue ("transpose")->load());
        knobCaption (transposeSlider.getBounds(), "TRANSPOSE", juce::String (st) + " st");
    }

    // Output meter (internal synth): a 12-segment strip beside the LEVEL knob.
    if (lastSynthOn && outputBox != nullptr)
    {
        meterLevel = juce::jmax (alea.synthPeak.load(), meterLevel * 0.82f);
        const float db = juce::Decibels::gainToDecibels (meterLevel, -48.0f);
        hw::meter (g, meterRect.toFloat(), juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f));

        const float lvl = alea.apvts.getRawParameterValue ("synthVol")->load();
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText ("OUT", meterRect.getX() - 4, meterRect.getBottom() + 2, meterRect.getWidth() + 8, 11, juce::Justification::centred);
        knobCaption (volSlider.getBounds(), "LEVEL", juce::String (lvl, 1) + " dB");
    }

    auto area = getLocalBounds();
    area.removeFromTop (74); // the compact sound/transpose/level/meter row + captions

    // Note displays are colored by the scale the note came from, matching
    // the history ticker.
    const auto srcColour = alea.activeSource.load() == 1 ? colors::cyan : colors::purple;

    // The monitor is a physical GREEN glass screen - a constant dark green
    // tint that is NOT lit when idle. The note lights in its scale colour
    // (purple A / cyan B) big on the left; BAR/BEAT sits to the RIGHT so the
    // note has room; the scanlines are the TOP layer, so the readout reads as
    // behind the glass (old-monitor look).
    const auto lcdRect = juce::Rectangle<float> ((float) area.getX(), (float) area.getY(),
                                                 (float) area.getWidth(), 54.0f);
    hw::lcd (g, lcdRect, colors::green);
    auto screen = lcdRect.toNearestInt().reduced (10, 4);
    auto barBeat = screen.removeFromRight (86);

    // Behind the glass: the activity LED + the big sounding note (the
    // scanlines cross over these).
    g.setColour (active >= 0 ? colors::playing : colors::playing.withAlpha (0.22f));
    g.fillEllipse (screen.removeFromLeft (22).withSizeKeepingCentre (11, 11).toFloat());
    const float bigNote = 30.0f;
    if (active >= 0)
    {
        g.setFont (juce::Font (juce::FontOptions (bigNote)).boldened());
        hw::glowText (g, noteName (active), screen, juce::Justification::centredLeft,
                      srcColour.brighter (0.35f));
    }
    else if (rest >= 0)
    {
        g.setColour (colors::green.withAlpha (0.4f));
        g.setFont (juce::FontOptions (20.0f, juce::Font::italic));
        g.drawText ("rest " + params::restNames[rest], screen, juce::Justification::centredLeft);
    }
    else // idle: last note dim on the dark green screen (not lit)
    {
        g.setColour (colors::green.withAlpha (0.22f));
        g.setFont (juce::Font (juce::FontOptions (bigNote)).boldened());
        g.drawText (noteName (last), screen, juce::Justification::centredLeft);
    }

    hw::lcdScanlines (g, lcdRect); // CRT lines over the note

    // BAR/BEAT sits in the CLOSEST layer, over the scanlines, so it stays
    // crisply readable.
    const int bar  = juce::jmax (1, (int) std::floor (ppq / 4.0) + 1);
    const int beat = juce::jmax (1, ((int) std::floor (ppq) % 4 + 4) % 4 + 1);
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.setColour (colors::green.withAlpha (playing ? 0.85f : 0.4f));
    g.drawText ("BAR " + juce::String (playing ? bar : 1), barBeat.removeFromTop (barBeat.getHeight() / 2),
                juce::Justification::centredRight);
    g.drawText ("BEAT " + juce::String (playing ? beat : 1), barBeat, juce::Justification::centredRight);
    area.removeFromTop (54); // past the LCD
    area.removeFromTop (6);

    // The 88-key monitor and history flow below the LCD; if the panel is too
    // short they are dropped rather than overlapping the screen above.
    if (area.getHeight() < 78)
        return;

    // 88-key monitor (A0-C8): the sounding note lights in its source
    // scale's color. Rests deliberately don't show here - silence has no
    // key; the LED going dark and the history ticker cover it.
    {
        const auto strip = area.removeFromTop (38).toFloat();
        const float ww = strip.getWidth() / 52.0f;
        auto isBlack = [] (int note) { const int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };

        int whiteIndex = 0;
        for (int note = 21; note <= 108; ++note)
        {
            if (isBlack (note))
                continue;
            const auto key = juce::Rectangle<float> (strip.getX() + ww * (float) whiteIndex, strip.getY(), ww, strip.getHeight());
            g.setColour (colors::text.withAlpha (0.14f));
            g.fillRect (key.reduced (0.5f, 0.0f));
            if (note == active) // velocity fills the key bottom-up, full color
            {
                const float vh = key.getHeight() * juce::jmax (0.15f, (float) alea.activeVelocity.load() / 127.0f);
                g.setColour (srcColour);
                g.fillRect (key.reduced (0.5f, 0.0f).withTop (key.getBottom() - vh));
            }
            ++whiteIndex;
        }

        whiteIndex = 0;
        for (int note = 21; note <= 108; ++note)
        {
            if (! isBlack (note))
            {
                ++whiteIndex;
                continue;
            }
            const float bw = ww * 0.7f;
            const auto key = juce::Rectangle<float> (strip.getX() + ww * (float) whiteIndex - bw * 0.5f,
                                                     strip.getY(), bw, strip.getHeight() * 0.62f);
            g.setColour (note == active ? srcColour : colors::background);
            g.fillRect (key);
        }

        // Octave extremes and transpose can push notes past the 88 keys -
        // a small arrow at the edge shows where the sound went.
        if (active >= 0 && (active < 21 || active > 108))
        {
            juce::Path arrow;
            const float cy = strip.getCentreY();
            if (active < 21)
                arrow.addTriangle (strip.getX() + 1.0f, cy, strip.getX() + 9.0f, cy - 7.0f, strip.getX() + 9.0f, cy + 7.0f);
            else
                arrow.addTriangle (strip.getRight() - 1.0f, cy, strip.getRight() - 9.0f, cy - 7.0f, strip.getRight() - 9.0f, cy + 7.0f);
            g.setColour (srcColour);
            g.fillPath (arrow);
        }
    }

    // History: a single readable ticker, newest event entering on the right,
    // sequence reading left-to-right, colored by source scale. Rests appear
    // as their duration in parentheses - they're events too.
    area.removeFromTop (6);
    auto historyLabelRow = area.removeFromTop (16);
    g.setColour (colors::secondary);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    g.drawText ("HISTORY", historyLabelRow, juce::Justification::centredLeft);

    const auto ticker = area.removeFromTop (28).toFloat();

    const int total = alea.historyCount.load();
    float xRight = ticker.getRight();
    const juce::Font tickerFont { juce::FontOptions (17.0f, juce::Font::bold) };
    g.setFont (tickerFont);

    for (int i = total - 1; i >= juce::jmax (0, total - 50) && xRight > ticker.getX(); --i)
    {
        const int packed = alea.history[(size_t) (i % AleaAudioProcessor::historyCapacity)].load();
        const bool isRest = (packed & 0x200) != 0;
        const auto name = isRest ? "(" + params::restNames[packed & 0xff] + ")"
                                 : noteName (packed & 0xff);
        const float w = juce::GlyphArrangement::getStringWidth (tickerFont, name);
        xRight -= w + (isRest ? 10.0f : 16.0f);
        if (xRight < ticker.getX())
            break;
        const float age = (float) (total - 1 - i);
        const float alpha = juce::jmax (0.35f, 1.0f - age * 0.06f) * (isRest ? 0.55f : 1.0f);
        const auto colour = ((packed >> 8) & 1 ? colors::cyan : colors::purple).withAlpha (alpha);
        g.setColour (colour);
        g.drawText (name, juce::Rectangle<float> (xRight, ticker.getY(), w + 2.0f, ticker.getHeight()),
                    juce::Justification::centredLeft);

        // A slim bar beside each note shows its velocity: the faint track is
        // the full range, the solid part is the actual hit.
        if (! isRest)
        {
            const float vel = (float) ((packed >> 10) & 0x7f) / 127.0f;
            const auto track = juce::Rectangle<float> (xRight + w + 4.0f, ticker.getY() + 3.0f,
                                                       3.0f, ticker.getHeight() - 6.0f);
            g.setColour (colour.withMultipliedAlpha (0.22f));
            g.fillRect (track);
            g.setColour (colour);
            g.fillRect (track.withTop (track.getBottom() - juce::jmax (2.0f, track.getHeight() * vel)));
        }
    }
}

} // namespace ui
