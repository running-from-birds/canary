# Canary

## What This Is

A VST3 vocoder plugin built with JUCE/C++ that takes a voice signal (main input) and a chord/synth signal (sidechain input) and makes the chords "speak" with the voice's articulation. The user drops it on a vocal track, routes a synth or pad into the sidechain, and the vocoder does the rest. Interface is intentionally minimal — complexity lives in the DSP, not the UI.

## Core Value

The sidechain vocoder effect must work reliably across all major DAWs (Ableton, FL Studio, Reaper, Logic) — if the sidechain bus doesn't show up, nothing else matters.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] VST3 plugin loads in major DAWs with a sidechain input bus visible
- [ ] 16-band (8–32 configurable) logarithmically-spaced bandpass filter bank for modulator and carrier
- [ ] Per-band envelope followers on the modulator (voice) path with configurable attack/release
- [ ] Vocoder engine: multiply carrier bands by modulator envelopes, sum, mix with dry signal
- [ ] Formant shift parameter (±12 semitones) shifts modulator filter frequencies relative to carrier
- [ ] Full parameter set (Bands, Attack, Release, Formant Shift, Dry/Wet, Output Gain) via APVTS
- [ ] Band visualizer showing real-time modulator envelope levels (~30fps)
- [ ] Minimal dark UI (450×380px) with 6 knobs and sidechain status indicator
- [ ] Graceful handling of no-sidechain, mono signals, sample rate/buffer size changes
- [ ] Noise blend and gate threshold (nice-to-have, Phase 6)
- [ ] Preset system via JUCE XML state (nice-to-have, Phase 6)

### Out of Scope

- MIDI input — carrier comes from sidechain audio, not MIDI
- Synth engine — plugin is an effect, not an instrument
- Mobile/standalone — VST3 only for now
- Real-time chord detection/tracking from MIDI — user handles routing in DAW

## Context

- Framework: JUCE (CMake-based, not Projucer)
- Language: C++17
- Plugin format: VST3 only
- Plugin code: "CVoc", name: "Canary"
- Three buses: Main Input (stereo, voice), Sidechain Input (stereo, chords), Main Output (stereo)
- DSP pattern: processBlock() must be allocation-free; SmoothedValue for all parameters; getBusBuffer() for sidechain access
- Test matrix: 44.1k/48k/96k sample rates, 64/256/1024 buffer sizes, all DAWs listed above
- Install path for testing: ~/Library/Audio/Plug-Ins/VST3/ (macOS)

## Constraints

- **Tech Stack**: JUCE + CMake + C++17 — no Projucer, no other audio frameworks
- **Real-time Safety**: processBlock() must be allocation-free, lock-free, no file I/O
- **Thread Safety**: GUI and audio threads fully separated — GUI reads via atomic reads only
- **Sidechain**: Must declare bus layout in both constructor (BusesProperties) AND isBusesLayoutSupported() — both required for DAW sidechain routing to appear

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| CMake over Projucer | Projucer is legacy, CMake is standard for modern JUCE projects | — Pending |
| VST3 only (no AU/AAX) | Simplest target covering all major DAWs; AU/AAX can follow | — Pending |
| 16 bands default, 8–32 range | Matches industry-standard vocoder UX; configurable for quality/CPU tradeoff | — Pending |
| Band count changes only on engine reset | Avoids mid-playback filter bank teardown complexity | — Pending |
| Minimal UI philosophy | Complexity in DSP not UI; dark theme with band visualizer only | — Pending |

---
*Last updated: 2026-03-20 after initialization*
