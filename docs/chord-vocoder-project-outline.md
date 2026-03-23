# Vocoder VST Plugin — Project Outline for Claude Code

## Project Overview

Build a vocoder VST3 plugin using the JUCE framework in C++. The plugin takes a voice signal (main input) and a musical/chord signal (sidechain input) and makes the chords "speak" with the voice's articulation. The user drops this plugin on a vocal track, routes a synth or pad track into the sidechain, and the vocoder does the rest.

Design philosophy: Keep the interface minimal — a handful of knobs, a band visualizer, and that's it. The complexity lives in the DSP, not the UI.

Target: A working VST3 plugin loadable in any major DAW (Ableton, FL Studio, Reaper, Logic, etc.)

---

## Phase 1: Project Setup & Sidechain Bus

1. Initialize a JUCE audio plugin project using CMake (not the Projucer).
2. Configure the project as a **VST3** plugin with the following metadata:
   - Plugin name: "Canary"
   - Manufacturer: (my name or placeholder)
   - Plugin code: "CVoc"
   - Is a synth: No (it's an effect)
   - Accepts MIDI: No (no MIDI needed — carrier comes from sidechain audio)
3. **Critical: Declare a sidechain input bus.** Override `isBusesLayoutSupported()` in the processor to accept:
   - Main input: stereo (the voice / modulator)
   - Sidechain input: stereo (the chord / carrier signal from another track)
   - Main output: stereo

   In the constructor, declare buses like:
   ```cpp
   BusesProperties()
       .withInput("Input", juce::AudioChannelSet::stereo(), true)       // voice
       .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)   // chords
       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
   ```

4. Set up the directory structure:

```
Canary/
├── CMakeLists.txt
├── src/
│   ├── PluginProcessor.h / .cpp    (main audio processor, bus config)
│   ├── PluginEditor.h / .cpp       (GUI)
│   ├── Vocoder.h / .cpp            (core DSP engine)
│   ├── FilterBank.h / .cpp         (bandpass filter bank)
│   └── EnvelopeFollower.h / .cpp   (per-band envelope detection)
└── resources/                       (any GUI assets)
```

5. Verify the project compiles and produces a .vst3 that:
   - Loads in a DAW
   - Shows a sidechain input routing option in the DAW
   - Passes the main input audio through to the output (no processing yet)

**This phase is the most important.** If the sidechain bus doesn't show up in the DAW, nothing else works. Test this thoroughly before moving on.

---

## Phase 2: Core DSP — Filter Bank

Build a bank of bandpass filters for splitting both the modulator and carrier signals into frequency bands.

### Requirements

- **Band count**: 16 bands (expose as a parameter, range 8–32)
- **Frequency range**: 80 Hz to 12,000 Hz
- **Band spacing**: Logarithmic (equal spacing on a log scale — lower bands are narrower in Hz, upper bands are wider, matching human frequency perception)
- **Filter type**: 2nd-order Butterworth bandpass (IIR) using JUCE's `dsp::IIR::Filter` or manual biquad coefficients
- **Two independent filter banks**: One for the modulator (voice) path, one for the carrier (chords) path. Identical center frequencies and bandwidths, separate filter state.

### Implementation Notes

- Calculate center frequencies: `freq[i] = minFreq * pow(maxFreq / minFreq, i / (numBands - 1))`
- Calculate Q values from the bandwidth between adjacent center frequencies
- Filters must be recalculated when sample rate changes or band count changes
- Each band gets its own filter instance
- Process in mono internally (sum stereo to mono before the filter bank, go stereo again at output) — or process L/R independently if CPU allows
- Use `juce::dsp::ProcessorDuplicator` if helpful for stereo processing

---

## Phase 3: Envelope Follower

Extract the amplitude envelope from each modulator (voice) band.

### Requirements

- One envelope follower per modulator band
- **Attack time**: ~1–10 ms (expose as parameter)
- **Release time**: ~10–100 ms (expose as parameter)
- Use a single-pole lowpass filter on the rectified signal:
  ```
  envelope = (1 - coeff) * abs(sample) + coeff * previous_envelope
  ```
  where `coeff = exp(-1 / (time_in_seconds * sampleRate))`

### Implementation Notes

- Use separate coefficients for attack (signal rising) and release (signal falling)
- Output: a smooth positive amplitude value per band per sample
- Optional smoothing to reduce "buzzy" artifacts at fast settings

---

## Phase 4: Vocoder Engine — Wiring It All Together

Connect the sidechain carrier, modulator analysis, and output mixing.

### Signal Flow

```
Main Input (voice/modulator)          Sidechain Input (chords/carrier)
        │                                        │
        ▼                                        ▼
[Modulator Filter Bank]               [Carrier Filter Bank]
        │                                        │
        ▼                                        │
[Envelope Followers]                             │
        │                                        │
        ▼                                        ▼
   band envelopes[0..N]  ────────►  [Multiply per band]
                                           │
                                           ▼
                                    [Sum All Bands]
                                           │
                                           ▼
                                    [Dry/Wet Mix with main input]
                                           │
                                           ▼
                                       Output
```

### Processing (per audio block)

1. Read audio from the **main input buffer** (voice/modulator)
2. Read audio from the **sidechain input buffer** (chords/carrier) — in JUCE, this is typically channels 2–3 (or the second bus) of the input buffer in `processBlock()`
3. Run the voice through the modulator filter bank → N band signals
4. Run each modulator band through its envelope follower → N envelope values
5. Run the chord signal through the carrier filter bank → N carrier band signals
6. Multiply each carrier band by the corresponding modulator envelope
7. Sum all modulated carrier bands together
8. Apply dry/wet mix (blend vocoder output with original voice)
9. Apply output gain
10. Write to output buffer

### Accessing the Sidechain in JUCE

```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto mainInput = getBusBuffer(buffer, true, 0);   // bus 0 = voice
    auto sidechain = getBusBuffer(buffer, true, 1);   // bus 1 = chords

    // If sidechain is not connected, either pass through or silence
    if (sidechain.getNumChannels() == 0)
    {
        // fallback: pass main input through unprocessed, or mute
        return;
    }

    // Process vocoder with mainInput as modulator, sidechain as carrier
}
```

---

## Phase 5: Parameters & GUI

### Parameters (keep it tight — just the essentials)

| Parameter         | Range          | Default | Description                              |
|-------------------|----------------|---------|------------------------------------------|
| Bands             | 8–32           | 16      | Number of vocoder bands                  |
| Attack            | 1–50 ms        | 5 ms    | Envelope follower attack                 |
| Release           | 10–200 ms      | 50 ms   | Envelope follower release                |
| Formant Shift     | -12 to +12 st  | 0       | Shift modulator filter freqs up/down     |
| Dry/Wet           | 0–100%         | 100%    | Blend original voice with vocoder output |
| Output Gain       | -24 to +12 dB  | 0 dB    | Final output level                       |

Use `juce::AudioProcessorValueTreeState` for all parameters.

**Formant Shift** is the one "special" knob — it shifts the center frequencies of the modulator filter bank up or down (in semitones) relative to the carrier bank, so the voice analysis pulls from a different frequency region than the carrier synthesis. This lets users push the vocoder voice up or down in character without changing the chords.

Implementation: multiply each modulator center frequency by `pow(2.0, semitones / 12.0)` before computing the modulator filter coefficients. The carrier filter bank stays fixed.

### GUI Layout

```
┌───────────────────────────────────────────────┐
│  Canary                                 │
├───────────────────────────────────────────────┤
│                                               │
│  ┌───────────────────────────────────────┐    │
│  │  Band Visualizer                      │    │
│  │  (16 vertical bars showing real-time  │    │
│  │   modulator envelope levels)          │    │
│  └───────────────────────────────────────┘    │
│                                               │
│  ┌────────┐ ┌────────┐ ┌────────┐            │
│  │ Bands  │ │ Attack │ │Release │            │
│  │ (knob) │ │ (knob) │ │ (knob) │            │
│  └────────┘ └────────┘ └────────┘            │
│                                               │
│  ┌────────┐ ┌────────┐ ┌────────┐            │
│  │Formant │ │Dry/Wet │ │ Gain   │            │
│  │ (knob) │ │ (knob) │ │ (knob) │            │
│  └────────┘ └────────┘ └────────┘            │
│                                               │
│  [ Sidechain status: Connected / None ]       │
│                                               │
└───────────────────────────────────────────────┘
```

### GUI Requirements

- Plugin window: approximately 450 x 380 pixels
- Band visualizer: vertical bars, color-coded by frequency (low = warm colors, high = cool colors), ~30fps refresh via timer callback
- All 6 knobs attached to APVTS parameters
- Dark background, bright accent colors via custom `LookAndFeel`
- Sidechain status indicator at the bottom — shows whether a sidechain signal is detected (check if sidechain buffer RMS > threshold). This helps the user confirm their routing is correct.

---

## Phase 6: Polish & Edge Cases

### Must-handle scenarios

- **No sidechain connected**: Don't crash. Either pass through the voice dry, or output silence with a visual indicator telling the user to connect a sidechain source.
- **Mono sidechain / mono main**: Handle gracefully — sum to mono for processing if needed.
- **Sample rate changes**: Recalculate all filter coefficients in `prepareToPlay()`.
- **Block size changes**: Ensure all internal buffers resize properly.
- **Denormals**: Flush to zero to prevent CPU spikes on near-silent signals.
- **Parameter changes**: Use `juce::SmoothedValue` on all parameters to prevent clicks/zipper noise when knobs are turned during playback.
- **Band count changes during playback**: This is the trickiest one — changing from 16 to 24 bands means re-creating filters and envelope followers. Consider only applying band count changes when the audio engine resets, or crossfade between configurations.

### Nice-to-haves (if time allows)

- **Noise blend**: Mix a small amount of filtered noise into the carrier to improve consonant intelligibility (the classic vocoder trick for making "s" and "t" sounds come through). Add a "Noise" knob (0–100%) that blends white noise through the upper bands of the carrier.
- **Gate threshold**: A knob that mutes bands where the modulator envelope is below a threshold, reducing background noise bleed.
- **Preset system**: Save/load parameter states using JUCE's built-in XML state mechanism.

---

## Build & Test Notes

- Target **C++17** standard minimum
- Use `juce_recommended_warning_flags` and `juce_recommended_config_flags` in CMake
- Copy the .vst3 output to the system plugin folder for DAW testing:
  - **macOS**: `~/Library/Audio/Plug-Ins/VST3/`
  - **Windows**: `C:\Program Files\Common Files\VST3\`
  - **Linux**: `~/.vst3/`
- **Critical test matrix**:
  - Sidechain connected with a synth pad → verify vocoder effect
  - Sidechain disconnected → verify graceful fallback
  - Different sample rates: 44.1k, 48k, 96k
  - Different buffer sizes: 64, 256, 1024
  - Tweak all knobs during playback → verify no clicks or crashes
  - Automate parameters in the DAW → verify smooth response
- **DAW sidechain routing** (for testing reference):
  - **Ableton**: Drop plugin on vocal track → in plugin UI click sidechain dropdown → select chord track
  - **FL Studio**: Route chord track to plugin's sidechain input via mixer routing
  - **Reaper**: Add plugin to vocal track → route chord track sends to plugin inputs 3–4
  - **Logic**: Use sidechain dropdown in plugin header bar

---

## Key C++ / JUCE Patterns to Follow

- Use `juce::dsp::ProcessSpec` for initializing all DSP objects
- Call `prepareToPlay()` / `releaseResources()` properly on sample rate or block size change
- Use `juce::AudioProcessorValueTreeState` with `ParameterLayout` for all parameters
- **Keep `processBlock()` allocation-free** — no `new`, no `std::vector` resizing, no locks, no file I/O
- Use `juce::SmoothedValue` for parameter smoothing
- Keep GUI and audio threads fully separated — GUI reads parameter values via atomic reads only, never shares mutable state with the audio thread
- Use `getBusBuffer()` to access main and sidechain inputs cleanly
- Declare bus layout support in both the constructor (BusesProperties) and `isBusesLayoutSupported()` — both are required for the DAW to offer sidechain routing
