#include "ChordsProcessor.h"
#include "ChordsEditor.h"

ChordsProcessor::ChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    rollSeries(); // never an empty screen: chords out of the box
}

void ChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear(); // silent until M2 brings the loop
}

bool ChordsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChordsProcessor::rollSeries()
{
    if (! series.empty())
    {
        history.push_front (series);
        trimHistory();
    }

    series.clear();
    for (int i = 0; i < seriesLength; ++i)
        series.push_back (chords::roll (rng, simplify, useSevenths));

    ++revision;
}

void ChordsProcessor::setSeriesLength (int newLength)
{
    seriesLength = juce::jlimit (1, 8, newLength);

    // Growing rolls fresh chords into the new slots; shrinking truncates.
    // Existing chords stay - the selector never silently rerolls your loop.
    while ((int) series.size() > seriesLength)
        series.pop_back();
    while ((int) series.size() < seriesLength)
        series.push_back (chords::roll (rng, simplify, useSevenths));

    ++revision;
}

void ChordsProcessor::recallRoll (int index)
{
    if (index < 0 || index >= (int) history.size())
        return;

    // Non-destructive: the recalled roll stays in history; the outgoing
    // series joins it up front, so nothing is ever lost by recalling.
    auto recalled = history[(size_t) index];
    if (! series.empty())
        history.push_front (series);
    series = std::move (recalled);
    seriesLength = juce::jlimit (1, 8, (int) series.size());
    trimHistory();
    ++revision;
}

void ChordsProcessor::trimHistory()
{
    int total = 0;
    for (auto& roll : history)
        total += (int) roll.size();

    while (total > 1000 && history.size() > 1)
    {
        total -= (int) history.back().size();
        history.pop_back();
    }
}

void ChordsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("ChordsState");
    state.setProperty ("seriesLength", seriesLength, nullptr);
    state.setProperty ("useSevenths", useSevenths, nullptr);
    state.setProperty ("simplify", simplify, nullptr);
    state.setProperty ("uiWidth", lastUIWidth, nullptr);
    state.setProperty ("uiHeight", lastUIHeight, nullptr);

    juce::ValueTree seriesTree ("Series");
    for (const auto& c : series)
    {
        juce::ValueTree chord ("Chord");
        chord.setProperty ("root", c.root, nullptr);
        chord.setProperty ("quality", (int) c.quality, nullptr);
        chord.setProperty ("seventh", (int) c.seventh, nullptr);
        seriesTree.appendChild (chord, nullptr);
    }
    state.appendChild (seriesTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChordsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType ("ChordsState"))
        return;

    seriesLength = juce::jlimit (1, 8, (int) state.getProperty ("seriesLength", 4));
    useSevenths  = state.getProperty ("useSevenths", false);
    simplify     = state.getProperty ("simplify", true);
    lastUIWidth  = juce::jlimit (560, 4000, (int) state.getProperty ("uiWidth", 900));
    lastUIHeight = juce::jlimit (380, 3000, (int) state.getProperty ("uiHeight", 560));

    auto seriesTree = state.getChildWithName ("Series");
    std::vector<chords::Chord> restored;
    for (auto chord : seriesTree)
    {
        chords::Chord c;
        c.root    = chord.getProperty ("root", "C").toString();
        c.quality = (chords::Quality) juce::jlimit (0, 3, (int) chord.getProperty ("quality", 0));
        c.seventh = (chords::Seventh) juce::jlimit (0, 3, (int) chord.getProperty ("seventh", 0));
        restored.push_back (c);
    }

    if (! restored.empty())
    {
        series = std::move (restored);
        setSeriesLength (seriesLength); // reconcile if length and series disagree
    }

    ++revision;
}

juce::AudioProcessorEditor* ChordsProcessor::createEditor()
{
    return new ChordsEditor (*this);
}

// The standalone wrapper needs this factory.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordsProcessor();
}
