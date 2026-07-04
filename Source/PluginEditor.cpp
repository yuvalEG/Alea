#include "PluginEditor.h"

// Spec color scheme (section 9.4)
namespace colors
{
    const juce::Colour background { 0xff0a0a0f };
    const juce::Colour panel      { 0xff12121a };
    const juce::Colour border     { 0xff2a2a3a };
    const juce::Colour text       { 0xffe8e8f0 };
    const juce::Colour secondary  { 0xff8888a0 };
    const juce::Colour green      { 0xff10b981 };
}

AleaAudioProcessorEditor::AleaAudioProcessorEditor (AleaAudioProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setSize (340, 210);
    startTimerHz (15);
}

void AleaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colors::background);

    auto panel = getLocalBounds().reduced (12);
    g.setColour (colors::panel);
    g.fillRoundedRectangle (panel.toFloat(), 6.0f);
    g.setColour (colors::border);
    g.drawRoundedRectangle (panel.toFloat(), 6.0f, 1.0f);

    auto area = panel.reduced (16, 12);

    g.setColour (colors::text);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("Alea — Milestone 1", area.removeFromTop (28), juce::Justification::centredLeft);

    g.setColour (colors::secondary);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Emits C3 every beat while the DAW plays.",
                area.removeFromTop (22), juce::Justification::centredLeft);

    area.removeFromTop (8);

    const bool playing = processor.hostIsPlaying.load();
    const double bpm   = processor.hostBpm.load();
    const double ppq   = processor.hostPpq.load();
    const int sent     = processor.notesSent.load();

    auto drawRow = [&] (const juce::String& label, const juce::String& value, juce::Colour valueColor)
    {
        auto row = area.removeFromTop (24);
        g.setColour (colors::secondary);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (label, row.removeFromLeft (130), juce::Justification::centredLeft);
        g.setColour (valueColor);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (value, row, juce::Justification::centredLeft);
    };

    drawRow ("Transport", playing ? "PLAYING" : "stopped",
             playing ? colors::green : colors::secondary);
    drawRow ("Host tempo", bpm > 0.0 ? juce::String (bpm, 1) + " BPM" : "-", colors::text);
    drawRow ("Beat (ppq)", playing ? juce::String (ppq, 2) : "-", colors::text);
    drawRow ("Notes sent", juce::String (sent), sent > 0 ? colors::green : colors::text);
}
