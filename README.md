# Polylogue

A polyphonic software synthesizer built on the architecture of a Korg monologue: two oscillators
with sync and ring modulation, a 2-pole low-pass filter with drive, two-stage envelopes and one
LFO. Original branding, interface and sounds; nothing from the hardware is copied.

VST3, AU and Standalone, macOS Apple Silicon first. C++20, JUCE 8, CMake.

The panel follows the monologue: master and drive, two oscillators, mixer, filter, two envelopes,
LFO and performance settings, plus a display with a visualizer, a preset browser and an on-screen
keyboard. Every control can be driven and learned from MIDI.

```
 POLYLOGUE                                                    [SAVE] [MAP]     MIDI •
┌─────────────────────────────────────────────────────────────────────────────────────┐
│ Warm Pad •      ‹  ›                                                                │
│ PAD  01/30                         oscilloscope  /  spectrum                        │
│ CUTOFF  1.29 kHz                                                                    │
└─────────────────────────────────────────────────────────────────────────────────────┘
 MASTER   VCO 1     VCO 2                    MIXER      FILTER
 AMP EG          MOD EG               LFO                        PLAY
 OCTAVE  TUNE  BEND  GLIDE  GLIDE MODE
 [ on-screen keyboard  C2 - C7 ]
```

## Install

Grab `Polylogue-macOS.zip` from **[Releases](https://github.com/rithulkamesh/polylogue/releases)**.
It contains the AU, the VST3 and the standalone app, as a universal binary (Apple Silicon and Intel).

### macOS (read this)

Builds are **unsigned and not notarized**: there is no paid Apple Developer Program membership on
this project, so Gatekeeper treats a fresh download as untrusted. That is an Apple policy and cost
constraint, not malware. Nothing phones home.

**Recommended:** the plain bash installer in this repo ([`scripts/install.sh`](scripts/install.sh)).
Read it before piping if you like.

```sh
curl -fsSL https://raw.githubusercontent.com/rithulkamesh/polylogue/master/scripts/install.sh | bash
```

It asks GitHub for the latest release, downloads `Polylogue-macOS.zip`, clears the
`com.apple.quarantine` attribute, ad-hoc signs the bundles (`codesign --force --deep -s -`; a local
signature only, not a Developer ID), and copies them into user paths, with no `sudo`:

- `~/Library/Audio/Plug-Ins/Components/Polylogue.component`
- `~/Library/Audio/Plug-Ins/VST3/Polylogue.vst3`
- `~/Applications/Polylogue.app`

**Manual:** download the zip, then right-click **Open** on each bundle (or allow it under
**System Settings > Privacy & Security**), and copy the AU and VST3 into the folders above.
Rescan plugins in your DAW afterwards.

## Build

Needs CMake 3.25+, Ninja, and the Xcode command line tools. Configure fetches JUCE and Catch2.

```sh
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release --output-on-failure
```

The plugins land in `build/release/src/plugin/Polylogue_artefacts/Release/{VST3,AU,Standalone}`.
To install them into `~/Library` after building, configure with `-DPOLYLOGUE_COPY_PLUGINS=ON`.

Presets: `dev` (Debug), `sanitize` (ASan + UBSan, no plugin bundles), `tsan` (ThreadSanitizer),
`release`. `-DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE` reuses a local JUCE checkout.

## Playing

- **Notes, velocity, pitch bend, sustain** come from any MIDI keyboard. In the standalone app,
  choose the device under *Options*.
- **Presets:** click the name on the display to browse by category, use `‹ ›` or the mouse wheel to
  step through them. 30 factory sounds: pads, bells, gongs, basses, leads, plucks, ambient,
  distorted, keys.
- **Saving:** **SAVE** (or *Save As...* in the preset menu) stores the current sound under a name.
  Saved sounds appear under *User* and live in `~/Library/Application Support/Polylogue/Presets`.
  A dot after the name means the sound was edited since it was loaded.
- **The visualizer** shows the output as a triggered oscilloscope. Click it for a spectrum.

## MIDI controller mapping

Controls answer to the monologue's own controller chart by default (attack 16, decay 17, LFO rate
24, EG int 25, LFO int 26, drive 28, VCO 2 pitch 35, shapes 36/37, levels 39/40, cutoff 43,
resonance 44, octave 49, waves 50/51, LFO target/wave/mode 56/58/59, sync/ring 60, EG type/target
61/62, master level 7), so a controller set up for a monologue works unchanged. Every control shows
its CC beneath it. To use your own controller:

1. Press **MAP** and click any control, knob or switch (or right-click it and choose *MIDI Learn*).
2. Move a control on your hardware. It is bound, and the label under the knob shows `CC n`.

Right-click a knob to clear its mapping or restore the defaults. The mapping is saved with your
session and also on this machine (`~/Library/Application Support/Polylogue/midi-map.xml`), so it
survives new instances, and changing presets never unmaps your controller.

Every parameter is automatable from your DAW.

## The panel

| Section | Controls |
| --- | --- |
| MASTER | Level, drive |
| VCO 1 | Wave (saw, triangle, square), shape |
| VCO 2 | Octave (16' - 2'), wave (saw, triangle, noise), sync/ring, pitch (fine near the centre, up to +/-1200 cents), shape |
| MIXER | VCO 1 and VCO 2 levels |
| FILTER | Cutoff, resonance (the top of the range self-oscillates), key tracking, velocity to cutoff |
| AMP EG | Type (A/D, A/G/D, gate), attack, decay (release for A/G/D), velocity to amp |
| MOD EG | Type, attack, decay, INT (bipolar), target (cutoff, pitch, pitch 2) |
| LFO | Wave, mode (fast, slow, one-shot), rate, INT (bipolar), target (pitch, shape, cutoff) |
| PLAY | Poly or mono, voices (1-16) |
| Below | Octave, tune, pitch-bend range, glide time, glide mode (auto or always) |

Unlike the monologue there are two independent envelopes, one for the amp and one for the filter
or pitch, because the voices are polyphonic. In mono mode with glide on, overlapping notes slide
without retriggering, as on the monologue.

## Tools

```sh
polylogue-render --list                       # every preset: peak, loudness, tail length
polylogue-render --preset "Glass Bell" --notes 48,55 --hold 2 --seconds 8 --out bell.wav
polylogue-benchmark                           # CPU per scenario (use a Release build)
polylogue-screenshot --preset "Tam Tam" --out editor.png [--spectrum] [--map] [--touch CUTOFF]
```

## Development

- `docs/architecture.md` describes the design, the research behind it, and how each real-time rule
  is checked.
- `scripts/format.sh` formats the code (K&R braces, 4 spaces, 100 columns; needs `clang-format`).
  `scripts/format.sh --check` verifies without changing files.
- The DSP (`src/dsp`) has no JUCE dependency and is tested offline; `src/host` is the JUCE
  processor layer; `src/ui` is the interface. Tests: `tests/dsp` and `tests/host`.
- The audio thread never allocates or locks. The tests prove the first with a counting allocator
  and check the second with ThreadSanitizer (`cmake --preset tsan`).

## Not built yet

The monologue's step sequencer and motion lanes, and tempo-synced LFO. The engine has room for
them, but the panel has no step editor yet. See `docs/architecture.md` section 5.6.

## Licence

[AGPL-3.0](LICENSE). JUCE is AGPLv3 for open source, so this project is too.
IBM Plex fonts (`assets/fonts`) are under the SIL Open Font License; see `assets/fonts/OFL-LICENSE.txt`.
