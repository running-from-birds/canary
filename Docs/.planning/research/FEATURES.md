# Feature Landscape

**Domain:** Vocoder VST3 plugin (sidechain-based audio effect)
**Researched:** 2026-03-20
**Confidence:** MEDIUM (based on training data knowledge of vocoder plugin market; web verification unavailable)

## Table Stakes

Features users expect. Missing any of these and the plugin feels broken or unusable.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Sidechain input routing | The entire plugin is useless without it. Every vocoder effect plugin exposes a sidechain bus for the carrier signal. | High | Must work across Ableton, FL Studio, Reaper, Logic. Declare in both BusesProperties and isBusesLayoutSupported(). This is the #1 make-or-break feature. |
| Band count control (8-32) | Users expect to trade intelligibility vs. smoothness. Every serious vocoder (TAL-Vocoder, Arturia Vocoder V) exposes this. 16 default is standard. | Medium | Logarithmic spacing mandatory. Band count changes should only apply on engine reset to avoid mid-playback crashes. |
| Attack / Release controls | Envelope follower speed is fundamental to vocoder character. Fast = robotic/crispy, slow = smooth/paddy. | Low | Two knobs, straightforward single-pole filter. Range: attack 1-50ms, release 10-200ms. |
| Dry/Wet mix | Standard on every audio effect plugin. Users need to blend vocoder output with dry voice for parallel processing. | Low | Simple linear crossfade. 0-100%, default 100% wet. |
| Output gain | Users need level matching. Vocoder processing often changes perceived volume significantly. | Low | Range -24 to +12 dB is standard. |
| No-sidechain graceful fallback | If the sidechain is not connected, the plugin must not crash, must not produce garbage audio. Pass-through or silence with visual indicator. | Medium | Critical for first-load experience. User will load plugin before routing sidechain -- this must not be scary. |
| Parameter smoothing (no zipper noise) | Turning knobs during playback must not click or pop. Users will automate parameters in the DAW. Every modern plugin handles this. | Medium | SmoothedValue on all parameters. Non-negotiable for production use. |
| Crash-free across sample rates and buffer sizes | Users run 44.1k, 48k, 96k, and buffer sizes from 64 to 2048. Plugin must handle all combinations. | Medium | Recalculate coefficients in prepareToPlay(). Resize internal buffers. Flush denormals. |
| DAW parameter automation | All parameters must be automatable via the DAW's automation lanes. This is expected of any VST3 plugin. | Low | APVTS handles this automatically when set up correctly. |

## Differentiators

Features that set Canary apart. Not expected by default, but valued when present.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Formant shift (+-12 semitones) | Shifts the modulator analysis frequencies relative to carrier synthesis. Lets users change vocal character without changing the chords. Few competing vocoders expose this as a simple knob -- most require manual band editing. | Medium | Multiply modulator center frequencies by pow(2.0, semitones/12.0). Carrier bank stays fixed. This is the plugin's "signature" control. |
| Band visualizer | Real-time display of modulator envelope levels across bands. Provides immediate visual feedback that the vocoder is working. Most budget vocoders lack this. Arturia Vocoder V has it; TAL-Vocoder does not. | Medium | ~30fps timer callback. Color-coded bars (warm=low, cool=high). GPU-efficient -- use JUCE's repaint() with dirty-rect optimization. Read envelope data via atomic floats from audio thread. |
| Sidechain status indicator | Shows whether a sidechain signal is actually detected (RMS check). Solves the #1 user frustration with sidechain vocoders: "Is my routing correct?" Most vocoders leave the user guessing. | Low | Simple RMS threshold check on sidechain buffer. Display "Connected" / "No Signal" in the UI. Enormous UX win for minimal effort. |
| Noise blend for consonants | Mixes filtered white noise into upper carrier bands to improve "s", "t", "k" intelligibility. Classic vocoder trick that separates musical vocoders from toy ones. Arturia and Waves Morphoder have this. | Medium | White noise generator, filtered through upper bands, blended based on a knob (0-100%). Dramatically improves speech clarity. |
| Gate threshold | Mutes bands where modulator envelope is below threshold. Reduces background noise bleed and tightens the vocoder response. Professional feature found in hardware vocoders. | Low | Per-band comparison against threshold value. Single knob. Clean improvement to output quality. |
| Minimal dark UI | Intentionally simple (6 knobs + visualizer + status). Stands out in a market where many vocoders (VocalSynth 2, Morphoder) overwhelm with options. The "it just works" aesthetic appeals to producers who want to drop and go. | Medium | Custom LookAndFeel. 450x380px. Restraint is the feature. |

## Anti-Features

Features to explicitly NOT build in v1. Each has a clear reason.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| MIDI input / internal carrier synth | Massively increases scope. The plugin is an effect, not an instrument. MIDI-driven vocoders (like VocalSynth) are a different product category. Adding a synth engine doubles the codebase and testing surface. | Carrier comes from sidechain audio. User routes any synth they want into the sidechain -- this is more flexible than a built-in synth anyway. |
| Real-time chord detection from audio | Extremely complex DSP problem (polyphonic pitch detection). Unreliable results. Out of scope for a vocoder effect. | User handles chord routing in the DAW. The plugin processes whatever audio arrives on the sidechain. |
| Preset marketplace / cloud presets | Infrastructure overhead (servers, auth, payments). Zero users on day one. Premature. | Ship with 3-5 factory presets via JUCE XML state. Users can save/load via DAW preset management. |
| Built-in effects chain (reverb, delay, EQ) | Scope creep. Every DAW already has these effects. Adding them inside the vocoder creates a maintenance burden and muddies the plugin's purpose. | Users chain their own effects after the vocoder in the DAW. |
| AU / AAX format support | Additional build targets, code signing requirements (AAX requires iLok/PACE), and DAW-specific testing. VST3 covers all target DAWs. | Ship VST3 only. Add AU as a fast-follow if there's demand (Logic users may request it, though Logic supports VST3). |
| Resizable UI / HiDPI scaling | Complex to implement correctly in JUCE. Many commercial plugins still don't support this well. Adds testing matrix explosion. | Fixed 450x380px window. Revisit in v2 if users request it. |
| Undo/redo system | Uncommon in effects plugins. DAW handles parameter undo via automation. Over-engineering for a 6-knob plugin. | Rely on DAW's native undo. |
| Multi-band solo/mute | Found in advanced vocoders (Arturia Vocoder V). Useful for sound design but adds significant UI complexity and breaks the "minimal" philosophy. | Omit entirely. If users want band-level control, they're looking for a different product. |
| Stereo width / mid-side processing | Adds complexity to the DSP path. Most vocoder use cases are mono-summed internally anyway. | Process in mono internally, output stereo. Stereo imaging can be done with downstream plugins. |

## Feature Dependencies

```
Sidechain Input Routing
    └──> Filter Bank (modulator + carrier)
            └──> Envelope Followers (modulator path only)
                    └──> Vocoder Engine (multiply + sum)
                            └──> Dry/Wet Mix
                                    └──> Output Gain
                                            └──> Output

Band Visualizer ──reads from──> Envelope Followers (atomic float array)

Sidechain Status Indicator ──reads from──> Sidechain Input (RMS check)

Formant Shift ──modifies──> Modulator Filter Bank center frequencies

Noise Blend ──injects into──> Carrier path (upper bands)

Gate Threshold ──gates──> Envelope Followers output

Preset System ──serializes──> All APVTS parameters
```

Key ordering constraint: The DSP chain is strictly sequential (sidechain -> filters -> envelopes -> multiply -> sum -> mix -> gain). The UI features (visualizer, status indicator) read from the DSP state but don't block it.

## MVP Recommendation

**Prioritize (ship-blocking):**

1. Sidechain input routing with graceful no-signal fallback -- without this, nothing works
2. Filter bank (16 bands, log-spaced, 80Hz-12kHz) -- core DSP
3. Envelope followers with attack/release -- core DSP
4. Vocoder engine (multiply + sum + dry/wet + gain) -- core DSP
5. APVTS parameters with SmoothedValue -- production-quality knob control
6. Minimal UI with 6 knobs -- usable interface

**Include if time allows (high-value, low-to-medium effort):**

7. Sidechain status indicator -- enormous UX win, trivial to implement
8. Band visualizer -- visual feedback that the effect is working
9. Formant shift -- the plugin's signature differentiator

**Defer to post-MVP (nice-to-have):**

10. Noise blend for consonants -- improves quality but not essential for first release
11. Gate threshold -- polish feature
12. Preset system -- JUCE XML state, straightforward but not ship-blocking

**Explicitly cut:**

- MIDI input, built-in synth, chord detection, AU/AAX, resizable UI, multi-band solo/mute, stereo width, preset marketplace

## Competitive Landscape Context

The vocoder plugin market has a clear hierarchy:

- **Premium tier** ($100+): Arturia Vocoder V, Waves Morphoder, iZotope VocalSynth 2. Feature-rich, complex UIs, built-in synths.
- **Mid tier** ($30-80): Softube Vocoder, MeldaProduction MVocoder. Solid DSP, moderate feature sets.
- **Budget/free tier**: TAL-Vocoder (free), mda Vocoder. Minimal features, sometimes dated UIs.

Canary's positioning: A clean, modern, sidechain-focused vocoder that sits between the free tier (better DSP and UX) and the premium tier (simpler, more focused). The formant shift and band visualizer differentiate from free options. The minimal UI and sidechain-only approach differentiate from bloated premium options.

## Sources

- Training data knowledge of JUCE framework and VST3 plugin development (MEDIUM confidence)
- Training data knowledge of vocoder plugin market (TAL-Vocoder, Arturia Vocoder V, Waves Morphoder, iZotope VocalSynth 2, Softube Vocoder) (MEDIUM confidence)
- Project outline document: chord-vocoder-project-outline.md (HIGH confidence -- primary source)
- PROJECT.md requirements (HIGH confidence -- primary source)

Note: Web search was unavailable during research. Market positioning claims are based on training data and should be validated against current product listings if precise competitive analysis is needed.
