# The monologue: research notes

What is publicly documented about the Korg monologue's synthesis architecture, how each item is
treated in Polylogue, and what cannot be known without proprietary details.

Polylogue reproduces the *architecture, workflow and character* of the monologue, made polyphonic.
It does not copy Korg source code, firmware, artwork or presets, none of which were consulted.
For how the findings map onto the implementation, see [Architecture](architecture.md).

## 1. Sources

- Korg, *monologue Owner's Manual* (E2, published 6/2019): block diagram (p.3), VCO/MIXER/FILTER/EG/LFO
  parameter pages (p.15–21), sequencer (p.22–28), edit-mode parameters (p.30–36), MIDI
  implementation chart (p.58).
- Korg *monologue MIDI Implementation* chart, as summarised by
  [midi.guide](https://midi.guide/d/korg/monologue/) (CC assignments).
- Sound On Sound, [*Korg Monologue* review](https://www.soundonsound.com/reviews/korg-monologue).
- Synth Anatomy, [*KORG Monologue review*, p.2](https://synthanatomy.com/2017/10/korg-monologue-analog-monophonic-synthesizer-review.html/2).

No Korg source, firmware, artwork or presets were consulted or used.

## 2. Confidence legend

| Tag | Meaning |
| --- | --- |
| **DOC** | Stated in the manual or MIDI chart. Reproduced faithfully. |
| **REV** | Stated by a reviewer who measured or listened. Reproduced, but softer. |
| **APPROX** | Not documented. Our own reasonable DSP approximation. |
| **UNKNOWABLE** | Depends on the proprietary analog design or firmware. We pick a musical default. |
| **DEVIATION** | Deliberately different from the monologue because Polylogue is polyphonic. |

## 3. Findings, item by item

**1. Oscillator architecture.** Two VCOs. VCO 1 pitch is locked to the main five-way keyboard OCTAVE
switch (±2 octaves); VCO 2 has its own octave and cent-level pitch. VCO 2 can hard-sync to VCO 1 or
ring-modulate with it (one three-way switch, centre = off). *DOC*.

**2. Waveforms.** VCO 1: saw, triangle, square. VCO 2: saw, triangle, noise. Each oscillator has a SHAPE
knob (0…1023) that "determines the final shape, complexity, or duty-cycle (Square)". Noise has no
shape. *DOC*. Reviewers confirm that on square, SHAPE is pulse-width and at maximum the pulse
practically vanishes *REV*. The exact SHAPE curves for saw and triangle are not published
*UNKNOWABLE*; see §5.1 for ours *APPROX*.

**3. Tuning / range.** VCO 2 OCTAVE: 16', 8', 4', 2'. VCO 2 PITCH: −1200…+1200 cents in 1-cent steps
(SHIFT = semitone steps). Program tuning ±50 cents. Pitch-bend range 1–12 notes per side. *DOC*.

**4. Mixer.** Two level knobs (VCO 1, VCO 2, 0…1023). Noise is VCO 2's third waveform, so noise level
is VCO 2's level. Block diagram shows ring-mod output taking the place of VCO 2 in the mix. *DOC*.

**5. Filter topology and slope.** "Updated" 2-pole (12 dB/oct) low-pass with resonance, "more bite" than the
minilogue, self-oscillates, bass is preserved at high resonance. Cutoff key tracking is 0/50/100 %
(edit mode). *DOC/REV*. The actual circuit is not published *UNKNOWABLE*.

**6. Filter drive.** A DRIVE knob adds "harmonics and distortion"; reviewers describe a rougher, darker
character up to an "overdriven roar" that does not excessively raise level. *DOC/REV*. In the block
diagram DRIVE sits at the output stage, after the VCA *(read from the diagram)*. Circuit
*UNKNOWABLE*.

**7. Filter envelope.** There is one EG. The TARGET switch sends it to PITCH (both VCOs), PITCH 2 (VCO 2
only) or CUTOFF. INT is bipolar (−511…+511). *DOC*.

**8. Amp envelope.** The same EG also drives the VCA. The TYPE switch selects: **A/D** (attack then decay to
0, ignores note-off; VCA percussive), **A/G/D** (attack, hold while gate is on, decay starts at
note-off; VCA sustains while held) or **GATE** (VCA is a plain gate; the target still gets an A/D
shape). ATTACK and DECAY only; there is no sustain level or separate release. *DOC*. Time
constants are not published *UNKNOWABLE*.

**9. LFO.** One LFO. Waves: saw, triangle, square. Modes: FAST (0.5 Hz – 2.8 kHz), SLOW (0.05 – 28 Hz),
1-SHOT (0.05 – 28 Hz; stops one half-cycle after the note starts, acting as a second envelope).
Tempo sync optional. INT can be negative (inverted saw becomes a ramp). No S&H. Audio-rate FAST mode
is used for FM-like and formant-like timbres. *DOC/REV*.

**10. Modulation routing.** Fixed and small. EG → {pitch, pitch 2, cutoff}. LFO → {pitch (both VCOs), shape
(both VCOs), cutoff}. Velocity → cutoff amount and amp amount (edit mode). Key track → cutoff.
No VCO cross-mod, no mod wheel. *DOC/REV*.

**11. Glide.** Portamento time (Off, 0…127) with mode **Auto** (only when playing legato) or **On**
(always). A separate **Slide** effect is set per sequencer step and uses Slide Time (0–100 %).
*DOC*. Curve shape is not published *UNKNOWABLE*.

**12. Sequencer / motion.** 16 steps; per step: note, gate time, slide on/off, active on/off, motion on/off.
Step length 1–16, step resolution 1/16…1/1, swing ±75 %, default gate time, tempo 10–600 BPM
(knob 56–240). Up to **4 motion lanes**, each recording one knob/switch; parameter locks write one
step. Motion data is stored per program. Key Trigger mode starts and transposes the sequence from
the keyboard, with a hold variant. Sync in/out and MIDI clock. *DOC*.

**13. Key modes.** The monologue is monophonic with last-note priority. It is **multi-trigger** normally
(envelope restarts on every note) and becomes **single-trigger** when portamento is engaged and
notes overlap. *DOC/REV*.

**14. MIDI.** Notes (velocity 1–127), pitch bend, program change 0–99, CCs for panel parameters (16 EG
attack, 17 EG decay, 24 LFO rate, 25 EG int, 26 LFO int, 28 drive, 35 VCO2 pitch, 36/37 shape,
39/40 level, 43 cutoff, 44 resonance, 49 VCO2 octave, 50/51 wave, 56/58/59 LFO
target/wave/mode, 60 sync/ring, 61/62 EG type/target), all-sound-off (120), reset-all-controllers
(121), all-notes-off (123–127), clock and transport. Note-off velocity ignored. *DOC*. The
value-to-position split for switch CCs is not in the chart *UNKNOWABLE*.

**15. Distinctive behaviour that shapes the sound.**

- The EG restarts *from zero* on a new note during release, so a slow attack after a fast note
  produces a gap. *REV*. We restart from zero too, but after a ≈2 ms declick fade (see [Architecture](architecture.md#stealing-and-retrigger-without-clicks)).
- Drive is level-compensated: it darkens and thickens rather than getting louder. *REV*.
- A/G/D vs A/D vs GATE gives three distinct VCA behaviours from two knobs. *DOC*.
- LFO FAST reaches audio rate on cutoff/shape/pitch, giving FM-like tones. *REV*.
- Pulse SHAPE is PWM, vanishing at 100 %. *REV*.
- Resonance keeps low end and the filter self-oscillates for tuned percussion. *REV*.
- 10-bit control resolution. Smooth sweeps; we use float and smooth every continuous parameter. *REV*.
- Sequencer slides are a portamento *between steps*, independent from the global portamento. *DOC*.

## 4. What "polyphonic" changes

| monologue | Polylogue | Tag |
| --- | --- | --- |
| One voice | 1–16 voices (default 8), preallocated | DEVIATION |
| One EG shared by VCA and target | Two independent envelopes: **amp** and **mod** (target/filter) | DEVIATION |
| Analog filter/oscillator drift | None. Voices are numerically identical | DEVIATION |
| Mono out | Stereo out, identical channels (voice pan spread is a later option) | DEVIATION |
| Single LFO | One LFO instance *per voice*, phase reset at note-on | DEVIATION |
| Drive after mono VCA | Drive per voice, after the VCA | DEVIATION |
| Fixed hardware CC map | Host automation of every parameter. CC map is a later option | DEVIATION |
