# Architecture Patterns

**Domain:** VST3 vocoder plugin (JUCE / C++17)
**Researched:** 2026-03-20

## Recommended Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        DAW Host Process                              │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │                    PluginProcessor                             │  │
│  │  (AudioProcessor subclass — lives on audio thread)            │  │
│  │                                                                │  │
│  │  ┌──────────────┐     ┌──────────────────────────────────┐    │  │
│  │  │    APVTS      │     │         Vocoder Engine           │    │  │
│  │  │ (parameters)  │────▶│                                  │    │  │
│  │  │               │     │  ┌────────────┐ ┌────────────┐  │    │  │
│  │  └──────┬────────┘     │  │ Modulator  │ │  Carrier   │  │    │  │
│  │         │              │  │ FilterBank │ │ FilterBank │  │    │  │
│  │         │ atomic       │  └─────┬──────┘ └─────┬──────┘  │    │  │
│  │         │ reads        │        │              │          │    │  │
│  │  ┌──────▼────────┐     │  ┌─────▼──────┐      │          │    │  │
│  │  │ PluginEditor  │     │  │  Envelope   │      │          │    │  │
│  │  │ (GUI thread)  │     │  │  Followers  │      │          │    │  │
│  │  │               │     │  └─────┬──────┘      │          │    │  │
│  │  │  ┌──────────┐ │     │        │   multiply  │          │    │  │
│  │  │  │Visualizer│◀┤     │        └──────┬──────┘          │    │  │
│  │  │  └──────────┘ │     │               ▼                 │    │  │
│  │  │  ┌──────────┐ │     │        [Sum + Mix + Gain]       │    │  │
│  │  │  │  Knobs   │ │     │               │                 │    │  │
│  │  │  └──────────┘ │     └───────────────┼─────────────────┘    │  │
│  │  └───────────────┘                     ▼                      │  │
│  │                                   Output Buffer               │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

### Component Boundaries

| Component | Responsibility | Communicates With | Thread |
|-----------|---------------|-------------------|--------|
| **PluginProcessor** | Bus declaration, `processBlock()` dispatch, state save/load, APVTS ownership | Vocoder, APVTS, PluginEditor (creates it) | Audio thread (processBlock), Message thread (state) |
| **PluginEditor** | GUI rendering, knob layout, visualizer, sidechain status | APVTS (via attachments), Processor (read-only atomic data) | GUI / Message thread |
| **Vocoder** | Owns FilterBanks + EnvelopeFollowers, orchestrates the full DSP chain per block | FilterBank (x2), EnvelopeFollower (xN), reads SmoothedValues | Audio thread only |
| **FilterBank** | Manages N bandpass filters, computes center frequencies logarithmically, splits signal into bands | Individual IIR filters (juce::dsp::IIR::Filter) | Audio thread only |
| **EnvelopeFollower** | Single-pole rectified lowpass per band, attack/release asymmetry | None (stateless apart from its own envelope value) | Audio thread only |
| **APVTS** | Parameter storage, undo/redo, XML state serialization | PluginProcessor (owns it), PluginEditor (reads via attachments) | Thread-safe by design |

### Data Flow

#### Audio Signal Path (per processBlock call)

```
1. DAW calls processBlock(buffer, midi)
2. PluginProcessor extracts buses:
     mainInput  = getBusBuffer(buffer, true, 0)   // voice (modulator)
     sidechain  = getBusBuffer(buffer, true, 1)   // chords (carrier)
     output     = getBusBuffer(buffer, false, 0)   // output

3. Guard: if sidechain.getNumChannels() == 0 → passthrough or silence

4. Vocoder.process(mainInput, sidechain, output, numSamples):
   a. Sum stereo → mono working buffers (modMono, carrierMono)
   b. For each band i (0..N-1):
      - modulatorFilterBank[i].process(modMono)      → modBand[i]
      - carrierFilterBank[i].process(carrierMono)     → carrBand[i]
      - envelopeFollower[i].process(modBand[i])       → envLevel[i]
      - carrBand[i] *= envLevel[i]                    (sample-by-sample)
      - accumulate into outputMono
   c. Apply dry/wet mix: output = wet * vocoded + dry * mainInput
   d. Apply output gain (SmoothedValue)
   e. Copy mono → stereo output (or process L/R independently)

5. Store envelope levels in atomic array for GUI visualizer
```

#### Parameter Flow

```
APVTS (thread-safe)
  │
  ├──▶ PluginEditor: SliderAttachment reads/writes parameter values
  │    (GUI thread — JUCE handles thread safety internally)
  │
  └──▶ PluginProcessor.processBlock():
       reads raw parameter values via getRawParameterValue() (returns std::atomic<float>*)
       feeds into SmoothedValue instances that smooth per-sample
       passes smoothed values to Vocoder engine
```

## Sidechain Access Pattern in JUCE

This is the single most critical architectural detail. Both pieces are required or the sidechain will not appear in DAWs.

### 1. Bus Declaration (Constructor)

```cpp
CanaryAudioProcessor::CanaryAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output",   juce::AudioChannelSet::stereo(), true))
{
}
```

The second `withInput()` declares the sidechain bus. The `true` parameter means "enabled by default." The name "Sidechain" is what DAWs display.

### 2. Bus Layout Validation

```cpp
bool CanaryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Main input must be stereo (or mono for graceful handling)
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    // Output must match main input
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Sidechain: accept stereo, mono, or disabled (empty)
    auto scLayout = layouts.getChannelSet(true, 1);  // input bus index 1
    if (!scLayout.isDisabled()
        && scLayout != juce::AudioChannelSet::stereo()
        && scLayout != juce::AudioChannelSet::mono())
        return false;

    return true;
}
```

Key detail: the sidechain must be allowed to be disabled (empty channel set). Some DAWs probe with sidechain disabled first, and if `isBusesLayoutSupported` rejects that, the plugin will fail to load. Always allow `isDisabled()` for the sidechain bus.

### 3. Accessing Sidechain in processBlock

```cpp
void CanaryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    auto mainInput = getBusBuffer(buffer, true, 0);   // input bus 0
    auto sidechain = getBusBuffer(buffer, true, 1);   // input bus 1

    if (sidechain.getNumChannels() == 0)
    {
        // No sidechain connected — passthrough or silence
        // Update atomic flag so GUI can show "No Sidechain" indicator
        sidechainConnected.store(false);
        return;  // buffer already contains main input, which becomes output
    }

    sidechainConnected.store(true);
    vocoder.process(mainInput, sidechain, buffer, buffer.getNumSamples());
}
```

`getBusBuffer()` returns a sub-view of the main buffer — it does not copy data. The output buffer is typically the same memory as the main input buffer (in-place processing), so the Vocoder engine must read main input before writing output, or use scratch buffers.

## APVTS Pattern

### Parameter Layout Declaration

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "bands", "Bands", 8, 32, 16));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "attack", "Attack",
        juce::NormalisableRange<float>(1.0f, 50.0f, 0.1f, 0.5f), 5.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "release", "Release",
        juce::NormalisableRange<float>(10.0f, 200.0f, 0.1f, 0.5f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "formantShift", "Formant Shift",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("st")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWet", "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outputGain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}
```

### Reading Parameters on the Audio Thread

```cpp
// In PluginProcessor header — cached raw pointers (set once in constructor)
std::atomic<float>* bandsParam   = nullptr;
std::atomic<float>* attackParam  = nullptr;
// ... etc

// In constructor:
bandsParam  = apvts.getRawParameterValue("bands");
attackParam = apvts.getRawParameterValue("attack");

// In processBlock — read atomically, feed into SmoothedValue:
float attackMs = attackParam->load();
attackSmoothed.setTargetValue(attackMs);
```

This pattern avoids string lookups on the audio thread. `getRawParameterValue()` returns a pointer to an `std::atomic<float>` that is safe to read from any thread.

## Thread Model

### Two Threads, Hard Boundary

| Aspect | Audio Thread | GUI / Message Thread |
|--------|-------------|---------------------|
| **Runs** | processBlock(), prepareToPlay(), releaseResources() | Editor constructor, paint(), resized(), timer callbacks |
| **Timing** | Real-time, deadline-driven (buffer size / sample rate) | Best-effort, ~30-60fps for animation |
| **Constraints** | No allocation, no locks, no I/O, no blocking | Can allocate, can lock (briefly), can do I/O |
| **Accesses** | Reads APVTS atomics, writes to atomic visualization data | Reads APVTS via attachments, reads atomic visualization data |

### Cross-Thread Communication Patterns

1. **Parameters (GUI to Audio):** APVTS handles this. `SliderAttachment` writes to atomic storage, `processBlock` reads via `getRawParameterValue()`. No mutex needed.

2. **Visualization data (Audio to GUI):** Use a fixed-size `std::array<std::atomic<float>, MAX_BANDS>` for envelope levels. Audio thread writes after envelope follower, GUI timer reads for visualizer bars. Atomic floats are sufficient for single-producer single-consumer float values.

3. **Sidechain status (Audio to GUI):** Single `std::atomic<bool>` written by audio thread, read by GUI timer.

4. **Band count changes:** Band count is an integer parameter. Audio thread reads it each block. If changed, the Vocoder engine rebuilds its filter bank arrays in `processBlock` on the next call. Pre-allocate arrays to MAX_BANDS (32) to avoid allocation. Alternatively, only apply band count changes in `prepareToPlay()` by tracking a "pending band count" — but this requires the user to restart playback, which is poor UX. Pre-allocation is the correct approach.

### What NOT to Do

- **Never use `std::mutex` or `juce::CriticalSection` in processBlock.** Priority inversion will cause audio dropouts.
- **Never call `new` or `delete` in processBlock.** Heap allocation is non-deterministic.
- **Never read GUI component state from the audio thread.** Always go through APVTS or atomics.
- **Never write to APVTS from the audio thread** (except through the built-in parameter change mechanism).

## Patterns to Follow

### Pattern 1: Scratch Buffer Pre-allocation

**What:** Allocate all working buffers in `prepareToPlay()`, never in `processBlock()`.

**When:** Always. Every DSP component needs working memory.

**Example:**
```cpp
void Vocoder::prepare(const juce::dsp::ProcessSpec& spec)
{
    maxBlockSize = spec.maximumBlockSize;

    // Pre-allocate all scratch buffers to max sizes
    modulatorMono.setSize(1, maxBlockSize);
    carrierMono.setSize(1, maxBlockSize);

    for (int i = 0; i < MAX_BANDS; ++i)
    {
        bandBuffer[i].setSize(1, maxBlockSize);
        envelopeFollowers[i].prepare(spec.sampleRate);
    }

    modulatorFilterBank.prepare(spec);
    carrierFilterBank.prepare(spec);
}
```

### Pattern 2: SmoothedValue for All Parameters

**What:** Wrap every parameter in `juce::SmoothedValue` to prevent zipper noise.

**When:** Any parameter that directly affects audio output.

**Example:**
```cpp
// In Vocoder or PluginProcessor:
juce::SmoothedValue<float> dryWetSmoothed;
juce::SmoothedValue<float> outputGainSmoothed;

// In prepareToPlay:
dryWetSmoothed.reset(sampleRate, 0.02);   // 20ms ramp
outputGainSmoothed.reset(sampleRate, 0.02);

// In processBlock:
dryWetSmoothed.setTargetValue(dryWetParam->load() / 100.0f);

for (int s = 0; s < numSamples; ++s)
{
    float wet = dryWetSmoothed.getNextValue();
    output[s] = wet * vocodedSample + (1.0f - wet) * drySample;
}
```

### Pattern 3: Flush Denormals

**What:** Set the CPU to flush denormalized floats to zero to prevent CPU spikes.

**When:** At the top of every `processBlock()` call.

**Example:**
```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // ... rest of processing
}
```

### Pattern 4: In-Place Processing Awareness

**What:** The main output buffer and main input buffer may share the same memory.

**When:** Always in JUCE effect plugins (output bus 0 aliases input bus 0).

**Implication:** The Vocoder engine must either:
- Read the entire main input into a scratch buffer before writing output, OR
- Process sample-by-sample, reading input before writing output for each sample

The scratch buffer approach is cleaner and more predictable.

## Anti-Patterns to Avoid

### Anti-Pattern 1: String Lookups in processBlock

**What:** Calling `apvts.getParameter("name")` or similar string-based lookups in the audio callback.

**Why bad:** String comparison and hash lookups allocate and are non-deterministic. Violates real-time safety.

**Instead:** Cache `getRawParameterValue()` pointers in the constructor. Read the returned `std::atomic<float>*` in processBlock.

### Anti-Pattern 2: Resizing Containers on the Audio Thread

**What:** Calling `std::vector::resize()`, `push_back()`, or `AudioBuffer::setSize()` in processBlock when band count changes.

**Why bad:** Heap allocation on the audio thread causes glitches.

**Instead:** Pre-allocate all containers to MAX_BANDS (32) in `prepareToPlay()`. Use a `numActiveBands` variable to track how many are currently in use. Iterate only up to `numActiveBands`.

### Anti-Pattern 3: Sharing Raw Pointers Between Threads

**What:** Giving the GUI thread a raw pointer to DSP objects for visualization.

**Why bad:** The DSP object's state changes mid-read, causing torn reads or crashes.

**Instead:** Copy visualization data into a fixed-size atomic array on the audio thread. GUI reads the atomics independently.

### Anti-Pattern 4: Blocking the Audio Thread for GUI Updates

**What:** Using mutexes, condition variables, or message queues with blocking waits in processBlock.

**Why bad:** The GUI thread can be delayed by the OS scheduler. If the audio thread waits for the GUI thread, you get buffer underruns.

**Instead:** Use lock-free communication only. Atomics for simple values. `juce::AbstractFifo` or a lock-free ring buffer for streaming data if needed (not needed for this project's simple visualizer).

## Scalability Considerations

| Concern | 16 bands (default) | 32 bands (max) | Future: 64+ bands |
|---------|--------------------|-----------------|--------------------|
| CPU per block | Light — 32 IIR filters + 16 envelope followers | Moderate — 64 IIR + 32 followers | Would need SIMD optimization |
| Memory | ~50KB scratch buffers | ~100KB scratch buffers | Not a concern |
| GUI complexity | 16 bars, trivial | 32 bars, still trivial | Would need scrolling or grouping |
| Parameter smoothing | Negligible cost | Negligible cost | Still negligible |
| Latency | Zero (IIR filters are sample-by-sample, no lookahead) | Zero | Zero |

The architecture supports 32 bands comfortably without SIMD. For this project's scope, scalability beyond 32 bands is not a concern.

## Suggested Build Order

Components have strict dependencies. Build in this order:

```
Phase 1: PluginProcessor + Bus Declaration + CMake
    │     (sidechain must appear in DAW — nothing works without this)
    │
    ▼
Phase 2: FilterBank
    │     (independent DSP class, testable in isolation)
    │
    ▼
Phase 3: EnvelopeFollower
    │     (independent DSP class, testable in isolation)
    │
    ▼
Phase 4: Vocoder Engine
    │     (wires FilterBank + EnvelopeFollower together,
    │      connects to processBlock via sidechain access)
    │
    ▼
Phase 5: APVTS Parameters + PluginEditor (GUI)
    │     (parameters need the Vocoder to exist so they can control it;
    │      GUI needs parameters to attach knobs to)
    │
    ▼
Phase 6: Polish (edge cases, presets, noise blend)
```

**Why this order:**
- Phase 1 blocks everything — if sidechain routing fails, the plugin is DOA
- FilterBank and EnvelopeFollower are independent leaf nodes with no dependencies on each other, but the Vocoder engine needs both
- APVTS could technically be set up in Phase 1, but parameters are meaningless until the DSP exists to consume them
- GUI comes last because it is pure presentation; the plugin should work headlessly with default parameter values before any GUI exists

## File / Directory Structure

```
Canary/
├── CMakeLists.txt                    # JUCE + CMake configuration
├── src/
│   ├── PluginProcessor.h             # AudioProcessor subclass declaration
│   ├── PluginProcessor.cpp           # Bus config, processBlock, state save/load
│   ├── PluginEditor.h                # Editor declaration
│   ├── PluginEditor.cpp              # GUI layout, knobs, visualizer, timer
│   ├── Vocoder.h                     # DSP engine interface
│   ├── Vocoder.cpp                   # Orchestrates FilterBanks + Followers
│   ├── FilterBank.h                  # Band of IIR bandpass filters
│   ├── FilterBank.cpp                # Logarithmic frequency calc, filter processing
│   ├── EnvelopeFollower.h            # Per-band envelope detection
│   └── EnvelopeFollower.cpp          # Attack/release single-pole filter
└── resources/                        # GUI assets (if any)
```

Each `.h/.cpp` pair is one class with one responsibility. No God objects. The Vocoder class is the only "orchestrator" — it owns the filter banks and envelope followers but delegates all actual DSP to them.

## Sources

- JUCE AudioProcessor documentation (bus system, `getBusBuffer()`, `isBusesLayoutSupported()`)
- JUCE AudioProcessorValueTreeState documentation (parameter layout, attachments, `getRawParameterValue()`)
- JUCE dsp module documentation (`IIR::Filter`, `ProcessSpec`, `SmoothedValue`)
- JUCE plugin examples (AudioPluginDemo, SideChainCompressor example)
- Project outline: `/Users/christopherjaeckel/Repos/canary/Docs/chord-vocoder-project-outline.md`

**Confidence note:** Architecture patterns described here are well-established JUCE conventions documented across official tutorials, the JUCE API reference, and the JUCE source code examples. MEDIUM-HIGH confidence overall. The sidechain bus pattern specifically is confirmed by the project outline's code snippets which match standard JUCE API usage. The one area of lower confidence is whether `AudioParameterFloatAttributes().withLabel()` exists in all JUCE versions — verify against the specific JUCE version used.
