#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

// =============================================================================
// Pedal Steel Glide - the note/pitch-bend state machine.
//
// A direct port of the "Pedal Steel Glide" Logic Pro Scripter MIDI FX (and its
// REAPER JSFX twin). Header-only so the tests can use it without the plugin
// wrapper.
//
// What it does: play legato (press a new key while still holding the last) on a
// note within `legatoSemitones` of the sounding one and the held note GLIDES
// into the new pitch instead of retriggering - velocity sets the glide speed.
// Play staccato or jump further and you get a normal new note. The pitch wheel
// bends everything at once on top; aftertouch (or mod/breath) adds a per-voice
// vibrato, each voice slightly de-synced so chords don't wobble in unison.
//
// Output is MPE-style: each voice is a MIDI channel of its own and carries its
// own pitch bend. Set `polyphony` to 1 for a single-channel monophonic mode
// that needs no MPE support in the receiving instrument.
// =============================================================================

struct GlideParams
{
    int   polyphony        = 6;     // 1 = mono single-channel; >1 = MPE voices
    int   masterChannel    = 1;     // 1-based; wheel/aftertouch arrive here, voices use the ones above it
    float legatoSemitones  = 2.0f;  // a new note this close (or closer) glides
    float glideMinMs       = 40.0f; // glide time at velocity 127
    float glideMaxMs       = 1200.0f;// glide time at velocity 0
    float glideCurve       = 2.0f;  // velocity->time exponent (1 = linear)
    int   bendRangeSemis   = 12;    // MUST match the receiving instrument's PB range
    float wheelRangeSemis  = 2.0f;  // how far the hardware wheel bends everything
    float vibratoRateHz    = 3.0f;
    float vibratoDepthSemis = 0.30f;// depth at full aftertouch, per-voice x0.5..1.0
    float vibratoRandom    = 0.15f; // per-voice rate spread
    int   vibratoSourceCC  = -1;    // -1 = channel pressure; else a CC number (1 = mod, 2 = breath)
    bool  cc7Passthrough   = true;
    bool  autoBendRange    = true;  // emit RPN 0 so the instrument sets its own PB range
};

class GlideEngine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = Voice{};
        keyToVoice.fill (-1);
        nowMs = 0.0;
        globalBendSemis = 0.0f;
        modAmount = 0.0f;
        nextVoice = 0;
        lastSentBendRange = -1;
    }

    // Consumes `midi` and replaces it with the transformed stream.
    void process (juce::MidiBuffer& midi, int numSamples, const GlideParams& pIn)
    {
        p = pIn;
        effMaster = juce::jlimit (1, 15, p.masterChannel);
        effPoly   = juce::jlimit (1, juce::jmax (1, 16 - effMaster),
                                  juce::jlimit (1, 8, p.polyphony));
        const int poly = effPoly, master = effMaster;

        juce::MidiBuffer out;

        if (p.autoBendRange && p.bendRangeSemis != lastSentBendRange)
        {
            emitBendRangeRPN (out, 0, poly, master);
            lastSentBendRange = p.bendRangeSemis;
        }

        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const int  s = meta.samplePosition;
            const double evtMs = nowMs + (sampleRate > 0.0 ? (s / sampleRate) * 1000.0 : 0.0);

            if (m.isNoteOn())
                handleNoteOn (out, s, evtMs, m.getNoteNumber(), m.getVelocity(), poly, master);
            else if (m.isNoteOff())
                handleNoteOff (out, s, m.getNoteNumber());
            else if (m.isPitchWheel())
                globalBendSemis = ((float) (m.getPitchWheelValue() - 8192) / 8191.0f) * p.wheelRangeSemis;
            else if (isVibratoSource (m))
                modAmount = juce::jlimit (0.0f, 1.0f, vibratoSourceValue (m) / 127.0f);
            else if (m.isController() && m.getControllerNumber() == 7)
            { if (p.cc7Passthrough) out.addEvent (m, s); }
            else if (! m.isController() || ! isConsumedController (m.getControllerNumber()))
                out.addEvent (m, s);   // everything else passes straight through
        }

        nowMs += sampleRate > 0.0 ? (numSamples / sampleRate) * 1000.0 : 0.0;

        // continuous per-block bend update (glide + wheel + vibrato)
        const int at = numSamples > 0 ? numSamples - 1 : 0;
        for (int i = 0; i < poly; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.note < 0) continue;
            const float combined = currentNoteBend (v, nowMs) + globalBendSemis
                                 + currentVibratoBend (v, nowMs);
            const int pb = semitoneToPB14 (combined);
            if (pb != v.lastPB)
            {
                out.addEvent (juce::MidiMessage::pitchWheel (voiceChannel (i, poly, master), pb + 8192), at);
                v.lastPB = pb;
            }
        }

        midi.swapWith (out);
    }

private:
    struct Voice
    {
        int   note = -1;          // the anchor note this voice was struck on (-1 = free)
        int   heldKeys = 0;       // physical keys keeping it alive
        float glideFrom = 0.0f, glideTo = 0.0f;
        double glideStartMs = 0.0, glideDurMs = 0.0;
        float vibPhase = 0.0f, vibRate = 3.0f, vibDepthMul = 1.0f;
        int   lastPB = -100000;
    };

    int voiceChannel (int i, int poly, int master) const
    {
        return poly == 1 ? master : master + 1 + i;
    }

    float currentNoteBend (const Voice& v, double curMs) const
    {
        if (v.glideDurMs <= 0.0) return v.glideTo;
        const double t = (curMs - v.glideStartMs) / v.glideDurMs;
        if (t >= 1.0) return v.glideTo;
        if (t <= 0.0) return v.glideFrom;
        const float e = (float) (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t));
        return v.glideFrom + (v.glideTo - v.glideFrom) * e;
    }

    float currentVibratoBend (const Voice& v, double curMs) const
    {
        if (modAmount <= 0.0f || p.vibratoDepthSemis <= 0.0f) return 0.0f;
        const double phase = juce::MathConstants<double>::twoPi * v.vibRate * (curMs / 1000.0) + v.vibPhase;
        return modAmount * (p.vibratoDepthSemis * v.vibDepthMul) * (float) std::sin (phase);
    }

    float velToGlideMs (int vel) const
    {
        const float soft = 1.0f - juce::jlimit (0.0f, 1.0f, (float) vel / 127.0f);
        return p.glideMinMs + std::pow (soft, juce::jmax (0.1f, p.glideCurve))
                              * (p.glideMaxMs - p.glideMinMs);
    }

    int semitoneToPB14 (float semis) const
    {
        const float span = (float) juce::jmax (1, p.bendRangeSemis);
        return juce::jlimit (-8192, 8191, (int) std::lround (semis / span * 8191.0f));
    }

    bool isConsumedController (int cc) const
    {
        return (cc == 1 && p.vibratoSourceCC == 1) || (cc == 2 && p.vibratoSourceCC == 2);
    }
    bool isVibratoSource (const juce::MidiMessage& m) const
    {
        if (p.vibratoSourceCC < 0) return m.isChannelPressure() || m.isAftertouch();
        return m.isController() && m.getControllerNumber() == p.vibratoSourceCC;
    }
    float vibratoSourceValue (const juce::MidiMessage& m) const
    {
        if (m.isChannelPressure()) return (float) m.getChannelPressureValue();
        if (m.isAftertouch())      return (float) m.getAfterTouchValue();
        return (float) m.getControllerValue();
    }

    void startGlide (Voice& v, float from, float to, float durMs, double startMs)
    {
        v.glideFrom = from; v.glideTo = to; v.glideDurMs = durMs; v.glideStartMs = startMs;
    }

    void handleNoteOn (juce::MidiBuffer& out, int s, double evtMs, int n, int vel,
                       int poly, int master)
    {
        // 1) legato match: a sounding voice whose current pitch is within reach
        int match = -1;
        for (int i = 0; i < poly; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.note < 0) continue;
            const float eff = (float) v.note + currentNoteBend (v, evtMs);
            if (std::abs ((float) n - eff) <= p.legatoSemitones) { match = i; break; }
        }

        if (match >= 0)
        {
            auto& v = voices[(size_t) match];
            startGlide (v, currentNoteBend (v, evtMs), (float) (n - v.note),
                        velToGlideMs (vel), evtMs);
            v.heldKeys += 1;
            keyToVoice[(size_t) n] = match;
            return;
        }

        // 2) allocate a fresh voice (round-robin, prefer a free one)
        int c = -1;
        for (int i = 0; i < poly; ++i)
        {
            const int cand = (nextVoice + i) % poly;
            if (voices[(size_t) cand].note < 0) { c = cand; break; }
        }
        if (c < 0) c = nextVoice % poly;
        nextVoice = (c + 1) % poly;

        auto& v = voices[(size_t) c];
        const int ch = voiceChannel (c, poly, master);

        if (v.note >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (ch, v.note), s);
            for (auto& k : keyToVoice) if (k == c) k = -1;
            v.heldKeys = 0;
        }

        v.note = n;
        v.glideFrom = v.glideTo = 0.0f;
        v.glideDurMs = 0.0; v.glideStartMs = evtMs;
        v.heldKeys = 1;
        v.lastPB = -100000;
        keyToVoice[(size_t) n] = c;

        v.vibPhase    = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        v.vibRate     = p.vibratoRateHz * (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * p.vibratoRandom);
        v.vibDepthMul = 0.5f + rng.nextFloat() * 0.5f;

        const int pb0 = semitoneToPB14 (globalBendSemis + currentVibratoBend (v, evtMs));
        out.addEvent (juce::MidiMessage::pitchWheel (ch, pb0 + 8192), s);
        v.lastPB = pb0;
        out.addEvent (juce::MidiMessage::noteOn (ch, n, (juce::uint8) vel), s);
    }

    void handleNoteOff (juce::MidiBuffer& out, int s, int n)
    {
        const int c = keyToVoice[(size_t) n];
        if (c < 0) return;
        keyToVoice[(size_t) n] = -1;
        auto& v = voices[(size_t) c];
        if (v.heldKeys > 0) v.heldKeys -= 1;
        if (v.heldKeys <= 0 && v.note >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (voiceChannel (c, effPoly, effMaster), v.note), s);
            v.note = -1;
            v.glideDurMs = 0.0;
            v.lastPB = -100000;
        }
    }

    void emitBendRangeRPN (juce::MidiBuffer& out, int s, int poly, int master)
    {
        const int range = juce::jlimit (0, 96, p.bendRangeSemis);
        for (int i = 0; i < poly; ++i)
        {
            const int ch = voiceChannel (i, poly, master);
            out.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 0), s);
            out.addEvent (juce::MidiMessage::controllerEvent (ch, 100, 0), s);
            out.addEvent (juce::MidiMessage::controllerEvent (ch, 6, range), s);
            out.addEvent (juce::MidiMessage::controllerEvent (ch, 38, 0), s);
            out.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 127), s);
            out.addEvent (juce::MidiMessage::controllerEvent (ch, 100, 127), s);
        }
    }

    GlideParams p;
    double sampleRate = 48000.0;
    double nowMs = 0.0;
    float  globalBendSemis = 0.0f;
    float  modAmount = 0.0f;
    int    nextVoice = 0;
    int    lastSentBendRange = -1;
    int    effPoly = 6, effMaster = 1;
    juce::Random rng;

    std::array<Voice, 8> voices {};
    std::array<int, 128> keyToVoice { };
};
