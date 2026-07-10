#include "PluginEditor.h"
#include "Presets.h"
#include "BinaryData.h"

using namespace ui;

namespace
{
    constexpr int kWidth = 940, kHeight = 742; // fits the OUTPUT row + LCD + monitor + history (compact output row)

    // The product's About text; the dialog shell is the shared family one.
    const juce::String aboutText = juce::String::fromUTF8 (
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
                "Plugin Made By Yuval Egozi");
}

AleaAudioProcessorEditor::AleaAudioProcessorEditor (AleaAudioProcessor& p)
    : AudioProcessorEditor (p), alea (p),
      keyboardA (p, 'a', 0, colors::purple),
      keyboardB (p, 'b', 1, colors::cyan),
      restsA (p, 'a', 0, colors::purple),
      restsB (p, 'b', 1, colors::cyan),
      intervalMode (*p.apvts.getParameter ("intervalMode"), params::timingModes, ui::hw::led),
      lengthMode   (*p.apvts.getParameter ("lengthMode"),   params::timingModes, ui::hw::led),
      morphBar (p),
      // Morph controls light amber (the panel accent) - gated to grey by Auto-Sweep.
      morphDurMode (*p.apvts.getParameter ("morphDurMode"), params::morphDurModes, colors::amber),
      morphMode (*p.apvts.getParameter ("morphMode"),
                 juce::StringArray { juce::String::fromUTF8 ("A \xe2\x86\x92 B"),
                                     juce::String::fromUTF8 ("A \xe2\x86\x92 B \xe2\x86\xbb"),
                                     juce::String::fromUTF8 ("A \xe2\x87\x84 B") },
                 colors::amber,
                 juce::StringArray { "One-Shot: travel to B, then stay there",
                                     "Loop: travel to B, jump back to A, repeat",
                                     "Bounce: back and forth between A and B" }),
      morphCurve (*p.apvts.getParameter ("morphCurve"), colors::amber),
      tempoSource  (*p.apvts.getParameter ("tempoSource"),  params::tempoSources, ui::hw::led),
      standalone (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
      output (p)
{
    // The family LookAndFeel (Hardware.h): Space Grotesk, hardware chrome.
    juce::LookAndFeel::setDefaultLookAndFeel (&ui::hardwareLookAndFeel());

    content.addAndMakeVisible (keyboardA);
    content.addAndMakeVisible (keyboardB);
    content.addAndMakeVisible (restsA);
    content.addAndMakeVisible (restsB);
    content.addAndMakeVisible (intervalMode);
    content.addAndMakeVisible (lengthMode);
    intervalMode.setVertical (true); // Sync/Free/Random stacked; knob to the right
    lengthMode.setVertical (true);
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
    // Free mode auto-formats seconds -> minutes in the readout (no unit control).
    morphDurFree.textFromValueFunction = [] (double v) { return params::morphTimeString ((float) v); };
    morphDurFree.updateText();

    // Double-click-editable value fields over each knob's readout (octave,
    // velocity, free-time, morph duration). Integers parse themselves; the
    // free-time fields accept seconds, or "N min".
    auto secs = [] (const juce::String& t)
    {
        const double n = t.retainCharacters ("0123456789.").getDoubleValue();
        return t.containsIgnoreCase ("min") ? n * 60.0 : n;
    };
    scaleAFields = { &addValueField (aOctMin, "aOctMin"), &addValueField (aOctMax, "aOctMax"),
                     &addValueField (aVelMin, "aVelMin"), &addValueField (aVelMax, "aVelMax") };
    scaleBFields = { &addValueField (bOctMin, "bOctMin"), &addValueField (bOctMax, "bOctMax"),
                     &addValueField (bVelMin, "bVelMin"), &addValueField (bVelMax, "bVelMax") };
    for (auto& fp : fieldPositions) fp.dy = 10;                       // oct/vel readouts sit +10
    intervalFreeField = &addValueField (intervalFree, "intervalFree", secs);
    lengthFreeField   = &addValueField (lengthFree,   "lengthFree",   secs);
    fieldPositions[fieldPositions.size() - 2].dy = 16;               // timing free readouts sit +16
    fieldPositions[fieldPositions.size() - 1].dy = 16;
    morphDurFreeField = &addValueField (morphDurFree, "morphDurFree", secs);
    fieldPositions.back().dy = 10;                                    // duration readout sits +10
    // Free fields start hidden (default modes are Sync); updateModeVisibility toggles them.
    intervalFreeField->setVisible (false);
    lengthFreeField->setVisible (false);
    morphDurFreeField->setVisible (false);

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
            ui::showAboutDialog ("About Alea", aboutText, 20.5f, 820, 760);
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
        content.repaint (playButton.getBounds().expanded (18)); // its bloom

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
    autoSweep.setColour (juce::TextButton::buttonOnColourId, colors::amber); // master lights amber
    autoSweep.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffc0c4d0));
    autoSweep.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff07120d));
    content.addAndMakeVisible (autoSweep);
    sweepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "autoSweep", autoSweep);
    // Auto-Sweep gates the four dependent morph controls: amber when ON, matte
    // grey when OFF. They stay fully interactive either way (never disabled,
    // values never cleared) - only the backlight colour changes.
    autoSweep.onStateChange = [this] { updateSweepGating(); };
    // The blooms behind FREEZE / AUTO-SWEEP live in paintMain - repaint the
    // metal around each key as its backlight crossfades.
    freezeButton.onLitChange = [this] { content.repaint (freezeButton.getBounds().expanded (18)); };
    autoSweep.onLitChange    = [this] { content.repaint (autoSweep.getBounds().expanded (18)); };
    updateSweepGating();

    // Every preset is a one-click bubble; the active one stays lit.
    for (size_t i = 0; i < presets::factory().size(); ++i)
    {
        auto b = std::make_unique<ui::AnimatedButton> (juce::String::fromUTF8 (presets::factory()[i].name));
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

    scaleAnim.onFrame = [this] { return advanceScaleAlpha(); };
    updateModeVisibility();
    timerCallback(); // apply dimming/visibility state before first paint
    alphaA = targetAlphaA; alphaB = targetAlphaB; advanceScaleAlpha(); // snap on open
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

juce::Label& AleaAudioProcessorEditor::addValueField (juce::Slider& knob, const juce::String& paramID,
                                                      std::function<double (const juce::String&)> parse)
{
    auto* rp = dynamic_cast<juce::RangedAudioParameter*> (alea.apvts.getParameter (paramID));
    jassert (rp != nullptr);

    auto label = std::make_unique<juce::Label>();
    auto* lp = label.get();
    lp->setJustificationType (juce::Justification::centred);
    lp->setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
    lp->setColour (juce::Label::textColourId, colors::text);
    lp->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    lp->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    lp->setColour (juce::Label::backgroundWhenEditingColourId, colors::control);
    lp->setColour (juce::Label::textWhenEditingColourId, colors::text);
    lp->setColour (juce::Label::outlineWhenEditingColourId, colors::green);
    lp->setEditable (false, true, false); // double-click to type a value (like the BPM box)
    content.addAndMakeVisible (lp);

    // Keep the field showing the parameter's own text unless the user is typing.
    auto att = std::make_unique<juce::ParameterAttachment> (*rp,
        [lp, rp] (float) { if (! lp->isBeingEdited()) lp->setText (rp->getCurrentValueAsText(), juce::dontSendNotification); });
    att->sendInitialUpdate();

    lp->onTextChange = [rp, lp, parse]
    {
        const float norm = parse ? juce::jlimit (0.0f, 1.0f, rp->convertTo0to1 ((float) parse (lp->getText())))
                                 : rp->getValueForText (lp->getText());
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
    };

    valueFieldAtts.push_back (std::move (att));
    valueFields.push_back (std::move (label));
    fieldPositions.push_back ({ lp, &knob, 8 });
    return *lp;
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
    morphMode.setVisible (bh >= 250);
    morphCurve.setVisible (bh >= 296);

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

    // Timing: each row (NOTE RATE, NOTE LENGTH) is a vertical Sync/Free/Random
    // selector on the left with a knob (synced division or free seconds) and
    // its value on the right.
    {
        const int x0 = timingPanel.getX() + 12;
        const int selW = 122, selH = 96, knob = 62; // bigger knobs (design size 62)
        const int knobX = x0 + selW + (timingPanel.getWidth() - 24 - selW - knob) / 2;
        const int row1 = timingPanel.getY() + 52;
        const int row2 = timingPanel.getY() + 190;
        intervalMode.setBounds (x0, row1, selW, selH);
        lengthMode.setBounds   (x0, row2, selW, selH);
        intervalSync.setBounds (knobX, row1 + 2, knob, knob);
        intervalFree.setBounds (knobX, row1 + 2, knob, knob);
        lengthSync.setBounds   (knobX, row2 + 2, knob, knob);
        lengthFree.setBounds   (knobX, row2 + 2, knob, knob);
    }

    // Morph
    {
        const int x = morphPanel.getX() + 12, w = morphPanel.getWidth() - 24;
        morphBar.setBounds (x, morphPanel.getY() + 34, w, 34);
        // AUTO-SWEEP full width, then a duration row: SYNC/FREE (with the
        // Seconds/Minutes unit below it in Free mode) on the left, and the
        // DURATION knob on the right.
        autoSweep.setBounds (x, morphPanel.getY() + 78, w, 28);
        // SYNC/FREE on the left, one dual-mode DURATION knob on the right -
        // the layout is identical in both modes (no unit dropdown).
        const int knob = 50, durY = morphPanel.getY() + 112;
        const int selW = w - knob - 70;
        morphDurMode.setBounds (x, durY + 10, selW, 26);
        // DURATION knob centred between the SYNC/FREE selector and the plate edge.
        const int selRight = x + selW, plateRight = morphPanel.getRight() - 12;
        const int durX = selRight + (plateRight - selRight - knob) / 2;
        morphDurBars.setBounds (durX, durY, knob, knob);
        morphDurFree.setBounds (durX, durY, knob, knob);
        morphMode.setBounds (x, morphPanel.getY() + 212, w, 26);  // pushed down so the
        morphCurve.setBounds (x, morphPanel.getY() + 256, w, 26); // duration value clears it
    }

    output.setBounds (outputPanel.getX() + 12, outputPanel.getY() + 34,
                      outputPanel.getWidth() - 24, outputPanel.getHeight() - 46);

    // Presets panel: two rows of four bubbles, Save/Load stacked at the right
    {
        const int x0 = presetsPanel.getX() + 90;
        const int rowW = presetsPanel.getRight() - 92 - x0; // leave room for Save/Load clear of the corner screw
        const int perRow = 5;
        const int w = (rowW - (perRow - 1) * 8) / perRow;
        for (size_t k = 0; k < presetBtns.size(); ++k)
        {
            const int row = (int) k / perRow, col = (int) k % perRow;
            presetBtns[k]->setBounds (x0 + col * (w + 8), presetsPanel.getY() + 10 + row * 30, w, 26);
        }
        // Right edge at -24 so the top-right screw (centre -10, r 5) is clear.
        savePreset.setBounds (presetsPanel.getRight() - 78, presetsPanel.getY() + 10, 54, 26);
        loadPreset.setBounds (presetsPanel.getRight() - 78, presetsPanel.getY() + 40, 54, 26);
    }

    // Editable value fields: centred over each knob's painted readout.
    for (auto& f : fieldPositions)
    {
        const auto b = f.knob->getBounds();
        f.label->setBounds (b.getCentreX() - 34, b.getBottom() + f.dy, 68, 18);
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
    updateSweepGating(); // idempotent; keeps the morph gating in sync with presets/automation

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

    // A fully one-sided morph means the other scale can't sound - dim it. The
    // fade is animated (super-fast) by scaleAnim; here we just set the target.
    {
        const bool sweep = alea.apvts.getRawParameterValue ("autoSweep")->load() > 0.5f;
        const float pct = sweep ? (float) alea.morphPercent.load()
                                : alea.apvts.getRawParameterValue ("morphPos")->load();
        const float newA = pct >= 99.95f ? 0.35f : 1.0f;
        const float newB = pct <= 0.05f  ? 0.35f : 1.0f;

        if (! juce::approximatelyEqual (newA, targetAlphaA) || ! juce::approximatelyEqual (newB, targetAlphaB))
        {
            targetAlphaA = newA;
            targetAlphaB = newB;
            scaleAnim.go();
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

bool AleaAudioProcessorEditor::advanceScaleAlpha()
{
    auto step = [] (float& a, float target)
    {
        a += (target - a) * 0.35f;                 // ~90ms fade
        if (std::abs (target - a) < 0.004f) { a = target; return false; }
        return true;
    };
    const bool moving = step (alphaA, targetAlphaA) | step (alphaB, targetAlphaB);
    for (auto* c : { (juce::Component*) &keyboardA, (juce::Component*) &restsA,
                     (juce::Component*) &aOctMin, (juce::Component*) &aOctMax,
                     (juce::Component*) &aVelMin, (juce::Component*) &aVelMax })
        c->setAlpha (alphaA);
    for (auto* c : { (juce::Component*) &keyboardB, (juce::Component*) &restsB,
                     (juce::Component*) &bOctMin, (juce::Component*) &bOctMax,
                     (juce::Component*) &bVelMin, (juce::Component*) &bVelMax })
        c->setAlpha (alphaB);
    for (auto* l : scaleAFields) l->setAlpha (alphaA);
    for (auto* l : scaleBFields) l->setAlpha (alphaB);
    content.repaint (scaleAPanel);
    content.repaint (scaleBPanel);
    return moving;
}

void AleaAudioProcessorEditor::updateSweepGating()
{
    // One input: sweepEnabled. ON -> the morph controls light amber (live);
    // OFF -> their backlight swaps to matte grey (set, but not driving). They
    // are never disabled and their values are never cleared - only the colour
    // passed to the child controls changes.
    //
    // Guarded + polled from the timer: a preset can change autoSweep without the
    // button's onStateChange firing (the attachment coalesces the reset->set, or
    // the value lands where it already was), which used to leave the accent
    // stale until a hover re-fired the callback. Reading the parameter each tick
    // and early-returning when unchanged keeps it in sync with no extra repaints.
    const bool on = alea.apvts.getRawParameterValue ("autoSweep")->load() > 0.5f;
    if ((int) on == sweepGate)
        return;
    sweepGate = (int) on;
    const juce::Colour lit = on ? colors::amber : juce::Colour (0xff3d4048);
    morphDurMode.setAccent (lit);
    morphMode.setAccent (lit);
    morphCurve.setAccent (lit);
    for (auto* k : { &morphDurBars, &morphDurFree })
    {
        k->setColour (juce::Slider::rotarySliderFillColourId, lit);
        k->repaint();
    }
}

void AleaAudioProcessorEditor::updateModeVisibility()
{
    const bool was[] = { intervalSync.isVisible(), intervalFree.isVisible(),
                         lengthSync.isVisible(), lengthFree.isVisible() };
    const int iMode = (int) alea.apvts.getRawParameterValue ("intervalMode")->load();
    intervalSync.setVisible (iMode == params::sync);
    intervalFree.setVisible (iMode == params::free);
    if (intervalFreeField != nullptr) intervalFreeField->setVisible (iMode == params::free);

    const int lMode = (int) alea.apvts.getRawParameterValue ("lengthMode")->load();
    lengthSync.setVisible (lMode == params::sync);
    lengthFree.setVisible (lMode == params::free);
    if (lengthFreeField != nullptr) lengthFreeField->setVisible (lMode == params::free);

    // Repaint the painted timing value only when a mode actually switched a
    // knob's visibility (not every timer tick).
    if (was[0] != intervalSync.isVisible() || was[1] != intervalFree.isVisible()
        || was[2] != lengthSync.isVisible() || was[3] != lengthFree.isVisible())
        content.repaint (timingPanel);

    const bool durWasSync = morphDurBars.isVisible();
    const bool durSync = (int) alea.apvts.getRawParameterValue ("morphDurMode")->load() == 0;
    morphDurBars.setVisible (durSync);
    morphDurFree.setVisible (! durSync);
    if (morphDurFreeField != nullptr) morphDurFreeField->setVisible (! durSync);
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

    // Header: the wordmark with a clean outer drop shadow (the letters fully
    // occlude the shadow - it never falls on the glyphs).
    {
        static const juce::Image logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);
        ui::drawWordmark (g, logo, { 20, 12, 87, 34 });
    }

    // LED blooms behind the lit keys, drawn by the container so they spread
    // onto the metal (a key's own paint clips to its rectangle).
    ui::hw::keyBloom (g, freezeButton, colors::ice, ui::hw::litAmount (freezeButton));
    ui::hw::keyBloom (g, autoSweep, colors::amber, ui::hw::litAmount (autoSweep));
    if (playButton.isVisible())
        ui::hw::keyBloom (g, playButton, colors::playing, playButton.getToggleState() ? 1.0f : 0.0f);
    // Header disclosure is purely additive as the window widens - nothing ever
    // appears, disappears, then reappears. The status LED sits right after the
    // logo and is ALWAYS shown; the subtitle adds next, then the status word.
    const bool playing = alea.hostIsPlaying.load();
    const int fx = freezeButton.getX();
    ui::hw::ledDot (g, { 124.0f, 28.0f }, playing ? 1.0f : 0.0f, colors::playing, 12.0f); // 1) LED - always
    if (fx >= 300)                               // 2) + subtitle
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Aleatoric Scale Shifter", 140, 16, 158, 28, juce::Justification::centredLeft);
    }
    if (fx >= 378)                               // 3) + status word
    {
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (playing ? "playing" : "stopped", 302, 14, 74, 28, juce::Justification::centredLeft);
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
        auto knobLabel = [&] (juce::Slider* k, const juce::String& cap, const juce::String&)
        {
            // Caption only; the value is a double-click-editable field component.
            const auto b = k->getBounds();
            g.setColour (colors::secondary.withMultipliedAlpha (s.alpha));
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (cap, b.getX() - 20, b.getBottom() - 2, b.getWidth() + 40, 13, juce::Justification::centred);
        };
        knobLabel (s.octMin, "OCT LO", juce::String::charToString (s.id) + "OctMin");
        knobLabel (s.octMax, "OCT HI", juce::String::charToString (s.id) + "OctMax");
        knobLabel (s.velMin, "VEL LO", juce::String::charToString (s.id) + "VelMin");
        knobLabel (s.velMax, "VEL HI", juce::String::charToString (s.id) + "VelMax");
    }
    g.setColour (colors::secondary);

    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened()); // sub-labels: smaller than the panel title
    g.drawText ("NOTE RATE",   timingPanel.getX() + 12, timingPanel.getY() + 40,  120, 14, juce::Justification::centredLeft);
    g.drawText ("NOTE LENGTH", timingPanel.getX() + 12, timingPanel.getY() + 172, 120, 14, juce::Justification::centredLeft);

    // Under each knob: a caption then the value ("DIVISION" / "1/4 note"),
    // centred on the knob. Random mode shows the last dice roll instead.
    auto timingKnobLabel = [&] (juce::Slider& sync, juce::Slider& freeS, int mode,
                                const juce::String& freeCaption, int randomPick)
    {
        auto& k = (mode == params::sync) ? sync : freeS;
        const auto kb = k.getBounds();
        const int cx = kb.getCentreX(), below = kb.getBottom();
        if (mode == params::random)
        {
            // A stepped knob showing the dice's current pick - not interactive,
            // just a live status readout (dimmed to read as read-only).
            const int n = params::randomPoolNames.size();
            const float pos = randomPick >= 0 ? (float) randomPick / (float) (n - 1) : 0.0f;
            ui::hw::knob (g, kb.toFloat(), pos, colors::green.withAlpha (0.55f), false);
            g.setColour (colors::secondary);
            g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
            g.drawText ("NOW", cx - 60, below + 2, 120, 14, juce::Justification::centred);
            g.setColour (colors::text);
            g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
            g.drawText (randomPick >= 0 ? params::randomPoolNames[randomPick] + " note" : "-",
                        cx - 60, below + 16, 120, 18, juce::Justification::centred);
            g.setColour (colors::secondary);
            return;
        }
        g.setColour (colors::secondary);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.drawText (mode == params::sync ? "DIVISION" : freeCaption, cx - 60, below + 2, 120, 14, juce::Justification::centred);
        // Sync value is painted here; the free value is an editable field.
        if (mode == params::sync)
        {
            g.setColour (colors::text);
            g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
            g.drawText (k.getTextFromValue (k.getValue()), cx - 60, below + 16, 120, 18, juce::Justification::centred);
        }
        g.setColour (colors::secondary);
    };
    const int iMode = (int) alea.apvts.getRawParameterValue ("intervalMode")->load();
    const int lMode = (int) alea.apvts.getRawParameterValue ("lengthMode")->load();
    timingKnobLabel (intervalSync, intervalFree, iMode, "RATE",   alea.lastRandomInterval.load());
    timingKnobLabel (lengthSync,   lengthFree,   lMode, "LENGTH", alea.lastRandomLength.load());
    // DURATION knob caption + value (the visible knob: bars in Sync, seconds
    // in Free), painted below it.
    {
        const bool sync = morphDurBars.isVisible();
        auto& durK = sync ? morphDurBars : morphDurFree;
        const auto kb = durK.getBounds();
        g.setColour (colors::secondary);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.drawText ("DURATION", kb.getX() - 24, kb.getBottom() - 2, kb.getWidth() + 48, 13, juce::Justification::centred);
        // Sync (bars) value painted here; the free (seconds) value is a field.
        if (sync)
        {
            g.setColour (colors::text);
            g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
            g.drawText (durK.getTextFromValue (durK.getValue()),
                        kb.getX() - 24, kb.getBottom() + 10, kb.getWidth() + 48, 14, juce::Justification::centred);
        }
    }

    g.setColour (colors::secondary);
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    if (morphMode.isVisible())
        g.drawText ("MORPH MODE",  morphPanel.getX() + 12, morphPanel.getY() + 196, 130, 12, juce::Justification::centredLeft);
    if (morphCurve.isVisible())
        g.drawText ("MORPH CURVE", morphPanel.getX() + 12, morphPanel.getY() + 240, 130, 12, juce::Justification::centredLeft);
}
