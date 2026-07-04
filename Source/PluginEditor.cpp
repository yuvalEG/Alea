#include "PluginEditor.h"

using namespace ui;

namespace
{
    constexpr int kWidth = 940, kHeight = 590;

    const juce::Rectangle<int> scaleAPanel   { 10,  64, 455, 240 };
    const juce::Rectangle<int> scaleBPanel   { 475, 64, 455, 240 };
    const juce::Rectangle<int> timingPanel   { 10,  314, 290, 266 };
    const juce::Rectangle<int> morphPanel    { 310, 314, 330, 266 };
    const juce::Rectangle<int> outputPanel   { 650, 314, 280, 266 };
}

AleaAudioProcessorEditor::AleaAudioProcessorEditor (AleaAudioProcessor& p)
    : AudioProcessorEditor (p), alea (p),
      keyboardA (p, 'a', 0, colors::purple),
      keyboardB (p, 'b', 1, colors::cyan),
      restsA (p, 'a', colors::purple),
      restsB (p, 'b', colors::cyan),
      intervalMode (*p.apvts.getParameter ("intervalMode"), params::timingModes, colors::text.withAlpha (0.9f)),
      lengthMode   (*p.apvts.getParameter ("lengthMode"),   params::timingModes, colors::text.withAlpha (0.9f)),
      morphBar (p),
      morphDurMode (*p.apvts.getParameter ("morphDurMode"), params::morphDurModes, colors::amber),
      tempoSource  (*p.apvts.getParameter ("tempoSource"),  params::tempoSources, colors::green),
      output (p)
{
    addAndMakeVisible (keyboardA);
    addAndMakeVisible (keyboardB);
    addAndMakeVisible (restsA);
    addAndMakeVisible (restsB);
    addAndMakeVisible (intervalMode);
    addAndMakeVisible (lengthMode);
    addAndMakeVisible (morphBar);
    addAndMakeVisible (morphDurMode);
    addAndMakeVisible (tempoSource);
    addAndMakeVisible (output);

    setupSlider (aOctMin, "aOctMin", colors::purple);   setupSlider (aOctMax, "aOctMax", colors::purple);
    setupSlider (aVelMin, "aVelMin", colors::purple);   setupSlider (aVelMax, "aVelMax", colors::purple);
    setupSlider (bOctMin, "bOctMin", colors::cyan);     setupSlider (bOctMax, "bOctMax", colors::cyan);
    setupSlider (bVelMin, "bVelMin", colors::cyan);     setupSlider (bVelMax, "bVelMax", colors::cyan);
    setupSlider (intervalFree, "intervalFree", colors::text.withAlpha (0.6f));
    setupSlider (lengthFree, "lengthFree", colors::text.withAlpha (0.6f));
    setupSlider (morphDurFree, "morphDurFree", colors::amber);
    setupSlider (internalTempo, "internalTempo", colors::green);

    setupCombo (intervalSync, "intervalSync");
    setupCombo (lengthSync, "lengthSync");
    setupCombo (morphDurBars, "morphDurBars");
    setupCombo (morphDurUnit, "morphDurUnit");
    setupCombo (morphMode, "morphMode");
    setupCombo (morphCurve, "morphCurve");

    autoSweep.setColour (juce::ToggleButton::textColourId, colors::text);
    autoSweep.setColour (juce::ToggleButton::tickColourId, colors::amber);
    autoSweep.setColour (juce::ToggleButton::tickDisabledColourId, colors::border);
    addAndMakeVisible (autoSweep);
    sweepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        alea.apvts, "autoSweep", autoSweep);

    setSize (kWidth, kHeight);
    updateModeVisibility();
    startTimerHz (15);
}

void AleaAudioProcessorEditor::setupSlider (juce::Slider& s, const juce::String& paramID, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::LinearBar);
    s.setColour (juce::Slider::trackColourId, accent.withAlpha (0.55f));
    s.setColour (juce::Slider::backgroundColourId, colors::control);
    s.setColour (juce::Slider::textBoxTextColourId, colors::text);
    s.setColour (juce::Slider::textBoxOutlineColourId, colors::border);
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        alea.apvts, paramID, s));
}

void AleaAudioProcessorEditor::setupCombo (juce::ComboBox& c, const juce::String& paramID)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (alea.apvts.getParameter (paramID)))
        c.addItemList (choice->choices, 1);

    c.setColour (juce::ComboBox::backgroundColourId, colors::control);
    c.setColour (juce::ComboBox::textColourId, colors::text);
    c.setColour (juce::ComboBox::outlineColourId, colors::border);
    c.setColour (juce::ComboBox::arrowColourId, colors::secondary);
    addAndMakeVisible (c);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        alea.apvts, paramID, c));
}

void AleaAudioProcessorEditor::resized()
{
    // Header
    tempoSource.setBounds (600, 16, 160, 26);
    internalTempo.setBounds (770, 16, 156, 26);

    auto scaleControls = [] (const juce::Rectangle<int>& panel, PianoKeyboard& kb, RestSelector& rests,
                                 juce::Slider& octMin, juce::Slider& octMax,
                                 juce::Slider& velMin, juce::Slider& velMax)
    {
        const int x = panel.getX() + 12, w = panel.getWidth() - 24;
        kb.setBounds (x, panel.getY() + 30, w, 96);
        rests.setBounds (x + 44, panel.getY() + 132, w - 44, 26);
        const int half = (w - 52) / 2;
        octMin.setBounds (x + 44, panel.getY() + 168, half, 24);
        octMax.setBounds (x + 52 + half, panel.getY() + 168, half, 24);
        velMin.setBounds (x + 44, panel.getY() + 202, half, 24);
        velMax.setBounds (x + 52 + half, panel.getY() + 202, half, 24);
    };

    scaleControls (scaleAPanel, keyboardA, restsA, aOctMin, aOctMax, aVelMin, aVelMax);
    scaleControls (scaleBPanel, keyboardB, restsB, bOctMin, bOctMax, bVelMin, bVelMax);

    // Timing
    {
        const int x = timingPanel.getX() + 12, w = timingPanel.getWidth() - 24;
        intervalMode.setBounds (x, timingPanel.getY() + 48, w, 26);
        intervalSync.setBounds (x, timingPanel.getY() + 80, w, 26);
        intervalFree.setBounds (x, timingPanel.getY() + 80, w, 26);
        lengthMode.setBounds (x, timingPanel.getY() + 146, w, 26);
        lengthSync.setBounds (x, timingPanel.getY() + 178, w, 26);
        lengthFree.setBounds (x, timingPanel.getY() + 178, w, 26);
    }

    // Morph
    {
        const int x = morphPanel.getX() + 12, w = morphPanel.getWidth() - 24;
        morphBar.setBounds (x, morphPanel.getY() + 34, w, 34);
        autoSweep.setBounds (x, morphPanel.getY() + 76, 130, 24);
        morphDurMode.setBounds (x + 140, morphPanel.getY() + 76, w - 140, 24);
        morphDurBars.setBounds (x, morphPanel.getY() + 122, w, 26);
        morphDurFree.setBounds (x, morphPanel.getY() + 122, w - 110, 26);
        morphDurUnit.setBounds (x + w - 102, morphPanel.getY() + 122, 102, 26);
        morphMode.setBounds (x, morphPanel.getY() + 168, w, 26);
        morphCurve.setBounds (x, morphPanel.getY() + 214, w, 26);
    }

    output.setBounds (outputPanel.getX() + 12, outputPanel.getY() + 34,
                      outputPanel.getWidth() - 24, outputPanel.getHeight() - 46);
}

void AleaAudioProcessorEditor::timerCallback()
{
    updateModeVisibility();
    keyboardA.repaint();
    keyboardB.repaint();
    morphBar.repaint();
    output.repaint();
    repaint (0, 0, getWidth(), 60); // header status dot
}

void AleaAudioProcessorEditor::updateModeVisibility()
{
    const int iMode = (int) alea.apvts.getRawParameterValue ("intervalMode")->load();
    intervalSync.setVisible (iMode == params::sync);
    intervalFree.setVisible (iMode == params::free);

    const int lMode = (int) alea.apvts.getRawParameterValue ("lengthMode")->load();
    lengthSync.setVisible (lMode == params::sync);
    lengthFree.setVisible (lMode == params::free);

    const bool durSync = (int) alea.apvts.getRawParameterValue ("morphDurMode")->load() == 0;
    morphDurBars.setVisible (durSync);
    morphDurFree.setVisible (! durSync);
    morphDurUnit.setVisible (! durSync);

    const bool freeRun = (int) alea.apvts.getRawParameterValue ("tempoSource")->load() == 1;
    internalTempo.setEnabled (freeRun);
    internalTempo.setAlpha (freeRun ? 1.0f : 0.4f);
}

void AleaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colors::background);

    // Header
    g.setColour (colors::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("ALEA", 20, 10, 120, 38, juce::Justification::centredLeft);
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("generative MIDI", 96, 16, 160, 28, juce::Justification::centredLeft);

    const bool playing = alea.hostIsPlaying.load();
    g.setColour (playing ? colors::green : colors::control);
    g.fillEllipse (262.0f, 22.0f, 12.0f, 12.0f);
    g.setColour (colors::secondary);
    g.drawText (playing ? "playing" : "stopped", 282, 14, 90, 28, juce::Justification::centredLeft);

    g.drawText ("TEMPO", 540, 16, 54, 26, juce::Justification::centredRight);

    auto drawPanel = [&g] (const juce::Rectangle<int>& r, const juce::String& title, juce::Colour accent)
    {
        g.setColour (colors::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (colors::border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (accent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 8, r.getWidth() - 24, 18, juce::Justification::centredLeft);
    };

    drawPanel (scaleAPanel, "SCALE A", colors::purple);
    drawPanel (scaleBPanel, "SCALE B", colors::cyan);
    drawPanel (timingPanel, "TIMING", colors::text.withAlpha (0.9f));
    drawPanel (morphPanel, "MORPH", colors::amber);
    drawPanel (outputPanel, "OUTPUT", colors::green);

    // Small row labels
    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));

    for (const auto* panel : { &scaleAPanel, &scaleBPanel })
    {
        g.drawText ("RESTS", panel->getX() + 12, panel->getY() + 132, 40, 26, juce::Justification::centredLeft);
        g.drawText ("OCT",   panel->getX() + 12, panel->getY() + 168, 40, 24, juce::Justification::centredLeft);
        g.drawText ("VEL",   panel->getX() + 12, panel->getY() + 202, 40, 24, juce::Justification::centredLeft);
    }

    g.drawText ("INTERVAL", timingPanel.getX() + 12, timingPanel.getY() + 30, 100, 16, juce::Justification::centredLeft);
    g.drawText ("LENGTH",   timingPanel.getX() + 12, timingPanel.getY() + 128, 100, 16, juce::Justification::centredLeft);
    g.drawText ("DURATION", morphPanel.getX() + 12, morphPanel.getY() + 108, 100, 12, juce::Justification::centredLeft);
    g.drawText ("MODE",     morphPanel.getX() + 12, morphPanel.getY() + 154, 100, 12, juce::Justification::centredLeft);
    g.drawText ("CURVE",    morphPanel.getX() + 12, morphPanel.getY() + 200, 100, 12, juce::Justification::centredLeft);
}
