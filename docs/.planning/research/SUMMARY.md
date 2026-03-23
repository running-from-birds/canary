# Project Research Summary

**Project:** Canary (Canary)
**Domain:** VST3 Vocoder Plugin (Sidechain Audio Effect)
**Researched:** 2026-03-20
**Confidence:** MEDIUM

## Executive Summary

Canary is a sidechain-based vocoder VST3 plugin built with JUCE 8.x and C++17. The vocoder plugin space is well-understood: the voice (modulator) enters on the main input, a chord/synth (carrier) enters via sidechain, and a filter bank with envelope followers cross-modulates them to produce the classic vocoder effect. JUCE is the clear framework choice -- it is the industry standard for cross-platform audio plugins, provides a mature DSP module with IIR filters and SmoothedValue, and handles VST3 wrapping entirely. There are no serious contenders for this use case.

The recommended approach is to build bottom-up along the DSP signal chain: first establish the sidechain bus routing (the single most failure-prone piece), then build the FilterBank and EnvelopeFollower as independent testable units, wire them into a Vocoder engine, and finally layer on parameters and GUI. This order is driven by hard dependencies -- nothing works without sidechain routing, and the GUI is pure presentation over working DSP. The plugin differentiates through a clean "6 knobs + visualizer" UI philosophy, a formant shift control (uncommon in competing vocoders), and a sidechain status indicator that solves the number one user frustration with sidechain effects.

The primary risks are: (1) sidechain bus misconfiguration causing the sidechain to silently not appear in DAWs, (2) real-time audio thread violations (allocation, locking) causing intermittent dropouts that are hard to reproduce, and (3) DAW-specific sidechain routing quirks across Ableton, FL Studio, and Reaper. All three are well-documented with clear prevention patterns. The sidechain bus issue specifically must be validated in target DAWs before any DSP work begins -- if it fails, everything downstream is wasted effort.

## Key Findings

### Recommended Stack

JUCE 8.x with C++17 on CMake 3.24+ using FetchContent for dependency management. This is the standard modern JUCE setup. The VST3 SDK is bundled with JUCE -- do not download it separately. Build with Xcode/Apple Clang on macOS and MSVC (Visual Studio 2022) on Windows. Ship VST3 format only; AU can be added with a single line change later if Logic users request it.

**Core technologies:**
- **JUCE 8.x:** Audio plugin framework -- industry standard, provides DSP module with IIR filters, APVTS parameter system, and complete VST3 wrapping
- **C++17:** Language standard -- JUCE 7+ requires it; gives structured bindings, std::optional, if-constexpr
- **CMake 3.24+ with FetchContent:** Build system -- only officially supported build system for JUCE; FetchContent pins version cleanly without submodule management
- **juce_dsp module:** DSP primitives -- provides IIR::Filter, SmoothedValue, ProcessSpec; this is where the filter bank lives

**Critical note:** Verify the latest JUCE 8.x release tag at https://github.com/juce-framework/JUCE/releases before pinning the FetchContent version. The STACK.md research was based on training data through May 2025.

### Expected Features

**Must have (table stakes):**
- Sidechain input routing with graceful no-signal fallback -- the entire plugin is useless without it
- Filter bank (8-32 bands, log-spaced, 80Hz-12kHz) with configurable band count
- Envelope followers with attack/release controls
- Dry/wet mix and output gain
- Parameter smoothing (SmoothedValue on all knobs) -- no zipper noise
- Crash-free operation across sample rates (44.1k-96k) and buffer sizes (64-2048)
- DAW parameter automation via APVTS

**Should have (differentiators):**
- Formant shift (+-12 semitones) -- the plugin's signature control, uncommon in competitors
- Band visualizer (~30fps) -- real-time visual feedback of envelope levels
- Sidechain status indicator -- "Connected" / "No Signal" display, solves top user frustration
- Noise blend for consonants -- improves speech intelligibility with filtered white noise in upper bands
- Gate threshold -- mutes low-level bands, reduces noise bleed

**Defer (v2+):**
- MIDI input / internal carrier synth -- different product category
- AU/AAX format support -- VST3 covers all target DAWs for now
- Resizable UI / HiDPI -- complex to implement correctly, fixed 450x380px is fine
- Preset marketplace, built-in effects chain, multi-band solo/mute, stereo width

### Architecture Approach

The architecture follows standard JUCE plugin patterns: PluginProcessor owns an APVTS and a Vocoder engine, runs on the audio thread; PluginEditor reads parameters via attachments and visualization data via atomics, runs on the GUI thread. The Vocoder engine orchestrates two FilterBanks (modulator + carrier) and an array of EnvelopeFollowers. All scratch buffers are pre-allocated to MAX_BANDS (32) in prepareToPlay(). Cross-thread communication uses only lock-free atomics -- no mutexes, no allocation on the audio thread, no exceptions.

**Major components:**
1. **PluginProcessor** -- bus declaration, processBlock() dispatch, APVTS ownership, state save/load
2. **Vocoder** -- DSP orchestrator; owns FilterBanks + EnvelopeFollowers, runs the full modulator-carrier-multiply-sum chain
3. **FilterBank** -- manages N bandpass IIR filters with logarithmic frequency spacing
4. **EnvelopeFollower** -- single-pole rectified lowpass per band, attack/release asymmetry
5. **PluginEditor** -- GUI rendering (6 knobs, band visualizer, sidechain status indicator)
6. **APVTS** -- thread-safe parameter storage with atomic reads for audio thread

### Critical Pitfalls

1. **Sidechain bus not appearing in DAW** -- Must declare sidechain in BOTH the constructor BusesProperties AND isBusesLayoutSupported(). Use `false` (not enabled by default) for the sidechain bus, or Ableton will not show the routing option. Test in all target DAWs before writing any DSP.

2. **Memory allocation on the audio thread** -- Any `new`, `delete`, vector resize, string construction, or mutex lock in processBlock() will cause intermittent dropouts under load. Pre-allocate everything for MAX_BANDS in prepareToPlay().

3. **Denormal CPU spikes** -- IIR filters produce denormal floats on silent input, causing 10-100x CPU slowdown. Use `juce::ScopedNoDenormals` as the first line of every processBlock(). Add tiny DC offset in envelope followers.

4. **Band count change during playback** -- Pre-allocate arrays to MAX_BANDS (32), use an atomic "active bands" counter, recalculate coefficients at block boundaries. Never resize containers on the audio thread.

5. **DAW-specific sidechain quirks** -- Ableton requires `false` default, FL Studio routes sidechain differently, Logic prefers AU. Test sidechain routing in each target DAW during Phase 1 validation.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Project Setup and Sidechain Bus Validation
**Rationale:** Sidechain routing is the single point of failure that makes the entire plugin non-functional. Every researcher flagged this as the number one risk. Must be validated in target DAWs before any DSP work begins.
**Delivers:** A VST3 that loads in Ableton, FL Studio, and Reaper with a visible sidechain routing option. Passthrough audio when no sidechain connected. CMake build system with FetchContent for JUCE.
**Addresses:** Sidechain input routing, no-sidechain graceful fallback, crash-free loading
**Avoids:** Pitfall 1 (invisible sidechain), Pitfall 5 (empty buffer crash), Pitfall 9 (DAW incompatibilities), Pitfall 14 (unique ID collision)

### Phase 2: Filter Bank
**Rationale:** Independent DSP component with no dependency on the envelope follower. Testable in isolation. Must establish the pre-allocation pattern (MAX_BANDS = 32) that all subsequent DSP depends on.
**Delivers:** A FilterBank class that splits a mono signal into N logarithmically-spaced bandpass-filtered outputs. Configurable band count (8-32).
**Uses:** juce_dsp module (IIR::Filter), C++17
**Implements:** FilterBank component from architecture
**Avoids:** Pitfall 2 (allocation on audio thread), Pitfall 3 (denormals), Pitfall 7 (band count change crash)

### Phase 3: Envelope Follower
**Rationale:** Independent DSP component. Once the filter bank is splitting bands, the envelope follower extracts amplitude envelopes from modulator bands. Simple single-pole filter with attack/release asymmetry.
**Delivers:** An EnvelopeFollower class that tracks per-band amplitude with configurable attack/release times.
**Avoids:** Pitfall 3 (denormal accumulation in feedback loop -- use DC offset)

### Phase 4: Vocoder Engine Integration
**Rationale:** Wires FilterBank + EnvelopeFollower into the complete DSP chain. Connects to PluginProcessor via the sidechain access pattern established in Phase 1.
**Delivers:** Working vocoder effect: voice in main input, chords in sidechain, vocoded output. Dry/wet mix and output gain. The plugin is now functionally complete (albeit with hardcoded default parameters and no GUI).
**Addresses:** Core DSP features (filter bank, envelope followers, vocoder multiply+sum, dry/wet, output gain)
**Avoids:** Pitfall 5 (sidechain buffer crash -- defensive check), Pitfall 2 (in-place processing awareness)

### Phase 5: Parameters and GUI
**Rationale:** Parameters are meaningless until the DSP exists to consume them. GUI is pure presentation over working DSP. APVTS setup, SmoothedValue wrapping, and the PluginEditor with 6 knobs + band visualizer + sidechain status indicator.
**Delivers:** Full UI with controls for bands, attack, release, formant shift, dry/wet, output gain. Band visualizer showing real-time envelope levels. Sidechain connection status indicator. All parameters automatable.
**Addresses:** Attack/release controls, band count control, formant shift (differentiator), band visualizer (differentiator), sidechain status indicator (differentiator), parameter smoothing, DAW automation
**Avoids:** Pitfall 4 (APVTS thread safety -- cache getRawParameterValue pointers), Pitfall 6 (SmoothedValue reset sweep), Pitfall 8 (filter coefficient clicks from formant shift), Pitfall 10 (GUI timer blocking)

### Phase 6: Polish and Edge Cases
**Rationale:** Production hardening. State save/restore, noise blend for consonants, gate threshold, DAW compatibility testing, mono input handling, preset system.
**Delivers:** Production-ready plugin with state persistence, improved intelligibility (noise blend), cleaner output (gate threshold), tested across DAWs.
**Addresses:** Noise blend (differentiator), gate threshold (differentiator), preset system, state save/restore
**Avoids:** Pitfall 12 (state save/restore failure), Pitfall 13 (mono/stereo mismatch), Pitfall 9 (DAW compatibility -- final verification pass)

### Phase Ordering Rationale

- **Phase 1 blocks everything:** If sidechain routing does not work in target DAWs, the entire project is dead. Validate before investing in DSP.
- **Phases 2-3 are independent leaf nodes:** FilterBank and EnvelopeFollower have no dependency on each other, but Phase 4 needs both. They could theoretically be built in parallel.
- **Phase 4 integrates the DSP chain:** The Vocoder engine is the first point where sidechain audio actually gets vocoded. This is the "it works" milestone.
- **Phase 5 after DSP:** Parameters and GUI are presentation over a working engine. The plugin should produce correct vocoded output with hardcoded defaults before any GUI exists.
- **Phase 6 is polish:** State persistence, edge cases, and production hardening come last because they are not blocking for a working demo.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** Sidechain bus validation across DAWs -- needs hands-on testing, not just code patterns. DAW behavior may vary by version.
- **Phase 5:** Formant shift implementation needs careful coefficient interpolation to avoid clicks. The SmoothedValue + per-block coefficient recalculation pattern should be prototyped and validated.

Phases with standard patterns (skip research-phase):
- **Phase 2:** FilterBank is a textbook IIR bandpass filter bank. JUCE's dsp::IIR::Filter handles the heavy lifting. Well-documented.
- **Phase 3:** EnvelopeFollower is a single-pole lowpass with rectification. Trivial DSP, no research needed.
- **Phase 4:** Vocoder multiply-and-sum is straightforward once FilterBank and EnvelopeFollower exist.
- **Phase 6:** State save/restore via APVTS is well-documented boilerplate.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | MEDIUM | JUCE 8.x is correct but exact version needs live verification. CMake pattern is HIGH confidence. |
| Features | MEDIUM | Feature list is solid based on vocoder market knowledge. Competitive positioning claims need validation against current product listings. |
| Architecture | MEDIUM-HIGH | Standard JUCE plugin patterns, well-documented. Sidechain bus pattern confirmed by project outline code. Minor uncertainty on `AudioParameterFloatAttributes::withLabel()` API availability. |
| Pitfalls | HIGH | All pitfalls are well-established, long-standing JUCE/VST3 issues. These have been stable for years. |

**Overall confidence:** MEDIUM

The core patterns (JUCE plugin architecture, sidechain bus declaration, real-time audio constraints, APVTS parameter system) are all HIGH confidence -- these are stable, well-documented conventions. The MEDIUM overall rating reflects that web verification was unavailable, so exact JUCE version numbers and DAW-specific behavior should be verified against live sources before development begins.

### Gaps to Address

- **JUCE version verification:** Confirm latest JUCE 8.x release tag before pinning FetchContent. Check for any breaking changes in recent releases.
- **DAW sidechain behavior:** The `false` default for sidechain bus enablement is critical for Ableton but needs verification against current Ableton version. FL Studio's sidechain routing should be tested early.
- **AudioParameterFloatAttributes API:** Verify that `.withLabel()` exists in the pinned JUCE version. If not, labels can be omitted without functional impact.
- **macOS code signing:** Ventura+ may require ad-hoc signing for dev testing. Verify the `codesign --force --deep --sign -` approach works with current macOS.
- **JUCE licensing:** Determine if GPLv3 or commercial license is appropriate for this project before distributing.

## Sources

### Primary (HIGH confidence)
- JUCE framework API documentation (AudioProcessor, APVTS, dsp::IIR::Filter, SmoothedValue)
- JUCE CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- Project outline document: chord-vocoder-project-outline.md
- PROJECT.md requirements

### Secondary (MEDIUM confidence)
- JUCE community forum patterns (sidechain bus, denormals, real-time safety)
- Vocoder plugin market knowledge (TAL-Vocoder, Arturia Vocoder V, Waves Morphoder, iZotope VocalSynth 2)
- Real-time audio programming best practices (Ross Bencina)

### Tertiary (LOW confidence)
- Exact JUCE 8.x version availability -- needs live verification at GitHub
- DAW-specific sidechain routing details -- may vary by DAW version
- Competitive product current pricing and features -- based on training data, may be outdated

---
*Research completed: 2026-03-20*
*Ready for roadmap: yes*
