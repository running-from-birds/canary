# Requirements: ChordVocoder

**Defined:** 2026-03-20
**Core Value:** The sidechain vocoder effect must work reliably across all major DAWs -- if the sidechain bus doesn't show up, nothing else matters.

## v1 Requirements

### Infrastructure

- [x] **INFRA-01**: Plugin compiles as VST3 and loads successfully in at least Ableton, FL Studio, Reaper, and Logic
- [x] **INFRA-02**: DAW presents a sidechain input routing option when the plugin is inserted
- [x] **INFRA-03**: Plugin passes main input audio through to output before any DSP is added (verifiable passthrough build)

### Filter Bank

- [ ] **FILT-01**: Plugin has a 16-band logarithmically-spaced bandpass filter bank, configurable 8-32 bands
- [ ] **FILT-02**: Separate modulator (voice) and carrier (chords) filter banks with identical center frequencies (80 Hz-12 kHz)
- [ ] **FILT-03**: Filter coefficients recalculate when sample rate or band count changes (in prepareToPlay)
- [ ] **FILT-04**: All DSP buffers pre-allocated to MAX_BANDS (32) at initialization -- no heap allocation in processBlock

### Envelope Follower

- [ ] **ENV-01**: Per-band single-pole envelope follower runs on rectified modulator signal (separate attack/release coefficients)
- [ ] **ENV-02**: Attack configurable 1-50 ms, release configurable 10-200 ms

### Vocoder Engine

- [ ] **ENGINE-01**: Each carrier band is multiplied by the corresponding modulator envelope value and all bands are summed to produce vocoder output
- [ ] **ENGINE-02**: Dry/wet mix blends vocoder output with the original voice (0-100%)
- [ ] **ENGINE-03**: Plugin handles absent sidechain gracefully -- no crash, either passes voice through dry or outputs silence with visual indicator
- [ ] **ENGINE-04**: Formant shift moves modulator filter center frequencies +/-12 semitones relative to the fixed carrier bank
- [ ] **ENGINE-05**: Noise blend mixes white noise into the upper carrier bands (0-100%) to restore consonant intelligibility
- [ ] **ENGINE-06**: Gate threshold mutes any band where the modulator envelope falls below a configurable threshold, reducing background noise bleed

### Stutter / Randomizer

- [ ] **STTR-01**: Random envelope modulation -- an LFO or randomized signal is applied to the per-band envelope values, creating unpredictable vocoder "breathing"
- [ ] **STTR-02**: Band gate stuttering -- bands are randomly muted/unmuted at a configurable rate, producing a chopped, glitchy rhythmic effect

### Parameters

- [ ] **PARAMS-01**: All parameters managed via APVTS: Bands, Attack, Release, Formant Shift, Dry/Wet, Output Gain, Noise Blend, Gate Threshold, Stutter Rate, Stutter Depth
- [ ] **PARAMS-02**: All parameters use SmoothedValue -- no clicks or zipper noise when knobs are moved during playback
- [ ] **PARAMS-03**: All parameters are automatable in the DAW

### GUI

- [ ] **GUI-01**: Plugin window is approximately 450x380 px with dark background and custom LookAndFeel (bright accent colors)
- [ ] **GUI-02**: 6 core knobs (Bands, Attack, Release, Formant Shift, Dry/Wet, Output Gain) wired to APVTS
- [ ] **GUI-03**: Band visualizer shows real-time modulator envelope levels as vertical bars, color-coded low->warm/high->cool, ~30 fps via timer
- [ ] **GUI-04**: Sidechain status indicator shows whether a sidechain signal is detected (RMS > threshold)
- [ ] **GUI-05**: Additional knobs for Noise Blend, Gate Threshold, Stutter Rate, and Stutter Depth

### Preset System

- [ ] **PRESET-01**: User can save and load parameter states using JUCE's built-in XML state mechanism (getStateInformation / setStateInformation)

## v2 Requirements

### Format Support

- **FORMAT-01**: Audio Units (AU) format for Logic Pro native support
- **FORMAT-02**: AAX format for Pro Tools

### Advanced Modulation

- **MOD-01**: Randomize all parameters button for happy accidents / sound design starting points
- **MOD-02**: Per-band control -- individual gain or enable/disable per band

### Distribution

- **DIST-01**: Signed and notarized macOS installer
- **DIST-02**: Windows installer

## Out of Scope

| Feature | Reason |
|---------|--------|
| MIDI input | Carrier is sidechain audio, not MIDI -- different product category |
| Built-in synth engine | Plugin is an effect, not an instrument |
| Real-time chord detection | Intractable DSP problem for v1; user handles routing in DAW |
| Mobile / standalone | VST3 only; no standalone app target |
| Preset marketplace / cloud sync | Out of scope for an initial release |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| INFRA-01 | Phase 1 | Complete |
| INFRA-02 | Phase 1 | Complete |
| INFRA-03 | Phase 1 | Complete |
| FILT-01 | Phase 2 | Pending |
| FILT-02 | Phase 2 | Pending |
| FILT-03 | Phase 2 | Pending |
| FILT-04 | Phase 2 | Pending |
| ENV-01 | Phase 2 | Pending |
| ENV-02 | Phase 2 | Pending |
| ENGINE-01 | Phase 2 | Pending |
| ENGINE-02 | Phase 2 | Pending |
| ENGINE-03 | Phase 2 | Pending |
| ENGINE-04 | Phase 2 | Pending |
| ENGINE-05 | Phase 4 | Pending |
| ENGINE-06 | Phase 4 | Pending |
| STTR-01 | Phase 4 | Pending |
| STTR-02 | Phase 4 | Pending |
| PARAMS-01 | Phase 2 | Pending |
| PARAMS-02 | Phase 2 | Pending |
| PARAMS-03 | Phase 2 | Pending |
| GUI-01 | Phase 3 | Pending |
| GUI-02 | Phase 3 | Pending |
| GUI-03 | Phase 3 | Pending |
| GUI-04 | Phase 3 | Pending |
| GUI-05 | Phase 4 | Pending |
| PRESET-01 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 26 total
- Mapped to phases: 26
- Unmapped: 0

---
*Requirements defined: 2026-03-20*
*Last updated: 2026-03-20 after roadmap creation*
