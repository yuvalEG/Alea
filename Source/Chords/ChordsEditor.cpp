#include "ChordsEditor.h"
#include "BinaryData.h"

using namespace ui;

namespace
{
    float textWidth (const juce::Font& font, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    // The product's About text; the dialog shell is the shared family one.
    const juce::String aboutText = juce::String::fromUTF8 (
        "Alea Chord Randomizer, version " CHORDS_VERSION "\n\n"
        "THE EXERCISE\n\n"
        "This app was born from an improvisation exercise by guitar "
        "teacher Yonatan Benaroche: generate a short random chord "
        "progression, loop it, and improvise over it with your "
        "instrument. A progression you did not choose forces your ear "
        "and your hands out of familiar shapes.\n\n"
        "HOW TO USE\n\n"
        "ROLL the dice (or press R), hit play (or the spacebar), jam. "
        "CHORDS decides what the dice can roll. LOOP decides how it "
        "sounds, voicing included: spread it open, smooth the "
        "voice-leading, add a bass note. Rolling mid-loop swaps the "
        "new chords in at the next change, and clicking a chord jumps "
        "there. Key lock keeps every roll diatonic, flavors included.\n\n"
        "MONITOR shows what is sounding. Make the window shorter to "
        "tuck it away and find the chord's notes yourself, a nice "
        "theory workout.\n\n"
        "AUTO rolls for you every few loops (press A to flip it). "
        "Pin a chord (the little dot on its card) and it survives "
        "rolls. Keep what you love, reroll the rest.\n\n"
        "OUT plays the built-in synth or sends MIDI to any device. "
        "Past rolls pile up in HISTORY. Click one to bring it back.\n\n"
        "GET IN TOUCH\n\n"
        "I'll be happy to hear your feedback, ideas and music! Reach "
        "me on GitHub (github.com/yuvalEG/Alea) or at "
        "yuvalprod@gmail.com\n\n"
        "Open source (GPLv3), built with JUCE. The piano is the "
        "Salamander Grand Piano by Alexander Holm (CC BY 3.0). "
        "Check for updates from the top-right menu.\n\n"
        "Made By Yuval Egozi");
}

//==============================================================================
juce::Rectangle<float> ChordsEditor::ChordCard::pinZone() const
{
    return { (float) getWidth() - 30.0f, 4.0f, 26.0f, 26.0f };
}

void ChordsEditor::ChordCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // The accent carries the meaning: purple = sounding now (the scale-A
    // colour), cyan = arrives at the next boundary.
    const auto accent = incoming ? colors::cyan : colors::purple;
    const bool lit = active || incoming;

    // Backlit glass pad (design .hw-card): dark purple-tinted glass, brighter
    // at the top centre, sinking to near-black at the bottom.
    {
        juce::ColourGradient glass (juce::Colour (0xff17131f), b.getCentreX(), b.getY(),
                                    juce::Colour (0xff0c0910), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (b, 10.0f);
        // Dark press at the very top (the inset shadow of a recessed pane).
        juce::ColourGradient press (juce::Colours::black.withAlpha (0.45f), b.getX(), b.getY(),
                                    juce::Colours::transparentBlack, b.getX(), b.getY() + 10.0f, false);
        g.setGradientFill (press);
        g.fillRoundedRectangle (b, 10.0f);
    }

    // A diagonal gloss, clipped to the pane (no scanlines here - the CRT
    // lines are the Scale Shifter monitor's idiom, per Yuval July 10).
    {
        juce::Path clip;
        clip.addRoundedRectangle (b, 10.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.08f), b.getX(), b.getY(),
                                    juce::Colours::transparentWhite,
                                    b.getX() + b.getWidth() * 0.45f, b.getY() + b.getHeight() * 0.75f, false);
        g.setGradientFill (gloss);
        g.fillRect (b);
    }

    // Frame: dark bezel always; the sounding/incoming card adds an accent
    // ring. Its outer bloom is drawn by the editor (a component's own paint
    // clips to its bounds - drawn here the glow became a hard square).
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
    if (lit)
    {
        g.setColour (accent.interpolatedWith (juce::Colours::black, 0.35f).withAlpha (0.95f));
        g.drawRoundedRectangle (b.reduced (1.2f), 9.0f, 1.6f);
    }

    // The chord name is phosphor behind the glass: dim scale-purple when idle,
    // fully lit with a glow while sounding (cyan when it is the incoming one).
    g.setFont (juce::Font (juce::FontOptions (fontSize)).boldened());
    if (lit)
        hw::glowText (g, text, getLocalBounds(), juce::Justification::centred, accent.brighter (0.35f));
    else
    {
        g.setColour (colors::purple.interpolatedWith (juce::Colour (0xff6b6f80), 0.45f));
        g.drawText (text, getLocalBounds(), juce::Justification::centred);
    }

    // The sounding chord fills a thin lit progress strip along its bottom edge.
    if (active && progress > 0.0f)
    {
        const auto strip = juce::Rectangle<float> (b.getX() + 8.0f, b.getBottom() - 7.0f,
                                                   (b.getWidth() - 16.0f) * juce::jlimit (0.0f, 1.0f, progress), 3.0f);
        juce::Path p;
        p.addRoundedRectangle (strip, 1.5f);
        hw::dropGlow (g, p, colors::purple.withAlpha (0.8f), 5);
        g.setColour (colors::purple.brighter (0.15f));
        g.fillPath (p);
    }

    // Pin, top-right: a lit amber dot when this chord survives rolls.
    const auto pin = pinZone().reduced (7.0f);
    if (pinned)
        hw::ledDot (g, pin.getCentre(), 1.0f, colors::amber, pin.getWidth());
    else
    {
        g.setColour (juce::Colour (0xff4a4d58));
        g.drawEllipse (pin.reduced (0.5f), 1.2f);
    }
}

void ChordsEditor::ChordCard::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown() || ! getLocalBounds().contains (e.getPosition()))
        return;
    if (pinZone().contains (e.position))
    {
        if (onPinToggle != nullptr)
            onPinToggle();
        return;
    }
    if (clickable && onPress != nullptr)
        onPress();
}

//==============================================================================
void ChordsEditor::MonitorStrip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    bool lit[128] = {};
    const auto lo = proc.soundingBitsLo.load();
    const auto hi = proc.soundingBitsHi.load();
    for (int n = 0; n < 64; ++n)
    {
        lit[n] = ((lo >> n) & 1) != 0;
        lit[n + 64] = ((hi >> n) & 1) != 0;
    }

    // The monitor screen (design LCD): near-black purple glass - radial
    // #140f1e at the top centre sinking to #0a070f - with a faint inner
    // phosphor wash, a diagonal gloss and a dark bezel.
    {
        juce::ColourGradient glass (juce::Colour (0xff140f1e), b.getCentreX(), b.getY(),
                                    juce::Colour (0xff0a070f), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (colors::purple.withAlpha (0.05f));
        g.fillRoundedRectangle (b.reduced (2.0f), 6.0f);
        juce::Path clip;
        clip.addRoundedRectangle (b, 8.0f);
        g.saveState();
        g.reduceClipRegion (clip);
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.08f), b.getX(), b.getY(),
                                    juce::Colours::transparentWhite,
                                    b.getX() + b.getWidth() * 0.45f, b.getY() + b.getHeight() * 0.75f, false);
        g.setGradientFill (gloss);
        g.fillRect (b);
        g.restoreState();
        // The glass catches the purple light of whatever is sounding -
        // brighter while notes are lit, a faint idle phosphor otherwise.
        bool anyLit = false;
        for (int n = 0; n < 128; ++n) anyLit = anyLit || lit[n];
        hw::lcdAmbience (g, b, colors::purple, anyLit ? 1.0f : 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    // C1 (24) to C7 (96): the voicing options stretched the reach (QA,
    // July 8) - the bass bottoms out at C1, open voicings at octave 4 top
    // out near A6. The shared family keybed does the drawing.
    constexpr int low = 24, high = 96;
    const auto bed = b.reduced (8.0f);
    hw::keybed (g, bed, low, high,
                [&lit] (int n) { return lit[n] ? 1.0f : 0.0f; }, colors::purple);

    // Notes beyond the strip get a small edge arrow - the family idiom.
    // C1-C7 covers everything reachable today; this is the safety net.
    bool below = false, above = false;
    for (int n = 0; n < low; ++n)        below = below || lit[n];
    for (int n = high + 1; n < 128; ++n) above = above || lit[n];
    auto arrow = [&g, &bed] (bool left)
    {
        const float cy = bed.getCentreY();
        const float x = left ? bed.getX() + 3.0f : bed.getRight() - 3.0f;
        const float back = left ? 9.0f : -9.0f;
        juce::Path p;
        p.addTriangle (x, cy, x + back, cy - 7.0f, x + back, cy + 7.0f);
        g.setColour (colors::purple.brighter (0.25f));
        g.fillPath (p);
    };
    if (below) arrow (true);
    if (above) arrow (false);
}

//==============================================================================
void ChordsEditor::HistoryTicker::paint (juce::Graphics& g)
{
    // The plate and its engraved HISTORY title are painted by the editor;
    // this component only lays the engraved roll groups into it.
    const auto area = getLocalBounds().toFloat();

    if (proc.history.empty())
    {
        g.setColour (juce::Colour (0xff868ba0).withAlpha (0.8f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::italic));
        g.drawText ("your past rolls will appear here", area, juce::Justification::centredLeft);
        return;
    }

    const juce::Font font { juce::FontOptions (16.0f, juce::Font::bold) };
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
    // past. Older rolls march left and fade - engraved into the metal.
    // Hovered rolls highlight; clicking one recalls it into the series row.
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

        const float alpha = juce::jmax (0.30f, 1.0f - 0.15f * (float) age);

        if (age == hoveredGroup)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (groupRect, 6.0f);
        }

        // A machined groove between this roll and the newer one to its right.
        if (age > 0)
        {
            const float gx = rightX + groupGap / 2.0f;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (juce::Rectangle<float> (gx, area.getY() + 3.0f, 1.0f, area.getHeight() - 6.0f));
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRect (juce::Rectangle<float> (gx + 1.0f, area.getY() + 3.0f, 1.0f, area.getHeight() - 6.0f));
        }

        const auto ink = juce::Colour (0xffc8ccd8)
                             .withAlpha (age == hoveredGroup ? 1.0f : alpha * (age == 0 ? 1.0f : 0.85f));
        g.setFont (font);

        float x = startX;
        for (const auto& c : roll)
        {
            const auto t = c.text();
            const float w = textWidth (font, t);
            const auto cell = juce::Rectangle<float> (x, area.getY(), w + 2.0f, area.getHeight()).toNearestInt();
            hw::engraved (g, t, cell, juce::Justification::centredLeft, ink);
            x += w + chordGap;
        }

        rightX = startX - groupGap;
        ++age;
    }

    g.restoreState();

    // Edge page keys, shown when there is more history in that direction.
    auto drawPager = [&] (juce::Rectangle<float>& store, float x, bool left)
    {
        store = { x, (float) getHeight() / 2.0f - 14.0f, 18.0f, 28.0f };
        hw::button (g, store, 0.0f, hw::led, false, false);
        juce::Path p;
        const float cx = store.getCentreX(), cy = store.getCentreY();
        if (left)
            p.addTriangle (cx - 3.5f, cy, cx + 3.0f, cy - 5.0f, cx + 3.0f, cy + 5.0f);
        else
            p.addTriangle (cx + 3.5f, cy, cx - 3.0f, cy - 5.0f, cx - 3.0f, cy + 5.0f);
        g.setColour (juce::Colour (0xffc0c4d0));
        g.fillPath (p);
    };

    olderButton = newerButton = {};
    if (scroll < maxScroll)
        drawPager (olderButton, 0.0f, true);
    if (scroll > 0.0f)
        drawPager (newerButton, (float) getWidth() - 18.0f, false);
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
namespace
{
    // Per-window colour choices (design handoff): lit selections are WHITE in
    // this window; cyan is reserved for the ROLL/AUTO action pair, purple for
    // the sounding chord, green for the internal-synth chrome.
    const juce::Colour kSel = colors::text;
}

ChordsEditor::ChordsEditor (ChordsProcessor& p)
    : AudioProcessorEditor (p), chordsProc (p),
      standalone (p.isStandaloneLike()),
      lengthRow  (juce::StringArray { "1", "2", "3", "4", "5", "6", "7", "8" }, kSel),
      typeRow    (juce::StringArray { "triads", "7ths", "9ths" }, kSel),
      everyRow   (juce::StringArray { "1", "2", "4" }, colors::cyan),
      barsRow    (juce::StringArray { "1", "2", "4" }, kSel),
      octaveRow  (juce::StringArray { "2", "3", "4" }, kSel),
      voicingRow (juce::StringArray { "close", "open" }, kSel),
      ticker (p), monitor (p)
{
    // The family LookAndFeel (Hardware.h): Space Grotesk, hardware chrome.
    juce::LookAndFeel::setDefaultLookAndFeel (&ui::hardwareLookAndFeel());

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    for (int i = 0; i < 8; ++i)
    {
        auto* card = cards.add (new ChordCard());
        card->onPress = [this, i] { chordsProc.jumpRequest.store (i); };
        card->onPinToggle = [this, i]
        {
            chordsProc.togglePin (i);
            refresh();
        };
        addChildComponent (card);
    }

    // ROLL is the hero action key: a big, permanently lit cyan key (the "next"
    // colour) with the LED bloom. Pressing it still physically clicks.
    rollButton.setColour (juce::TextButton::buttonOnColourId, colors::cyan);
    rollButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07120d));
    rollButton.getProperties().set ("litAmt", 1.0f);
    rollButton.getProperties().set ("fontSize", 19.0f);
    rollButton.onClick = [this] { doRoll(); };
    addAndMakeVisible (rollButton);

    // AUTO is the automatic version of ROLL, directly beneath it: engage it
    // and the series re-rolls on its own. Lit cyan when armed.
    autoButton.setClickingTogglesState (true);
    autoButton.setColour (juce::TextButton::buttonOnColourId, colors::cyan);
    autoButton.onClick = [this]
    {
        const bool on = autoButton.getToggleState();
        chordsProc.autoRollOn.store (on);
        if (! on)
        {
            chordsProc.cancelAutoRollSwap();
            refresh();
        }
        applyAutoGate();
    };
    addAndMakeVisible (autoButton);

    // The AUTO cadence: re-roll every 1 / 2 / 4 loops. Relevant only while
    // AUTO is armed - dimmed and inert otherwise (applyAutoGate).
    everyRow.onChange = [this] (int idx)
    {
        chordsProc.autoRollLoops.store (idx == 0 ? 1 : idx == 1 ? 2 : 4);
    };
    addAndMakeVisible (everyRow);

    // Transport: the loop is the product (spec M2). Space toggles it.
    // In a DAW the host owns transport and tempo (spec M4), so the button
    // hides and the tempo LCD becomes a readout.
    playButton.onClick = [this]
    {
        const bool on = playButton.getToggleState();
        chordsProc.playing.store (on);
        if (! on)
            chordsProc.handleStopped(); // an unheard pending roll is discarded
    };
    addChildComponent (playButton);
    playButton.setVisible (standalone);

    // Tempo: the glass BPM LCD - drag to set (the shared LookAndFeel draws
    // any linear slider with component ID "bpm" as the family LCD).
    tempoBox.setComponentID ("bpm");
    tempoBox.setSliderStyle (juce::Slider::LinearBar);
    tempoBox.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    tempoBox.setRange (30.0, 300.0, 1.0);
    tempoBox.setTextValueSuffix (" BPM");
    tempoBox.setValue ((double) chordsProc.bpm.load(), juce::dontSendNotification);
    tempoBox.onValueChange = [this] { chordsProc.bpm.store ((float) tempoBox.getValue()); };
    tempoBox.setEnabled (standalone);
    addAndMakeVisible (tempoBox);

    // FREEZE holds the sounding chord (time stops); PANIC is instant silence.
    // Opposite jobs, opposite ends of the header - the family rule. Active
    // FREEZE wears the family ice blue; PANIC is a red legend, never a red key.
    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, colors::ice);
    freezeButton.onClick = [this] { chordsProc.frozen.store (freezeButton.getToggleState()); };
    addAndMakeVisible (freezeButton);

    panicButton.setColour (juce::TextButton::textColourOffId, colors::red);
    panicButton.onClick = [this] { chordsProc.panicRequest.store (true); };
    addAndMakeVisible (panicButton);

    lengthRow.onChange = [this] (int idx)
    {
        chordsProc.setSeriesLength (idx + 1);
        refresh();
    };
    addAndMakeVisible (lengthRow);

    // Bars per chord: 1, 2 or 4 bars of 4/4 before the loop moves on.
    barsRow.onChange = [this] (int idx)
    {
        chordsProc.barsPerChord.store (idx == 0 ? 1 : idx == 1 ? 2 : 4);
    };
    addAndMakeVisible (barsRow);

    // Voicing octaves, multi-select: every checked octave sounds together.
    octaveRow.setMulti (false); // never empty - something must sound
    octaveRow.onChange = [this] (int newMask)
    {
        chordsProc.octaveMask.store (newMask);
        chordsProc.updateLoop();
    };
    addAndMakeVisible (octaveRow);

    // Voicings (spec M5), output stage only - never the dice. Changes
    // re-voice at the next chord boundary silently: the chords aren't
    // changing, so no switching theater.
    voicingRow.onChange = [this] (int idx)
    {
        chordsProc.openVoicing.store (idx == 1);
        chordsProc.updateLoop();
    };
    addAndMakeVisible (voicingRow);

    // Toggle switches flip independent options; all wear the window's white.
    for (auto* t : { &simplifyToggle, &susToggle, &keyLockToggle, &smoothToggle, &bassToggle })
        t->setColour (juce::ToggleButton::tickColourId, kSel);

    smoothToggle.onClick = [this]
    {
        chordsProc.smoothVoicing.store (smoothToggle.getToggleState());
        chordsProc.updateLoop();
    };
    addAndMakeVisible (smoothToggle);

    bassToggle.onClick = [this]
    {
        chordsProc.bassNote.store (bassToggle.getToggleState());
        chordsProc.updateLoop();
    };
    addAndMakeVisible (bassToggle);

    // Metronome lives next to the tempo - a white-lit key (colored accents
    // stay reserved for meaning).
    clickButton.setClickingTogglesState (true);
    clickButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffc8ccd8));
    clickButton.onClick = [this] { chordsProc.metronomeOn.store (clickButton.getToggleState()); };
    addChildComponent (clickButton);
    clickButton.setVisible (standalone); // DAWs bring their own metronome

    clickVolKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    clickVolKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    clickVolKnob.setPopupDisplayEnabled (true, true, this);
    clickVolKnob.setColour (juce::Slider::rotarySliderFillColourId, kSel.withAlpha (0.8f));
    clickVolKnob.setRange (-12.0, 12.0, 0.1);
    clickVolKnob.setDoubleClickReturnValue (true, 0.0);
    clickVolKnob.setValue ((double) chordsProc.clickVolDb.load(), juce::dontSendNotification);
    clickVolKnob.setTextValueSuffix (" dB click");
    clickVolKnob.onValueChange = [this] { chordsProc.clickVolDb.store ((float) clickVolKnob.getValue()); };
    addChildComponent (clickVolKnob);
    clickVolKnob.setVisible (standalone);

    addAndMakeVisible (monitor);

    // OUT chooser + volume knob, straight from the Scale Shifter playbook.
    outputBox.setColour (juce::ComboBox::textColourId, colors::text);
    outputBox.setColour (juce::ComboBox::arrowColourId, colors::secondary);
    buildOutputBox();
    addAndMakeVisible (outputBox);

    volKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volKnob.setPopupDisplayEnabled (true, true, this);
    volKnob.setColour (juce::Slider::rotarySliderFillColourId, colors::green.withAlpha (0.85f));
    volKnob.setRange (-24.0, 6.0, 0.1);
    volKnob.setDoubleClickReturnValue (true, 0.0);
    volKnob.setValue ((double) chordsProc.synthVolDb.load(), juce::dontSendNotification);
    volKnob.setTextValueSuffix (" dB");
    volKnob.onValueChange = [this] { chordsProc.synthVolDb.store ((float) volKnob.getValue()); };
    addAndMakeVisible (volKnob);

    simplifyToggle.onClick = [this] { chordsProc.simplify = simplifyToggle.getToggleState(); };
    addAndMakeVisible (simplifyToggle);
    susToggle.onClick = [this] { chordsProc.susOn = susToggle.getToggleState(); };
    addAndMakeVisible (susToggle);

    // CHORD TYPE: how tall the stack is - a triad, a seventh chord, or a
    // ninth chord (which by nature contains its seventh). One of three;
    // no couplings to explain.
    typeRow.onChange = [this] (int level)
    {
        chordsProc.useSevenths = level >= 1;
        chordsProc.ninthsOn = level == 2;
    };
    addAndMakeVisible (typeRow);

    // Key lock: rolls draw only from the chosen key and scale's seven
    // diatonic chords - flavors included, kept diatonic (Simplify is
    // ignored while locked).
    keyLockToggle.onClick = [this] { chordsProc.keyLockOn = keyLockToggle.getToggleState(); };
    addAndMakeVisible (keyLockToggle);
    for (auto* box : { &keyBox, &scaleBox })
    {
        box->setColour (juce::ComboBox::textColourId, colors::text);
        box->setColour (juce::ComboBox::arrowColourId, colors::secondary);
        addAndMakeVisible (*box);
    }
    for (int i = 0; i < chords::scaleTypeNames().size(); ++i)
        scaleBox.addItem (chords::scaleTypeNames()[i], i + 1);
    scaleBox.setSelectedId (chordsProc.keyScale + 1, juce::dontSendNotification);
    scaleBox.onChange = [this]
    {
        if (scaleBox.getSelectedId() > 0)
        {
            const auto oldTonic = keyBox.getText();
            chordsProc.keyScale = scaleBox.getSelectedId() - 1;
            rebuildKeyBox();
            // Keep the tonic if the new scale offers it.
            const auto keys = chords::keyNamesFor ((chords::ScaleType) chordsProc.keyScale);
            const int keep = keys.indexOf (oldTonic);
            chordsProc.keyIndex = juce::jmax (0, keep);
            keyBox.setSelectedId (chordsProc.keyIndex + 1, juce::dontSendNotification);
        }
    };
    rebuildKeyBox();
    keyBox.setSelectedId (chordsProc.keyIndex + 1, juce::dontSendNotification);
    keyBox.onChange = [this]
    {
        if (keyBox.getSelectedId() > 0)
            chordsProc.keyIndex = keyBox.getSelectedId() - 1;
    };

    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    // Plugin only: the most common support question is routing, exactly as
    // in Scale Shifter; the standalone's synth needs none.
    helpLink.setButtonText ("No sound? Routing Help");
    helpLink.setURL (juce::URL ("https://github.com/yuvalEG/Alea#troubleshooting"));
    helpLink.setFont (juce::FontOptions (13.0f), false, juce::Justification::centredLeft);
    helpLink.setColour (juce::HyperlinkButton::textColourId, colors::secondary);
    addChildComponent (helpLink);
    helpLink.setVisible (! standalone);

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
                     (juce::Component*) &simplifyToggle,
                     (juce::Component*) &lengthRow, (juce::Component*) &barsRow,
                     (juce::Component*) &octaveRow, (juce::Component*) &monitor,
                     (juce::Component*) &freezeButton, (juce::Component*) &panicButton,
                     (juce::Component*) &playButton, (juce::Component*) &outputBox,
                     (juce::Component*) &volKnob, (juce::Component*) &tempoBox,
                     (juce::Component*) &clickButton, (juce::Component*) &susToggle,
                     (juce::Component*) &autoButton, (juce::Component*) &everyRow,
                     (juce::Component*) &clickVolKnob,
                     (juce::Component*) &typeRow, (juce::Component*) &voicingRow,
                     (juce::Component*) &smoothToggle, (juce::Component*) &bassToggle,
                     (juce::Component*) &keyLockToggle, (juce::Component*) &keyBox,
                     (juce::Component*) &scaleBox })
        c->setWantsKeyboardFocus (false);
    // Space belongs to the host in a DAW; the plugin never grabs keys.
    setWantsKeyboardFocus (standalone);

    // Every open is 1040x760 (the handoff faceplate width) - size is not
    // persisted (family behavior, and persisted sizes kept restoring shutdown
    // transients). Short windows are legitimate within a session: the
    // tucked-monitor practice mode.
    setResizable (true, true);
    setResizeLimits (900, 520, 4096, 2400);
    setSize (1040, 760);

    // The blooms behind the backlit keys are drawn by this editor (paint), so
    // each key's crossfade must repaint the metal around it too.
    for (auto* b : { &autoButton, &freezeButton, &clickButton })
        b->onLitChange = [this, b] { repaint (b->getBounds().expanded (18)); };

    refresh();
    applyAutoGate();
    startTimerHz (30); // fast enough for the bar-progress strip and meter
}

void ChordsEditor::applyAutoGate()
{
    // The EVERY selector matters only while AUTO is armed: dim it to ~35%
    // and let it sleep (the design system's "dim irrelevant controls" rule).
    const bool on = chordsProc.autoRollOn.load();
    everyRow.setEnabled (on);
    everyRow.setAlpha (on ? 1.0f : 0.35f);
    if (lastAutoOn != on)
    {
        lastAutoOn = on;
        repaint (chordsPanel); // the EVERY caption dims with its control
    }
}

void ChordsEditor::buildOutputBox()
{
    // The sound list comes from the shared flavour table (Source/Sound.h),
    // grouped by section: SYNTH / INSTRUMENT, then MIDI. Flavour ids are
    // 1 + alea::Flavour; "MIDI to DAW" is 50; devices from 100.
    outputBox.clear (juce::dontSendNotification);
    if (! standalone)
        outputBox.addItem ("MIDI to DAW", 50); // plugin default, listed first

    for (int group : { alea::groupSynth, alea::groupInstrument })
    {
        outputBox.addSectionHeading (alea::groupName (group));
        for (const auto& f : alea::flavourTable())
            if (f.group == group)
                outputBox.addItem (f.name, 1 + f.flavour);
    }

    devices.clear();
    if (standalone)
    {
        devices = juce::MidiOutput::getAvailableDevices();
        if (! devices.isEmpty())
            outputBox.addSectionHeading ("MIDI");
        for (int i = 0; i < devices.size(); ++i)
            outputBox.addItem (devices[i].name, 100 + i);
    }

    const auto current = chordsProc.getStandaloneOutput();
    outputBox.setSelectedId (standalone ? 1 + alea::warmPad : 50, juce::dontSendNotification);
    if (const int flavour = alea::flavourFromChoice (current); flavour >= 0 && chordsProc.synthOn.load())
        outputBox.setSelectedId (1 + flavour, juce::dontSendNotification);
    for (int i = 0; i < devices.size(); ++i)
        if (devices[i].identifier == current)
            outputBox.setSelectedId (100 + i, juce::dontSendNotification);

    outputBox.onChange = [this]
    {
        const int id = outputBox.getSelectedId();
        if (id >= 1 && id <= alea::numFlavours)
            chordsProc.setStandaloneOutput (alea::choiceForFlavour (id - 1));
        else if (id == 50)
            chordsProc.setStandaloneOutput ({});
        else if (id >= 100 && id - 100 < devices.size())
            chordsProc.setStandaloneOutput (devices[id - 100].identifier);
    };
}

void ChordsEditor::rebuildKeyBox()
{
    keyBox.clear (juce::dontSendNotification);
    const auto keys = chords::keyNamesFor ((chords::ScaleType) chordsProc.keyScale);
    for (int i = 0; i < keys.size(); ++i)
        keyBox.addItem (keys[i], i + 1);
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
    // During a pending swap the sounding card still shows the old chord, so
    // the lit highlight stays on - refresh() keeps that card honest.
    const bool loopRunning = chordsProc.playing.load();
    const bool pendingNow = loopRunning && chordsProc.swapPending();
    const int sounding = chordsProc.playingChord.load();
    const float progress = chordsProc.chordProgress.load();
    for (int i = 0; i < cards.size(); ++i)
    {
        auto* card = cards[i];
        const bool nowActive = (i == sounding);
        const bool canJump = loopRunning && standalone; // the DAW timeline owns position in a plugin
        if (card->clickable != canJump)
        {
            card->clickable = canJump;
            card->setMouseCursor (canJump ? juce::MouseCursor::PointingHandCursor
                                          : juce::MouseCursor::NormalCursor);
        }
        if (card->active != nowActive || (nowActive && std::abs (card->progress - progress) > 0.005f))
        {
            const bool blinked = card->active != nowActive;
            card->active = nowActive;
            card->progress = nowActive ? progress : 0.0f;
            card->repaint();
            if (blinked)
                repaint (card->getBounds().expanded (24)); // the bloom behind it
        }
    }

    // Monitor strip follows the sounding notes.
    const auto bitsLo = chordsProc.soundingBitsLo.load();
    const auto bitsHi = chordsProc.soundingBitsHi.load();
    if (bitsLo != lastSounding || bitsHi != lastSoundingHi)
    {
        lastSounding = bitsLo;
        lastSoundingHi = bitsHi;
        monitor.repaint();
    }

    // Host mode: the tempo LCD is a readout of the DAW's tempo.
    if (! standalone && std::abs (tempoBox.getValue() - (double) chordsProc.bpm.load()) > 0.5)
        tempoBox.setValue ((double) chordsProc.bpm.load(), juce::dontSendNotification);

    // MIDI hotplug: refresh the OUT list when devices appear or vanish.
    if (standalone && --devicePollCountdown <= 0)
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
    if (chordsProc.autoRollOn.load() != autoButton.getToggleState())
    {
        autoButton.setToggleState (chordsProc.autoRollOn.load(), juce::dontSendNotification);
        applyAutoGate();
    }

    // Pending swap: the sounding card stays lit with the old chord, all
    // other cards preview the incoming series in cyan.
    if (pendingNow != lastPending)
    {
        lastPending = pendingNow;
        refresh();
        repaint (0, 0, getWidth(), 56); // status word
    }

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
        playButton.repaint();
        repaint (0, 0, getWidth(), 56); // status LED + word
    }
}

void ChordsEditor::refresh()
{
    seenRevision = chordsProc.revision;

    lengthRow.setSelected (chordsProc.seriesLength - 1);
    const int bars = chordsProc.barsPerChord.load();
    barsRow.setSelected (bars >= 4 ? 2 : bars - 1);
    octaveRow.setMask (juce::jlimit (1, 7, chordsProc.octaveMask.load()));
    voicingRow.setSelected (chordsProc.openVoicing.load() ? 1 : 0);
    smoothToggle.setToggleState (chordsProc.smoothVoicing.load(), juce::dontSendNotification);
    bassToggle.setToggleState (chordsProc.bassNote.load(), juce::dontSendNotification);
    autoButton.setToggleState (chordsProc.autoRollOn.load(), juce::dontSendNotification);
    const int loops = chordsProc.autoRollLoops.load();
    everyRow.setSelected (loops >= 4 ? 2 : loops >= 2 ? 1 : 0);
    applyAutoGate();
    keyLockToggle.setToggleState (chordsProc.keyLockOn, juce::dontSendNotification);
    if (scaleBox.getSelectedId() != chordsProc.keyScale + 1)
    {
        scaleBox.setSelectedId (chordsProc.keyScale + 1, juce::dontSendNotification);
        rebuildKeyBox();
    }
    keyBox.setSelectedId (chordsProc.keyIndex + 1, juce::dontSendNotification);
    tempoBox.setValue ((double) chordsProc.bpm.load(), juce::dontSendNotification);
    clickButton.setToggleState (chordsProc.metronomeOn.load(), juce::dontSendNotification);
    clickVolKnob.setValue ((double) chordsProc.clickVolDb.load(), juce::dontSendNotification);
    volKnob.setValue ((double) chordsProc.synthVolDb.load(), juce::dontSendNotification);
    typeRow.setSelected (chordsProc.ninthsOn ? 2 : chordsProc.useSevenths ? 1 : 0);
    simplifyToggle.setToggleState (chordsProc.simplify, juce::dontSendNotification);
    susToggle.setToggleState (chordsProc.susOn, juce::dontSendNotification);

    // While a swap is pending: the SOUNDING card keeps its old chord (lit,
    // progress running), every other card previews the incoming series in
    // cyan - the "next" colour.
    const bool pendingSwap = chordsProc.playing.load() && chordsProc.swapPending()
                          && ! chordsProc.pendingOldSeries.empty();
    const int soundingIdx = pendingSwap
        ? juce::jlimit (0, (int) chordsProc.pendingOldSeries.size() - 1, chordsProc.playingChord.load())
        : -1;
    const int visibleCount = juce::jmax ((int) chordsProc.series.size(),
                                         pendingSwap ? soundingIdx + 1 : 0);

    for (int i = 0; i < cards.size(); ++i)
    {
        const bool visible = i < visibleCount;
        cards[i]->setVisible (visible);
        if (! visible)
            continue;
        if (pendingSwap && i == soundingIdx)
        {
            cards[i]->text = chordsProc.pendingOldSeries[(size_t) i].text();
            cards[i]->incoming = false;
        }
        else if (i < (int) chordsProc.series.size())
        {
            cards[i]->text = chordsProc.series[(size_t) i].text();
            // Cyan means "about to change" - a card whose chord survives
            // the swap (pinned, typically) stays calm.
            cards[i]->incoming = pendingSwap
                && ! (i < (int) chordsProc.pendingOldSeries.size()
                      && chordsProc.pendingOldSeries[(size_t) i] == chordsProc.series[(size_t) i]);
        }
        cards[i]->pinned = chordsProc.pinned[(size_t) i];
        cards[i]->repaint();
    }

    ticker.repaint();
    resized(); // card widths depend on how many are visible
    repaint(); // card blooms + gated captions live in the editor's layer
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
    // A flips AUTO - the one-key path to armed/disarmed.
    if (key.getTextCharacter() == 'a' || key.getTextCharacter() == 'A')
    {
        const bool on = ! chordsProc.autoRollOn.load();
        chordsProc.autoRollOn.store (on);
        if (! on)
            chordsProc.cancelAutoRollSwap();
        refresh();
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
                        "Check for Updates", "You're up to date. Chord Randomizer " + current + ".");
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
        ui::showAboutDialog ("About Alea Chord Randomizer", aboutText, 17.5f, 720, 700);
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
}

void ChordsEditor::paint (juce::Graphics& g)
{
    // The whole window is one raised brushed-gunmetal faceplate slab.
    hw::brushedMetal (g, getLocalBounds().toFloat(), 16.0f, false);

    // Header: wordmark with a clean outer drop shadow (letters fully in front
    // of it), then the engraved subtitle and the PLAYING LED.
    drawWordmark (g, logo, { 20, 11, 87, 34 });

    int statusX = 118;
    if (getWidth() > 560)
    {
        g.setFont (juce::FontOptions (14.0f));
        hw::engraved (g, "Chord Randomizer", { 118, 14, 150, 28 },
                      juce::Justification::centredLeft, juce::Colour (0xff9297a8));
        statusX = 254;
    }
    const bool isPlaying = chordsProc.playing.load();
    const bool pending = isPlaying && chordsProc.swapPending();
    hw::ledDot (g, { (float) statusX + 5.0f, 28.0f }, isPlaying ? 1.0f : 0.0f,
                pending ? colors::cyan : colors::playing);
    if (getWidth() > 640)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        const auto word = pending ? "SWITCHING" : isPlaying ? "PLAYING" : "STOPPED";
        const auto ink = pending ? colors::cyan : isPlaying ? juce::Colour (0xff9fe6b6)
                                                            : juce::Colour (0xff7f8496);
        hw::engraved (g, word, { statusX + 16, 14, 90, 28 }, juce::Justification::centredLeft, ink);
    }

    // The module plates: CHORDS (what gets rolled), LOOP (how it plays),
    // MONITOR (what sounds) and HISTORY (what sounded before).
    hw::plate (g, chordsPanel, "CHORDS");
    hw::plate (g, loopPanel, "LOOP");
    if (monitor.isVisible())
        hw::plate (g, monitorPanel, "MONITOR");
    hw::plate (g, historyPanel, "HISTORY");

    // The LED blooms, drawn HERE (the parent) so they spread onto the metal
    // instead of clipping to each key's rectangle: the always-lit ROLL, the
    // crossfading header keys, and the lit chord cards.
    hw::keyBloom (g, rollButton, colors::cyan, 1.0f);
    hw::keyBloom (g, autoButton, colors::cyan, hw::litAmount (autoButton));
    hw::keyBloom (g, freezeButton, colors::ice, hw::litAmount (freezeButton));
    hw::keyBloom (g, clickButton, juce::Colour (0xffc8ccd8), hw::litAmount (clickButton));
    if (playButton.isVisible())
        hw::keyBloom (g, playButton, colors::playing, playButton.getToggleState() ? 1.0f : 0.0f);
    for (auto* card : cards)
        if (card->isVisible() && (card->active || card->incoming))
            hw::keyBloom (g, *card, card->incoming ? colors::cyan : colors::purple, 0.8f, 10.0f);

    // Engraved control captions, above their rows.
    auto caption = [&g] (const juce::Component& c, const juce::String& text, float alpha = 1.0f)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.setColour (juce::Colours::black.withAlpha (0.8f * alpha));
        const auto area = juce::Rectangle<int> (c.getX(), c.getY() - 17, juce::jmax (150, c.getWidth()), 14);
        g.drawText (text, area.translated (1, 1), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff868ba0).withAlpha (alpha));
        g.drawText (text, area, juce::Justification::centredLeft);
    };
    caption (lengthRow, "NUMBER OF CHORDS");
    caption (typeRow, "CHORD TYPE");
    caption (everyRow, juce::String::fromUTF8 ("EVERY \xc2\xb7 LOOPS"),
             chordsProc.autoRollOn.load() ? 1.0f : 0.35f);
    caption (barsRow, "BARS");
    caption (octaveRow, "OCTAVE");
    caption (outputBox, "OUT");
    caption (voicingRow, "VOICING");

    // LEVEL knob caption + the output meter (internal synth only).
    if (chordsProc.synthOn.load())
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        const auto kb = volKnob.getBounds();
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText ("LEVEL", kb.getX() - 11, kb.getBottom() + 1, kb.getWidth() + 24, 13, juce::Justification::centred);
        g.setColour (juce::Colour (0xff868ba0));
        g.drawText ("LEVEL", kb.getX() - 12, kb.getBottom(), kb.getWidth() + 24, 13, juce::Justification::centred);

        if (! meterRect.isEmpty())
        {
            meterLevel = juce::jlimit (0.0f, 1.0f, meterLevel);
            hw::meter (g, meterRect.toFloat(), meterLevel);
        }
    }
}

void ChordsEditor::resized()
{
    auto b = getLocalBounds();
    constexpr int M = 18; // faceplate margin

    // Plugin footer: routing help bottom-left (the resize handle owns the
    // bottom-right corner); absent in the standalone.
    if (! standalone)
        helpLink.setBounds (b.removeFromBottom (20).withTrimmedLeft (16).withTrimmedBottom (2).withWidth (200));

    // Header, right cluster: Transport - FREEZE - CLICK - BPM - PANIC - menu.
    auto header = b.removeFromTop (54);
    menuButton.setBounds (header.getRight() - M - 36, 13, 36, 28);
    panicButton.setBounds (menuButton.getX() - 10 - 62, 13, 62, 28);
    tempoBox.setBounds (panicButton.getX() - 10 - 96, 12, 96, 30);
    int leftEdge = tempoBox.getX();
    if (standalone) // the plugin has no click - DAWs bring their own
    {
        clickVolKnob.setBounds (leftEdge - 6 - 30, 12, 30, 30);
        clickButton.setBounds (clickVolKnob.getX() - 6 - 58, 13, 58, 28);
        leftEdge = clickButton.getX();
    }
    freezeButton.setBounds (leftEdge - 10 - 70, 13, 70, 28);
    playButton.setBounds (freezeButton.getX() - 10 - 76, 13, 76, 28);

    // HISTORY plate at the very bottom.
    auto hist = b.removeFromBottom (92).reduced (M, 0).withTrimmedBottom (10);
    historyPanel = hist;
    ticker.setBounds (hist.reduced (12, 0).withTrimmedTop (28).withTrimmedBottom (8));

    // MONITOR: the keyboard earns the full window width (QA, July 8 -
    // C1-C7 was cramped at half width inside LOOP). Full and titled, or
    // tucked away entirely - never condensed or title-less (two flexing
    // designs fell in QA). Hiding it by dragging the window shorter is a
    // FEATURE (Yuval): practice naming a chord's notes without the
    // keyboard giving them away. The default window shows it with room
    // to spare, so only a deliberate drag tucks it.
    const bool showMonitor = b.getHeight() >= 216 + 128 + 116; // plates + monitor + a card floor
    monitor.setVisible (showMonitor);
    monitorPanel = {};
    if (showMonitor)
    {
        monitorPanel = b.removeFromBottom (128).reduced (M, 0).withTrimmedBottom (10);
        monitor.setBounds (monitorPanel.reduced (12, 0).withTrimmedTop (26).withTrimmedBottom (10));
    }

    // The two module plates. CHORDS: the ROLL/AUTO action column, then one
    // captioned row per dice concept. LOOP: bars, octaves, output chain, and
    // the voicing row (M5).
    auto panels = b.removeFromBottom (216).reduced (M, 4);
    chordsPanel = panels.removeFromLeft ((panels.getWidth() - 12) / 2);
    panels.removeFromLeft (12);
    loopPanel = panels;

    auto dice = chordsPanel.reduced (12).withTrimmedTop (24);

    // Left: the action column - the big lit ROLL key, AUTO beneath it, and
    // the gated EVERY cadence below.
    auto rollStack = dice.removeFromLeft (124);
    rollButton.setBounds (rollStack.removeFromTop (52));
    rollStack.removeFromTop (8);
    autoButton.setBounds (rollStack.removeFromTop (32));
    rollStack.removeFromTop (21); // EVERY caption breathes here
    everyRow.setBounds (rollStack.removeFromTop (26));
    dice.removeFromLeft (16);

    // Right: the dice themselves, one captioned row per concept.
    lengthRow.setBounds (dice.removeFromTop (43).withTrimmedTop (17).withHeight (26));
    dice.removeFromTop (4);
    typeRow.setBounds (dice.removeFromTop (43).withTrimmedTop (17).withHeight (26));
    dice.removeFromTop (8);

    auto checks = dice.removeFromTop (28);
    simplifyToggle.setBounds (checks.removeFromLeft (juce::jmin (132, checks.getWidth() / 2)));
    checks.removeFromLeft (8);
    susToggle.setBounds (checks);
    dice.removeFromTop (4);

    auto rowE = dice.removeFromTop (28);
    keyLockToggle.setBounds (rowE.removeFromLeft (110));
    rowE.removeFromLeft (2);
    keyBox.setBounds (rowE.removeFromLeft (60).withSizeKeepingCentre (60, 26));
    rowE.removeFromLeft (6);
    const int scaleW = juce::jmin (118, rowE.getWidth());
    scaleBox.setBounds (rowE.removeFromLeft (scaleW).withSizeKeepingCentre (scaleW, 26));

    // LOOP plate: BARS / OCTAVE / OUT on the first row; VOICING, the voicing
    // toggles, and the green LEVEL knob + meter on the second.
    auto loop = loopPanel.reduced (12).withTrimmedTop (24);
    auto row1 = loop.removeFromTop (43).withTrimmedTop (17).withHeight (26);
    barsRow.setBounds (row1.removeFromLeft (104));
    row1.removeFromLeft (14);
    octaveRow.setBounds (row1.removeFromLeft (104));
    row1.removeFromLeft (14);
    outputBox.setBounds (row1);
    loop.removeFromTop (12);

    auto row2 = loop.removeFromTop (86);
    auto voiceCol = row2.removeFromLeft (128);
    voicingRow.setBounds (voiceCol.withTrimmedTop (17).withHeight (26));
    row2.removeFromLeft (16);
    auto togglesCol = row2.removeFromLeft (juce::jmin (158, row2.getWidth() - 96)).withTrimmedTop (10);
    smoothToggle.setBounds (togglesCol.removeFromTop (28));
    togglesCol.removeFromTop (4);
    bassToggle.setBounds (togglesCol.removeFromTop (28));

    meterRect = juce::Rectangle<int> (row2.getRight() - 14, row2.getY() + 6, 14, 58);
    volKnob.setBounds (row2.getRight() - 14 - 8 - 58, row2.getY() + 6, 58, 58);
    volKnob.setVisible (chordsProc.synthOn.load());

    // Series cards fill the middle, growing with the window.
    auto area = b.reduced (M, 10);
    const int n = juce::jmax (1, (int) chordsProc.series.size());
    constexpr int gap = 12;
    const int cw = (area.getWidth() - gap * (n - 1)) / juce::jmax (1, n);
    for (int i = 0; i < cards.size(); ++i)
        if (cards[i]->isVisible())
            cards[i]->setBounds (area.getX() + i * (cw + gap), area.getY(), cw, area.getHeight());

    updateCardFonts();
}
