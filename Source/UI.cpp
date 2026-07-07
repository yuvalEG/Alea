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
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.10f), seg.getX(), seg.getY(),
                                                     accent.darker (0.15f), seg.getX(), seg.getBottom(), false));
            g.fillRoundedRectangle (seg.reduced (2.0f), 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
        }
        else
        {
            g.setColour (colors::secondary);
        }
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
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
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.10f), seg.getX(), seg.getY(),
                                                     accent.darker (0.15f), seg.getX(), seg.getBottom(), false));
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

    // Slot i holds interval i from the root: the shape stays put when the
    // root changes; the letters (and black/white identity) move instead.
    int whiteSlotCounter = 0;
    for (int i = 0; i < 12; ++i)
    {
        const int pc = (root + i) % 12;
        if (blackPc[pc])
            continue;
        const auto key = whiteKeyBounds (whiteSlotCounter++).reduced (1.0f);
        const bool isPlayingKey = mine && i == activePc;
        const auto fill = selected[i] ? accent : accent.withAlpha (0.10f);
        if (selected[i]) // top-lit gradient on lit keys
            g.setGradientFill (juce::ColourGradient (fill.brighter (0.12f), key.getX(), key.getY(),
                                                     fill.darker (0.18f), key.getX(), key.getBottom(), false));
        else
            g.setColour (fill);
        g.fillRoundedRectangle (key, 3.0f);
        if (isPlayingKey)
            velocityFill (key);
        g.setColour (colors::border);
        g.drawRoundedRectangle (key, 3.0f, 1.0f);

        g.setColour (selected[i] || isPlayingKey ? juce::Colours::black.withAlpha (0.75f) : colors::secondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (params::pitchClassNames[pc],
                    key.withTop (key.getBottom() - 18.0f), juce::Justification::centred);
    }

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
        const auto fill = selected[i] ? accent : juce::Colour (0xff08080c);
        if (selected[i])
            g.setGradientFill (juce::ColourGradient (fill.brighter (0.12f), key.getX(), key.getY(),
                                                     fill.darker (0.18f), key.getX(), key.getBottom(), false));
        else
            g.setColour (fill);
        g.fillRoundedRectangle (key, 3.0f);
        if (isPlayingKey)
            velocityFill (key);
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
    const float ix = 17.0f;
    if (on) // pause bars: the clock holds, it does not reset
    {
        g.fillRoundedRectangle (ix - 6.5f, cy - 6.0f, 4.0f, 12.0f, 1.0f);
        g.fillRoundedRectangle (ix + 0.5f, cy - 6.0f, 4.0f, 12.0f, 1.0f);
    }
    else
    {
        juce::Path p;
        p.addTriangle (ix - 5.0f, cy - 6.5f, ix - 5.0f, cy + 6.5f, ix + 6.5f, cy);
        g.fillPath (p);
    }
    g.setFont (juce::FontOptions (14.0f));
    g.drawText (on ? "PAUSE" : "PLAY", (int) ix + 12, 0, getWidth() - (int) ix - 16, getHeight(),
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

    // Synth flavours share ids 1-4 in both modes; the mapping to the
    // engine's output-choice string is one table.
    static const std::array<const char*, 4> synthChoices { "synth", "synth:sine", "synth:saw", "synth:strings" };
    static const std::array<const char*, 4> synthNames { "Synth: Classic", "Synth: Pure Sine", "Synth: Soft Saw", "Synth: Strings" };

    if (! standalone)
        outputBox->addItem ("MIDI to DAW", 9); // plugin default, listed first

    for (int i = 0; i < 4; ++i)
        outputBox->addItem (synthNames[(size_t) i], i + 1);

    if (standalone)
    {
        // Standalone: the synth flavours (Warm Pad default) or any MIDI device.
        devices = juce::MidiOutput::getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
            outputBox->addItem ("MIDI: " + devices[i].name, 10 + i);

        outputBox->setSelectedId (1, juce::dontSendNotification);
        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].identifier == current)
                outputBox->setSelectedId (10 + i, juce::dontSendNotification);

        outputBox->onChange = [this]
        {
            const int id = outputBox->getSelectedId();
            alea.setStandaloneOutput (id <= 4 ? synthChoices[(size_t) (id - 1)]
                                              : devices[id - 10].identifier);
        };
    }
    else
    {
        // Plugin: pure MIDI by default; the synth makes hosts that can't
        // route plugin MIDI (AU in Live/Logic) hear Alea directly.
        outputBox->setSelectedId (9, juce::dontSendNotification);
        outputBox->onChange = [this]
        {
            const int id = outputBox->getSelectedId();
            alea.setStandaloneOutput (id <= 4 ? synthChoices[(size_t) (id - 1)] : "");
        };
    }

    for (int i = 0; i < 4; ++i)
        if (current == synthChoices[(size_t) i])
            outputBox->setSelectedId (i + 1, juce::dontSendNotification);

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

    // Global transpose: an output transform, so it lives in OUTPUT.
    transposeSlider.setSliderStyle (juce::Slider::LinearBar);
    transposeSlider.setColour (juce::Slider::trackColourId, colors::text.withAlpha (0.35f));
    transposeSlider.setColour (juce::Slider::backgroundColourId, colors::control);
    transposeSlider.setColour (juce::Slider::textBoxTextColourId, colors::text);
    transposeSlider.setColour (juce::Slider::textBoxOutlineColourId, colors::border);
    addAndMakeVisible (transposeSlider);
    transposeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        alea.apvts, "transpose", transposeSlider);
}

void OutputPanel::resized()
{
    // The output chooser is the panel's top row - it decides whether Alea
    // makes sound at all, so it hides nowhere. With the synth active it
    // shares the top-right corner with a volume knob (left) and a vertical
    // meter (right). TRANSPOSE gets the second row.
    if (outputBox == nullptr)
        return;
    const bool synth = alea.synthOn.load();
    outputBox->setBounds (0, 0, synth ? getWidth() - 92 : getWidth(), 26);
    volSlider.setBounds (getWidth() - 82, 2, 44, 44);
    volSlider.setVisible (synth);
    transposeSlider.setBounds (0, 54, getWidth(), 20);
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

    // Output meter (internal synth): vertical falling peak with green /
    // amber / red zones - amber warns, red means the limiter is working.
    if (lastSynthOn && outputBox != nullptr)
    {
        meterLevel = juce::jmax (alea.synthPeak.load(), meterLevel * 0.82f);
        const auto meter = juce::Rectangle<float> ((float) getWidth() - 30.0f, 2.0f, 8.0f, 44.0f);
        g.setColour (colors::control);
        g.fillRoundedRectangle (meter, 3.0f);

        const float db = juce::Decibels::gainToDecibels (meterLevel, -48.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        struct Zone { float from, to; juce::Colour colour; };
        for (const auto& z : { Zone { 0.00f, 0.70f, colors::green },
                               Zone { 0.70f, 0.90f, colors::amber },
                               Zone { 0.90f, 1.00f, colors::red } })
        {
            const float lit = juce::jmin (frac, z.to) - z.from;
            if (lit <= 0.0f)
                continue;
            g.setColour (z.colour);
            g.fillRect (juce::Rectangle<float> (meter.getX(),
                                                meter.getBottom() - (z.from + lit) * meter.getHeight(),
                                                meter.getWidth(), lit * meter.getHeight()));
        }
    }

    auto area = getLocalBounds();
    if (outputBox != nullptr)
    {
        area.removeFromTop (78); // chooser row + transpose line
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("TRANSPOSE", 0, 36, 100, 14, juce::Justification::centredLeft);
    }

    // Note displays are colored by the scale the note came from, matching
    // the history ticker.
    const auto srcColour = alea.activeSource.load() == 1 ? colors::cyan : colors::purple;

    // Activity LED + big note display (shows the sounding rest, too)
    auto noteRow = area.removeFromTop (outputBox != nullptr ? 30 : 56);
    g.setColour (active >= 0 ? colors::playing : colors::control);
    g.fillEllipse (noteRow.removeFromLeft (26).withSizeKeepingCentre (14, 14).toFloat());

    const float bigNote = outputBox != nullptr ? 26.0f : 36.0f;
    if (active >= 0)
    {
        g.setColour (srcColour);
        g.setFont (juce::FontOptions (bigNote, juce::Font::bold));
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
        g.setFont (juce::FontOptions (bigNote, juce::Font::bold));
        g.drawText (noteName (last), noteRow, juce::Justification::centredLeft);
    }

    // Bar / beat, with a persistent velocity readout (white, no blink) at
    // the row's right.
    const int bar  = juce::jmax (1, (int) std::floor (ppq / 4.0) + 1);
    const int beat = juce::jmax (1, ((int) std::floor (ppq) % 4 + 4) % 4 + 1);

    auto row = area.removeFromTop (20);
    g.setFont (juce::FontOptions (14.0f));
    g.setColour (colors::secondary);
    g.drawText ("BAR " + juce::String (playing ? bar : 1)
                + "  BEAT " + juce::String (playing ? beat : 1), row, juce::Justification::centredLeft);


    // At small window sizes the bottom-anchored monitors would collide with
    // the rows above - drop them; the essentials stay.
    if (getHeight() < 250)
        return;

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
    auto historyLabelRow = juce::Rectangle<int> (0, getHeight() - 76, getWidth(), 18);
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("HISTORY", historyLabelRow, juce::Justification::centredLeft);

    const auto ticker = juce::Rectangle<int> (0, getHeight() - 54, getWidth(), 26).toFloat();

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
