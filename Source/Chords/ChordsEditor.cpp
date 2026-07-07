#include "ChordsEditor.h"
#include "BinaryData.h"

namespace
{
    // Spec palette - identical to Scale Shifter's (spec section 7.1).
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
        const juce::Colour green      { 0xff10b981 };
        const juce::Colour playing    { 0xff22c55e };
        const juce::Colour amber      { 0xfff59e0b };
        const juce::Colour red        { 0xffef4444 };
    }

    float textWidth (const juce::Font& font, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    // About dialog: wordmark over a subtle vertical gradient, text below
    // (same construction as Scale Shifter's).
    struct AboutComponent : juce::Component
    {
        AboutComponent()
        {
            logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

            text.setMultiLine (true);
            text.setReadOnly (true);
            text.setCaretVisible (false);
            text.setScrollbarsShown (true);
            text.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            text.setColour (juce::TextEditor::textColourId, colors::text);
            text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            text.setFont (juce::FontOptions (20.5f));
            text.setText (juce::String::fromUTF8 (
                "Alea Chord Randomizer - Version " CHORDS_VERSION "\n\n\n"
                "HOW TO USE\n\n"
                "Press ROLL (or R / Enter) and get a random series of chords. "
                "Press PLAY (or the spacebar) and the loop plays them - each "
                "chord held for its bars at your tempo - while you improvise "
                "over it.\n\n"
                "CHORDS sets how many chords each roll gives you - a series of "
                "1 behaves like a classic flash card. BARS sets how long each "
                "chord holds, OCTAVE where the voicing sits. Rolling while "
                "the loop plays swaps in the new chords at the next chord "
                "change, and clicking any chord card jumps the loop there.\n\n"
                "Use Seventh Chords adds sevenths to the mix. Simplify Chords "
                "narrows the roll to guitar-friendly keys and mostly major or "
                "minor chords.\n\n"
                "FREEZE holds the sounding chord until you let go; PANIC "
                "silences everything instantly.\n\n"
                "OUT plays through the built-in synth (pick a flavour) or "
                "sends MIDI to any device on your system. Your past rolls "
                "pile up in HISTORY at the bottom - scroll through them, "
                "click one to bring it back.\n\n\n"
                "THE EXERCISE\n\n"
                "This app was born from an improvisation exercise by my guitar "
                "teacher, Yonatan Benaroche: generate a short random chord "
                "progression, loop it, and improvise over it with your "
                "instrument. A progression you did not choose forces your ear "
                "and your hands out of familiar shapes.\n\n\n"
                "GET IN TOUCH\n\n"
                "I'll be more than happy to hear your feedback, ideas and music! "
                "You can reach me through GitHub (github.com/yuvalEG/Alea) or my "
                "email: yuvalprod@gmail.com\n\n"
                "Alea Chord Randomizer is open source (GPLv3), built with JUCE. "
                "Check for updates from the menu in the top-right corner.\n\n\n"
                "Made By Yuval Egozi"),
                juce::dontSendNotification);
            addAndMakeVisible (text);
            setSize (720, 640);
        }

        void paint (juce::Graphics& g) override
        {
            g.setGradientFill (juce::ColourGradient (colors::panel.brighter (0.08f), 0.0f, 0.0f,
                                                     colors::background, 0.0f, (float) getHeight(), false));
            g.fillRect (getLocalBounds());

            if (logo.isValid())
                g.drawImage (logo, juce::Rectangle<float> ((float) getWidth() / 2.0f - 62.0f, 16.0f, 124.0f, 48.0f),
                             juce::RectanglePlacement::centred);

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

//==============================================================================
void ChordsEditor::ChordCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colors::panel);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (active ? colors::playing.withAlpha (0.85f) : colors::border);
    g.drawRoundedRectangle (b.reduced (active ? 1.0f : 0.5f), 10.0f, active ? 2.0f : 1.0f);

    // The sounding chord fills a thin progress strip along its bottom edge.
    if (active && progress > 0.0f)
    {
        g.setColour (colors::playing.withAlpha (0.45f));
        g.fillRoundedRectangle (b.getX() + 8.0f, b.getBottom() - 7.0f,
                                (b.getWidth() - 16.0f) * juce::jlimit (0.0f, 1.0f, progress), 3.0f, 1.5f);
    }

    g.setColour (colors::text);
    g.setFont (juce::FontOptions (fontSize));
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

void ChordsEditor::ChordCard::mouseUp (const juce::MouseEvent& e)
{
    if (clickable && ! e.mouseWasDraggedSinceMouseDown()
        && getLocalBounds().contains (e.getPosition()) && onPress != nullptr)
        onPress();
}

//==============================================================================
void ChordsEditor::MonitorStrip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    bool lit[128] = {};
    const auto packed = proc.soundingPacked.load();
    for (int i = 0; i < 8; ++i)
        if (const int byte = (int) ((packed >> (8 * i)) & 0xFF); byte > 0)
            lit[juce::jlimit (0, 127, byte - 1)] = true;

    auto isBlack = [] (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; };

    // A0 (21) to C8 (108): 52 white keys, blacks drawn over the gaps.
    const float ww = b.getWidth() / 52.0f;
    int white = 0;
    for (int n = 21; n <= 108; ++n)
    {
        if (isBlack (n)) continue;
        g.setColour (lit[n] ? colors::playing : colors::control.brighter (0.22f));
        g.fillRect (juce::Rectangle<float> (b.getX() + (float) white * ww, b.getY(), ww - 1.0f, b.getHeight()));
        ++white;
    }
    white = 0;
    for (int n = 21; n <= 108; ++n)
    {
        if (! isBlack (n)) { ++white; continue; }
        const float bw = ww * 0.62f;
        g.setColour (lit[n] ? colors::playing : juce::Colour (0xff08080c));
        g.fillRect (juce::Rectangle<float> (b.getX() + (float) white * ww - bw / 2.0f, b.getY(), bw, b.getHeight() * 0.62f));
    }
}

//==============================================================================
void ChordsEditor::SegmentRow::paint (juce::Graphics& g)
{
    const int n = juce::jmax (1, labels.size());
    const float segW = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (segW * (float) i, 0.0f, segW, (float) getHeight()).reduced (2.0f, 0.0f);
        const bool isSelected = (i == selected);

        g.setColour (isSelected ? colors::text.withAlpha (0.92f) : colors::control);
        g.fillRoundedRectangle (seg, 6.0f);
        if (! isSelected)
        {
            g.setColour (colors::border);
            g.drawRoundedRectangle (seg.reduced (0.5f), 6.0f, 1.0f);
        }

        g.setColour (isSelected ? juce::Colours::black : colors::secondary);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (labels[i], seg, juce::Justification::centred);
    }
}

void ChordsEditor::SegmentRow::mouseDown (const juce::MouseEvent& e)
{
    const int n = juce::jmax (1, labels.size());
    const int idx = juce::jlimit (0, n - 1, (int) ((float) e.x / ((float) getWidth() / (float) n)));
    if (idx != selected && onChange != nullptr)
        onChange (idx);
}

//==============================================================================
void ChordsEditor::HistoryTicker::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colors::panel);
    g.fillRoundedRectangle (b, 8.0f);

    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("HISTORY", 12, 6, 120, 12, juce::Justification::centredLeft);

    const auto area = getLocalBounds().withTrimmedTop (18).reduced (12, 4).toFloat();

    if (proc.history.empty())
    {
        g.setFont (juce::FontOptions (13.0f, juce::Font::italic));
        g.drawText ("your past rolls will appear here", area, juce::Justification::centredLeft);
        return;
    }

    const juce::Font font { juce::FontOptions (17.0f) };
    constexpr float chordGap = 10.0f, groupGap = 24.0f;

    // Measure every group once: total content width bounds the scroll range.
    std::vector<float> groupWidths;
    groupWidths.reserve (proc.history.size());
    float contentW = 0.0f;
    for (const auto& roll : proc.history)
    {
        float groupW = 0.0f;
        for (const auto& c : roll)
            groupW += textWidth (font, c.text()) + chordGap;
        groupW -= chordGap;
        groupWidths.push_back (groupW);
        contentW += groupW + groupGap;
    }
    contentW -= groupGap;

    maxScroll = juce::jmax (0.0f, contentW - area.getWidth());
    scroll = juce::jlimit (0.0f, maxScroll, scroll);

    // Newest roll sits at the right edge; scrolling shifts the view into the
    // past. Older rolls march left and fade. Hovered rolls highlight -
    // clicking one recalls it into the series row.
    groupRects.clear();
    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    float rightX = area.getRight() + scroll;
    int age = 0;

    for (const auto& roll : proc.history)
    {
        const float groupW = groupWidths[(size_t) age];
        const float startX = rightX - groupW;

        if (startX > area.getRight())      // still off-screen to the right
        {
            rightX = startX - groupGap;
            ++age;
            continue;
        }
        if (rightX < area.getX())
            break;                          // everything older is off-screen left

        const auto groupRect = juce::Rectangle<float> (startX - 8.0f, area.getY(),
                                                       groupW + 16.0f, area.getHeight());
        groupRects.push_back ({ groupRect, age });

        const float alpha = juce::jmax (0.25f, 1.0f - 0.15f * (float) age);

        if (age == hoveredGroup)
        {
            g.setColour (colors::control.brighter (0.06f));
            g.fillRoundedRectangle (groupRect, 6.0f);
        }

        // A hair of a divider between this roll and the newer one to its right.
        if (age > 0)
        {
            g.setColour (colors::border.withAlpha (alpha + 0.15f));
            g.fillRect (juce::Rectangle<float> (rightX + groupGap / 2.0f, area.getY() + 4.0f,
                                                1.0f, area.getHeight() - 8.0f));
        }

        g.setColour (colors::text.withAlpha (age == hoveredGroup ? 1.0f
                                                                 : alpha * (age == 0 ? 1.0f : 0.85f)));
        g.setFont (font);

        float x = startX;
        for (const auto& c : roll)
        {
            const auto t = c.text();
            const float w = textWidth (font, t);
            g.drawText (t, juce::Rectangle<float> (x, area.getY(), w + 2.0f, area.getHeight()),
                        juce::Justification::centredLeft);
            x += w + chordGap;
        }

        rightX = startX - groupGap;
        ++age;
    }

    g.restoreState();

    // Edge page buttons, shown when there is more history in that direction.
    auto drawPager = [&] (juce::Rectangle<float>& store, float x, const char* glyph)
    {
        store = { x, (float) getHeight() / 2.0f - 14.0f, 18.0f, 28.0f };
        g.setColour (colors::control);
        g.fillRoundedRectangle (store, 5.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (store.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (juce::String::fromUTF8 (glyph), store, juce::Justification::centred);
    };

    olderButton = newerButton = {};
    if (scroll < maxScroll)
        drawPager (olderButton, 4.0f, "\xe2\x97\x82");
    if (scroll > 0.0f)
        drawPager (newerButton, (float) getWidth() - 22.0f, "\xe2\x96\xb8");
}

int ChordsEditor::HistoryTicker::groupAt (juce::Point<float> pos) const
{
    for (const auto& [rect, index] : groupRects)
        if (rect.contains (pos))
            return index;
    return -1;
}

void ChordsEditor::HistoryTicker::scrollBy (float delta)
{
    scroll = juce::jlimit (0.0f, maxScroll, scroll + delta);
    repaint();
}

void ChordsEditor::HistoryTicker::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    // Wheel down / trackpad swipe left = further into the past.
    scrollBy (-(wheel.deltaX + wheel.deltaY) * 220.0f);
}

void ChordsEditor::HistoryTicker::mouseDown (const juce::MouseEvent& e)
{
    dragStartScroll = scroll;
    dragStartX = e.x;
}

void ChordsEditor::HistoryTicker::mouseDrag (const juce::MouseEvent& e)
{
    // Dragging right moves the content right, revealing older rolls.
    scroll = juce::jlimit (0.0f, maxScroll, dragStartScroll + (float) (e.x - dragStartX));
    repaint();
}

void ChordsEditor::HistoryTicker::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
        return;

    const auto pos = e.position;
    if (olderButton.contains (pos))  { scrollBy ((float) getWidth() * 0.8f);  return; }
    if (newerButton.contains (pos))  { scrollBy ((float) getWidth() * -0.8f); return; }

    if (const int group = groupAt (pos); group >= 0 && onRecall != nullptr)
        onRecall (group);
}

void ChordsEditor::HistoryTicker::mouseMove (const juce::MouseEvent& e)
{
    const bool overButton = olderButton.contains (e.position) || newerButton.contains (e.position);
    const int group = overButton ? -1 : groupAt (e.position);
    setMouseCursor ((group >= 0 || overButton) ? juce::MouseCursor::PointingHandCursor
                                               : juce::MouseCursor::NormalCursor);
    if (group != hoveredGroup)
    {
        hoveredGroup = group;
        repaint();
    }
}

void ChordsEditor::HistoryTicker::mouseExit (const juce::MouseEvent&)
{
    if (hoveredGroup != -1)
    {
        hoveredGroup = -1;
        repaint();
    }
}

//==============================================================================
ChordsEditor::ChordsEditor (ChordsProcessor& p)
    : AudioProcessorEditor (p), chordsProc (p), ticker (p), monitor (p)
{
    // Space Grotesk everywhere - the family typeface (spec section 7.1).
    struct ChordsLookAndFeel : juce::LookAndFeel_V4
    {
        ChordsLookAndFeel()
        {
            setDefaultSansSerifTypeface (juce::Typeface::createSystemTypefaceFor (
                BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize));
            setColour (juce::ToggleButton::textColourId, colors::text);
            setColour (juce::ToggleButton::tickColourId, colors::text);
            setColour (juce::ToggleButton::tickDisabledColourId, colors::border);
        }
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        { return juce::FontOptions (juce::jmin (16.5f, (float) buttonHeight * 0.62f)); }
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (16.0f); }
        juce::Font getPopupMenuFont() override { return juce::FontOptions (16.0f); }
        juce::Font getLabelFont (juce::Label& label) override
        { return juce::FontOptions (juce::jmin (15.5f, (float) label.getHeight() * 0.72f)); }

        // Sleek knob, as in Scale Shifter: small filled body, hairline value
        // arc, thin pointer - no hollow ring.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
            const auto centre = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);

            g.setColour (colors::control.brighter (0.10f));
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (colors::border);
            g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius + 2.5f, radius + 2.5f, 0.0f, startAngle, angle, true);
            g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour (colors::text.withAlpha (0.9f));
            g.drawLine (centre.x + std::sin (angle) * radius * 0.45f, centre.y - std::cos (angle) * radius * 0.45f,
                        centre.x + std::sin (angle) * radius * 0.88f, centre.y - std::cos (angle) * radius * 0.88f, 1.8f);
        }
    };
    static ChordsLookAndFeel chordsLnf; // process lifetime: dialogs may outlive the editor
    juce::LookAndFeel::setDefaultLookAndFeel (&chordsLnf);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    for (int i = 0; i < 8; ++i)
    {
        auto* card = cards.add (new ChordCard());
        card->onPress = [this, i] { chordsProc.jumpRequest.store (i); };
        addChildComponent (card);
    }

    rollButton.setColour (juce::TextButton::buttonColourId, colors::control.brighter (0.06f));
    rollButton.setColour (juce::TextButton::textColourOffId, colors::text);
    rollButton.onClick = [this] { doRoll(); };
    addAndMakeVisible (rollButton);

    // Transport: the loop is the product (spec M2). Space toggles it.
    playButton.setClickingTogglesState (true);
    playButton.setColour (juce::TextButton::buttonColourId, colors::control);
    playButton.setColour (juce::TextButton::buttonOnColourId, colors::green);
    playButton.setColour (juce::TextButton::textColourOffId, colors::text);
    playButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    playButton.onClick = [this]
    {
        const bool on = playButton.getToggleState();
        playButton.setButtonText (on ? "STOP" : "PLAY");
        chordsProc.playing.store (on);
    };
    addAndMakeVisible (playButton);

    // Tempo: a draggable bar, exactly like Scale Shifter's tempo box.
    tempoBox.setSliderStyle (juce::Slider::LinearBar);
    tempoBox.setRange (30.0, 300.0, 1.0);
    tempoBox.setColour (juce::Slider::trackColourId, colors::green.withAlpha (0.55f));
    tempoBox.setColour (juce::Slider::backgroundColourId, colors::control);
    tempoBox.setColour (juce::Slider::textBoxTextColourId, colors::text);
    tempoBox.setColour (juce::Slider::textBoxOutlineColourId, colors::border);
    tempoBox.setTextValueSuffix (" BPM");
    tempoBox.setValue ((double) chordsProc.bpm.load(), juce::dontSendNotification);
    tempoBox.onValueChange = [this] { chordsProc.bpm.store ((float) tempoBox.getValue()); };
    addAndMakeVisible (tempoBox);

    // FREEZE holds the sounding chord (time stops); PANIC is instant silence.
    // Opposite jobs, opposite ends of the header - the family rule.
    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonColourId, colors::control);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, colors::cyan);
    freezeButton.setColour (juce::TextButton::textColourOffId, colors::text);
    freezeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    freezeButton.onClick = [this] { chordsProc.frozen.store (freezeButton.getToggleState()); };
    addAndMakeVisible (freezeButton);

    panicButton.setColour (juce::TextButton::buttonColourId, colors::control);
    panicButton.setColour (juce::TextButton::textColourOffId, colors::red);
    panicButton.onClick = [this] { chordsProc.panicRequest.store (true); };
    addAndMakeVisible (panicButton);

    lengthRow.labels = { "1", "2", "3", "4", "5", "6", "7", "8" };
    lengthRow.onChange = [this] (int idx)
    {
        chordsProc.setSeriesLength (idx + 1);
        refresh();
    };
    addAndMakeVisible (lengthRow);

    // Bars per chord: 1, 2 or 4 bars of 4/4 before the loop moves on.
    barsRow.labels = { "1", "2", "4" };
    barsRow.onChange = [this] (int idx)
    {
        chordsProc.barsPerChord.store (idx == 0 ? 1 : idx == 1 ? 2 : 4);
        barsRow.selected = idx;
        barsRow.repaint();
    };
    addAndMakeVisible (barsRow);

    // Voicing octave: a position, not a range - where the roots sit.
    octaveRow.labels = { "2", "3", "4" };
    octaveRow.onChange = [this] (int idx)
    {
        chordsProc.octave.store (idx + 2);
        chordsProc.updateLoop();
        octaveRow.selected = idx;
        octaveRow.repaint();
    };
    addAndMakeVisible (octaveRow);

    addAndMakeVisible (monitor);

    // OUT chooser + volume knob, straight from the Scale Shifter playbook.
    outputBox.setColour (juce::ComboBox::backgroundColourId, colors::control);
    outputBox.setColour (juce::ComboBox::textColourId, colors::text);
    outputBox.setColour (juce::ComboBox::outlineColourId, colors::border);
    outputBox.setColour (juce::ComboBox::arrowColourId, colors::secondary);
    buildOutputBox();
    addAndMakeVisible (outputBox);

    volKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volKnob.setPopupDisplayEnabled (true, true, this);
    volKnob.setColour (juce::Slider::rotarySliderFillColourId, colors::green.withAlpha (0.85f));
    volKnob.setColour (juce::Slider::thumbColourId, colors::text);
    volKnob.setRange (-24.0, 6.0, 0.1);
    volKnob.setDoubleClickReturnValue (true, 0.0);
    volKnob.setValue ((double) chordsProc.synthVolDb.load(), juce::dontSendNotification);
    volKnob.setTextValueSuffix (" dB");
    volKnob.onValueChange = [this] { chordsProc.synthVolDb.store ((float) volKnob.getValue()); };
    addAndMakeVisible (volKnob);

    seventhToggle.onClick = [this] { chordsProc.useSevenths = seventhToggle.getToggleState(); };
    simplifyToggle.onClick = [this] { chordsProc.simplify = simplifyToggle.getToggleState(); };
    addAndMakeVisible (seventhToggle);
    addAndMakeVisible (simplifyToggle);

    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    menuButton.setColour (juce::TextButton::buttonColourId, colors::control);
    menuButton.setColour (juce::TextButton::textColourOffId, colors::text);
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    ticker.onRecall = [this] (int index)
    {
        chordsProc.recallRoll (index);
        ticker.scroll = 0.0f; // the outgoing series is now the newest entry
        ticker.hoveredGroup = -1;
        refresh();
    };
    addAndMakeVisible (ticker);

    // Space must always mean PLAY/STOP (and R must roll): no child ever
    // takes keyboard focus.
    for (auto* c : { (juce::Component*) &rollButton, (juce::Component*) &menuButton,
                     (juce::Component*) &seventhToggle, (juce::Component*) &simplifyToggle,
                     (juce::Component*) &lengthRow, (juce::Component*) &barsRow,
                     (juce::Component*) &octaveRow, (juce::Component*) &monitor,
                     (juce::Component*) &freezeButton, (juce::Component*) &panicButton,
                     (juce::Component*) &playButton, (juce::Component*) &outputBox,
                     (juce::Component*) &volKnob, (juce::Component*) &tempoBox })
        c->setWantsKeyboardFocus (false);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (880, 480, 4096, 2400);
    setSize (juce::jmax (880, chordsProc.lastUIWidth), juce::jmax (480, chordsProc.lastUIHeight));

    refresh();
    startTimerHz (30); // fast enough for the bar-progress strip and meter
}

void ChordsEditor::buildOutputBox()
{
    // Synth flavours share ids 1-4; MIDI devices follow from id 10.
    static const std::array<const char*, 4> synthChoices { "synth", "synth:sine", "synth:saw", "synth:strings" };
    static const std::array<const char*, 4> synthNames { "Synth: Warm Pad", "Synth: Pure Sine", "Synth: Soft Saw", "Synth: Strings" };

    outputBox.clear (juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        outputBox.addItem (synthNames[(size_t) i], i + 1);

    devices = juce::MidiOutput::getAvailableDevices();
    for (int i = 0; i < devices.size(); ++i)
        outputBox.addItem ("MIDI: " + devices[i].name, 10 + i);

    const auto current = chordsProc.getStandaloneOutput();
    outputBox.setSelectedId (1, juce::dontSendNotification);
    for (int i = 0; i < 4; ++i)
        if (current == synthChoices[(size_t) i])
            outputBox.setSelectedId (i + 1, juce::dontSendNotification);
    for (int i = 0; i < devices.size(); ++i)
        if (devices[i].identifier == current)
            outputBox.setSelectedId (10 + i, juce::dontSendNotification);

    outputBox.onChange = [this]
    {
        const int id = outputBox.getSelectedId();
        chordsProc.setStandaloneOutput (id >= 1 && id <= 4 ? juce::String (synthChoices[(size_t) (id - 1)])
                                                           : devices[id - 10].identifier);
    };
}

ChordsEditor::~ChordsEditor()
{
    stopTimer();
}

void ChordsEditor::timerCallback()
{
    if (seenRevision != chordsProc.revision)
        refresh();

    // Live playback feedback: light the sounding card, run its progress
    // strip, drive the meter, dim the OUT extras when a MIDI device owns
    // the sound.
    const int sounding = chordsProc.playingChord.load();
    const float progress = chordsProc.chordProgress.load();
    const bool loopRunning = chordsProc.playing.load();
    for (int i = 0; i < cards.size(); ++i)
    {
        auto* card = cards[i];
        const bool nowActive = (i == sounding);
        if (card->clickable != loopRunning)
        {
            card->clickable = loopRunning;
            card->setMouseCursor (loopRunning ? juce::MouseCursor::PointingHandCursor
                                              : juce::MouseCursor::NormalCursor);
        }
        if (card->active != nowActive || (nowActive && std::abs (card->progress - progress) > 0.005f))
        {
            card->active = nowActive;
            card->progress = nowActive ? progress : 0.0f;
            card->repaint();
        }
    }

    // Monitor strip follows the sounding notes.
    if (const auto packed = chordsProc.soundingPacked.load(); packed != lastSounding)
    {
        lastSounding = packed;
        monitor.repaint();
    }

    // MIDI hotplug: refresh the OUT list when devices appear or vanish.
    if (--devicePollCountdown <= 0)
    {
        devicePollCountdown = 90;
        auto fresh = juce::MidiOutput::getAvailableDevices();
        bool changed = fresh.size() != devices.size();
        for (int i = 0; ! changed && i < fresh.size(); ++i)
            changed = fresh[i].identifier != devices[i].identifier;
        if (changed)
            buildOutputBox();
    }

    if (chordsProc.frozen.load() != freezeButton.getToggleState())
        freezeButton.setToggleState (chordsProc.frozen.load(), juce::dontSendNotification);

    const bool synth = chordsProc.synthOn.load();
    if (synth != lastSynthOn)
    {
        lastSynthOn = synth;
        volKnob.setVisible (synth);
        repaint();
    }
    if (synth)
    {
        meterLevel = juce::jmax (chordsProc.synthPeak.load(), meterLevel * 0.86f);
        repaint (meterRect.expanded (2));
    }

    const bool isPlaying = chordsProc.playing.load();
    if (isPlaying != lastPlaying)
    {
        lastPlaying = isPlaying;
        playButton.setToggleState (isPlaying, juce::dontSendNotification);
        playButton.setButtonText (isPlaying ? "STOP" : "PLAY");
        repaint (0, 0, getWidth(), 56); // status dot
    }
}

void ChordsEditor::refresh()
{
    seenRevision = chordsProc.revision;

    lengthRow.selected = chordsProc.seriesLength - 1;
    lengthRow.repaint();
    const int bars = chordsProc.barsPerChord.load();
    barsRow.selected = bars >= 4 ? 2 : bars - 1;
    barsRow.repaint();
    octaveRow.selected = juce::jlimit (2, 4, chordsProc.octave.load()) - 2;
    octaveRow.repaint();
    tempoBox.setValue ((double) chordsProc.bpm.load(), juce::dontSendNotification);
    volKnob.setValue ((double) chordsProc.synthVolDb.load(), juce::dontSendNotification);
    seventhToggle.setToggleState (chordsProc.useSevenths, juce::dontSendNotification);
    simplifyToggle.setToggleState (chordsProc.simplify, juce::dontSendNotification);

    for (int i = 0; i < cards.size(); ++i)
    {
        const bool shown = i < (int) chordsProc.series.size();
        cards[i]->setVisible (shown);
        if (shown)
        {
            cards[i]->text = chordsProc.series[(size_t) i].text();
            cards[i]->repaint();
        }
    }

    ticker.repaint();
    resized(); // card widths depend on how many are visible
}

void ChordsEditor::updateCardFonts()
{
    // Fit each visible chord name to its card, then give every card the
    // smallest result - a series always reads at one size.
    float common = 1000.0f;
    for (auto* card : cards)
        if (card->isVisible())
        {
            const auto b = card->getLocalBounds().toFloat();
            const float availW = b.getWidth() * 0.86f;
            float size = juce::jmax (18.0f, b.getHeight() * 0.42f);
            const float w = textWidth (juce::Font (juce::FontOptions (size)), card->text);
            if (w > availW)
                size = juce::jmax (14.0f, size * availW / w);
            common = juce::jmin (common, size);
        }

    for (auto* card : cards)
        if (card->isVisible())
        {
            card->fontSize = common;
            card->repaint();
        }
}

void ChordsEditor::doRoll()
{
    chordsProc.rollSeries();
    ticker.scroll = 0.0f; // a fresh roll snaps history back to the newest
    refresh();
}

bool ChordsEditor::keyPressed (const juce::KeyPress& key)
{
    // From M2 on, Space is the transport (family consistency with Scale
    // Shifter standalone); R and Enter roll.
    if (key == juce::KeyPress::spaceKey)
    {
        playButton.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::returnKey
        || key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
    {
        doRoll();
        return true;
    }
    return false;
}

void ChordsEditor::showMenu()
{
    juce::PopupMenu m;
    m.addItem ("Check for Updates...", []
    {
        juce::Thread::launch ([]
        {
            // Chord Randomizer releases are tagged "chords-vX.Y.Z" (Scale
            // Shifter owns the plain "vX.Y.Z" tags), so scan the list for
            // the newest chords tag rather than hitting /latest.
            const auto response = juce::URL ("https://api.github.com/repos/yuvalEG/Alea/releases?per_page=30")
                                      .readEntireTextStream();
            juce::String latest;
            if (const auto* releases = juce::JSON::parse (response).getArray())
                for (const auto& release : *releases)
                {
                    const auto tag = release.getProperty ("tag_name", juce::String()).toString();
                    if (tag.startsWith ("chords-v"))
                    {
                        latest = tag.fromFirstOccurrenceOf ("chords-v", false, false);
                        break;
                    }
                }

            juce::MessageManager::callAsync ([response, latest]
            {
                const juce::String current (CHORDS_VERSION);
                if (response.isEmpty())
                {
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                        "Check for Updates",
                        "Couldn't reach GitHub. Open the releases page instead?",
                        "Open", "Close", nullptr,
                        juce::ModalCallbackFunction::create ([] (int r)
                        {
                            if (r == 1)
                                juce::URL ("https://github.com/yuvalEG/Alea/releases").launchInDefaultBrowser();
                        }));
                }
                else if (latest.isEmpty() || latest == current)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                        "Check for Updates", "You're up to date - Chord Randomizer " + current + ".");
                }
                else
                {
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::InfoIcon,
                        "Update Available",
                        "Chord Randomizer " + latest + " is available (you have " + current + ").",
                        "Get It", "Later", nullptr,
                        juce::ModalCallbackFunction::create ([] (int r)
                        {
                            if (r == 1)
                                juce::URL ("https://github.com/yuvalEG/Alea/releases").launchInDefaultBrowser();
                        }));
                }
            });
        });
    });
    m.addSeparator();
    m.addItem ("About Alea Chord Randomizer...", []
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new AboutComponent());
        o.dialogTitle = "About Alea Chord Randomizer";
        o.dialogBackgroundColour = colors::panel;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
}

void ChordsEditor::paint (juce::Graphics& g)
{
    g.fillAll (colors::background);

    // Header: wordmark, subtitle, then the status dot + word, like the
    // Scale Shifter header reads.
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (20.0f, 12.0f, 87.0f, 34.0f),
                     juce::RectanglePlacement::centred);

    int statusX = 118;
    if (getWidth() > 470)
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Chord Randomizer", 118, 0, 150, 56, juce::Justification::centredLeft);
        statusX = 258;
    }
    const bool isPlaying = chordsProc.playing.load();
    g.setColour (isPlaying ? colors::playing : colors::control.brighter (0.15f));
    g.fillEllipse ((float) statusX, 23.0f, 10.0f, 10.0f);
    if (getWidth() > 620)
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (isPlaying ? "playing" : "stopped", statusX + 16, 0, 70, 56, juce::Justification::centredLeft);
    }

    g.setGradientFill (juce::ColourGradient (colors::purple.withAlpha (0.35f), 0.0f, 0.0f,
                                             colors::cyan.withAlpha (0.35f), (float) getWidth(), 0.0f, false));
    g.fillRect (0, 55, getWidth(), 1);

    // The two control blocks: DICE (what gets rolled) and LOOP (how it plays).
    auto paintPanel = [&g] (juce::Rectangle<int> r, const char* title)
    {
        g.setColour (colors::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (colors::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 6, 160, 18, juce::Justification::centredLeft);
    };
    paintPanel (dicePanel, "DICE");
    paintPanel (loopPanel, "LOOP");

    // Control captions.
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("CHORDS", lengthRow.getX(), lengthRow.getY() - 16,
                lengthRow.getWidth(), 14, juce::Justification::centredLeft);
    g.drawText ("BARS", barsRow.getX(), barsRow.getY() - 16,
                barsRow.getWidth(), 14, juce::Justification::centredLeft);
    g.drawText ("OCTAVE", octaveRow.getX(), octaveRow.getY() - 16,
                octaveRow.getWidth(), 14, juce::Justification::centredLeft);
    g.drawText ("OUT", outputBox.getX(), outputBox.getY() - 16,
                outputBox.getWidth(), 14, juce::Justification::centredLeft);

    // Output meter (internal synth): vertical falling peak with green /
    // amber / red zones - red means the limiter is working.
    if (chordsProc.synthOn.load() && ! meterRect.isEmpty())
    {
        const auto m = meterRect.toFloat();
        g.setColour (colors::control);
        g.fillRoundedRectangle (m, 2.0f);
        const float lvl = juce::jlimit (0.0f, 1.0f, meterLevel);
        auto fill = m.withTrimmedTop (m.getHeight() * (1.0f - lvl));
        g.setColour (lvl > 0.92f ? colors::red : lvl > 0.72f ? colors::amber : colors::playing);
        g.fillRoundedRectangle (fill, 2.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (m, 2.0f, 1.0f);
    }
}

void ChordsEditor::resized()
{
    chordsProc.lastUIWidth = getWidth();
    chordsProc.lastUIHeight = getHeight();

    auto b = getLocalBounds();
    auto header = b.removeFromTop (56);
    menuButton.setBounds (header.getRight() - 48, 14, 36, 28);
    panicButton.setBounds (menuButton.getX() - 8 - 66, 15, 66, 26);
    tempoBox.setBounds (panicButton.getX() - 8 - 100, 15, 100, 26);
    playButton.setBounds (tempoBox.getX() - 8 - 74, 15, 74, 26);
    freezeButton.setBounds (playButton.getX() - 8 - 74, 15, 74, 26);

    auto hist = b.removeFromBottom (100).reduced (16, 0).withTrimmedBottom (12);
    ticker.setBounds (hist);

    // The two control blocks. DICE: roll button, series length, the engine
    // toggles. LOOP: bars, octave, output chain, and the monitor keyboard.
    auto panels = b.removeFromBottom (164).reduced (16, 6);
    dicePanel = panels.removeFromLeft ((int) ((float) panels.getWidth() * 0.42f));
    panels.removeFromLeft (12);
    loopPanel = panels;

    auto dice = dicePanel.reduced (12).withTrimmedTop (24);
    rollButton.setBounds (dice.removeFromLeft (104).withSizeKeepingCentre (104, 44));
    dice.removeFromLeft (14);
    auto diceRight = dice;
    lengthRow.setBounds (diceRight.removeFromTop (48).withTrimmedTop (16).withHeight (28));
    auto toggles = diceRight.withTrimmedTop (6);
    seventhToggle.setBounds (toggles.removeFromTop (24));
    simplifyToggle.setBounds (toggles.removeFromTop (24));

    auto loop = loopPanel.reduced (12).withTrimmedTop (24);
    auto loopTop = loop.removeFromTop (48);
    auto controlsRow = loopTop.withTrimmedTop (16).withHeight (28);
    barsRow.setBounds (controlsRow.removeFromLeft (78));
    controlsRow.removeFromLeft (14);
    octaveRow.setBounds (controlsRow.removeFromLeft (78));
    controlsRow.removeFromLeft (14);
    meterRect = controlsRow.removeFromRight (12).withSizeKeepingCentre (12, 40).translated (0, -6);
    controlsRow.removeFromRight (4);
    volKnob.setBounds (controlsRow.removeFromRight (44).withSizeKeepingCentre (42, 42).translated (0, -7));
    volKnob.setVisible (chordsProc.synthOn.load());
    controlsRow.removeFromRight (4);
    outputBox.setBounds (controlsRow);

    // Monitor keyboard: the sounding chord, visible.
    monitor.setBounds (loop.withTrimmedTop (8));

    // Series cards fill the middle, growing with the window.
    auto area = b.reduced (16, 14);
    const int n = juce::jmax (1, (int) chordsProc.series.size());
    constexpr int gap = 12;
    const int cw = (area.getWidth() - gap * (n - 1)) / juce::jmax (1, n);
    for (int i = 0; i < cards.size(); ++i)
        if (cards[i]->isVisible())
            cards[i]->setBounds (area.getX() + i * (cw + gap), area.getY(), cw, area.getHeight());

    updateCardFonts();
}
