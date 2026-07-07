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
                "Press ROLL (or the spacebar) and get a random series of chords. "
                "Play them, or improvise over them.\n\n"
                "CHORDS sets how many chords each roll gives you - a series of 1 "
                "behaves like a classic flash card.\n\n"
                "Use Seventh Chords adds sevenths to the mix. Simplify Chords "
                "narrows the roll to guitar-friendly keys and mostly major or "
                "minor chords.\n\n"
                "Your past rolls pile up in HISTORY at the bottom of the "
                "window.\n\n\n"
                "THE EXERCISE\n\n"
                "This app was born from an improvisation exercise by my guitar "
                "teacher, Yonatan Benaroche: generate a short random chord "
                "progression, loop it, and improvise over it with your "
                "instrument. A progression you did not choose forces your ear "
                "and your hands out of familiar shapes.\n\n"
                "A future version will play the loop for you - for now, ROLL a "
                "series and jam.\n\n\n"
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
    g.setColour (colors::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    // Fit the chord name: start big, shrink until it fits the card.
    const float availW = b.getWidth() * 0.86f;
    float size = juce::jmax (18.0f, b.getHeight() * 0.42f);
    juce::Font font { juce::FontOptions (size) };
    const float w = textWidth (font, text);
    if (w > availW)
        font = juce::FontOptions (juce::jmax (14.0f, size * availW / w));

    g.setColour (colors::text);
    g.setFont (font);
    g.drawText (text, getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
void ChordsEditor::LengthSelector::paint (juce::Graphics& g)
{
    const float segW = (float) getWidth() / 8.0f;
    for (int i = 0; i < 8; ++i)
    {
        auto seg = juce::Rectangle<float> (segW * (float) i, 0.0f, segW, (float) getHeight()).reduced (2.0f, 0.0f);
        const bool selected = (i + 1 == value);

        g.setColour (selected ? colors::text.withAlpha (0.92f) : colors::control);
        g.fillRoundedRectangle (seg, 6.0f);
        if (! selected)
        {
            g.setColour (colors::border);
            g.drawRoundedRectangle (seg.reduced (0.5f), 6.0f, 1.0f);
        }

        g.setColour (selected ? juce::Colours::black : colors::secondary);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (juce::String (i + 1), seg, juce::Justification::centred);
    }
}

void ChordsEditor::LengthSelector::mouseDown (const juce::MouseEvent& e)
{
    const int idx = juce::jlimit (0, 7, (int) ((float) e.x / ((float) getWidth() / 8.0f)));
    if (idx + 1 != value && onChange != nullptr)
        onChange (idx + 1);
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

    // Newest roll sits at the right edge; older rolls march left and fade.
    const juce::Font font { juce::FontOptions (17.0f) };
    constexpr float chordGap = 10.0f, groupGap = 24.0f;
    float rightX = area.getRight();
    int age = 0;

    for (const auto& roll : proc.history)
    {
        float groupW = 0.0f;
        for (const auto& c : roll)
            groupW += textWidth (font, c.text()) + chordGap;
        groupW -= chordGap;

        const float startX = rightX - groupW;
        if (startX < area.getX())
            break; // older rolls have scrolled off the left edge

        const float alpha = juce::jmax (0.25f, 1.0f - 0.15f * (float) age);

        // A hair of a divider between this roll and the newer one to its right.
        if (age > 0)
        {
            g.setColour (colors::border.withAlpha (alpha + 0.15f));
            g.fillRect (juce::Rectangle<float> (rightX + groupGap / 2.0f, area.getY() + 4.0f,
                                                1.0f, area.getHeight() - 8.0f));
        }

        g.setColour (colors::text.withAlpha (alpha * (age == 0 ? 1.0f : 0.85f)));
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
}

//==============================================================================
ChordsEditor::ChordsEditor (ChordsProcessor& p)
    : AudioProcessorEditor (p), chordsProc (p), ticker (p)
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
        juce::Font getPopupMenuFont() override { return juce::FontOptions (16.0f); }
        juce::Font getLabelFont (juce::Label& label) override
        { return juce::FontOptions (juce::jmin (15.5f, (float) label.getHeight() * 0.72f)); }
    };
    static ChordsLookAndFeel chordsLnf; // process lifetime: dialogs may outlive the editor
    juce::LookAndFeel::setDefaultLookAndFeel (&chordsLnf);

    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    for (int i = 0; i < 8; ++i)
        addChildComponent (cards.add (new ChordCard()));

    rollButton.setColour (juce::TextButton::buttonColourId, colors::control.brighter (0.06f));
    rollButton.setColour (juce::TextButton::textColourOffId, colors::text);
    rollButton.onClick = [this] { doRoll(); };
    addAndMakeVisible (rollButton);

    lengthSelector.onChange = [this] (int n)
    {
        chordsProc.setSeriesLength (n);
        refresh();
    };
    addAndMakeVisible (lengthSelector);

    seventhToggle.onClick = [this] { chordsProc.useSevenths = seventhToggle.getToggleState(); };
    simplifyToggle.onClick = [this] { chordsProc.simplify = simplifyToggle.getToggleState(); };
    addAndMakeVisible (seventhToggle);
    addAndMakeVisible (simplifyToggle);

    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    menuButton.setColour (juce::TextButton::buttonColourId, colors::control);
    menuButton.setColour (juce::TextButton::textColourOffId, colors::text);
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    addAndMakeVisible (ticker);

    // Space must always mean ROLL: no child ever takes keyboard focus.
    for (auto* c : { (juce::Component*) &rollButton, (juce::Component*) &menuButton,
                     (juce::Component*) &seventhToggle, (juce::Component*) &simplifyToggle,
                     (juce::Component*) &lengthSelector })
        c->setWantsKeyboardFocus (false);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    setResizeLimits (560, 380, 4096, 2400);
    setSize (chordsProc.lastUIWidth, chordsProc.lastUIHeight);

    refresh();
    startTimerHz (10);
}

ChordsEditor::~ChordsEditor()
{
    stopTimer();
}

void ChordsEditor::timerCallback()
{
    if (seenRevision != chordsProc.revision)
        refresh();
}

void ChordsEditor::refresh()
{
    seenRevision = chordsProc.revision;

    lengthSelector.value = chordsProc.seriesLength;
    lengthSelector.repaint();
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

void ChordsEditor::doRoll()
{
    chordsProc.rollSeries();
    refresh();
}

bool ChordsEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey || key == juce::KeyPress::returnKey
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

    // Header: wordmark, subtitle, and a whisper of the family gradient.
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (20.0f, 12.0f, 87.0f, 34.0f),
                     juce::RectanglePlacement::centred);

    if (getWidth() > 430)
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Chord Randomizer", 118, 0, 220, 56, juce::Justification::centredLeft);
    }

    g.setGradientFill (juce::ColourGradient (colors::purple.withAlpha (0.35f), 0.0f, 0.0f,
                                             colors::cyan.withAlpha (0.35f), (float) getWidth(), 0.0f, false));
    g.fillRect (0, 55, getWidth(), 1);

    // Caption for the length selector.
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("CHORDS", lengthSelector.getX(), lengthSelector.getY() - 16,
                lengthSelector.getWidth(), 14, juce::Justification::centredLeft);
}

void ChordsEditor::resized()
{
    chordsProc.lastUIWidth = getWidth();
    chordsProc.lastUIHeight = getHeight();

    auto b = getLocalBounds();
    auto header = b.removeFromTop (56);
    menuButton.setBounds (header.getRight() - 48, 14, 36, 28);

    auto hist = b.removeFromBottom (100).reduced (16, 0).withTrimmedBottom (12);
    ticker.setBounds (hist);

    auto controls = b.removeFromBottom (76).reduced (16, 0);
    auto row = controls.withSizeKeepingCentre (controls.getWidth(), 40);
    rollButton.setBounds (row.removeFromLeft (juce::jmin (150, row.getWidth() / 4)));
    row.removeFromLeft (20);

    const int segTotal = juce::jmin (272, juce::jmax (176, row.getWidth() - 240));
    lengthSelector.setBounds (row.removeFromLeft (segTotal).withSizeKeepingCentre (segTotal, 30));
    row.removeFromLeft (20);

    // The two engine toggles, stacked.
    auto toggles = row.withSizeKeepingCentre (row.getWidth(), 48);
    seventhToggle.setBounds (toggles.removeFromTop (24));
    simplifyToggle.setBounds (toggles);

    // Series cards fill the middle, growing with the window.
    auto area = b.reduced (16, 14);
    const int n = juce::jmax (1, (int) chordsProc.series.size());
    constexpr int gap = 12;
    const int cw = (area.getWidth() - gap * (n - 1)) / juce::jmax (1, n);
    for (int i = 0; i < cards.size(); ++i)
        if (cards[i]->isVisible())
            cards[i]->setBounds (area.getX() + i * (cw + gap), area.getY(), cw, area.getHeight());
}
