# Roadmap: ChordVocoder

## Overview

ChordVocoder goes from zero to a working vocoder VST3 in four phases. Phase 1 validates the one thing that can kill the project: sidechain bus routing across DAWs. Phase 2 builds the entire DSP engine (filter bank, envelope followers, vocoder core, parameters). Phase 3 layers on the GUI with band visualizer and sidechain status. Phase 4 adds creative features (stutter, noise blend, gate) and preset persistence.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3, 4): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Plugin Scaffold and Sidechain Validation** - VST3 loads in all target DAWs with sidechain routing visible and passthrough audio working
- [ ] **Phase 2: Vocoder DSP Engine** - Complete vocoder signal chain with filter bank, envelope followers, formant shift, and all core parameters
- [ ] **Phase 3: GUI and Visualization** - Minimal dark UI with 6 knobs, band visualizer, and sidechain status indicator
- [ ] **Phase 4: Creative Features and Polish** - Stutter/randomizer, noise blend, gate threshold, additional knobs, and preset system

## Phase Details

### Phase 1: Plugin Scaffold and Sidechain Validation
**Goal**: Users can load the plugin in Ableton, FL Studio, Reaper, and Logic, see sidechain routing, and pass audio through
**Depends on**: Nothing (first phase)
**Requirements**: INFRA-01, INFRA-02, INFRA-03
**Success Criteria** (what must be TRUE):
  1. Plugin appears in DAW plugin list and loads without error in Ableton, FL Studio, Reaper, and Logic
  2. Each DAW shows a sidechain input routing option when the plugin is inserted on a track
  3. Audio on the main input passes through to the output unmodified (passthrough build)
  4. Plugin does not crash when no sidechain is connected or when sample rate/buffer size changes
**Plans**: 2 plans

Plans:
- [x] 01-01-PLAN.md -- Create plugin scaffold (CMake + JUCE FetchContent, PluginProcessor with sidechain bus, PluginEditor, build and sign)
- [ ] 01-02-PLAN.md -- Verify build artifacts and DAW sidechain validation (human checkpoint)

### Phase 2: Vocoder DSP Engine
**Goal**: Users hear vocoded output when voice enters main input and chords enter sidechain, with controllable parameters
**Depends on**: Phase 1
**Requirements**: FILT-01, FILT-02, FILT-03, FILT-04, ENV-01, ENV-02, ENGINE-01, ENGINE-02, ENGINE-03, ENGINE-04, PARAMS-01, PARAMS-02, PARAMS-03
**Success Criteria** (what must be TRUE):
  1. Voice on main input and chords on sidechain produce recognizable vocoded output with speech articulation audible through the chord timbre
  2. Changing band count (8-32), attack, release, formant shift, dry/wet, and output gain parameters audibly affects the output with no clicks or zipper noise
  3. All parameters are automatable in the DAW and smoothly interpolate during automation playback
  4. Plugin handles absent sidechain gracefully -- passes voice through dry or outputs silence, no crash
  5. No audio dropouts or glitches at 44.1k/48k/96k sample rates and 64/256/1024 buffer sizes
**Plans**: TBD

Plans:
- [ ] 02-01: TBD
- [ ] 02-02: TBD
- [ ] 02-03: TBD

### Phase 3: GUI and Visualization
**Goal**: Users interact with a polished dark interface showing knobs, real-time band activity, and sidechain connection status
**Depends on**: Phase 2
**Requirements**: GUI-01, GUI-02, GUI-03, GUI-04
**Success Criteria** (what must be TRUE):
  1. Plugin opens a 450x380 px dark-themed window with 6 labeled knobs (Bands, Attack, Release, Formant Shift, Dry/Wet, Output Gain)
  2. Turning any knob immediately changes the corresponding parameter (wired through APVTS)
  3. Band visualizer displays real-time modulator envelope levels as vertical bars updating at approximately 30 fps
  4. Sidechain status indicator visually distinguishes between "signal detected" and "no signal" states
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: Creative Features and Polish
**Goal**: Users access stutter/randomizer effects, noise blend for consonant clarity, gate for clean output, and can save/load presets
**Depends on**: Phase 3
**Requirements**: ENGINE-05, ENGINE-06, STTR-01, STTR-02, GUI-05, PRESET-01
**Success Criteria** (what must be TRUE):
  1. Noise blend knob audibly restores consonant intelligibility by mixing filtered white noise into upper carrier bands
  2. Gate threshold knob audibly reduces background noise bleed by muting low-level bands
  3. Stutter rate and depth controls produce rhythmic chopped/glitchy vocoder effects and unpredictable envelope breathing
  4. Four additional knobs (Noise Blend, Gate Threshold, Stutter Rate, Stutter Depth) are visible in the GUI and automatable
  5. User can save plugin state and reload it in a new session with all parameters restored
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 --> 2 --> 3 --> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Plugin Scaffold and Sidechain Validation | 1/2 | In progress | - |
| 2. Vocoder DSP Engine | 0/3 | Not started | - |
| 3. GUI and Visualization | 0/1 | Not started | - |
| 4. Creative Features and Polish | 0/1 | Not started | - |
