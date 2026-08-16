// Dev tool: renders the plugin editor into a PNG so layout can be checked
// without loading the plugin in a host. Usage: AleaUISnapshot <out.png>
//
// The shot is posed mid-performance for the README: the Major -> Minor
// preset, morph parked at 70%, a fake host playhead running long enough to
// fill the history with notes from both scales, and a note sounding.
#include "../Source/Shifter/PluginEditor.h"
#include "../Source/Shifter/Presets.h"
#include <iostream>

namespace
{
    struct FakePlayHead : juce::AudioPlayHead
    {
        double ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (true);
            p.setBpm (110.0);
            p.setPpqPosition (ppq);
            return p;
        }
    };
}

// Regression check for the "switch preset mid-sweep bleeds the morph into the
// next preset" bug: sweep a preset up, switch to another, and confirm the new
// preset's morph starts fresh at A instead of inheriting the old position.
static int morphSwitchTest()
{
    AleaAudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);

    FakePlayHead playHead;
    processor.setPlayHead (&playHead);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    auto pump = [&] (int blocks)
    {
        for (int i = 0; i < blocks; ++i)
        {
            processor.processBlock (buffer, midi);
            midi.clear();
            playHead.ppq += 512.0 / 44100.0 * (110.0 / 60.0);
        }
    };
    auto applyPreset = [&] (int idx) // mirrors the editor's applyPresetAndMark
    {
        presets::apply (processor.apvts, presets::factory()[(size_t) idx]);
        processor.presetReanchor.store (true);
    };

    // Pentatonic Drift (index 2): a 16-bar Loop sweep. Run it a few bars in.
    applyPreset (2);
    pump (900);
    const double midMorph = processor.morphPercent.load();

    // Now switch presets mid-sweep, to another sweeping preset and to a
    // static one, and check the morph restarts at A each time.
    int failures = 0;
    for (int idx : { 3 /*Octave Climb, sweeps*/, 6 /*Major->Minor, static*/, 2 /*back to a sweep*/ })
    {
        applyPreset (idx);
        pump (2); // one block to consume the re-anchor, one to settle
        const double after = processor.morphPercent.load();
        const bool ok = after < 2.0; // fresh presets start at A (0%); allow a block of travel
        std::cout << "switch to preset " << idx << ": morph " << after
                  << "% " << (ok ? "OK" : "<-- BLED IN") << "\n";
        failures += ok ? 0 : 1;
    }

    std::cout << "(mid-sweep morph before switching was " << midMorph << "%)\n";
    processor.setPlayHead (nullptr);
    return failures == 0 ? 0 : 3;
}

// Regression check for two QA July 11 bugs. (1) Free timing is SECONDS: a
// note every 1.0 s must stay 1.0 s apart - and a free-duration sweep must
// hold its position - through a mid-play tempo change (the beat-domain
// schedule used to warp the pending gap and teleport the sweep). (2) The
// lit key slot must be root-relative to the ATTRIBUTED scale: a shared
// note picked from one pool but attributed to the other used to light the
// wrong key whenever the roots differed.
static int freeTempoTest()
{
    AleaAudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);

    struct TempoPlayHead : juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 120.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setIsPlaying (true);
            p.setBpm (bpm);
            p.setPpqPosition (ppq);
            return p;
        }
    };
    TempoPlayHead playHead;
    processor.setPlayHead (&playHead);

    auto set = [&] (const juce::String& id, float v)
    {
        auto* p = processor.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };

    // --- Part 1: free timing through a tempo slam (120 -> 30 mid-play) ---
    set ("intervalMode", (float) params::free);
    set ("intervalFree", 1.0f);
    set ("lengthMode", (float) params::free);
    set ("lengthFree", 0.4f);
    set ("autoSweep", 1.0f);
    set ("morphDurMode", 1.0f); // Free (seconds)
    set ("morphDurFree", 60.0f);
    set ("morphMode", (float) params::oneShot);
    set ("morphCurve", (float) params::linear);
    for (int r = 0; r < params::numRests; ++r) // rests are bar-based and would
    {                                          // legitimately stretch the gaps
        set (params::restId ('a', r), 0.0f);
        set (params::restId ('b', r), 0.0f);
    }

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    juce::int64 samples = 0;
    std::vector<double> onsets;
    auto pump = [&] (int blocks)
    {
        for (int i = 0; i < blocks; ++i)
        {
            processor.processBlock (buffer, midi);
            for (const auto meta : midi)
                if (meta.getMessage().isNoteOn())
                    onsets.push_back ((double) (samples + meta.samplePosition) / 44100.0);
            midi.clear();
            samples += 512;
            playHead.ppq += 512.0 / 44100.0 * (playHead.bpm / 60.0);
        }
    };

    pump (430); // ~5 s at 120 BPM
    const double morphBefore = processor.morphPercent.load();
    playHead.bpm = 30.0;
    pump (1);
    const double morphAfter = processor.morphPercent.load();
    pump (430); // ~5 s more at 30 BPM

    int failures = 0;
    for (size_t i = 1; i < onsets.size(); ++i)
        if (std::abs (onsets[i] - onsets[i - 1] - 1.0) > 0.05)
        {
            std::cout << "onset gap " << onsets[i] - onsets[i - 1] << " s before note "
                      << i << " <-- WARPED BY TEMPO\n";
            ++failures;
        }
    std::cout << onsets.size() << " onsets across the tempo change, gaps checked ~1.0 s\n";

    const bool jumped = std::abs (morphAfter - morphBefore) > 1.5;
    std::cout << "free 60 s sweep across the change: " << morphBefore << "% -> "
              << morphAfter << "% " << (jumped ? "<-- JUMPED" : "OK") << "\n";
    failures += jumped ? 1 : 0;

    // --- Part 2: lit key slot agrees with the sounding pitch ---
    // A = Am pentatonic shapes rooted at A, B = just C rooted at C, so the
    // two roots differ and a slot read against the wrong root shows up at
    // once: the lit key plus its scale's root must equal the note sounding.
    set ("autoSweep", 0.0f);
    set ("morphPos", 60.0f);
    set ("intervalMode", (float) params::sync);
    set ("intervalSync", 6.0f); // 1/16 notes - lots of draws
    set ("lengthMode", (float) params::sync);
    set ("lengthSync", 6.0f);
    const bool amPent[12] = { true, false, false, true, false, true, false, true, false, false, true, false };
    for (int i = 0; i < 12; ++i)
    {
        set (params::noteId ('a', i), amPent[i] ? 1.0f : 0.0f);
        set (params::noteId ('b', i), i == 0 ? 1.0f : 0.0f);
    }
    set ("aRoot", 9.0f); // A
    set ("bRoot", 0.0f); // C

    int checked = 0, wrong = 0;
    for (int block = 0; block < 2000; ++block)
    {
        pump (1);
        const int note = processor.activeNote.load();
        if (note < 0)
            continue;
        const int source = processor.activeSource.load();
        const int slot = processor.activeSourcePc.load();
        const int root = source == 0 ? 9 : 0;
        ++checked;
        if ((slot + root) % 12 != note % 12)
            ++wrong;
    }
    std::cout << "lit-key slot vs sounding note: " << checked << " checks, "
              << wrong << " wrong keys" << (wrong > 0 ? " <-- UI LIED" : " OK") << "\n";
    failures += wrong > 0 ? 1 : 0;

    // --- Part 3: notes report the pool that actually drew them (QA Aug 3) ---
    // B's notes are a subset of A's here, so under the old "shared notes
    // follow the morph" rule every B draw was stamped A and Scale B looked
    // dead for the whole first half of a sweep. At 25% morph roughly a
    // quarter of the draws come from B, and all of them must say so.
    set ("morphPos", 25.0f);
    set ("aRoot", 0.0f); // both scales in C, so every B note is shared with A
    playHead.bpm = 120.0; // back up from the tempo slam, for draws to count
    const bool pentachord[12] = { true, false, true, false, true, true, false, true, false, false, false, false };
    for (int i = 0; i < 12; ++i)
    {
        set (params::noteId ('a', i), pentachord[i] ? 1.0f : 0.0f);
        set (params::noteId ('b', i), i == 0 ? 1.0f : 0.0f);
    }

    int fromB = 0, total = 0, lastCount = processor.historyCount.load();
    for (int block = 0; block < 4000; ++block)
    {
        pump (1);
        const int now = processor.historyCount.load();
        for (int i = lastCount; i < now; ++i)
        {
            const int entry = processor.history[(size_t) (i % AleaAudioProcessor::historyCapacity)].load();
            ++total;
            fromB += (entry >> 8) & 1;
        }
        lastCount = now;
    }

    // Sampling noise around the 25% coin flip, not a tolerance on the rule.
    const double share = total > 0 ? 100.0 * fromB / total : 0.0;
    const bool ok = total > 100 && share > 15.0 && share < 35.0;
    std::cout << "shared notes attributed to B at 25% morph: " << fromB << "/" << total
              << " (" << share << "%)" << (ok ? " OK" : " <-- B IS INVISIBLE") << "\n";
    failures += ok ? 0 : 1;

    processor.setPlayHead (nullptr);
    return failures == 0 ? 0 : 4;
}

// Regression check for the update checker's tag selection (QA Aug 5, 2026).
// One repo ships both products, so picking "the newest release" is a real
// decision and it used to be got wrong: Scale Shifter read /releases/latest,
// which returned the Chord Randomizer's chords-v0.4.1, and offered it as an
// Alea update. This pins the selection rules without touching the network.
static int updateTagTest (bool live)
{
    struct Case
    {
        const char* what;
        const char* prefix;
        const char* json;
        const char* expected;
    };

    // The live repo's shape: two interleaved tag families, and note that
    // GitHub's ordering is not reliably newest-first.
    const char* bothFamilies = R"([
        {"tag_name":"v0.3.2"},        {"tag_name":"chords-v0.4.1"},
        {"tag_name":"v0.3.3"},        {"tag_name":"chords-v0.3.0"},
        {"tag_name":"v0.2.0"},        {"tag_name":"chords-v0.4.0"}])";

    const Case cases[] =
    {
        { "Scale Shifter ignores the chords family", "v", bothFamilies, "v0.3.3" },
        { "Chord Randomizer ignores the plain family", "chords-v", bothFamilies, "chords-v0.4.1" },
        { "highest version wins, not list position", "v",
          R"([{"tag_name":"v0.2.0"},{"tag_name":"v0.9.1"},{"tag_name":"v0.3.3"}])", "v0.9.1" },
        { "versions compare numerically, not as text", "v",
          R"([{"tag_name":"v0.9.0"},{"tag_name":"v0.10.0"}])", "v0.10.0" },
        { "a bare v prefix cannot claim another product's tag", "v",
          R"([{"tag_name":"viola-v9.9.9"},{"tag_name":"v0.1.0"}])", "v0.1.0" },
        { "no release of this product yet", "chords-v",
          R"([{"tag_name":"v0.3.3"}])", "" },
        { "empty release list", "v", "[]", "" },
        { "unreachable GitHub (empty body)", "v", "", "" },
        { "reply that will not parse", "v", "<html>502 Bad Gateway</html>", "" },
    };

    int failures = 0;
    for (const auto& c : cases)
    {
        const auto got = ui::newestReleaseTag (c.json, c.prefix);
        const bool ok = got == juce::String (c.expected);
        std::cout << (ok ? "OK   " : "FAIL ") << c.what
                  << ": prefix \"" << c.prefix << "\" -> \""
                  << got << "\" (expected \"" << c.expected << "\")\n";
        failures += ok ? 0 : 1;
    }

    std::cout << (failures == 0 ? "update tag selection OK\n"
                                : "update tag selection BROKEN\n");

    // "--updatetest live" additionally asks the real GitHub what each product
    // would be told right now. Informational only: it never moves the exit
    // code, so the check stays deterministic and offline by default.
    if (live)
    {
        const auto json = juce::URL ("https://api.github.com/repos/yuvalEG/Alea/releases?per_page=30")
                              .readEntireTextStream();
        std::cout << "\nlive GitHub reply: " << json.length() << " bytes\n";
        for (const auto* prefix : { "v", "chords-v" })
        {
            const auto tag = ui::newestReleaseTag (json, prefix);
            std::cout << "  prefix \"" << prefix << "\" -> "
                      << (tag.isEmpty() ? juce::String ("(none)") : tag) << "\n";
        }
    }

    return failures == 0 ? 0 : 5;
}

// The Scale Shifter half of the store poses (see ChordsUISnapshot --posetest).
static int posesTest()
{
    int failures = 0;
    auto check = [&failures] (const char* what, bool ok)
    {
        std::cout << (ok ? "OK   " : "FAIL ") << what << "\n";
        failures += ok ? 0 : 1;
    };
    auto value = [] (AleaAudioProcessor& p, const char* id)
    {
        auto* raw = p.apvts.getRawParameterValue (id);
        return raw != nullptr ? raw->load() : -1.0f;
    };

    for (const char* pose : { "shifter-01", "shifter-02", "shifter-03" })
    {
        AleaAudioProcessor p;
        p.applyScreenshotPose (pose);
        check ((juce::String (pose) + " arms the transport").toRawUTF8(),
               p.standaloneTransport.load());
        check ((juce::String (pose) + " puts the synth in frame").toRawUTF8(),
               p.synthOn.load());
        check ((juce::String (pose) + " lights a preset bubble").toRawUTF8(),
               p.currentPreset.load() >= 0);
    }

    {   AleaAudioProcessor p;
        p.applyScreenshotPose ("shifter-02");
        check ("shifter-02 is sweeping", value (p, "autoSweep") > 0.5f);
    }
    {   AleaAudioProcessor p;
        p.applyScreenshotPose ("shifter-01");
        const auto morph = value (p, "morphPos");
        check ("shifter-01 is part-way across, not parked at either end",
               morph > 5.0f && morph < 95.0f);
    }
    {   AleaAudioProcessor p;
        const bool was = p.standaloneTransport.load();
        p.applyScreenshotPose ("");
        check ("an empty pose changes nothing", p.standaloneTransport.load() == was);
    }

    std::cout << (failures == 0 ? "shifter poses OK\n" : "shifter poses BROKEN\n");
    return failures == 0 ? 0 : 7;
}

// Renders the real OUT ComboBox popup (grouped SYNTH / INSTRUMENT sections)
// to a PNG so the section-header styling can be eyeballed without opening the
// app and clicking the menu.
static int menuShot (const juce::String& path)
{
    AleaAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (940, 704); // realises the editor so its LookAndFeel is live

    juce::ComboBox box;
    box.setLookAndFeel (&editor->getLookAndFeel());
    // Built by the shared chooser, so this shot shows the real menu rather
    // than a third copy of the population loop that could drift from it.
    // Posed as a plugin: no device list, so the shot is the same everywhere.
    juce::Array<juce::MidiDeviceInfo> devices;
    ui::buildOutputChooser (box, false, false, {}, devices);

    auto& menu = *box.getRootMenu();
    juce::Image image (juce::Image::ARGB, 300, 460, true);
    {
        juce::Graphics g (image);
        g.fillAll (juce::Colour (0xff12121a));
        // Draw each row through the live LookAndFeel, headers included.
        int y = 8;
        auto& lf = editor->getLookAndFeel();
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            const auto& item = it.getItem();
            const juce::Rectangle<int> area (8, y, 284, item.isSectionHeader ? 28 : 30);
            if (item.isSectionHeader)
                lf.drawPopupMenuSectionHeader (g, area, item.text);
            else
            {
                g.setColour (juce::Colour (0xffe8e8f0));
                g.setFont (juce::FontOptions (16.0f));
                g.drawText (item.text, area.reduced (14, 0), juce::Justification::centredLeft);
            }
            y += area.getHeight();
        }
    }
    editor.reset();

    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    return (stream.openedOk() && png.writeImageToStream (image, stream)) ? 0 : 1;
}

// Renders each hardware primitive isolated on a gallery card, to compare
// cell-by-cell against design_handoff .../Component Gallery.html.
static int galleryShot (const juce::String& path)
{
    using namespace ui;
    const int cellW = 300, cellH = 140, cols = 2, pad = 16;
    struct Cell { const char* title; std::function<void (juce::Graphics&, juce::Rectangle<float>)> draw; };
    const juce::Colour green (0xff10b981), purple (0xff7c3aed), cyan (0xff06b6d4);

    std::vector<Cell> cells = {
        { "KNOB - unipolar", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { hw::knob (g, a.withSizeKeepingCentre (84, 84).translated (0, -8), 0.68f, green, false);
              g.setColour (juce::Colour (0xff9297a8)); g.setFont (juce::FontOptions (11.0f));
              g.drawText ("LEVEL", a.removeFromBottom (34).removeFromTop (16).toNearestInt(), juce::Justification::centred);
              g.setColour (juce::Colour (0xffd6d9e4)); g.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());
              g.drawText ("-3.1 dB", a.removeFromBottom (18).toNearestInt(), juce::Justification::centred); } },
        { "KNOB - bipolar", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { hw::knob (g, a.withSizeKeepingCentre (84, 84).translated (0, -8), 0.5f, green, true); } },
        { "LCD / glass screen", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { auto r = a.withSizeKeepingCentre (150, 60);
              hw::lcd (g, r, purple);
              g.setFont (juce::Font (juce::FontOptions (30.0f)).boldened());
              hw::glowText (g, "C5", r.toNearestInt(), juce::Justification::centred, purple.brighter (0.4f)); } },
        { "TEMPO display", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { auto r = a.withSizeKeepingCentre (120, 40);
              hw::lcd (g, r, green);
              g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
              hw::glowText (g, "120 BPM", r.toNearestInt(), juce::Justification::centred, green.brighter (0.4f)); } },
        { "METER", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { hw::meter (g, a.withSizeKeepingCentre (16, 90), 0.62f); } },
        { "PUSH-BUTTON off / lit", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { auto off = juce::Rectangle<float> (a.getCentreX() - 130, a.getCentreY() - 14, 84, 28);
              auto on  = juce::Rectangle<float> (a.getCentreX() + 6, a.getCentreY() - 14, 120, 28);
              auto c1 = hw::button (g, off, false, hw::led, false, false);
              g.setColour (c1); g.setFont (juce::FontOptions (12.0f)); g.drawText ("FREEZE", off, juce::Justification::centred);
              juce::Path p; p.addRoundedRectangle (on, 4.0f); hw::dropGlow (g, p, hw::led, 6);
              auto c2 = hw::button (g, on, true, hw::led, false, false);
              g.setColour (c2); g.drawText ("AUTO-SWEEP", on, juce::Justification::centred); } },
        { "SEGMENTED", [&] (juce::Graphics& g, juce::Rectangle<float> a)
            { auto r = a.withSizeKeepingCentre (220, 30);
              hw::insetWell (g, r, 6.0f);
              const char* opts[] = { "SYNC", "FREE", "RAND" };
              for (int i = 0; i < 3; ++i)
              { auto seg = juce::Rectangle<float> (r.getX() + r.getWidth() / 3.0f * i, r.getY(), r.getWidth() / 3.0f, r.getHeight()).reduced (3.0f);
                if (i == 1) { juce::Path p; p.addRoundedRectangle (seg, 4.0f); hw::dropGlow (g, p, hw::led, 6); }
                auto lc = hw::button (g, seg, i == 1, hw::led, false, false);
                g.setColour (lc); g.setFont (juce::FontOptions (12.0f)); g.drawText (opts[i], seg, juce::Justification::centred); } } },
    };

    const int rows = ((int) cells.size() + cols - 1) / cols;
    juce::Image img (juce::Image::ARGB, pad + cols * (cellW + pad), pad + rows * (cellH + pad) + 40, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour (0xff0a0a0f));
    g.setColour (juce::Colour (0xffe8e8f0)); g.setFont (juce::Font (juce::FontOptions (16.0f)).boldened());
    g.drawText ("Alea Hardware - JUCE primitives", pad, 8, 600, 24, juce::Justification::centredLeft);
    for (size_t i = 0; i < cells.size(); ++i)
    {
        const int cx = pad + (int) (i % cols) * (cellW + pad);
        const int cy = 40 + (int) (i / cols) * (cellH + pad);
        auto card = juce::Rectangle<float> ((float) cx, (float) cy, (float) cellW, (float) cellH);
        g.setColour (juce::Colour (0xff17171f)); g.fillRoundedRectangle (card, 10.0f);
        g.setColour (juce::Colour (0xff6b6f80)); g.setFont (juce::FontOptions (10.5f));
        g.drawText (juce::String (cells[i].title).toUpperCase(), card.reduced (14, 8).removeFromTop (14).toNearestInt(),
                    juce::Justification::centredLeft);
        cells[i].draw (g, card.reduced (16.0f).withTrimmedTop (18.0f));
    }

    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    return (stream.openedOk() && png.writeImageToStream (img, stream)) ? 0 : 1;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--morphtest")
        return morphSwitchTest();
    if (argc > 1 && juce::String (argv[1]) == "--posetest")
        return posesTest();
    if (argc > 1 && juce::String (argv[1]) == "--updatetest")
        return updateTagTest (argc > 2 && juce::String (argv[2]) == "live");
    if (argc > 1 && juce::String (argv[1]) == "--freetempotest")
        return freeTempoTest();
    if (argc > 2 && juce::String (argv[1]) == "--gallery")
        return galleryShot (argv[2]);
    if (argc > 2 && juce::String (argv[1]) == "--menushot")
        return menuShot (argv[2]);

    AleaAudioProcessor processor;
    processor.setPlayConfigDetails (0, 2, 44100.0, 512);
    processor.prepareToPlay (44100.0, 512);

    // The tool builds the processor outside any host, which now counts as
    // standalone, so the shot wears the transport the app really shows. Arm
    // it: notes are about to sound from the fake playhead below, and a PLAY
    // button beside a sounding note would be the UI lying in the README.
    processor.standaloneTransport.store (true);

    // Optional: pose with a synth output so the OUTPUT panel shows the
    // level meter + volume knob (dev-only calibration flag).
    if (argc > 2 && juce::String (argv[2]) == "synth")
        processor.setStandaloneOutput ("synth");

    // Major <-> Minor by default, sweep off, morph posed by hand. A bare
    // number anywhere in the args poses another preset by index instead
    // (dev calibration: verify a preset edit on the real UI).
    int presetIdx = 6;
    for (int i = 2; i < argc; ++i)
        if (juce::String (argv[i]).isNotEmpty() && juce::String (argv[i]).containsOnly ("0123456789"))
            presetIdx = juce::jlimit (0, (int) presets::factory().size() - 1,
                                      juce::String (argv[i]).getIntValue());
    presets::apply (processor.apvts, presets::factory()[(size_t) presetIdx]);
    processor.currentPreset.store (presetIdx); // keep its bubble lit
    if (auto* sweep = processor.apvts.getParameter ("autoSweep"))
        sweep->setValueNotifyingHost (0.0f);
    auto* morph = processor.apvts.getParameter ("morphPos");

    FakePlayHead playHead;
    processor.setPlayHead (&playHead);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    auto pump = [&]
    {
        processor.processBlock (buffer, midi);
        midi.clear();
        playHead.ppq += 512.0 / 44100.0 * (110.0 / 60.0);
    };

    // Two acts, so the history holds notes from BOTH scales: a stretch near
    // A (purple entries), then the pose position at 70% (cyan takes over).
    if (morph != nullptr)
        morph->setValueNotifyingHost (morph->convertTo0to1 (25.0f));
    for (int block = 0; block < 1400; ++block)
        pump();
    if (morph != nullptr)
        morph->setValueNotifyingHost (morph->convertTo0to1 (70.0f));
    for (int block = 0; block < 170; ++block) // a couple of notes at 70%, so the
        pump();                               // visible history keeps both colors
    for (int block = 0; block < 500 && processor.activeNote.load() < 0; ++block)
        pump(); // land on a sounding note

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300); // timers paint the live state

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    editor.reset();
    processor.setPlayHead (nullptr);

    const auto path = argc > 1 ? juce::String (argv[1]) : juce::String ("ui_snapshot.png");
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
        return 1;

    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream) ? 0 : 1;
}
