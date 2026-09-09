# Pedal Steel Glide

A small **MIDI-effect plugin** (VST3 / AU / Standalone) that makes a keyboard
play like a pedal steel: play legato and the held note **glides** into the new
pitch instead of retriggering — hard playing snaps, soft playing eases in.
The pitch wheel bends everything on top; aftertouch adds a per-voice vibrato.

It's a faithful port of a Logic Pro Scripter MIDI FX (and its REAPER JSFX
twin) to a real cross-platform plugin. It produces **only MIDI** — insert it
*before* an instrument.

## How it works

- **Legato glide.** Press a new key while still holding the last one, within
  `Legato Interval` semitones of it, and that voice bends to the new note with
  no new attack. Jump further, or play staccato, and you get a normal new note.
- **Velocity → glide speed.** `Glide Fast` is the time at velocity 127,
  `Glide Slow` at velocity 0, shaped by `Glide Curve`.
- **Polyphony.** Each voice is its own MIDI channel with its own bend
  (MPE-style), so chords glide independently. Set **`Polyphony = 1`** for a
  single-channel monophonic mode that works with *any* synth and needs no MPE.
- **Pitch wheel** bends every sounding voice at once, `Wheel Range` semitones.
- **Vibrato.** Aftertouch (or Mod Wheel / Breath — `Vibrato Source`) drives a
  ~`Vibrato Rate` Hz vibrato up to `Vibrato Depth` semitones, each voice
  slightly de-tuned in rate and depth so chords don't wobble in unison.

## Setting up the receiving instrument

Because independent per-note bends need per-note channels, in **polyphonic**
mode the target instrument must be in **MPE / MIDI Mono mode**:

| plugin control | set the instrument to |
|---|---|
| `Master Channel` (default 1) | its MPE **base / global channel** |
| `Bend Range` (default 12) | its **Pitch Bend Up/Down** range — must match exactly |

With `Auto-Set Bend Range` on, the plugin sends RPN 0 so many instruments
configure their own bend range automatically.

**`Polyphony = 1`** skips all of that — one channel, one voice, works
everywhere (Master Channel is the channel it uses).

### Host notes

Works as a MIDI insert in Logic (AU MIDI FX), Cubase, Reaper, Bitwig, Studio
One. Ableton Live doesn't host MIDI-only VST3s well — use the original REAPER
JSFX or a Max device there.

## Build it yourself

No submodules — JUCE is fetched by CMake.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

Built plugins land under `build/PedalSteelGlide_artefacts/Release/`.

**Linux** also needs the usual JUCE dev packages
(`libasound2-dev libfreetype-dev libx11-dev libxext-dev libxinerama-dev
libxrandr-dev libxcursor-dev libcurl4-openssl-dev` …).

**Fork it and CI just works** — `.github/workflows/build.yml` builds Linux /
macOS / Windows on every push and uploads the plugins as artifacts. No secrets
required. (The macOS/Windows artifacts are unsigned; you'll get a Gatekeeper /
SmartScreen warning unless you sign them yourself.)

## License

GPLv3 — see [LICENSE](LICENSE). This project links JUCE under its free
licensing tier, which requires GPL-compatible distribution.
