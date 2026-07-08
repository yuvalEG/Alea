// Dev tool: turns raw piano samples (Salamander Grand flacs) into the
// small mono oggs embedded in the apps. Run once when (re)building the
// sample set; the outputs live in Assets/piano/ as p<midi>.ogg.
// Usage: SamplePrep <inDir> <outDir> [seconds=3.6] [fade=0.6] [quality 0..1=0.5]
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

static int rootMidiFromName (const juce::String& stem) // "D#3v10" -> 51
{
    static constexpr int letterPc[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    if (stem.isEmpty())
        return -1;
    int pc = letterPc[juce::jlimit (0, 6, (int) (stem[0] - 'A'))];
    int i = 1;
    if (stem.length() > i && stem[i] == '#') { ++pc; ++i; }
    if (stem.length() <= i || ! juce::CharacterFunctions::isDigit (stem[i]))
        return -1;
    const int octave = stem[i] - '0';
    return 12 * (octave + 1) + ((pc + 12) % 12);
}

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: SamplePrep <inDir> <outDir> [seconds] [fade] [quality]\n";
        return 1;
    }

    const juce::File inDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    const double seconds = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 3.6;
    const double fade = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 0.6;
    const float quality = argc > 5 ? (float) juce::String (argv[5]).getDoubleValue() : 0.5f;
    outDir.createDirectory();

    juce::FlacAudioFormat flac;
    juce::OggVorbisAudioFormat ogg;
    juce::int64 totalOut = 0;

    for (const auto& in : inDir.findChildFiles (juce::File::findFiles, false, "*.flac"))
    {
        const int root = rootMidiFromName (in.getFileNameWithoutExtension());
        if (root < 0)
        {
            std::cout << "skip (name): " << in.getFileName() << "\n";
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (in.createInputStream().release(), true));
        if (reader == nullptr)
        {
            std::cout << "skip (read): " << in.getFileName() << "\n";
            continue;
        }

        const int wanted = (int) juce::jmin<juce::int64> (reader->lengthInSamples,
                                                          (juce::int64) (seconds * reader->sampleRate));
        juce::AudioBuffer<float> stereo ((int) reader->numChannels, wanted);
        reader->read (&stereo, 0, wanted, 0, true, reader->numChannels > 1);

        // Mixdown to mono (the family chain widens with delay and reverb)
        // and fade the cut tail so the trim is inaudible.
        juce::AudioBuffer<float> mono (1, wanted);
        mono.clear();
        for (int ch = 0; ch < stereo.getNumChannels(); ++ch)
            mono.addFrom (0, 0, stereo, ch, 0, wanted, 1.0f / (float) stereo.getNumChannels());
        const int fadeSamples = juce::jmin (wanted, (int) (fade * reader->sampleRate));
        if (fadeSamples > 0)
            mono.applyGainRamp (0, wanted - fadeSamples, fadeSamples, 1.0f, 0.0f);

        const auto out = outDir.getChildFile ("p" + juce::String (root).paddedLeft ('0', 3) + ".ogg");
        out.deleteFile();
        const int qualityIndex = juce::jlimit (0, ogg.getQualityOptions().size() - 1,
                                               (int) (quality * (float) (ogg.getQualityOptions().size() - 1)));
        std::unique_ptr<juce::AudioFormatWriter> writer (
            ogg.createWriterFor (new juce::FileOutputStream (out), reader->sampleRate,
                                 1, 16, {}, qualityIndex));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (mono, 0, wanted))
        {
            std::cout << "FAILED: " << in.getFileName() << "\n";
            return 2;
        }
        writer.reset();
        totalOut += out.getSize();
        std::cout << in.getFileName() << " -> " << out.getFileName()
                  << " (" << out.getSize() / 1024 << " KB)\n";
    }

    std::cout << "total: " << totalOut / 1024 << " KB\n";
    return 0;
}
