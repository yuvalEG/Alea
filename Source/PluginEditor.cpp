#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

using namespace ui;

namespace
{
    constexpr int kWidth = 940, kHeight = 742; // fits the OUTPUT row + LCD + monitor + history (compact output row)

    // About dialog: wordmark over a subtle vertical gradient, text below.
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
                "Aleatoric Scale Shifter, version " ALEA_VERSION "\n\n\n"
                "HOW TO USE\n\n"
                "Alea generates MIDI notes. It makes no sound of its own "
                "(unless you pick Internal Synth under OUT).\n\n"
                "1. Load Alea on a MIDI track.\n"
                "2. Create a second MIDI track and put any instrument on it.\n"
                "3. Route the instrument track's MIDI input from the Alea track "
                "(in Ableton Live: set 'MIDI From' to the Alea track and pick "
                "'Alea' in the chooser below it). In Live use the VST3, since "
                "Live cannot route MIDI from AU plugins. In Logic or GarageBand use "
                "the AU with OUT set to Internal Synth.\n"
                "4. Arm the instrument track and press Play. Alea follows the "
                "host transport and you should hear notes drawn from Scale A.\n"
                "5. From there: pick a preset, set up your own Scale A and "
                "Scale B, drag the morph bar to blend between them, or hit "
                "AUTO-SWEEP and let Alea travel on its own.\n\n"
                "Hearing nothing? Check the instrument track is armed and the "
                "header dot says 'playing'.\n\n\n"
                "THE IDEA\n\n"
                "ALEA was made with a particular vision in mind: exploring the "
                "relationship between an improvising human player and a machine "
                "that randomly shifts from a diatonic scale to complete "
                "dodecaphony over time (hence the last factory preset).\n\n"
                "That said, I'm sure any musician playing with it will have all "
                "kinds of ideas, and I hope it can serve your musical "
                "aspirations.\n\n\n"
                "GET IN TOUCH\n\n"
                "I'll be more than happy to hear your feedback, ideas and music "
                "made with ALEA! You can reach me through GitHub "
                "(github.com/yuvalEG/Alea) or my email: yuvalprod@gmail.com\n\n"
                "Alea is open source (GPLv3), built with JUCE. The piano is "
                "the Salamander Grand Piano by Alexander Holm (CC BY 3.0). "
                "Check for updates from the menu in the top-right corner.\n\n\n"
                "Plugin Made By Yuval Egozi"),
                juce::dontSendNotification);
            addAndMakeVisible (text);
            setSize (820, 760);
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

AleaAudioProcessorEditor::AleaAudioProcessorEditor (AleaAudioProcessor& p)
    : AudioProcessorEditor (p), alea (p),
      keyboardA (p, 'a', 0, colors::purple),
      keyboardB (p, 'b', 1, colors::cyan),
      restsA (p, 'a', 0, colors::purple),
      restsB (p, 'b', 1, colors::cyan),
      intervalMode (*p.apvts.getParameter ("intervalMode"), params::timingModes, colors::text.withAlpha (0.9f)),
      lengthMode   (*p.apvts.getParameter ("lengthMode"),   params::timingModes, colors::text.withAlpha (0.9f)),
      morphBar (p),
      morphDurMode (*p.apvts.getParameter ("morphDurMode"), params::morphDurModes, colors::text.withAlpha (0.9f)),
      morphMode (*p.apvts.getParameter ("morphMode"),
                 juce::StringArray { juce::String::fromUTF8 ("A \xe2\x86\x92 B"),
                                     juce::String::fromUTF8 ("A \xe2\x86\x92 B \xe2\x86\xbb"),
                                     juce::String::fromUTF8 ("A \xe2\x87\x84 B") },
                 colors::text.withAlpha (0.9f),
                 juce::StringArray { "One-Shot: travel to B, then stay there",
                                     "Loop: travel to B, jump back to A, repeat",
                                     "Bounce: back and forth between A and B" }),
      morphCurve (*p.apvts.getParameter ("morphCurve"), colors::text.withAlpha (0.9f)),
      tempoSource  (*p.apvts.getParameter ("tempoSource"),  params::tempoSources, colors::text.withAlpha (0.9f)),
      standalone (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
      output (p)
{
    // Space Grotesk everywhere, sized up: buttons, combos, menus, textboxes.
    struct AleaLookAndFeel : juce::LookAndFeel_V4
    {
        AleaLookAndFeel()
        {
            setDefaultSansSerifTypeface (juce::Typeface::createSystemTypefaceFor (
                BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize));
            // Toggle buttons light emerald by default (the hardware LED);
            // FREEZE overrides to ice, PANIC never lights (red legend).
            setColour (juce::TextButton::buttonOnColourId, ui::hw::led);
            setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc0c4d0));
            setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07120d));
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
            ui::hw::knob (g, juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height),
                          pos, slider.findColour (juce::Slider::rotarySliderFillColourId), bipolar);
        }

        // Hardware push-button: metal face, or a backlit LED key when lit.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool over, bool down) override
        {
            const bool lit = b.getToggleState();
            const auto led = b.findColour (juce::TextButton::buttonOnColourId);
            ui::hw::button (g, b.getLocalBounds().toFloat(), lit, led, over, down);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool) override
        {
            const bool lit = b.getToggleState();
            auto colour = lit ? b.findColour (juce::TextButton::textColourOnId)
                              : b.findColour (juce::TextButton::textColourOffId);
            if (over && ! lit)
                colour = colour.brighter (0.4f);
            g.setColour (colour);
            g.setFont (juce::FontOptions (juce::jmin (13.0f, (float) b.getHeight() * 0.5f)));
            g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred);
        }

        // Recessed metal combo box with a chevron.
        void drawComboBox (juce::Graphics& g, int width, int height, bool,
                           int, int, int, int, juce::ComboBox&) override
        {
            const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
            ui::hw::insetWell (g, r, 4.0f);
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
        // linear slider keeps the JUCE default draw.
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle style, juce::Slider& s) override
        {
            if (s.getComponentID() == "bpm")
            {
                const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
                ui::hw::lcd (g, r, colors::green);
                g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
                ui::hw::glowText (g, s.getTextFromValue (s.getValue()), r.toNearestInt(),
                                  juce::Justification::centred,
                                  colors::green.brighter (0.35f).withAlpha (s.isEnabled() ? 1.0f : 0.55f));
                return;
            }
            // MiniSlider: recessed track, accent fill with a soft glow, round
            // cap (JUCE_DRAWING_GUIDE section 5).
            const float cy = (float) y + (float) height * 0.5f;
            auto track = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) width, 6.0f);
            ui::hw::insetWell (g, track, 3.0f);
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
    static AleaLookAndFeel aleaLnf; // process lifetime: dialogs may outlive the editor
    juce::LookAndFeel::setDefaultLookAndFeel (&aleaLnf);

    content.addAndMakeVisible (keyboardA);
    content.addAndMakeVisible (keyboardB);
    content.addAndMakeVisible (restsA);
    content.addAndMakeVisible (restsB);
    content.addAndMakeVisible (intervalMode);
    content.addAndMakeVisible (lengthMode);
    content.addAndMakeVisible (morphBar);
    content.addAndMakeVisible (morphDurMode);
    content.addAndMakeVisible (morphMode);
    content.addAndMakeVisible (morphCurve);
    content.addAndMakeVisible (tempoSource);
    content.addAndMakeVisible (output);

    // Oct/Vel are hardware knobs (design 1:1): rotary, accent value-arc, the
    // caption + value painted below each in paintMain. Attach to params.
    auto makeKnob = [this] (juce::Slider& s, const juce::String& paramID, juce::Colour accent)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setColour (juce::Slider::rotarySliderFillColourId, accent);
        s.setVelocityBasedMode (false);
        // The value under each knob is painted in the content layer, so it
        // must repaint when the knob turns.
        s.onValueChange = [this] { content.repaint (scaleAPanel); content.repaint (scaleBPanel); };
        content.addAndMakeVisible (s);
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            alea.apvts, paramID, s));
    };
    makeKnob (aOctMin, "aOctMin", colors::purple); makeKnob (aOctMax, "aOctMax", colors::purple);
    makeKnob (aVelMin, "aVelMin", colors::purple); makeKnob (aVelMax, "aVelMax", colors::purple);
    makeKnob (bOctMin, "bOctMin", colors::cyan);   makeKnob (bOctMax, "bOctMax", colors::cyan);
    makeKnob (bVelMin, "bVelMin", colors::cyan);   makeKnob (bVelMax, "bVelMax", colors::cyan);
    setupSlider (intervalFree, "intervalFree", colors::text.withAlpha (0.6f));
    setupSlider (lengthFree, "lengthFree", colors::text.withAlpha (0.6f));
    // Free-mode timing is a knob as well, matching the synced knobs.
    for (auto* s : { &intervalFree, &lengthFree })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setColour (juce::Slider::rotarySliderFillColourId, colors::green.withAlpha (0.85f));
        // The value text ("0.5 s") is painted in the content layer, so it must
        // repaint when the knob turns.
        s->onValueChange = [this] { content.repaint (timingPanel); };
    }
    // The morph DURATION knob: bars in Sync mode, seconds in Free mode. Both
    // are knobs; the value ("2 bars" / "0.5 s") is painted beside them.
    for (auto& [slider, id] : { std::pair<juce::Slider*, const char*> { &morphDurBars, "morphDurBars" },
                                { &morphDurFree, "morphDurFree" } })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f, true);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider->setColour (juce::Slider::rotarySliderFillColourId, colors::amber);
        slider->onValueChange = [this] { content.repaint (morphPanel); };
        content.addAndMakeVisible (*slider);
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            alea.apvts, id, *slider));
    }
    morphDurBars.textFromValueFunction = [] (double v)
    {
        const int i = juce::jlimit (0, params::morphDurBarNames.size() - 1, (int) std::lround (v));
        return params::morphDurBarNames[i] + (params::morphDurBarNames[i] == "1" ? " bar" : " bars");
    };
    morphDurBars.updateText();

    setupSlider (internalTempo, "internalTempo", colors::green);
    internalTempo.setComponentID ("bpm"); // drawn as a glass green BPM LCD
    internalTempo.textFromValueFunction = [] (double v)
    { return juce::String ((int) std::lround (v)) + " BPM"; };
    internalTempo.updateText();

    // Sync divisions are fractions of a bar; in 4/4 that maps 1:1 onto note
    // values. Discrete sliders (like the octave controls): drag through the
    // divisions, the readout names them. Long left, short right.
    static const juce::StringArray divisionDisplay {
        "4 bars", "2 bars", "1 bar", "1/2 note", "1/4 note", "1/8 note",
        "1/16 note", "1/32 note", "1/64 note", "1/128 note" };
    // The synced divisions are now hardware knobs too (Yuval): dial through
    // "2 bars" ... "1/64 note"; the value is painted beside the knob.
    for (auto& [slider, id] : { std::pair<juce::Slider*, const char*> { &intervalSync, "intervalSync" },
                                { &lengthSync, "lengthSync" } })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f, true);
        slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider->setColour (juce::Slider::rotarySliderFillColourId, colors::green.withAlpha (0.85f));
        slider->onValueChange = [this] { content.repaint (timingPanel); }; // painted value follows the knob
        content.addAndMakeVisible (*slider);
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            alea.apvts, id, *slider));
        // After attaching: the attachment installs the parameter's own text
        // function ("1/16"); ours names the note value.
        slider->textFromValueFunction = [] (double v)
        { return divisionDisplay[juce::jlimit (0, divisionDisplay.size() - 1, (int) std::lround (v))]; };
        slider->updateText();
    }
    setupCombo (morphDurUnit, "morphDurUnit");

    // Root pickers: the pitch each scale's octave span starts from (a real
    // parameter - root A + octave 3 plays A3..G#4).
    setupCombo (rootABox, "aRoot");
    setupCombo (rootBBox, "bRoot");

    menuButton.setButtonText (juce::String::fromUTF8 ("\xe2\x8b\xaf"));
    menuButton.setColour (juce::TextButton::buttonColourId, colors::control);
    menuButton.setColour (juce::TextButton::textColourOffId, colors::text);
    menuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem ("Check for Updates...", []
        {
            juce::Thread::launch ([]
            {
                const auto response = juce::URL ("https://api.github.com/repos/yuvalEG/Alea/releases/latest")
                                          .readEntireTextStream();
                auto latest = juce::JSON::parse (response)
                                  .getProperty ("tag_name", juce::String()).toString()
                                  .trimCharactersAtStart ("v");
                juce::MessageManager::callAsync ([latest]
                {
                    const juce::String current (ALEA_VERSION);
                    if (latest.isEmpty())
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
                    else if (latest == current)
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                            "Check for Updates", "You're up to date. Alea " + current + ".");
                    }
                    else
                    {
                        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::InfoIcon,
                            "Update Available",
                            "Alea " + latest + " is available (you have " + current + ").",
                            "Get It", "Later", nullptr,
                            juce::ModalCallbackFunction::create ([] (int r)
                            {
                                if (r == 1)
                                    juce::URL ("https://github.com/yuvalEG/Alea/releases/latest").launchInDefaultBrowser();
                            }));
                    }
                });
            });
        });
        m.addSeparator();
        m.addItem ("About Alea...", []
        {
            juce::DialogWindow::LaunchOptions o;
            o.content.setOwned (new AboutComponent());
            o.dialogTitle = "About Alea";
            o.dialogBackgroundColour = colors::panel;
            o.escapeKeyTriggersCloseButton = true;
            o.useNativeTitleBar = true;
            o.resizable = false;
            o.launchAsync();
        });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
    };
    content.addAndMakeVisible (menuButton);

    // Standalone: no host transport, so PLAY/STOP lives where the Host/
    // Free-Run selector would be (the clock is always internal there),
    // with spacebar as the shortcut - like any DAW. The plugin never
    // takes keyboard focus; space belongs to the host there.
    setWantsKeyboardFocus (standalone);
    tempoSource.setVisible (! standalone);
    // Drawn play/pause transport - shared design with the Chord Randomizer.
    // The icon tells the truth: pausing holds the internal clock.
    playButton.onClick = [this]
    {
        alea.standaloneTransport.store (playButton.getToggleState());
    };
    playButton.setToggleState (alea.standaloneTransport.load(), juce::dontSendNotification);
    content.addChildComponent (playButton);
    playButton.setVisible (standalone);

    // Plugin only: the most common support question is routing, so the
    // answer lives one click away. The standalone synth needs no routing.
    helpLink.setButtonText ("No sound? Routing Help");
    helpLink.setURL (juce::URL ("https://github.com/yuvalEG/Alea#troubleshooting"));
    helpLink.setFont (juce::FontOptions (13.0f), false, juce::Justification::centredLeft);
    helpLink.setColour (juce::HyperlinkButton::textColourId, colors::secondary);
    content.addChildComponent (helpLink);
    helpLink.setVisible (! standalone);

    // Family design (the Chord Randomizer's version won): quiet control
    // background, red text - danger named, not shouted.
    panicButton.setColour (juce::TextButton::buttonColourId, colors::control);
    panicButton.setColour (juce::TextButton::textColourOffId, colors::red);
    panicButton.onClick = [this] { alea.panicRequested.store (true); };
    content.addAndMakeVisible (panicButton);

    // Freeze is a performance control and Panic an escape hatch - they live
    // far apart in the header so neither is hit by accident.
    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::buttonColourId, colors::control);
    freezeButton.setColour (juce::TextButton::buttonOnColourId, colors::ice); // icy, not Scale B's cyan
    freezeButton.setColour (juce::TextButton::textColourOffId, colors::secondary);
    freezeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    content.addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "freeze", freezeButton);

    // Auto-sweep is the heart of the plugin - a full toggle button, not a tick.
    autoSweep.setButtonText (juce::String::fromUTF8 ("AUTO-SWEEP \xe2\x86\x92"));
    autoSweep.setClickingTogglesState (true);
    autoSweep.setColour (juce::TextButton::buttonColourId, colors::control);
    autoSweep.setColour (juce::TextButton::buttonOnColourId, ui::hw::led);
    autoSweep.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc0c4d0));
    autoSweep.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07120d));
    content.addAndMakeVisible (autoSweep);
    sweepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "autoSweep", autoSweep);

    // Every preset is a one-click bubble; the active one stays lit.
    for (size_t i = 0; i < presets::factory().size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (juce::String::fromUTF8 (presets::factory()[i].name));
        b->setColour (juce::TextButton::buttonColourId, colors::control);
        b->setColour (juce::TextButton::buttonOnColourId, ui::hw::led);
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc0c4d0));
        b->setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07120d));
        const int idx = (int) i;
        b->onClick = [this, idx] { applyPresetAndMark (idx); };
        content.addAndMakeVisible (*b);
        presetBtns.push_back (std::move (b));
    }

    for (auto* b : { &savePreset, &loadPreset })
    {
        b->setColour (juce::TextButton::buttonColourId, colors::panel);
        b->setColour (juce::TextButton::textColourOffId, colors::secondary);
        content.addAndMakeVisible (*b);
    }

    savePreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save Alea preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.alea");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;
                auto state = alea.apvts.copyState();
                state.setProperty ("stateVersion", 2, nullptr);
                if (auto xml = state.createXml())
                    file.replaceWithText (xml->toString());
            });
    };

    loadPreset.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load Alea preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.alea");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (! file.existsAsFile())
                    return;
                if (auto xml = juce::XmlDocument::parse (file))
                    if (xml->hasTagName (alea.apvts.state.getType()))
                        alea.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            });
    };

    viewHeight = standalone ? kHeight : kHeight + 20; // plugin: footer row for the help link
    addAndMakeVisible (content);

    setResizable (true, true);
    setResizeLimits (kWidth * 4 / 5, 398 + 208 + (standalone ? 10 : 30), kWidth * 2, viewHeight * 2);
    setSize (kWidth, viewHeight); // triggers resized() -> layoutMain()

    updateModeVisibility();
    timerCallback(); // apply dimming/visibility state before first paint
    startTimerHz (15);
}


void AleaAudioProcessorEditor::applyPresetAndMark (int index)
{
    presets::apply (alea.apvts, presets::factory()[(size_t) index]);
    alea.presetReanchor.store (true); // its sweep starts fresh, not mid-journey from the last preset
    alea.currentPreset.store (index);
    shownPreset = index;
    // Don't snapshot yet: the host echoes parameter edits back asynchronously
    // with its own rounding, which would read as instant divergence.
    presetSnapshot.clear();
    snapshotCountdown = 4;
    markPreset (index);
}

void AleaAudioProcessorEditor::markPreset (int index)
{
    for (size_t k = 0; k < presetBtns.size(); ++k)
        presetBtns[k]->setToggleState ((int) k == index, juce::dontSendNotification);
}

void AleaAudioProcessorEditor::setupSlider (juce::Slider& s, const juce::String& paramID, juce::Colour accent,
                                            bool positionStyle)
{
    if (positionStyle)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 30, 20);
        s.setColour (juce::Slider::thumbColourId, accent);
        s.setColour (juce::Slider::trackColourId, colors::control);
    }
    else
    {
        s.setSliderStyle (juce::Slider::LinearBar);
        s.setColour (juce::Slider::trackColourId, accent.withAlpha (0.55f));
    }
    s.setColour (juce::Slider::backgroundColourId, colors::control);
    s.setColour (juce::Slider::textBoxTextColourId, colors::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, colors::border);
    content.addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        alea.apvts, paramID, s));
}

void AleaAudioProcessorEditor::setupCombo (juce::ComboBox& c, const juce::String& paramID,
                                           const juce::StringArray& customLabels)
{
    if (customLabels.size() > 0)
        c.addItemList (customLabels, 1);
    else if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (alea.apvts.getParameter (paramID)))
        c.addItemList (choice->choices, 1);

    c.setColour (juce::ComboBox::backgroundColourId, colors::control);
    c.setColour (juce::ComboBox::textColourId, colors::text);
    c.setColour (juce::ComboBox::outlineColourId, colors::border);
    c.setColour (juce::ComboBox::arrowColourId, colors::secondary);
    content.addAndMakeVisible (c);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        alea.apvts, paramID, c));
}

void AleaAudioProcessorEditor::layoutMain()
{
    // Responsive geometry: panels stretch with the window, controls keep
    // their size. Extra height goes to the bottom row (monitoring benefits).
    const int vw = juce::jmax (kWidth * 4 / 5, content.getWidth());
    const int vh = juce::jmax (kHeight * 4 / 5, content.getHeight());
    const int footer = standalone ? 8 : 28;

    presetsPanel = { 10, 64, vw - 20, 76 };
    const int scaleW = (vw - 30) / 2;
    const int scaleH = 264 + juce::jmax (0, vh - kHeight) * 2 / 5; // keyboards grow on big windows
    scaleAPanel = { 10, 148, scaleW, scaleH };
    scaleBPanel = { 20 + scaleW, 148, vw - 30 - scaleW, scaleH };

    const int by = 148 + scaleH + 10;
    const int bh = juce::jmax (208, vh - by - footer);
    const int avail = vw - 40;
    const int tw = avail * 29 / 90;
    const int mw = avail * 33 / 90;
    timingPanel = { 10, by, tw, bh };
    morphPanel  = { 20 + tw, by, mw, bh };
    outputPanel = { 30 + tw + mw, by, avail - tw - mw, bh };

    // Short windows drop the morph panel's lower rows instead of clipping.
    morphMode.setVisible (bh >= 232);
    morphCurve.setVisible (bh >= 278);

    // Header
    // Header: left cluster fixed, right cluster anchored to the window edge
    freezeButton.setBounds (vw - 560, 16, 80, 26);
    menuButton.setBounds (vw - 38, 16, 28, 26);
    panicButton.setBounds (vw - 120, 16, 72, 26);
    internalTempo.setBounds (vw - 232, 16, 100, 26);
    tempoSource.setBounds (vw - 410, 16, 140, 26);
    playButton.setBounds (vw - 412, 16, 76, 26);

    // Bottom of each scale plate, top to bottom: keybed, RESTS + ROOT row,
    // then a row of four knobs (OCT LO / OCT HI / VEL LO / VEL HI).
    auto scaleControls = [] (const juce::Rectangle<int>& panel, PianoKeyboard& kb, RestSelector& rests,
                                 juce::Slider& octMin, juce::Slider& octMax,
                                 juce::Slider& velMin, juce::Slider& velMax)
    {
        const int x = panel.getX() + 12, w = panel.getWidth() - 24;
        const int knobRowH = 92;
        const int restsY = panel.getBottom() - knobRowH - 34;
        kb.setBounds (x, panel.getY() + 30, w, restsY - (panel.getY() + 30) - 10);
        rests.setBounds (x + 44, restsY, w - 44 - 110, 26);

        const int col = w / 4;
        const int knob = juce::jmin (58, col - 8);
        const int ky = panel.getBottom() - knobRowH + 4;
        juce::Slider* ks[] = { &octMin, &octMax, &velMin, &velMax };
        for (int i = 0; i < 4; ++i)
            ks[i]->setBounds (x + col * i + (col - knob) / 2, ky, knob, knob);
    };

    rootABox.setBounds (scaleAPanel.getRight() - 12 - 58, scaleAPanel.getBottom() - 92 - 34, 58, 26);
    rootBBox.setBounds (scaleBPanel.getRight() - 12 - 58, scaleBPanel.getBottom() - 92 - 34, 58, 26);

    scaleControls (scaleAPanel, keyboardA, restsA, aOctMin, aOctMax, aVelMin, aVelMax);
    scaleControls (scaleBPanel, keyboardB, restsB, bOctMin, bOctMax, bVelMin, bVelMax);

    // Timing: each row is a Sync/Free/Random selector, then a knob (synced
    // divisions or free seconds) with its value painted beside it.
    {
        const int x = timingPanel.getX() + 12, w = timingPanel.getWidth() - 24;
        constexpr int knob = 50;
        intervalMode.setBounds (x, timingPanel.getY() + 48, w, 26);
        intervalSync.setBounds (x, timingPanel.getY() + 82, knob, knob);
        intervalFree.setBounds (x, timingPanel.getY() + 82, knob, knob);
        lengthMode.setBounds (x, timingPanel.getY() + 156, w, 26);
        lengthSync.setBounds (x, timingPanel.getY() + 190, knob, knob);
        lengthFree.setBounds (x, timingPanel.getY() + 190, knob, knob);
    }

    // Morph
    {
        const int x = morphPanel.getX() + 12, w = morphPanel.getWidth() - 24;
        morphBar.setBounds (x, morphPanel.getY() + 34, w, 34);
        // AUTO-SWEEP full width, then a duration row: SYNC/FREE (with the
        // Seconds/Minutes unit below it in Free mode) on the left, and the
        // DURATION knob on the right.
        autoSweep.setBounds (x, morphPanel.getY() + 78, w, 28);
        const int knob = 50, durY = morphPanel.getY() + 116;
        morphDurMode.setBounds (x, durY, w - knob - 70, 26);
        morphDurUnit.setBounds (x, durY + 32, w - knob - 70, 26); // free mode only
        morphDurBars.setBounds (morphPanel.getRight() - 12 - knob, durY, knob, knob);
        morphDurFree.setBounds (morphPanel.getRight() - 12 - knob, durY, knob, knob);
        morphMode.setBounds (x, morphPanel.getY() + 194, w, 26);
        morphCurve.setBounds (x, morphPanel.getY() + 238, w, 26);
    }

    output.setBounds (outputPanel.getX() + 12, outputPanel.getY() + 34,
                      outputPanel.getWidth() - 24, outputPanel.getHeight() - 46);

    // Presets panel: two rows of four bubbles, Save/Load stacked at the right
    {
        const int x0 = presetsPanel.getX() + 90;
        const int rowW = presetsPanel.getRight() - 76 - x0;
        const int perRow = 5;
        const int w = (rowW - (perRow - 1) * 8) / perRow;
        for (size_t k = 0; k < presetBtns.size(); ++k)
        {
            const int row = (int) k / perRow, col = (int) k % perRow;
            presetBtns[k]->setBounds (x0 + col * (w + 8), presetsPanel.getY() + 10 + row * 30, w, 26);
        }
        savePreset.setBounds (presetsPanel.getRight() - 64, presetsPanel.getY() + 10, 54, 26);
        loadPreset.setBounds (presetsPanel.getRight() - 64, presetsPanel.getY() + 40, 54, 26);
    }

    helpLink.setBounds (12, vh - 22, 240, 18); // left: the resize handle owns the right corner
}

bool AleaAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (standalone && key == juce::KeyPress::spaceKey)
    {
        playButton.triggerClick(); // toggles and fires onClick
        return true;
    }
    return false;
}

void AleaAudioProcessorEditor::timerCallback()
{
    updateModeVisibility();

    // The engine owns "which preset is active" (it survives editor close/
    // reopen and session reload); the bubbles follow it.
    {
        const int cp = alea.currentPreset.load();
        if (cp != shownPreset)
        {
            shownPreset = cp;
            markPreset (cp);
            presetSnapshot.clear();
            if (cp >= 0)
                snapshotCountdown = 2; // arm the divergence check
        }
    }

    // A fully one-sided morph means the other scale can't sound - dim it.
    {
        const bool sweep = alea.apvts.getRawParameterValue ("autoSweep")->load() > 0.5f;
        const float pct = sweep ? (float) alea.morphPercent.load()
                                : alea.apvts.getRawParameterValue ("morphPos")->load();
        const float newA = pct >= 99.95f ? 0.35f : 1.0f;
        const float newB = pct <= 0.05f  ? 0.35f : 1.0f;

        if (! juce::approximatelyEqual (newA, alphaA) || ! juce::approximatelyEqual (newB, alphaB))
        {
            alphaA = newA;
            alphaB = newB;
            for (auto* c : { (juce::Component*) &keyboardA, (juce::Component*) &restsA,
                             (juce::Component*) &aOctMin, (juce::Component*) &aOctMax,
                             (juce::Component*) &aVelMin, (juce::Component*) &aVelMax })
                c->setAlpha (alphaA);
            for (auto* c : { (juce::Component*) &keyboardB, (juce::Component*) &restsB,
                             (juce::Component*) &bOctMin, (juce::Component*) &bOctMax,
                             (juce::Component*) &bVelMin, (juce::Component*) &bVelMax })
                c->setAlpha (alphaB);
            content.repaint (scaleAPanel);
            content.repaint (scaleBPanel);
        }
    }

    // Any manual tweak after applying a preset un-marks it. The snapshot is
    // captured a few ticks after apply (host echoes settle), and compared
    // with a tolerance well above echo jitter but below any human tweak.
    if (snapshotCountdown > 0)
    {
        if (--snapshotCountdown == 0)
            for (auto* param : alea.getParameters())
                presetSnapshot.push_back (param->getValue());
    }
    else if (! presetSnapshot.empty())
    {
        const auto& ps = alea.getParameters();
        for (size_t i = 0; i < (size_t) ps.size() && i < presetSnapshot.size(); ++i)
        {
            if (std::abs (ps[(int) i]->getValue() - presetSnapshot[i]) > 4.0e-3f)
            {
                presetSnapshot.clear();
                alea.currentPreset.store (-1);
                shownPreset = -1;
                markPreset (-1);
                break;
            }
        }
    }

    keyboardA.repaint();
    keyboardB.repaint();
    restsA.repaint();
    restsB.repaint();
    morphBar.repaint();
    output.repaint();
    content.repaint (0, 0, content.getWidth(), 60); // header status dot

    // Random-mode "current pick" readouts live in the timing panel
    if ((int) alea.apvts.getRawParameterValue ("intervalMode")->load() == params::random
        || (int) alea.apvts.getRawParameterValue ("lengthMode")->load() == params::random)
        content.repaint (timingPanel);
}

void AleaAudioProcessorEditor::updateModeVisibility()
{
    const bool was[] = { intervalSync.isVisible(), intervalFree.isVisible(),
                         lengthSync.isVisible(), lengthFree.isVisible() };
    const int iMode = (int) alea.apvts.getRawParameterValue ("intervalMode")->load();
    intervalSync.setVisible (iMode == params::sync);
    intervalFree.setVisible (iMode == params::free);

    const int lMode = (int) alea.apvts.getRawParameterValue ("lengthMode")->load();
    lengthSync.setVisible (lMode == params::sync);
    lengthFree.setVisible (lMode == params::free);

    // Repaint the painted timing value only when a mode actually switched a
    // knob's visibility (not every timer tick).
    if (was[0] != intervalSync.isVisible() || was[1] != intervalFree.isVisible()
        || was[2] != lengthSync.isVisible() || was[3] != lengthFree.isVisible())
        content.repaint (timingPanel);

    const bool durWasSync = morphDurBars.isVisible();
    const bool durSync = (int) alea.apvts.getRawParameterValue ("morphDurMode")->load() == 0;
    morphDurBars.setVisible (durSync);
    morphDurFree.setVisible (! durSync);
    morphDurUnit.setVisible (! durSync);
    if (durWasSync != durSync)
        content.repaint (morphPanel); // the painted DURATION value follows the mode

    const bool freeRun = standalone
                         || (int) alea.apvts.getRawParameterValue ("tempoSource")->load() == 1;
    internalTempo.setEnabled (freeRun);
    internalTempo.setAlpha (freeRun ? 1.0f : 0.4f);
}

void AleaAudioProcessorEditor::resized()
{
    // Responsive: the view fills the window and re-lays itself out.
    content.setBounds (getLocalBounds());
    layoutMain();
}

void AleaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colors::background);
}

void AleaAudioProcessorEditor::paintMain (juce::Graphics& g)
{
    // The whole window is one raised brushed-gunmetal faceplate slab.
    ui::hw::brushedMetal (g, content.getLocalBounds().toFloat(), 16.0f, false);

    // Header: the wordmark image, with the subtitle beside it
    {
        static const juce::Image logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
        if (logo.isValid())
            g.drawImage (logo, juce::Rectangle<float> (20.0f, 12.0f, 87.0f, 34.0f),
                         juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    }
    // Subtitle and status text yield at narrow widths - the right cluster
    // (FREEZE..menu) is anchored to the window edge and must not collide.
    const bool playing = alea.hostIsPlaying.load();
    if (freezeButton.getX() >= 378)
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Aleatoric Scale Shifter", 120, 16, 180, 28, juce::Justification::centredLeft);

        g.setColour (playing ? colors::green : colors::control);
        g.fillEllipse (262.0f, 22.0f, 12.0f, 12.0f);
        g.setColour (colors::secondary);
        g.drawText (playing ? "playing" : "stopped", 282, 14, 90, 28, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (playing ? colors::green : colors::control);
        g.fillEllipse (126.0f, 22.0f, 12.0f, 12.0f); // dot only, next to the logo
    }

    // (The tempo bar reads "120 BPM" itself; the word "TEMPO" was dropped in
    // the hardware redesign - the HOST/FREE-RUN switch names itself.)

    // Each panel is a recessed module plate with two corner screws and an
    // engraved label. Scale plates carry a colour tab that dims when their
    // side of the morph can't currently sound.
    auto drawPlate = [&g] (const juce::Rectangle<int>& r, const juce::String& title,
                           juce::Colour tab, float tabAlpha)
    {
        const auto rf = r.toFloat();
        ui::hw::brushedMetal (g, rf, 10.0f, true);
        ui::hw::screw (g, { rf.getX() + 10.0f, rf.getY() + 10.0f }, 42.0f);
        ui::hw::screw (g, { rf.getRight() - 10.0f, rf.getY() + 10.0f }, -20.0f);

        float tx = rf.getX() + 22.0f; // clear the top-left screw
        if (tab.getAlpha() > 0)
        {
            g.setColour (tab.withMultipliedAlpha (tabAlpha));
            g.fillRoundedRectangle (tx, rf.getY() + 13.0f, 20.0f, 4.0f, 2.0f);
            tx += 28.0f;
        }
        // Engraved panel title - larger than the sub-labels inside the plate.
        g.setFont (juce::Font (juce::FontOptions (15.5f)).boldened());
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText (title, (int) tx + 1, r.getY() + 8, r.getWidth(), 18, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffc2c5d0));
        g.drawText (title, (int) tx, r.getY() + 7, r.getWidth(), 18, juce::Justification::centredLeft);
    };

    drawPlate (scaleAPanel, "SCALE A", colors::purple, alphaA);
    drawPlate (scaleBPanel, "SCALE B", colors::cyan, alphaB);
    drawPlate (timingPanel, "TIMING", juce::Colours::transparentBlack, 1.0f);
    drawPlate (morphPanel, "SCALE MORPH", juce::Colours::transparentBlack, 1.0f);
    drawPlate (outputPanel, "OUTPUT", juce::Colours::transparentBlack, 1.0f);
    drawPlate (presetsPanel, "PRESETS", juce::Colours::transparentBlack, 1.0f);

    // Small row labels
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));

    struct ScaleRef { juce::Rectangle<int>* panel; float alpha; char id;
                      juce::Slider* octMin; juce::Slider* octMax; juce::Slider* velMin; juce::Slider* velMax; };
    for (const auto& s : { ScaleRef { &scaleAPanel, alphaA, 'a', &aOctMin, &aOctMax, &aVelMin, &aVelMax },
                           ScaleRef { &scaleBPanel, alphaB, 'b', &bOctMin, &bOctMax, &bVelMin, &bVelMax } })
    {
        g.setColour (colors::secondary.withMultipliedAlpha (s.alpha));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("RESTS", s.panel->getX() + 12, s.panel->getBottom() - 126, 40, 26, juce::Justification::centredLeft);
        g.drawText ("ROOT", s.panel->getRight() - 12 - 58 - 44, s.panel->getBottom() - 126, 40, 26, juce::Justification::centredRight);

        // Each knob's caption + live value, painted below the dial.
        auto knobLabel = [&] (juce::Slider* k, const juce::String& cap, const juce::String& id)
        {
            const auto b = k->getBounds();
            const int val = (int) std::lround (alea.apvts.getRawParameterValue (id)->load());
            g.setColour (colors::secondary.withMultipliedAlpha (s.alpha));
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (cap, b.getX() - 20, b.getBottom() - 2, b.getWidth() + 40, 13, juce::Justification::centred);
            g.setColour (colors::text.withMultipliedAlpha (s.alpha));
            g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
            g.drawText (juce::String (val), b.getX() - 20, b.getBottom() + 10, b.getWidth() + 40, 15, juce::Justification::centred);
        };
        knobLabel (s.octMin, "OCT LO", juce::String::charToString (s.id) + "OctMin");
        knobLabel (s.octMax, "OCT HI", juce::String::charToString (s.id) + "OctMax");
        knobLabel (s.velMin, "VEL LO", juce::String::charToString (s.id) + "VelMin");
        knobLabel (s.velMax, "VEL HI", juce::String::charToString (s.id) + "VelMax");
    }
    g.setColour (colors::secondary);

    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened()); // sub-labels: smaller than the panel title
    g.drawText ("NOTE RATE",   timingPanel.getX() + 12, timingPanel.getY() + 32, 120, 14, juce::Justification::centredLeft);
    g.drawText ("NOTE LENGTH", timingPanel.getX() + 12, timingPanel.getY() + 140, 120, 14, juce::Justification::centredLeft);

    // The value of each timing knob, painted to its right ("1/4 note", "0.5 s").
    auto timingValue = [&] (juce::Slider& sync, juce::Slider& freeS, int mode)
    {
        if (mode == params::random)
            return;
        auto& k = (mode == params::sync) ? sync : freeS;
        g.setColour (colors::text);
        g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
        g.drawText (k.getTextFromValue (k.getValue()),
                    k.getRight() + 10, k.getY(), timingPanel.getRight() - k.getRight() - 18, k.getHeight(),
                    juce::Justification::centredLeft);
    };
    const int iMode = (int) alea.apvts.getRawParameterValue ("intervalMode")->load();
    const int lMode = (int) alea.apvts.getRawParameterValue ("lengthMode")->load();
    timingValue (intervalSync, intervalFree, iMode);
    timingValue (lengthSync, lengthFree, lMode);

    // Random-mode monitoring: show what the dice just rolled, beside the row.
    auto drawRandomPick = [&] (int y, int poolIndex)
    {
        g.setColour (colors::text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (poolIndex >= 0 ? "now: " + params::randomPoolNames[poolIndex] : "now: -",
                    timingPanel.getX() + 12, y, timingPanel.getWidth() - 24, 26, juce::Justification::centredLeft);
        g.setColour (colors::secondary);
    };

    if (iMode == params::random)
        drawRandomPick (timingPanel.getY() + 92, alea.lastRandomInterval.load());
    if (lMode == params::random)
        drawRandomPick (timingPanel.getY() + 200, alea.lastRandomLength.load());
    // DURATION knob caption + value (the visible knob: bars in Sync, seconds
    // in Free), painted below it.
    {
        auto& durK = morphDurBars.isVisible() ? morphDurBars : morphDurFree;
        const auto kb = durK.getBounds();
        g.setColour (colors::secondary);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.drawText ("DURATION", kb.getX() - 24, kb.getBottom() - 2, kb.getWidth() + 48, 13, juce::Justification::centred);
        g.setColour (colors::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
        g.drawText (durK.getTextFromValue (durK.getValue()),
                    kb.getX() - 24, kb.getBottom() + 10, kb.getWidth() + 48, 14, juce::Justification::centred);
    }

    g.setColour (colors::secondary);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    if (morphMode.isVisible())
        g.drawText ("MORPH MODE",  morphPanel.getX() + 12, morphPanel.getY() + 178, 130, 12, juce::Justification::centredLeft);
    if (morphCurve.isVisible())
        g.drawText ("MORPH CURVE", morphPanel.getX() + 12, morphPanel.getY() + 222, 130, 12, juce::Justification::centredLeft);
}
