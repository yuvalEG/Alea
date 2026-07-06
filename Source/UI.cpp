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
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (colors::control);
    g.fillRoundedRectangle (bounds, 5.0f);

    const float w = bounds.getWidth() / (float) options.size();
    for (int i = 0; i < options.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + w * (float) i, bounds.getY(), w, bounds.getHeight());
        if (i == value)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (seg.reduced (2.0f), 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
        }
        else
        {
            g.setColour (colors::secondary);
        }
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (options[i], seg, juce::Justification::centred);
    }

    g.setColour (colors::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
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

    const float w = bounds.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + w * (float) i, bounds.getY(), w, bounds.getHeight());
        if (i == value)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (seg.reduced (2.0f), 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
        }
        else
        {
            g.setColour (colors::secondary);
        }

        const auto plot = seg.withSizeKeepingCentre (juce::jmin (34.0f, w - 16.0f), seg.getHeight() - 12.0f);
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
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (colors::border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
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
    // pc -> white-key slot (C D E F G A B), or -1 for black keys
    constexpr int whiteSlot[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    // black pc -> the white slot it sits after
    constexpr int blackAfter[12] = { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };
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
}

juce::Rectangle<float> PianoKeyboard::whiteKeyBounds (int slot) const
{
    const float w = (float) getWidth() / 7.0f;
    return { w * (float) slot, 0.0f, w, (float) getHeight() };
}

juce::Rectangle<float> PianoKeyboard::blackKeyBounds (int pc) const
{
    const float w = (float) getWidth() / 7.0f;
    const float bw = w * 0.62f;
    return { w * (float) (blackAfter[pc] + 1) - bw * 0.5f, 0.0f, bw, (float) getHeight() * 0.6f };
}

void PianoKeyboard::paint (juce::Graphics& g)
{
    const int active = alea.activeNote.load();
    const bool mine = active >= 0 && alea.activeSource.load() == sourceIndex;
    const int activePc = active >= 0 ? active % 12 : -1;

    for (int pc = 0; pc < 12; ++pc)
    {
        if (whiteSlot[pc] < 0)
            continue;
        const auto key = whiteKeyBounds (whiteSlot[pc]).reduced (1.0f);
        const bool isPlayingKey = mine && pc == activePc;
        g.setColour (isPlayingKey ? colors::playing
                                  : selected[pc] ? accent
                                                 : accent.withAlpha (0.10f));
        g.fillRoundedRectangle (key, 3.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (key, 3.0f, 1.0f);

        g.setColour (selected[pc] || isPlayingKey ? juce::Colours::black.withAlpha (0.75f) : colors::secondary);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (params::pitchClassNames[pc],
                    key.withTop (key.getBottom() - 18.0f), juce::Justification::centred);
    }

    for (int pc = 0; pc < 12; ++pc)
    {
        if (blackAfter[pc] < 0)
            continue;
        const auto key = blackKeyBounds (pc);
        const bool isPlayingKey = mine && pc == activePc;
        g.setColour (isPlayingKey ? colors::playing
                                  : selected[pc] ? accent
                                                 : juce::Colour (0xff08080c));
        g.fillRoundedRectangle (key, 3.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (key, 3.0f, 1.0f);
    }
}

void PianoKeyboard::mouseDown (const juce::MouseEvent& e)
{
    auto toggle = [this] (int pc)
    {
        attachments[(size_t) pc]->setValueAsCompleteGesture (selected[pc] ? 0.0f : 1.0f);
    };

    for (int pc = 0; pc < 12; ++pc)
        if (blackAfter[pc] >= 0 && blackKeyBounds (pc).contains (e.position))
            return toggle (pc);

    for (int pc = 0; pc < 12; ++pc)
        if (whiteSlot[pc] >= 0 && whiteKeyBounds (whiteSlot[pc]).contains (e.position))
            return toggle (pc);
}

//==============================================================================
RestSelector::RestSelector (AleaAudioProcessor& p, char scaleId, int sourceIdx, juce::Colour accentColour)
    : alea (p), sourceIndex (sourceIdx), accent (accentColour)
{
    for (int r = 0; r < 5; ++r)
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

    const float w = (float) getWidth() / 5.0f;
    for (int r = 0; r < 5; ++r)
    {
        const bool resting = mine && r == active;
        const auto cell = juce::Rectangle<float> (w * (float) r, 0.0f, w, (float) getHeight()).reduced (2.0f);
        g.setColour (resting ? colors::playing : selected[r] ? accent : colors::control);
        g.fillRoundedRectangle (cell, 4.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (cell, 4.0f, 1.0f);
        g.setColour (selected[r] || resting ? juce::Colours::black.withAlpha (0.75f) : colors::secondary);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (params::restNames[r], cell, juce::Justification::centred);
    }
}

void RestSelector::mouseDown (const juce::MouseEvent& e)
{
    const int r = juce::jlimit (0, 4, (int) (e.position.x / ((float) getWidth() / 5.0f)));
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
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
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
OutputPanel::OutputPanel (AleaAudioProcessor& p) : alea (p) {}

void OutputPanel::paint (juce::Graphics& g)
{
    const int active = alea.activeNote.load();
    const int rest   = alea.activeRest.load();
    const int last   = alea.lastNote.load();
    const double ppq = alea.hostPpq.load();
    const bool playing = alea.hostIsPlaying.load();

    auto area = getLocalBounds();

    // Note displays are colored by the scale the note came from, matching
    // the history ticker.
    const auto srcColour = alea.activeSource.load() == 1 ? colors::cyan : colors::purple;

    // Activity LED + big note display (shows the sounding rest, too)
    auto noteRow = area.removeFromTop (56);
    g.setColour (active >= 0 ? colors::playing : colors::control);
    g.fillEllipse (noteRow.removeFromLeft (26).withSizeKeepingCentre (14, 14).toFloat());

    if (active >= 0)
    {
        g.setColour (srcColour);
        g.setFont (juce::FontOptions (36.0f, juce::Font::bold));
        g.drawText (noteName (active), noteRow, juce::Justification::centredLeft);
    }
    else if (rest >= 0)
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (20.0f, juce::Font::italic));
        g.drawText ("rest " + params::restNames[rest], noteRow, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (36.0f, juce::Font::bold));
        g.drawText (noteName (last), noteRow, juce::Justification::centredLeft);
    }

    // Bar / beat / morph source
    const int bar  = juce::jmax (1, (int) std::floor (ppq / 4.0) + 1);
    const int beat = juce::jmax (1, ((int) std::floor (ppq) % 4 + 4) % 4 + 1);

    auto row = area.removeFromTop (22);
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (colors::secondary);
    g.drawText ("BAR " + juce::String (playing ? bar : 1)
                + "  BEAT " + juce::String (playing ? beat : 1), row, juce::Justification::centredLeft);

    // 88-key monitor (A0-C8): the sounding note lights in its source
    // scale's color. Rests deliberately don't show here - silence has no
    // key; the LED going dark and the history ticker cover it.
    {
        const auto strip = juce::Rectangle<float> (0.0f, (float) getHeight() - 124.0f, (float) getWidth(), 40.0f);
        const float ww = strip.getWidth() / 52.0f;
        auto isBlack = [] (int note) { const int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };

        int whiteIndex = 0;
        for (int note = 21; note <= 108; ++note)
        {
            if (isBlack (note))
                continue;
            const auto key = juce::Rectangle<float> (strip.getX() + ww * (float) whiteIndex, strip.getY(), ww, strip.getHeight());
            g.setColour (note == active ? srcColour : colors::text.withAlpha (0.14f));
            g.fillRect (key.reduced (0.5f, 0.0f));
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
    }

    // History: a single readable ticker, newest event entering on the right,
    // sequence reading left-to-right, colored by source scale. Rests appear
    // as their duration in parentheses - they're events too.
    auto historyLabelRow = juce::Rectangle<int> (0, getHeight() - 76, getWidth(), 18);
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("HISTORY", historyLabelRow, juce::Justification::centredLeft);

    const auto ticker = juce::Rectangle<int> (0, getHeight() - 54, getWidth(), 26).toFloat();
    const juce::Font tickerFont { juce::FontOptions (16.0f, juce::Font::bold) };
    g.setFont (tickerFont);

    const int total = alea.historyCount.load();
    float xRight = ticker.getRight();
    for (int i = total - 1; i >= juce::jmax (0, total - 50) && xRight > ticker.getX(); --i)
    {
        const int packed = alea.history[(size_t) (i % AleaAudioProcessor::historyCapacity)].load();
        const bool isRest = (packed & 0x200) != 0;
        const auto name = isRest ? "(" + params::restNames[packed & 0xff] + ")"
                                 : noteName (packed & 0xff);
        const float w = juce::GlyphArrangement::getStringWidth (tickerFont, name);
        xRight -= w + 10.0f;
        if (xRight < ticker.getX())
            break;
        const float age = (float) (total - 1 - i);
        const float alpha = juce::jmax (0.35f, 1.0f - age * 0.06f) * (isRest ? 0.55f : 1.0f);
        g.setColour (((packed >> 8) & 1 ? colors::cyan : colors::purple).withAlpha (alpha));
        g.drawText (name, juce::Rectangle<float> (xRight, ticker.getY(), w + 2.0f, ticker.getHeight()),
                    juce::Justification::centredLeft);
    }
}

} // namespace ui
