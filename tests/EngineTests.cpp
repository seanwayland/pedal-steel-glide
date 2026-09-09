// Minimal dependency-light tests for GlideEngine (juce_audio_basics only).
// Exit code 0 = pass. Run via CTest or directly.
#include "GlideEngine.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } \
                              else std::printf("ok:   %s\n", msg); } while (0)

struct Harness
{
    GlideEngine eng;
    GlideParams p;
    double sr = 48000.0;
    int    blockLen = 256;

    Harness() { eng.prepare (sr); }

    // feed one block containing the given messages (at sample 0), return the output
    std::vector<juce::MidiMessage> block (std::vector<juce::MidiMessage> in = {})
    {
        juce::MidiBuffer mb;
        for (auto& m : in) mb.addEvent (m, 0);
        eng.process (mb, blockLen, p);
        std::vector<juce::MidiMessage> out;
        for (const auto meta : mb) out.push_back (meta.getMessage());
        return out;
    }
};

static int countNoteOns (const std::vector<juce::MidiMessage>& v)
{
    int n = 0; for (auto& m : v) if (m.isNoteOn()) ++n; return n;
}
static const juce::MidiMessage* lastPitchBend (const std::vector<juce::MidiMessage>& v, int ch)
{
    const juce::MidiMessage* r = nullptr;
    for (auto& m : v) if (m.isPitchWheel() && m.getChannel() == ch) r = &m;
    return r;
}

int main()
{
    // ---- fresh note -> one noteOn on a voice channel + an initial bend
    {
        Harness h; h.p.polyphony = 6; h.p.masterChannel = 1;
        auto out = h.block ({ juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
        CHECK (countNoteOns (out) == 1, "fresh note emits exactly one noteOn");
        bool onCh2 = false;
        for (auto& m : out) if (m.isNoteOn()) onCh2 = (m.getChannel() == 2);
        CHECK (onCh2, "first MPE voice is on masterChannel+1");
    }

    // ---- legato within the interval glides, does NOT retrigger
    {
        Harness h; h.p.polyphony = 6; h.p.legatoSemitones = 2.0f; h.p.glideMinMs = 40; h.p.glideMaxMs = 40;
        h.block ({ juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110) });
        auto out = h.block ({ juce::MidiMessage::noteOn (1, 62, (juce::uint8) 110) });   // whole step up
        CHECK (countNoteOns (out) == 0, "legato step up does not retrigger");
        // track the last bend the engine emitted anywhere over the glide
        int lastPB = 8192; bool sawBend = false;
        for (int i = 0; i < 40; ++i)
        {
            auto blk = h.block();
            if (auto* pb = lastPitchBend (blk, 2)) { lastPB = pb->getPitchWheelValue(); sawBend = true; }
        }
        CHECK (sawBend, "gliding voice emits pitch bend");
        const double semis = (lastPB - 8192) / 8191.0 * 12.0;   // bendRange 12
        CHECK (std::abs (semis - 2.0) < 0.15, "glide lands ~2 semitones up");
    }

    // ---- a jump larger than the interval starts a new note
    {
        Harness h; h.p.polyphony = 6; h.p.legatoSemitones = 2.0f;
        h.block ({ juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
        auto out = h.block ({ juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) });   // a fifth
        CHECK (countNoteOns (out) == 1, "a large jump retriggers (new voice)");
    }

    // ---- note off only when every key feeding the voice is released
    {
        Harness h; h.p.polyphony = 6; h.p.legatoSemitones = 2.0f;
        h.block ({ juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
        h.block ({ juce::MidiMessage::noteOn (1, 61, (juce::uint8) 100) });   // legato, voice now held by 2 keys
        auto a = h.block ({ juce::MidiMessage::noteOff (1, 61) });
        int offs = 0; for (auto& m : a) if (m.isNoteOff()) ++offs;
        CHECK (offs == 0, "releasing the newer key keeps the voice sounding");
        auto b = h.block ({ juce::MidiMessage::noteOff (1, 60) });
        offs = 0; for (auto& m : b) if (m.isNoteOff()) ++offs;
        CHECK (offs == 1, "releasing the last key stops the voice");
    }

    // ---- monophonic mode: everything on the master channel, no MPE
    {
        Harness h; h.p.polyphony = 1; h.p.masterChannel = 1;
        auto out = h.block ({ juce::MidiMessage::noteOn (5, 48, (juce::uint8) 90) });
        bool allCh1 = true;
        for (auto& m : out) if (m.isNoteOnOrOff() || m.isPitchWheel()) allCh1 &= (m.getChannel() == 1);
        CHECK (allCh1, "mono mode keeps every note/bend on the master channel");
    }

    // ---- the hardware wheel bends a sounding voice
    {
        Harness h; h.p.polyphony = 6; h.p.wheelRangeSemis = 2.0f; h.p.bendRangeSemis = 12;
        h.block ({ juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) });
        auto out = h.block ({ juce::MidiMessage::pitchWheel (1, 8192 + 8191) });   // wheel full up
        auto* pb = lastPitchBend (out, 2);
        CHECK (pb != nullptr, "wheel produces a voice bend");
        if (pb)
        {
            const double semis = (pb->getPitchWheelValue() - 8192) / 8191.0 * 12.0;
            CHECK (std::abs (semis - 2.0) < 0.1, "full wheel = +wheelRange semitones");
        }
    }

    // ---- long random hammering must not crash / assert
    {
        Harness h; h.p.polyphony = 6;
        juce::Random r (99);
        for (int i = 0; i < 4000; ++i)
        {
            std::vector<juce::MidiMessage> in;
            if (r.nextFloat() < 0.3f) in.push_back (juce::MidiMessage::noteOn (1, 36 + r.nextInt (48), (juce::uint8) (1 + r.nextInt (126))));
            if (r.nextFloat() < 0.3f) in.push_back (juce::MidiMessage::noteOff (1, 36 + r.nextInt (48)));
            if (r.nextFloat() < 0.1f) in.push_back (juce::MidiMessage::pitchWheel (1, r.nextInt (16384)));
            if (r.nextFloat() < 0.1f) in.push_back (juce::MidiMessage::channelPressureChange (1, r.nextInt (128)));
            h.block (in);
        }
        CHECK (true, "4000 blocks of random MIDI survived");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
