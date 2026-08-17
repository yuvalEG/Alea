// Dev tool: turns a raw piano sample set (flacs) into the small mono oggs
// embedded in the apps. Run once when (re)building the sample set; the
// outputs live in Assets/piano/ as p<midi>_<soft|hard>.ogg.
//
// Input names are the FreePats convention: "<note><octave>v<L|H>.flac", so
// "D#3vL" is D#3 played softly and "A0vH" is A0 played hard. A set with no
// velocity letter at all still works and lands in the hard layer, which is
// what a one-layer library should behave like.
//
// TWO THINGS THIS DELIBERATELY DOES NOT DO, both learned the hard way:
//
//   It does not flatten the velocity layers. Playing a piano softly makes it
//   DARKER, not merely quieter, and a sampler with one layer can only do
//   quieter. That single fact is most of what made the old embedded piano
//   sound artificial, and it cost nothing but file size to fix.
//
//   It does not impose one length on every note. A piano's decay roughly
//   halves each octave up, and a good sample set is recorded that way: here
//   A0 runs 7.6 seconds and C8 runs 0.7. Truncating everything to a fixed
//   few seconds threw that away and cut the low notes off mid-ring. The cap
//   below is a safety limit, not a target - normally nothing reaches it.
//
// Re-running this on the SAME input produces different bytes - the Ogg encoder
// is not deterministic - so expect every file to show as modified even when
// nothing about the sound changed. Only commit a regenerated set when you
// meant to change something; otherwise `git checkout -- Assets/piano`.
//
// Usage: SamplePrep <inDir> <outDir> [maxSeconds=8] [fade=0.35] [quality 0..1=0.65]
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>

// "D#3vL" -> 51, "A0vH" -> 21. Returns -1 if the name is not a note.
static int rootMidiFromName (const juce::String& stem)
{
    static constexpr int letterPc[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    if (stem.isEmpty() || stem[0] < 'A' || stem[0] > 'G')
        return -1;
    int pc = letterPc[(int) (stem[0] - 'A')];
    int i = 1;
    if (stem.length() > i && stem[i] == '#') { ++pc; ++i; }
    else if (stem.length() > i && stem[i] == 'b') { --pc; ++i; }
    if (stem.length() <= i || ! juce::CharacterFunctions::isDigit (stem[i]))
        return -1;
    const int octave = stem[i] - '0';
    return 12 * (octave + 1) + ((pc + 12) % 12);
}

// The trailing "vL" / "vH". Anything else is treated as the hard layer, so a
// single-layer set still produces a playable instrument rather than nothing.
static bool isSoftLayer (const juce::String& stem)
{
    return stem.endsWithIgnoreCase ("vL");
}

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage: SamplePrep <inDir> <outDir> [maxSeconds] [fade] [quality]\n";
        return 1;
    }

    const juce::File inDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    const double maxSeconds = argc > 3 ? juce::String (argv[3]).getDoubleValue() : 8.0;
    const double fade = argc > 4 ? juce::String (argv[4]).getDoubleValue() : 0.35;
    const float quality = argc > 5 ? (float) juce::String (argv[5]).getDoubleValue() : 0.65f;
    outDir.createDirectory();

    // Sweep the previous set first. This tool REPLACES the embedded samples,
    // and the output names have changed once already (p060.ogg became
    // p060_soft.ogg / p060_hard.ogg). Writing over a directory that still
    // holds the old scheme leaves those files behind, CMake globs *.ogg, and
    // the engine reads any name without "soft" as a hard-layer zone - so the
    // instrument silently becomes a blend of two libraries, with the stale
    // roots competing to be the nearest. Only p*.ogg is touched.
    int swept = 0;
    for (const auto& old : outDir.findChildFiles (juce::File::findFiles, false, "p*.ogg"))
        swept += old.deleteFile() ? 1 : 0;
    if (swept > 0)
        std::cout << "swept " << swept << " sample(s) from the previous set\n";

    juce::FlacAudioFormat flac;
    juce::OggVorbisAudioFormat ogg;
    juce::int64 totalOut = 0;
    int written = 0, softCount = 0, hardCount = 0;

    for (const auto& in : inDir.findChildFiles (juce::File::findFiles, false, "*.flac"))
    {
        const auto stem = in.getFileNameWithoutExtension();
        const int root = rootMidiFromName (stem);
        if (root < 0)
        {
            std::cout << "skip (name): " << in.getFileName() << "\n";
            continue;
        }
        const bool soft = isSoftLayer (stem);

        std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (in.createInputStream().release(), true));
        if (reader == nullptr)
        {
            std::cout << "skip (read): " << in.getFileName() << "\n";
            continue;
        }

        // Keep the whole recording unless it runs past the safety cap. The
        // set already tapers its own lengths with pitch, which is exactly
        // what a piano does, so the right move is to leave that alone.
        const juce::int64 capped = (juce::int64) (maxSeconds * reader->sampleRate);
        const bool truncated = reader->lengthInSamples > capped;
        const int wanted = (int) juce::jmin (reader->lengthInSamples, capped);

        juce::AudioBuffer<float> stereo ((int) reader->numChannels, wanted);
        reader->read (&stereo, 0, wanted, 0, true, reader->numChannels > 1);

        // Mixdown to mono (the family chain widens with delay and reverb).
        juce::AudioBuffer<float> mono (1, wanted);
        mono.clear();
        for (int ch = 0; ch < stereo.getNumChannels(); ++ch)
            mono.addFrom (0, 0, stereo, ch, 0, wanted, 1.0f / (float) stereo.getNumChannels());

        // Fade only what we actually cut. A note allowed to ring out to its
        // own end needs a short declick, not a long ramp that eats the tail.
        const double fadeSeconds = truncated ? fade : juce::jmin (fade, 0.02);
        const int fadeSamples = juce::jmin (wanted, (int) (fadeSeconds * reader->sampleRate));
        if (fadeSamples > 0)
            mono.applyGainRamp (0, wanted - fadeSamples, fadeSamples, 1.0f, 0.0f);

        const auto out = outDir.getChildFile ("p" + juce::String (root).paddedLeft ('0', 3)
                                              + (soft ? "_soft" : "_hard") + ".ogg");
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
        ++written;
        (soft ? softCount : hardCount)++;
        std::cout << in.getFileName() << " -> " << out.getFileName()
                  << "  " << juce::String (wanted / reader->sampleRate, 2) << "s"
                  << (truncated ? " (capped)" : "")
                  << "  (" << out.getSize() / 1024 << " KB)\n";
    }

    std::cout << "\n" << written << " samples: " << softCount << " soft, " << hardCount << " hard\n";
    std::cout << "total: " << totalOut / 1024 << " KB\n";

    // A set that lost a whole layer still "works" and sounds wrong in a way
    // nobody traces back to here, so say it rather than leaving it silent.
    if (softCount == 0 || hardCount == 0)
        std::cout << "WARNING: only one velocity layer was produced. Check the input names"
                     " - the soft samples should end in \"vL\".\n";
    return 0;
}
