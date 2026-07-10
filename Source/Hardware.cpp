#include "Hardware.h"
#include "BinaryData.h"

namespace ui
{

void drawWordmark (juce::Graphics& g, const juce::Image& logo, juce::Rectangle<int> box)
{
    if (! logo.isValid())
        return;
    const int lw = box.getWidth();
    const int lh = (int) std::round ((float) lw * (float) logo.getHeight() / (float) logo.getWidth());

    // Bake at a supersampled resolution so the final draw stays crisp on Retina
    // (a 1:1 blit of a logical-size image would be upscaled and look pixelated).
    const int ss = 3;
    const int m  = 6 * ss;               // shadow margin, in supersampled pixels
    const int lwS = lw * ss, lhS = lh * ss;

    static juce::Image baked;
    static int bakedW = -1, bakedH = -1;
    if (bakedW != lw || bakedH != lh)
    {
        bakedW = lw; bakedH = lh;
        juce::Image glyphs (juce::Image::ARGB, lwS, lhS, true);
        { juce::Graphics ig (glyphs);
          ig.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
          ig.drawImage (logo, glyphs.getBounds().toFloat()); }

        baked = juce::Image (juce::Image::ARGB, lwS + 2 * m, lhS + 2 * m, true);
        { juce::Graphics og (baked);
          og.setOrigin (m, m);
          juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 3 * ss, { ss, 3 * ss }).drawForImage (og, glyphs); }

        // Punch the glyph footprint out of the shadow so nothing shadows the
        // letters, then lay the bright wordmark on top.
        { juce::Image::BitmapData src (glyphs, juce::Image::BitmapData::readOnly);
          juce::Image::BitmapData dst (baked,  juce::Image::BitmapData::readWrite);
          for (int y = 0; y < lhS; ++y)
              for (int x = 0; x < lwS; ++x)
              {
                  const float a = src.getPixelColour (x, y).getFloatAlpha();
                  if (a <= 0.0f) continue;
                  const auto c = dst.getPixelColour (x + m, y + m);
                  dst.setPixelColour (x + m, y + m, c.withMultipliedAlpha (1.0f - a));
              } }
        { juce::Graphics og (baked); og.drawImageAt (glyphs, m, m); }
    }

    // Draw scaled into the logical target rect (JUCE resamples to device res).
    const float mgn = (float) m / (float) ss;
    const int ix = box.getX(), iy = box.getY() + (box.getHeight() - lh) / 2;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (baked, juce::Rectangle<float> ((float) ix - mgn, (float) iy - mgn,
                                                (float) lw + 2.0f * mgn, (float) lh + 2.0f * mgn));
}

//==============================================================================
// Hardware faceplate drawing (the skeuomorphic reskin).

namespace hw
{
    // Baked finish (design handoff): brush 0 = clean satin, no grain;
    // sheen 1.9 = strong specular; glow 1.4 = brighter LED bloom. The metal
    // uses the handoff's EXACT stop colours (hue 220, brightness 1) - an
    // earlier 15% "moodier" dim drifted from the design and was rolled back
    // when Yuval asked for 1:1 (July 10 QA).
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

    // Glowing text (LCD phosphor / lit chord name): a REAL Gaussian halo of
    // the glyph shapes (the design's text-shadow: 0 0 Npx currentColor), then
    // crisp text on top so it stays readable. Uses the Graphics' current font.
    void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification just, juce::Colour colour, int blur)
    {
        const auto font = g.getCurrentFont();
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.0f, 0.0f);
        const auto bb = ga.getBoundingBox (0, -1, true);
        if (! bb.isEmpty())
        {
            // Place the glyph path exactly where drawText will draw.
            float x = (float) area.getX();
            if (just.testFlags (juce::Justification::horizontallyCentred))
                x = (float) area.getCentreX() - bb.getWidth() * 0.5f - bb.getX();
            else if (just.testFlags (juce::Justification::right))
                x = (float) area.getRight() - bb.getWidth() - bb.getX();
            const float baseline = (float) area.getY()
                                 + ((float) area.getHeight() - font.getHeight()) * 0.5f
                                 + font.getAscent();
            juce::Path p;
            ga.createPath (p);
            p.applyTransform (juce::AffineTransform::translation (x, baseline));
            dropGlow (g, p, colour.withAlpha (0.60f), blur);
        }
        g.setColour (colour);
        g.drawText (text, area, just);
    }

    void keyBloom (juce::Graphics& g, const juce::Component& key, juce::Colour colour,
                   float amount, float radius)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (amount <= 0.01f)
            return;
        juce::Path p;
        p.addRoundedRectangle (key.getBounds().toFloat(), radius);
        // Two passes read like the design's bloom: a broad soft spread
        // (11px x glow-mul at ~50%) plus a tight bright hug at the edge.
        dropGlow (g, p, colour.withAlpha (0.50f * amount), (int) (11.0f * kGlow));
        dropGlow (g, p, colour.withAlpha (0.45f * amount), 6);
    }

    float litAmount (const juce::Button& b)
    {
        return (float) b.getProperties().getWithDefault ("litAmt", b.getToggleState() ? 1.0f : 0.0f);
    }

    void brushedMetal (juce::Graphics& g, juce::Rectangle<float> r, float radius, bool isPlate)
    {
        if (isPlate)
        {
            // Flat satin vertical base (hsl(220 12% 17->12%)), hairline seam,
            // top inner highlight - no grain.
            juce::ColourGradient grad (juce::Colour (0xff262a31), r.getX(), r.getY(),
                                       juce::Colour (0xff1b1e22), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, radius);
            // A whisper of top-lit shading (~2%) - just enough to read as a
            // slightly raised, convex plate rather than a flat fill.
            juce::ColourGradient depth (juce::Colours::white.withAlpha (0.022f), r.getX(), r.getY(),
                                        juce::Colours::black.withAlpha (0.022f), r.getX(), r.getBottom(), false);
            depth.addColour (0.5, juce::Colours::transparentBlack);
            g.setGradientFill (depth);
            g.fillRoundedRectangle (r, radius);
            g.setColour (metalLine);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.11f));
            g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
        }
        else
        {
            // Raised slab: the 97deg anisotropic sheen sweep, stops copied
            // 1:1 from the handoff (hsl 220-hue bands 11%..27% lightness).
            juce::ColourGradient grad (juce::Colour (0xff1a1b1e), r.getX(), r.getY(),
                                       juce::Colour (0xff1a1b1d), r.getRight(), r.getY() + r.getHeight() * 0.12f, false);
            grad.addColour (0.18, juce::Colour (0xff2d2f34));
            grad.addColour (0.34, juce::Colour (0xff3d424c)); // sheen peak
            grad.addColour (0.50, juce::Colour (0xff25272c));
            grad.addColour (0.68, juce::Colour (0xff383c44));
            grad.addColour (0.84, juce::Colour (0xff212327));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, radius);
            // Specular: radial-gradient(120% 75% at 30% -15%, white .16*sheen, transparent 42%).
            juce::ColourGradient spec (juce::Colours::white.withAlpha (0.16f * kSheen),
                                       r.getX() + r.getWidth() * 0.30f, r.getY() - r.getHeight() * 0.15f,
                                       juce::Colours::transparentWhite,
                                       r.getX() + r.getWidth() * 0.30f + r.getWidth() * 0.50f, r.getY() - r.getHeight() * 0.15f, true);
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

    void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification just, juce::Colour bright)
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText (text, area.translated (1, 1), just);
        g.setColour (bright);
        g.drawText (text, area, just);
    }

    void plate (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title,
                juce::Colour tab, float tabAlpha)
    {
        const auto rf = r.toFloat();
        brushedMetal (g, rf, 10.0f, true);
        screw (g, { rf.getX() + 10.0f, rf.getY() + 10.0f }, 42.0f);
        screw (g, { rf.getRight() - 10.0f, rf.getY() + 10.0f }, -20.0f);

        float tx = rf.getX() + 22.0f; // clear the top-left screw
        if (tab.getAlpha() > 0)
        {
            g.setColour (tab.withMultipliedAlpha (tabAlpha));
            g.fillRoundedRectangle (tx, rf.getY() + 13.0f, 20.0f, 4.0f, 2.0f);
            tx += 28.0f;
        }
        // Engraved panel title - larger than the sub-labels inside the plate.
        g.setFont (juce::Font (juce::FontOptions (15.5f)).boldened());
        engraved (g, title, { (int) tx, r.getY() + 7, r.getWidth(), 18 },
                  juce::Justification::centredLeft, juce::Colour (0xffc2c5d0));
    }

    void sublabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   juce::Justification just)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        engraved (g, text.toUpperCase(), area, just, juce::Colour (0xff868ba0));
    }

    void ledDot (juce::Graphics& g, juce::Point<float> centre, float on01, juce::Colour colour,
                 float diameter)
    {
        on01 = juce::jlimit (0.0f, 1.0f, on01);
        const auto r = juce::Rectangle<float> (diameter, diameter).withCentre (centre);

        // Dark metal dome base.
        juce::ColourGradient off (juce::Colour (0xff3a3d44), r.getX() + diameter * 0.4f, r.getY() + diameter * 0.35f,
                                  juce::Colour (0xff17181b), r.getRight(), r.getBottom(), true);
        g.setGradientFill (off);
        g.fillEllipse (r);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (r.reduced (0.3f), 0.8f);

        if (on01 <= 0.004f)
            return;

        // Lit dome + bloom, composited over the base so it can crossfade.
        juce::Path dot;
        dot.addEllipse (r);
        dropGlow (g, dot, colour.withAlpha (0.85f * on01), (int) (diameter * 0.8f));
        g.beginTransparencyLayer (on01);
        juce::ColourGradient on (colour.interpolatedWith (juce::Colours::white, 0.55f),
                                 r.getX() + diameter * 0.4f, r.getY() + diameter * 0.35f,
                                 colour, r.getCentreX(), r.getBottom(), true);
        g.setGradientFill (on);
        g.fillEllipse (r);
        g.endTransparencyLayer();
    }

    void toggleSwitch (juce::Graphics& g, juce::Rectangle<float> r, float amt01, juce::Colour accent)
    {
        amt01 = juce::jlimit (0.0f, 1.0f, amt01);
        const float radius = r.getHeight() * 0.5f;

        // Recessed track; crossfades to the lit accent (design .hw-toggle.on).
        insetWell (g, r, radius);
        if (amt01 > 0.004f)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            dropGlow (g, p, accent.withAlpha (0.55f * amt01), 7);
            g.beginTransparencyLayer (amt01);
            juce::ColourGradient grad (accent.interpolatedWith (juce::Colours::black, 0.30f), r.getX(), r.getY(),
                                       accent, r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, radius);
            g.setColour (juce::Colours::black.withAlpha (0.30f));
            g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.2f);
            g.endTransparencyLayer();
        }

        // Metal knob slides across.
        const float kd = r.getHeight() - 4.0f;
        const float kx = r.getX() + 2.0f + (r.getWidth() - kd - 4.0f) * amt01;
        const auto knobR = juce::Rectangle<float> (kx, r.getY() + 2.0f, kd, kd);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (knobR.translated (0.0f, 1.2f).expanded (0.4f));
        juce::ColourGradient body (juce::Colour (0xff4d5058), knobR.getX() + kd * 0.4f, knobR.getY() + kd * 0.32f,
                                   juce::Colour (0xff16171b), knobR.getRight(), knobR.getBottom(), true);
        body.addColour (0.6, juce::Colour (0xff2a2c31));
        g.setGradientFill (body);
        g.fillEllipse (knobR);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawLine (knobR.getX() + kd * 0.25f, knobR.getY() + 1.0f, knobR.getRight() - kd * 0.25f, knobR.getY() + 1.0f, 1.0f);
    }

    void lcdAmbience (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float strength)
    {
        // The glass glows from the light inside it (design: inset 0 0 22px
        // phosphor at ~25%): layered inner strokes, clipped to the pane, that
        // fade toward the centre.
        juce::Path clip;
        clip.addRoundedRectangle (r, 8.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        // Kept WELL below the design's nominal 25% - at full strength the
        // glass read as a glowing frame and drowned the content (July 10 QA).
        const float steps[3][2] = { { 3.0f, 0.07f }, { 8.0f, 0.045f }, { 14.0f, 0.025f } };
        for (auto& s : steps)
        {
            g.setColour (colour.withAlpha (s[1] * strength));
            g.drawRoundedRectangle (r.reduced (s[0]), 8.0f, s[0] * 1.4f);
        }
    }

    void lcd (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour phosphor)
    {
        juce::ColourGradient bg (phosphor.withMultipliedBrightness (0.16f).withMultipliedSaturation (1.4f), r.getCentreX(), r.getY(),
                                 juce::Colour (0xff06120d).interpolatedWith (phosphor.withMultipliedBrightness (0.06f), 0.5f),
                                 r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (r, 8.0f);
        // Faint inner phosphor wash (kept low so an idle screen reads dark,
        // not lit; a stronger edge ambience here made the BPM LCD look wrong -
        // July 10 QA. Big monitor screens add lcdAmbience themselves).
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

    void keybed (juce::Graphics& g, juce::Rectangle<float> bed, int lowNote, int highNote,
                 const std::function<float (int)>& lit, juce::Colour accent)
    {
        auto isBlack = [] (int n) { const int pc = ((n % 12) + 12) % 12;
                                    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };

        // The recessed bed (.hw-keybed): dark vertical base, top inner shadow.
        {
            juce::ColourGradient grad (juce::Colour (0xff0c0d10), bed.getX(), bed.getY(),
                                       juce::Colour (0xff16171b), bed.getX(), bed.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bed, 8.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawLine (bed.getX() + 8.0f, bed.getY() + 1.0f, bed.getRight() - 8.0f, bed.getY() + 1.0f, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawLine (bed.getX() + 8.0f, bed.getBottom() - 1.0f, bed.getRight() - 8.0f, bed.getBottom() - 1.0f, 1.0f);
        }

        const auto keys = bed.reduced (4.0f);
        int nWhites = 0;
        for (int n = lowNote; n <= highNote; ++n)
            if (! isBlack (n)) ++nWhites;
        if (nWhites <= 0 || keys.getWidth() <= 0.0f)
            return;
        const float pitch = keys.getWidth() / (float) nWhites;

        auto whiteRect = [&] (int idx)
        { return juce::Rectangle<float> (keys.getX() + pitch * (float) idx, keys.getY(), pitch - 1.0f, keys.getHeight()); };
        auto blackRect = [&] (int whitesBefore)
        {
            const float bw = pitch * 0.62f;
            return juce::Rectangle<float> (keys.getX() + pitch * (float) whitesBefore - bw * 0.5f,
                                           keys.getY(), bw, keys.getHeight() * 0.58f);
        };
        auto keyPath = [] (juce::Rectangle<float> k, float r)
        {
            juce::Path p;
            p.addRoundedRectangle (k.getX(), k.getY(), k.getWidth(), k.getHeight(), r, r, false, false, true, true);
            return p;
        };

        auto whiteFace = [&] (juce::Rectangle<float> k, float amt)
        {
            const auto path = keyPath (k, 2.0f);
            if (amt >= 0.999f)
            {
                // Fully lit: the whole key face is the accent (design .on).
                juce::ColourGradient lg (accent.brighter (0.10f), k.getX(), k.getY(),
                                         accent.darker (0.08f), k.getX(), k.getBottom(), false);
                g.setGradientFill (lg);
                g.fillPath (path);
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.drawLine (k.getX() + 1.0f, k.getY() + 0.8f, k.getRight() - 1.0f, k.getY() + 0.8f, 1.2f);
                return;
            }
            // Pale silver key (design: #cfd2dc -> #a9adbb 68% -> #8b8f9e).
            juce::ColourGradient grad (juce::Colour (0xffcfd2dc), k.getX(), k.getY(),
                                       juce::Colour (0xff8b8f9e), k.getX(), k.getBottom(), false);
            grad.addColour (0.68, juce::Colour (0xffa9adbb));
            g.setGradientFill (grad);
            g.fillPath (path);
            if (amt > 0.0f)
            {
                // Velocity slice rising from the bottom, dissolving upward
                // (the family VU-in-key, kept from the flat design).
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (path);
                const float h = k.getHeight() * juce::jmax (0.15f, amt);
                const auto fill = k.withTop (k.getBottom() - h);
                juce::ColourGradient lg (accent.withAlpha (0.0f), fill.getX(), fill.getY(),
                                         accent, fill.getX(), fill.getBottom(), false);
                lg.addColour (0.40, accent.withAlpha (0.85f));
                g.setGradientFill (lg);
                g.fillRect (fill);
            }
            // Top highlight + the worn bottom lip (inset 0 -3px 4px black .28).
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawLine (k.getX() + 1.0f, k.getY() + 0.8f, k.getRight() - 1.0f, k.getY() + 0.8f, 1.0f);
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (path);
            juce::ColourGradient lip (juce::Colours::transparentBlack, k.getX(), k.getBottom() - 5.0f,
                                      juce::Colours::black.withAlpha (0.28f), k.getX(), k.getBottom(), false);
            g.setGradientFill (lip);
            g.fillRect (k.withTop (k.getBottom() - 5.0f));
        };

        auto blackFace = [&] (juce::Rectangle<float> k, float amt)
        {
            const auto path = keyPath (k, 2.0f);
            if (amt >= 0.999f)
            {
                juce::ColourGradient lg (accent.brighter (0.15f), k.getX(), k.getY(),
                                         accent.darker (0.05f), k.getX(), k.getBottom(), false);
                g.setGradientFill (lg);
                g.fillPath (path);
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawLine (k.getX() + 1.0f, k.getY() + 0.8f, k.getRight() - 1.0f, k.getY() + 0.8f, 1.0f);
                return;
            }
            juce::ColourGradient dg (juce::Colour (0xff26282d), k.getX(), k.getY(),
                                     juce::Colour (0xff0c0d10), k.getX(), k.getBottom(), false);
            g.setGradientFill (dg);
            g.fillPath (path);
            if (amt > 0.0f)
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (path);
                const float h = k.getHeight() * juce::jmax (0.15f, amt);
                const auto fill = k.withTop (k.getBottom() - h);
                juce::ColourGradient lg (accent.withAlpha (0.0f), fill.getX(), fill.getY(),
                                         accent, fill.getX(), fill.getBottom(), false);
                g.setGradientFill (lg);
                g.fillRect (fill);
            }
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawLine (k.getX() + 1.0f, k.getY() + 0.8f, k.getRight() - 1.0f, k.getY() + 0.8f, 1.0f);
        };

        // Layered like the design: whites, the lit whites' bloom (spilling over
        // their neighbours), crisp lit faces re-drawn over the bloom, then the
        // black keys floating on a soft drop shadow, and their bloom on top.
        const auto local = keys.getSmallestIntegerContainer().expanded (16);
        auto maskOf = [&] (const std::function<void (juce::Graphics&)>& fill)
        {
            juce::Image mask (juce::Image::ARGB, local.getWidth(), local.getHeight(), true);
            juce::Graphics mg (mask);
            mg.addTransform (juce::AffineTransform::translation ((float) -local.getX(), (float) -local.getY()));
            mg.setColour (juce::Colours::white);
            fill (mg);
            return mask;
        };
        auto bloom = [&] (const juce::Image& mask)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::translation ((float) local.getX(), (float) local.getY()));
            juce::DropShadow (accent.withAlpha (0.85f), 12, {}).drawForImage (g, mask);
        };

        int white = 0;
        bool anyLitWhite = false, anyLitBlack = false, anyBlack = false;
        for (int n = lowNote; n <= highNote; ++n)
        {
            if (isBlack (n)) { anyBlack = true; if (lit (n) > 0.0f) anyLitBlack = true; continue; }
            whiteFace (whiteRect (white), lit (n));
            if (lit (n) > 0.0f) anyLitWhite = true;
            ++white;
        }

        if (anyLitWhite)
        {
            bloom (maskOf ([&] (juce::Graphics& mg)
            {
                int w = 0;
                for (int n = lowNote; n <= highNote; ++n)
                {
                    if (isBlack (n)) continue;
                    if (lit (n) > 0.0f) mg.fillPath (keyPath (whiteRect (w), 2.0f));
                    ++w;
                }
            }));
            int w = 0;
            for (int n = lowNote; n <= highNote; ++n) // crisp faces back over the bloom
            {
                if (isBlack (n)) { continue; }
                if (lit (n) > 0.0f) whiteFace (whiteRect (w), lit (n));
                ++w;
            }
        }

        if (anyBlack)
        {
            // One soft drop shadow under ALL black keys (design 0 2px 3px).
            juce::Image mask = maskOf ([&] (juce::Graphics& mg)
            {
                int w = 0;
                for (int n = lowNote; n <= highNote; ++n)
                {
                    if (! isBlack (n)) { ++w; continue; }
                    mg.fillPath (keyPath (blackRect (w), 2.0f));
                }
            });
            juce::Graphics::ScopedSaveState ss (g);
            g.addTransform (juce::AffineTransform::translation ((float) local.getX(), (float) local.getY()));
            juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 3, { 0, 2 }).drawForImage (g, mask);
        }

        white = 0;
        for (int n = lowNote; n <= highNote; ++n)
        {
            if (! isBlack (n)) { ++white; continue; }
            blackFace (blackRect (white), lit (n));
        }

        if (anyLitBlack)
        {
            bloom (maskOf ([&] (juce::Graphics& mg)
            {
                int w = 0;
                for (int n = lowNote; n <= highNote; ++n)
                {
                    if (! isBlack (n)) { ++w; continue; }
                    if (lit (n) > 0.0f) mg.fillPath (keyPath (blackRect (w), 2.0f));
                }
            }));
            int w = 0;
            for (int n = lowNote; n <= highNote; ++n)
            {
                if (! isBlack (n)) { ++w; continue; }
                if (lit (n) > 0.0f) blackFace (blackRect (w), lit (n));
            }
        }
    }

    juce::Colour button (juce::Graphics& g, juce::Rectangle<float> r, float lit,
                         juce::Colour ledColour, bool over, bool down)
    {
        lit = juce::jlimit (0.0f, 1.0f, lit);

        // Unlit metal base - always drawn. The backlit face crossfades over it,
        // so toggling animates smoothly (lit is a 0..1 amount, not a bool).
        {
            auto top = juce::Colour (0xff30323a), mid = juce::Colour (0xff23252b), bot = juce::Colour (0xff191a1e);
            if (down) std::swap (top, bot);
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
        }
        const juce::Colour unlitLegend = over ? juce::Colour (0xffe8eaf1) : juce::Colour (0xffc0c4d0);
        if (lit <= 0.004f)
            return unlitLegend;

        // Backlit key (design .hw-btn.on): a smooth top-lit coloured face, one
        // crisp top highlight, a soft dark inner shadow. Composited at `lit`
        // opacity over the base so it fades in/out.
        g.beginTransparencyLayer (lit);
        {
            const auto topCol = ledColour.interpolatedWith (juce::Colours::white, 0.18f);
            const auto botCol = ledColour.interpolatedWith (juce::Colours::black, 0.22f);
            juce::ColourGradient grad (topCol, r.getX(), r.getY(), botCol, r.getX(), r.getBottom(), false);
            grad.addColour (0.52, ledColour);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 4.0f);
            juce::ColourGradient innerBot (juce::Colours::transparentBlack, r.getX(), r.getBottom() - 7.0f,
                                           juce::Colours::black.withAlpha (0.28f), r.getX(), r.getBottom(), false);
            g.setGradientFill (innerBot);
            g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawLine (r.getX() + 4.0f, r.getY() + 1.0f, r.getRight() - 4.0f, r.getY() + 1.0f, 1.0f);
            g.setColour (ledColour.interpolatedWith (juce::Colours::black, 0.55f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        }
        g.endTransparencyLayer();

        const juce::Colour litLegend = ledColour.getPerceivedBrightness() > 0.45f ? juce::Colour (0xff07120d)
                                                                                  : juce::Colour (0xffd6d9e4);
        return unlitLegend.interpolatedWith (litLegend, lit);
    }

    void knob (juce::Graphics& g, juce::Rectangle<float> bounds, float pos, juce::Colour accent, bool bipolar)
    {
        // Geometry measured 1:1 from the CSS .hw-knob layers, normalised to
        // the full visual extent R = the tick ring's outer edge (a 62px knob
        // box + 8px tick inset = 78px extent in the design):
        //   well 0.95R - arc ring 0.66..0.846R (flat, hard-edged) -
        //   ticks 0.879..0.941R (11 marks) - cap 0.641R (4-stop radial,
        //   knurled grip 0.42..0.615R, drop shadow below) - dome 0.308R -
        //   pointer an accent-to-dark indent spanning 0.24..0.65R.
        const auto c = bounds.getCentre();
        const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto ring = [&] (float rad) { return juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c); };
        const float a0 = juce::MathConstants<float>::pi * 1.25f;
        const float a1 = juce::MathConstants<float>::pi * 2.75f;
        const float av = a0 + juce::jlimit (0.0f, 1.0f, pos) * (a1 - a0);
        const float amid = (a0 + a1) * 0.5f;

        // Recessed well (.hw-knob::before): dark centre opening to a lighter
        // rim, darker toward the top (its inset top shadow).
        {
            const float wellR = R * 0.95f;
            juce::ColourGradient well (juce::Colour (0xff0c0d10), c.x, c.y - wellR * 0.24f,
                                       juce::Colour (0xff26282e), c.x, c.y + wellR * 0.9f, true);
            well.addColour (0.70, juce::Colour (0xff17181c));
            g.setGradientFill (well);
            g.fillEllipse (ring (wellR));
        }

        // Value arc: a FLAT ring - dark track + solid accent span (the design
        // has no gloss core on the arc).
        const float arcMid = R * 0.753f, arcT = R * 0.186f;
        {
            juce::Path track;
            track.addCentredArc (c.x, c.y, arcMid, arcMid, 0.0f, a0, a1, true);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (track, juce::PathStrokeType (arcT, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            juce::Path lit;
            lit.addCentredArc (c.x, c.y, arcMid, arcMid, 0.0f, bipolar ? amid : a0, av, true);
            // The lit arc emits a tint of light (kept faint - it's a value
            // readout, not a lamp).
            if (std::abs (av - (bipolar ? amid : a0)) > 0.05f)
            {
                juce::Path glowSrc;
                juce::PathStrokeType (arcT, juce::PathStrokeType::curved, juce::PathStrokeType::butt)
                    .createStrokedPath (glowSrc, lit);
                dropGlow (g, glowSrc, accent.withAlpha (0.45f), 9);
            }
            g.setColour (accent);
            g.strokePath (lit, juce::PathStrokeType (arcT, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // Tick ring, outside the arc: 11 marks over the 270deg travel.
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (float) i / 10.0f * (a1 - a0);
            g.drawLine (c.x + std::sin (a) * R * 0.879f, c.y - std::cos (a) * R * 0.879f,
                        c.x + std::sin (a) * R * 0.941f, c.y - std::cos (a) * R * 0.941f,
                        juce::jmax (1.0f, R * 0.028f));
        }

        // Cap drop shadow (0 4px 8px black .6): the cap floats over the well.
        const float capR = R * 0.641f;
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillEllipse (ring (capR + R * 0.10f).translated (0.0f, R * 0.10f));
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (ring (capR + R * 0.04f).translated (0.0f, R * 0.06f));

        // Cap (.hw-knob__body): 4-stop radial biased to the top (50% 30%).
        {
            juce::ColourGradient body (juce::Colour (0xff3a3d44), c.x, c.y - capR * 0.40f,
                                       juce::Colour (0xff101114), c.x, c.y + capR * 0.96f, true);
            body.addColour (0.42, juce::Colour (0xff26282d));
            body.addColour (0.72, juce::Colour (0xff191a1e));
            g.setGradientFill (body);
            g.fillEllipse (ring (capR));
        }
        // Inset top highlight + bottom inner shadow (the machined edge).
        {
            juce::Path topArc;
            topArc.addCentredArc (c.x, c.y, capR - 1.0f, capR - 1.0f, 0.0f, -1.1f, 1.1f, true);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.strokePath (topArc, juce::PathStrokeType (juce::jmax (1.0f, R * 0.035f),
                                                        juce::PathStrokeType::curved, juce::PathStrokeType::butt));
            juce::Path botArc;
            botArc.addCentredArc (c.x, c.y, capR - 2.0f, capR - 2.0f, 0.0f,
                                  juce::MathConstants<float>::pi - 1.2f,
                                  juce::MathConstants<float>::pi + 1.2f, true);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.strokePath (botArc, juce::PathStrokeType (juce::jmax (1.5f, R * 0.08f),
                                                        juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        // Knurled grip ring: 30 faint spokes between the dome and the rim.
        {
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            const float r1 = R * 0.42f, r2 = R * 0.615f;
            for (int k = 0; k < 30; ++k)
            {
                const float a = (float) k * (juce::MathConstants<float>::twoPi / 30.0f);
                g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1,
                            c.x + std::sin (a) * r2, c.y - std::cos (a) * r2,
                            juce::jmax (0.8f, R * 0.022f)); // 2deg fins (design duty 2/12)
            }
        }
        // Domed top cap (inset 26% of the body): its own shadow, then the dome.
        const float domR = R * 0.308f;
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.fillEllipse (ring (domR + R * 0.025f).translated (0.0f, R * 0.03f));
        {
            juce::ColourGradient dome (juce::Colour (0xff33363d), c.x, c.y - domR * 0.36f,
                                       juce::Colour (0xff16171b), c.x, c.y + domR, true);
            dome.addColour (0.70, juce::Colour (0xff202226));
            g.setGradientFill (dome);
            g.fillEllipse (ring (domR));
            juce::Path domeTop;
            domeTop.addCentredArc (c.x, c.y, domR - 0.8f, domR - 0.8f, 0.0f, -1.2f, 1.2f, true);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.strokePath (domeTop, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // Pointer: an indent on the cap, accent at the tip fading dark toward
        // the centre (design: linear accent -> 30% accent mixed with black).
        {
            const float pw = juce::jmax (2.2f, R * 0.077f);
            const float rOut = R * 0.65f, rIn = R * 0.24f;
            juce::Path ptr;
            ptr.addRoundedRectangle (c.x - pw * 0.5f, c.y - rOut, pw, rOut - rIn, pw * 0.5f);
            ptr.applyTransform (juce::AffineTransform::rotation (av, c.x, c.y));
            const auto tip = juce::Point<float> (c.x + std::sin (av) * rOut, c.y - std::cos (av) * rOut);
            const auto tail = juce::Point<float> (c.x + std::sin (av) * rIn, c.y - std::cos (av) * rIn);
            juce::ColourGradient pg (accent, tip.x, tip.y,
                                     accent.interpolatedWith (juce::Colours::black, 0.7f), tail.x, tail.y, false);
            g.setGradientFill (pg);
            g.fillPath (ptr);
        }
    }
} // namespace hw

//==============================================================================
// The family transport: a real backlit key. Playing = lit in playing-green
// with a black stop square; stopped = metal key with a green-tinted face,
// green play triangle and legend.
void TransportButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    const bool on = getToggleState();
    const auto grn = colors::playing;

    // The lit bloom is the parent's job (hw::keyBloom) - drawn here it would
    // clip to the button's bounds.
    const auto legend = hw::button (g, b, on ? 1.0f : 0.0f, grn, over, down);
    if (! on)
    {
        // The quiet green wash that says "this key is the transport".
        juce::ColourGradient tint (grn.withAlpha (over ? 0.20f : 0.14f), b.getX(), b.getY(),
                                   grn.withAlpha (0.04f), b.getX(), b.getBottom(), false);
        g.setGradientFill (tint);
        g.fillRoundedRectangle (b.reduced (1.0f), 4.0f);
    }

    const auto ink = on ? legend : grn.brighter (0.45f);
    g.setColour (ink);
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
SegmentedSelector::SegmentedSelector (juce::RangedAudioParameter& param,
                                      const juce::StringArray& opts, juce::Colour accentColour,
                                      const juce::StringArray& tips)
    : options (opts), tooltips (tips), accent (accentColour)
{
    litAmt.resize ((size_t) options.size(), 0.0f);
    anim.onFrame = [this] { return advanceLit(); };
    attachment = std::make_unique<juce::ParameterAttachment> (
        param, [this] (float v) { value = (int) v; startLitAnim(); });
    attachment->sendInitialUpdate();
    if (value < (int) litAmt.size()) litAmt[(size_t) value] = 1.0f; // snap on open
}

SegmentedSelector::SegmentedSelector (const juce::StringArray& opts, juce::Colour accentColour)
    : options (opts), accent (accentColour)
{
    litAmt.resize ((size_t) options.size(), 0.0f);
    anim.onFrame = [this] { return advanceLit(); };
    litAmt[0] = 1.0f; // snap on open; setSelected / setMask sync the real state
}

void SegmentedSelector::setSelected (int index)
{
    index = juce::jlimit (0, options.size() - 1, index);
    if (settled && value == index)
        return;
    value = index;
    syncLit();
}

void SegmentedSelector::setMask (int newMask)
{
    if (settled && mask == newMask)
        return;
    mask = newMask;
    syncLit();
}

void SegmentedSelector::syncLit()
{
    if (! settled)
    {
        // First sync (editor just opened): snap, don't animate.
        settled = true;
        for (int i = 0; i < (int) litAmt.size(); ++i)
            litAmt[(size_t) i] = targetFor (i);
        repaint();
        return;
    }
    startLitAnim();
}

void SegmentedSelector::setMulti (bool canBeEmpty)
{
    multi = true;
    allowEmpty = canBeEmpty;
    startLitAnim();
}

float SegmentedSelector::targetFor (int i) const
{
    return (multi ? ((mask >> i) & 1) != 0 : i == value) ? 1.0f : 0.0f;
}

void SegmentedSelector::startLitAnim() { anim.go(); }

bool SegmentedSelector::advanceLit()
{
    bool moving = false;
    for (int i = 0; i < (int) litAmt.size(); ++i)
    {
        const float target = targetFor (i);
        litAmt[(size_t) i] += (target - litAmt[(size_t) i]) * 0.4f; // ~80ms
        if (std::abs (target - litAmt[(size_t) i]) < 0.01f) litAmt[(size_t) i] = target;
        else moving = true;
    }
    repaint();
    return moving;
}

juce::String SegmentedSelector::getTooltip()
{
    if (tooltips.isEmpty())
        return {};
    const int n = options.size();
    const auto m = getMouseXYRelative();
    const float pos = vertical ? (float) m.y / ((float) getHeight() / (float) n)
                               : (float) m.x / ((float) getWidth()  / (float) n);
    return tooltips[juce::jlimit (0, tooltips.size() - 1, (int) pos)];
}

void SegmentedSelector::paint (juce::Graphics& g)
{
    // Hardware segmented switch: a recessed inset track holding push-button
    // keys; the selected one is a backlit key in the selector's accent.
    const auto bounds = getLocalBounds().toFloat();
    hw::insetWell (g, bounds, 6.0f);

    const int n = options.size();
    const float w = vertical ? bounds.getWidth()  : bounds.getWidth()  / (float) n;
    const float h = vertical ? bounds.getHeight() / (float) n : bounds.getHeight();
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + (vertical ? 0.0f : w * (float) i),
                                           bounds.getY() + (vertical ? h * (float) i : 0.0f),
                                           w, h).reduced (3.0f, 3.0f);
        // Lit segment uses the selector's accent, crossfading via litAmt. A
        // bright accent (emerald / amber = active) glows; a dark/matte accent
        // (inactive grey) stays solid with no bloom.
        const float lit = litAmt[(size_t) i];
        if (lit > 0.004f && accent.getPerceivedBrightness() > 0.4f)
        {
            juce::Path p;
            p.addRoundedRectangle (seg, 4.0f);
            hw::dropGlow (g, p, accent.withAlpha (0.45f * lit), 11);
            hw::dropGlow (g, p, accent.withAlpha (0.50f * lit), 5);
        }
        const auto legend = hw::button (g, seg, lit, accent, false, false);
        g.setColour (legend);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (options[i].toUpperCase(), seg, juce::Justification::centred);
    }
}

void SegmentedSelector::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;
    const int n = options.size();
    const float pos = vertical ? e.position.y / ((float) getHeight() / (float) n)
                               : e.position.x / ((float) getWidth()  / (float) n);
    const int idx = juce::jlimit (0, n - 1, (int) pos);

    if (attachment != nullptr)
    {
        attachment->setValueAsCompleteGesture ((float) idx);
        return;
    }
    if (multi)
    {
        const int toggled = mask ^ (1 << idx);
        if (! allowEmpty && toggled == 0)
            return; // at least one segment always stays on
        mask = toggled;
        startLitAnim();
        if (onChange != nullptr)
            onChange (mask);
    }
    else if (idx != value)
    {
        value = idx;
        startLitAnim();
        if (onChange != nullptr)
            onChange (idx);
    }
}

//==============================================================================
// The family LookAndFeel (Space Grotesk everywhere; hardware chrome).
namespace
{
    struct HardwareLookAndFeel : juce::LookAndFeel_V4
    {
        HardwareLookAndFeel()
        {
            setDefaultSansSerifTypeface (juce::Typeface::createSystemTypefaceFor (
                BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize));
            // Toggle buttons light emerald by default (the hardware LED);
            // FREEZE overrides to ice, PANIC never lights (red legend).
            setColour (juce::TextButton::buttonOnColourId, hw::led);
            setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc0c4d0));
            setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07120d));
            setColour (juce::ToggleButton::textColourId, juce::Colour (0xffaeb2c0));
            setColour (juce::ToggleButton::tickColourId, hw::led);
        }
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        { return juce::FontOptions (juce::jmin (16.5f, (float) buttonHeight * 0.62f)); }
        juce::Font getPopupMenuFont() override { return juce::FontOptions (16.0f); }
        juce::Font getLabelFont (juce::Label& label) override
        { return juce::FontOptions (juce::jmin (15.5f, (float) label.getHeight() * 0.72f)); }

        // OUT menu group titles read as real titles, not the greyed default:
        // a purple-to-cyan divider above, then the name in bright bold caps.
        void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                         const juce::String& name) override
        {
            auto r = area.toFloat();
            g.setGradientFill (juce::ColourGradient (colors::purple.withAlpha (0.55f), r.getX() + 10.0f, 0.0f,
                                                     colors::cyan.withAlpha (0.55f), r.getRight() - 10.0f, 0.0f, false));
            g.fillRect (r.getX() + 10.0f, r.getY() + 4.0f, r.getWidth() - 20.0f, 1.5f);
            g.setColour (colors::text);
            g.setFont (juce::Font (juce::FontOptions (13.5f)).boldened());
            g.drawText (name.toUpperCase(), area.reduced (12, 0).withTrimmedTop (4),
                        juce::Justification::centredLeft);
        }

        // Hardware knob, 1:1 with the handoff: a recessed well, a backlit
        // value-arc ring (from 225deg over 270deg of travel), a fine tick
        // ring, a domed metal cap, and an accent pointer at the top.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                               float, float, juce::Slider& slider) override
        {
            // Bipolar (arc grows from 12 o'clock) only when the range is
            // symmetric around 0 - e.g. Transpose (-24..+24). The volume knob
            // (-24..+6) is a normal knob: its centre is not the default.
            const bool bipolar = std::abs (slider.getMinimum() + slider.getMaximum()) < 0.001;
            hw::knob (g, juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height),
                      pos, slider.findColour (juce::Slider::rotarySliderFillColourId), bipolar);
        }

        // Hardware push-button: metal face crossfading to a backlit LED key.
        // AnimatedButton drives "litAmt" (0..1); others fall back to toggle.
        // NOTE: the LED bloom is NOT drawn here - it would clip to the button's
        // bounds and read as a hard square. Editors draw hw::keyBloom behind
        // their lit keys instead.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool over, bool down) override
        {
            const auto led = b.findColour (juce::TextButton::buttonOnColourId);
            hw::button (g, b.getLocalBounds().toFloat(), hw::litAmount (b), led, over, down);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
        {
            const float lit = hw::litAmount (b);
            auto colour = b.findColour (juce::TextButton::textColourOffId)
                             .interpolatedWith (b.findColour (juce::TextButton::textColourOnId), lit);
            if (over && lit < 0.5f)
                colour = colour.brighter (0.4f);
            g.setColour (colour);
            const float fs = (float) b.getProperties().getWithDefault ("fontSize", 0.0f);
            g.setFont (juce::FontOptions (fs > 0.0f ? fs : juce::jmin (13.0f, (float) b.getHeight() * 0.5f)));
            g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred);
        }

        // Hardware toggle switch (.hw-toggle): recessed track + sliding metal
        // knob, lit in the button's tick colour; sentence-case engraved label.
        // The track sits INSET from the component edges so its glow has dark
        // metal to spread into - flush against the bounds it was clipped to a
        // hard square (July 10 QA).
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
        {
            const auto area = b.getLocalBounds();
            const float trackH = 18.0f, trackW = 38.0f, inset = 5.0f;
            const auto track = juce::Rectangle<float> (inset, (float) area.getHeight() * 0.5f - trackH * 0.5f,
                                                       trackW, trackH);
            hw::toggleSwitch (g, track, b.getToggleState() ? 1.0f : 0.0f,
                              b.findColour (juce::ToggleButton::tickColourId));
            auto ink = b.findColour (juce::ToggleButton::textColourId);
            if (over)
                ink = ink.brighter (0.3f);
            g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
            hw::engraved (g, b.getButtonText(),
                          area.withTrimmedLeft ((int) (inset + trackW) + 9),
                          juce::Justification::centredLeft, ink);
        }

        // Recessed metal combo box with a chevron.
        void drawComboBox (juce::Graphics& g, int width, int height, bool,
                           int, int, int, int, juce::ComboBox&) override
        {
            const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
            hw::insetWell (g, r, 4.0f);
            juce::Path chev;
            const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
            chev.startNewSubPath (cx - 4.0f, cy - 2.0f);
            chev.lineTo (cx, cy + 2.5f);
            chev.lineTo (cx + 4.0f, cy - 2.0f);
            g.setColour (juce::Colour (0xff7f8496));
            g.strokePath (chev, juce::PathStrokeType (1.6f));
        }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (14.5f); }

        // The tempo readout is a glass green BPM LCD (design). Every other
        // linear slider is the design MiniSlider.
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float pos, float, float,
                               juce::Slider::SliderStyle, juce::Slider& s) override
        {
            if (s.getComponentID() == "bpm")
            {
                const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
                hw::lcd (g, r, colors::green);

                // Design TempoBox: big tabular value, small BPM unit on the
                // same baseline, a green drag-position strip at the bottom.
                const auto ink = colors::green.brighter (0.35f).withAlpha (s.isEnabled() ? 1.0f : 0.55f);
                const auto full = s.getTextFromValue (s.getValue());
                const auto value = full.upToFirstOccurrenceOf (" ", false, false);
                const juce::Font valueFont { juce::Font (juce::FontOptions (16.0f)).boldened() };
                const juce::Font unitFont  { juce::Font (juce::FontOptions (9.5f)).boldened() };
                const float vw = juce::GlyphArrangement::getStringWidth (valueFont, value);
                const float uw = juce::GlyphArrangement::getStringWidth (unitFont, "BPM");
                const int x0 = (int) (r.getCentreX() - (vw + 4.0f + uw) * 0.5f);
                const auto textArea = r.toNearestInt().withTrimmedBottom (4);
                g.setFont (valueFont);
                hw::glowText (g, value, textArea.withX (x0).withWidth ((int) vw + 2),
                              juce::Justification::centredLeft, ink, 4);
                g.setFont (unitFont);
                g.setColour (ink.withMultipliedAlpha (0.8f));
                g.drawText ("BPM", textArea.withX (x0 + (int) vw + 4).withWidth ((int) uw + 2)
                                           .withTrimmedBottom (1),
                            juce::Justification::bottomLeft);

                const auto track = juce::Rectangle<float> (r.getX() + 5.0f, r.getBottom() - 5.0f,
                                                           r.getWidth() - 10.0f, 2.0f);
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRoundedRectangle (track, 1.0f);
                const float t = (float) ((s.getValue() - s.getMinimum())
                                         / juce::jmax (1.0, s.getMaximum() - s.getMinimum()));
                g.setColour (ink);
                g.fillRoundedRectangle (track.withWidth (juce::jmax (2.0f, track.getWidth() * t)), 1.0f);
                hw::lcdScanlines (g, r); // the barely-visible CRT lines over the glass
                return;
            }
            // MiniSlider: recessed track, accent fill with a soft glow, round
            // cap (JUCE_DRAWING_GUIDE section 5).
            const float cy = (float) y + (float) height * 0.5f;
            auto track = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) width, 6.0f);
            hw::insetWell (g, track, 3.0f);
            auto acc = s.findColour (juce::Slider::trackColourId);
            if (acc.getAlpha() < 40) acc = colors::green;
            const float fillEnd = juce::jlimit ((float) x, (float) x + (float) width, pos);
            g.setColour (acc.withAlpha (0.35f));
            g.fillRoundedRectangle (track.withRight (fillEnd).expanded (0.0f, 1.0f), 3.5f);
            g.setColour (acc);
            g.fillRoundedRectangle (track.withRight (fillEnd), 3.0f);
            // Round cap.
            juce::Rectangle<float> cap (15.0f, 15.0f);
            cap.setCentre (fillEnd, cy);
            juce::ColourGradient cg (juce::Colour (0xff4a4d55), cap.getX(), cap.getY(),
                                     juce::Colour (0xff131418), cap.getX(), cap.getBottom(), false);
            g.setGradientFill (cg);
            g.fillEllipse (cap);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawEllipse (cap.reduced (0.5f), 1.0f);
        }
    };
}

juce::LookAndFeel& hardwareLookAndFeel()
{
    static HardwareLookAndFeel lnf; // process lifetime: dialogs may outlive editors
    return lnf;
}

//==============================================================================
namespace
{
    // The About shell both products share: wordmark over a subtle gradient,
    // the family divider, scrolling text below.
    struct AboutComponent : juce::Component
    {
        AboutComponent (const juce::String& body, float fontSize, int width, int height)
        {
            logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

            text.setMultiLine (true);
            text.setReadOnly (true);
            text.setCaretVisible (false);
            text.setScrollbarsShown (true);
            text.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            text.setColour (juce::TextEditor::textColourId, colors::text);
            text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            text.setFont (juce::FontOptions (fontSize));
            text.setText (body, juce::dontSendNotification);
            addAndMakeVisible (text);
            setSize (width, height);
        }

        void paint (juce::Graphics& g) override
        {
            g.setGradientFill (juce::ColourGradient (colors::panel.brighter (0.08f), 0.0f, 0.0f,
                                                     colors::background, 0.0f, (float) getHeight(), false));
            g.fillRect (getLocalBounds());

            if (logo.isValid())
                g.drawImage (logo, juce::Rectangle<float> ((float) getWidth() / 2.0f - 62.0f, 16.0f, 124.0f, 48.0f),
                             juce::RectanglePlacement::centred);

            // A whisper of the scale colors under the wordmark
            g.setGradientFill (juce::ColourGradient (colors::purple.withAlpha (0.5f), 40.0f, 0.0f,
                                                     colors::cyan.withAlpha (0.5f), (float) getWidth() - 40.0f, 0.0f, false));
            g.fillRect (40, 74, getWidth() - 80, 1);
        }

        void resized() override
        {
            text.setBounds (getLocalBounds().withTrimmedTop (86).reduced (18, 10));
        }

        juce::Image logo;
        juce::TextEditor text;
    };
}

void showAboutDialog (const juce::String& title, const juce::String& body,
                      float fontSize, int width, int height)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new AboutComponent (body, fontSize, width, height));
    o.dialogTitle = title;
    o.dialogBackgroundColour = colors::panel;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

} // namespace ui
