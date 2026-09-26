# Changelog

## 0.2.0

- **PLAY screen:** eight large knobs (WAVE, METAL, GRIT, BRIGHT, ATTACK, SUSTAIN, EVOLVE, MOTION)
  that describe a sound by how it sounds, with a live spectrum and each knob's value shown on it.
  EDIT is the full panel as before. Switching to EDIT bakes the knobs into the parameters so the
  sound can be fine tuned there. See `docs/play-knobs.md`.
- The knobs are host-automatable and MIDI-learnable, default CC 70 to 77. Each factory preset stores
  where its knobs sit.
- **Animation:** knobs glide to new values, and PLAY and EDIT crossfade.
- **Windows and Linux:** CI and the release workflow now build, test and package VST3 and standalone
  for all three platforms.
- `polylogue-fit` measures how well the eight knobs reproduce the factory presets.
- An existing MIDI map file now gives controllers to parameters added since it was written.

## 0.1.0

First release: a polyphonic monologue-style synthesizer with band-limited oscillators, FM, sync
and ring modulation, a 2-pole filter with drive, two envelopes, an LFO, a stereo chorus, 31 factory
presets, a full panel with MIDI learn on every control, an LCD with a visualizer, a preset browser,
click-to-type values and an on-screen keyboard. VST3, AU and Standalone.
