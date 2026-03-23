# Domain Pitfalls

**Domain:** JUCE VST3 Vocoder Plugin (Sidechain Effect)
**Researched:** 2026-03-20
**Sources:** JUCE framework documentation, established JUCE community knowledge, VST3 SDK behavior (training data -- these are long-standing, stable issues in the JUCE/VST3 ecosystem)

## Critical Pitfalls

Mistakes that cause DAW crashes, invisible sidechain routing, or unusable plugins.

---

### Pitfall 1: Sidechain Bus Not Appearing in DAW

**What goes wrong:** The plugin loads but the DAW never shows a sidechain input routing option. The entire vocoder is useless because no carrier signal can reach it.

**Why it happens:** JUCE requires sidechain bus declaration in TWO separate places, and most developers only do one:
1. The constructor `BusesProperties` chain
2. The `isBusesLayoutSupported()` override

If either is missing or inconsistent, the DAW silently drops the sidechain bus. There is no error message.

**Consequences:** Plugin appears to work (loads, passes audio) but the sidechain dropdown never appears in any DAW. Users cannot route a carrier signal. The entire plugin is non-functional for its core purpose.

**Prevention:**
```cpp
// 1. Constructor -- declare the sidechain bus
MyProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)  // false = not enabled by default
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{}

// 2. isBusesLayoutSupported -- MUST explicitly accept sidechain layouts
bool isBusesLayoutSupported(const BusesLayout& layouts) const override
{
    // Main input and output must be stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain can be disabled (empty) or stereo
    const auto& scLayout = layouts.getChannelSet(true, 1);  // input bus 1
    if (!scLayout.isDisabled() && scLayout != juce::AudioChannelSet::stereo())
        return false;

    return true;
}
```

**Detection:** First thing to test after Phase 1 build. Load the VST3 in Ableton, Reaper, and Logic. If sidechain routing is missing in any DAW, this is the problem.

**Critical detail -- the `false` default:** The sidechain bus should be declared with `false` (not enabled by default) in `BusesProperties`. Some DAWs (notably Ableton) will not show the sidechain routing option if it is declared as enabled-by-default (`true`). The DAW enables it when the user explicitly routes a signal to it.

**Confidence:** HIGH -- this is the single most common JUCE sidechain issue, extensively documented in JUCE forums and GitHub issues.

**Phase:** Phase 1 (Project Setup & Sidechain Bus). This must be validated before any DSP work begins.

---

### Pitfall 2: Memory Allocation on the Audio Thread

**What goes wrong:** Plugin causes audio dropouts, clicks, pops, or full DAW hangs during playback. Worse: intermittent -- works in testing, fails in production when the system is under load.

**Why it happens:** `processBlock()` runs on a real-time audio thread with a hard deadline (e.g., ~1.5ms at 44.1kHz/64 samples). Any operation that might block -- `new`, `delete`, `std::vector::push_back`, `std::string` construction, mutex locks, file I/O -- can miss the deadline and cause audible glitches or DAW freezes.

**Consequences:** Audio dropouts, clicks, pops. Under heavy system load, the DAW may freeze entirely. These bugs are intermittent and extremely hard to reproduce in isolated testing.

**Prevention:**
- Pre-allocate ALL buffers in `prepareToPlay()`. Never resize in `processBlock()`.
- No `std::vector::push_back`, `std::string`, `new`, `delete`, `malloc` in `processBlock()`.
- No `std::mutex`, `std::lock_guard`, or any locking primitive. Use `std::atomic` for cross-thread communication.
- No `DBG()`, `juce::Logger`, `std::cout`, or file I/O.
- No JUCE `String` construction (it allocates).
- Use `juce::SmoothedValue` (pre-allocated) not `juce::LinearSmoothedValue` construction in the block.
- For the filter bank: pre-allocate for the maximum band count (32) even if default is 16. Index into the pre-allocated array rather than resizing.

**Detection:** Use a real-time safety checker during development. On macOS, Xcode's Thread Sanitizer can catch some issues. The definitive test is running the plugin under heavy CPU load with small buffer sizes (64 samples).

**Confidence:** HIGH -- fundamental real-time audio programming constraint, universally agreed upon.

**Phase:** Phase 2 onward (every phase that touches `processBlock()`).

---

### Pitfall 3: Denormal Numbers Causing CPU Spikes

**What goes wrong:** Plugin idles at 2-5% CPU during playback, but when input goes silent (or nearly silent), CPU suddenly spikes to 50-100%. Fans spin up. DAW becomes sluggish.

**Why it happens:** IIR filters (bandpass filters in the filter bank) produce tiny floating-point values as they decay toward zero. These "denormal" numbers (values below ~1e-38) cause the CPU's floating-point unit to switch from hardware to software processing, which is 10-100x slower. With 16-32 bands of IIR filters, this multiplies the problem.

**Consequences:** Massive CPU spikes on silent or near-silent audio. Users will see the DAW CPU meter spike when the vocalist stops singing. Especially bad with vocoders because there are many parallel filter instances.

**Prevention:**
```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
{
    juce::ScopedNoDenormals noDenormals;  // FIRST LINE of processBlock
    // ... rest of processing
}
```
This sets the CPU's flush-to-zero (FTZ) and denormals-are-zero (DAZ) flags for the duration of the block. JUCE provides this wrapper -- use it. Do NOT rely on compiler flags alone (`-ffast-math` or `/fp:fast`) because they are not guaranteed across all platforms and build configs.

Additionally, in envelope followers, add a tiny DC offset (e.g., 1e-18f) to prevent the filter from settling exactly at zero:
```cpp
envelope = coeff * envelope + (1.0f - coeff) * std::abs(sample + 1e-18f);
```

**Detection:** Play audio through the vocoder, then stop the input. Watch the DAW's CPU meter. If it spikes when audio stops, denormals are the cause.

**Confidence:** HIGH -- this is one of the most well-known DSP pitfalls, and vocoders with many parallel filters are especially susceptible.

**Phase:** Phase 2 (Filter Bank) and Phase 3 (Envelope Follower). Must be in place from the first DSP code.

---

### Pitfall 4: APVTS Parameter Access from Audio Thread is Not Thread-Safe by Default

**What goes wrong:** Occasional crashes, corrupted parameter values, or undefined behavior when reading parameters in `processBlock()` while the GUI or DAW automation is writing them.

**Why it happens:** `AudioProcessorValueTreeState::getRawParameterValue()` returns a `std::atomic<float>*`, which is safe. But many developers instead call `getParameter()`, `treeState.getParameterAsValue()`, or access the ValueTree directly from the audio thread -- these are NOT thread-safe and involve locking internally.

**Consequences:** Rare but catastrophic crashes. Data races that are nearly impossible to reproduce but will surface in real-world use when users automate parameters.

**Prevention:**
```cpp
// In the processor header, cache atomic pointers:
std::atomic<float>* bandCountParam;
std::atomic<float>* attackParam;
std::atomic<float>* releaseParam;
// ... etc

// In constructor, after creating APVTS:
bandCountParam = apvts.getRawParameterValue("bands");
attackParam = apvts.getRawParameterValue("attack");

// In processBlock, read via load():
float attack = attackParam->load();
```

Never call `apvts.getParameter("name")->getValue()` from the audio thread. Never access `ValueTree` nodes from the audio thread. Never use `Value` objects from the audio thread.

**Detection:** Thread Sanitizer (TSan) in Xcode will catch data races. Also test with DAW parameter automation running while audio plays.

**Confidence:** HIGH -- well-documented JUCE pattern.

**Phase:** Phase 5 (Parameters & GUI), but the pattern should be established from Phase 2 when first reading any parameter.

---

### Pitfall 5: getBusBuffer Returning Empty Buffer Without Crash Protection

**What goes wrong:** Plugin crashes with a segfault or access violation when the sidechain is not connected, or when a DAW disconnects the sidechain mid-playback (e.g., user deletes the source track).

**Why it happens:** `getBusBuffer(buffer, true, 1)` for the sidechain input returns a buffer with 0 channels when no sidechain is routed. Code that blindly calls `sidechain.getReadPointer(0)` on a 0-channel buffer will crash.

**Consequences:** Hard crash of the DAW. Users lose unsaved work. Plugin gets a reputation as unstable.

**Prevention:**
```cpp
auto sidechain = getBusBuffer(buffer, true, 1);
if (sidechain.getNumChannels() == 0 || sidechain.getNumSamples() == 0)
{
    // No sidechain: pass through dry signal or output silence
    // Update the "sidechain connected" flag for the GUI
    sidechainConnected.store(false);
    return;
}
sidechainConnected.store(true);
```

This check must be the FIRST thing after getting the sidechain buffer. Do not assume the sidechain will always be available.

**Detection:** Test by loading the plugin without routing any sidechain signal. Test by routing a sidechain, playing audio, then deleting the source track mid-playback.

**Confidence:** HIGH -- standard defensive programming for JUCE sidechain plugins.

**Phase:** Phase 1 (the passthrough/silence fallback) and Phase 4 (the full vocoder bypass path).

---

## Moderate Pitfalls

### Pitfall 6: SmoothedValue Not Reset in prepareToPlay

**What goes wrong:** After a sample rate change or transport restart, the first few audio blocks have a parameter "sweep" artifact -- a click, pop, or brief wrong sound as SmoothedValue ramps from its old value to the current one.

**Why it happens:** `SmoothedValue::reset()` must be called in `prepareToPlay()` with the new sample rate AND the current target value. If you only call `reset(sampleRate, rampTimeSeconds)` without also setting the current value via `setCurrentAndTargetValue()`, the smoother starts from 0 (or its last value from the previous session) and ramps to the target, causing an audible sweep.

**Prevention:**
```cpp
void prepareToPlay(double sampleRate, int samplesPerBlock) override
{
    dryWetSmoothed.reset(sampleRate, 0.02);  // 20ms ramp time
    dryWetSmoothed.setCurrentAndTargetValue(dryWetParam->load());  // snap to current value

    gainSmoothed.reset(sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue(gainParam->load());
    // ... for all smoothed parameters
}
```

**Detection:** Change sample rate in DAW preferences while plugin is loaded. Listen for a brief "sweep" or click on the first buffer after playback resumes.

**Confidence:** HIGH -- common JUCE gotcha, well-documented.

**Phase:** Phase 5 (Parameters), but relevant from Phase 2 if any parameter smoothing is used early.

---

### Pitfall 7: Band Count Change During Playback Causes Glitch or Crash

**What goes wrong:** User changes the "Bands" knob from 16 to 24 during playback. The filter bank array is resized, envelope followers are re-created, and the audio thread accesses partially-initialized data. Result: crash, loud pop, or garbage audio.

**Why it happens:** Changing band count requires re-creating the filter bank and envelope follower arrays. If this happens on the audio thread (via parameter change callback), it involves allocation. If it happens on the GUI thread while the audio thread is iterating the arrays, it is a data race.

**Consequences:** At best, a loud click. At worst, a DAW crash from accessing freed memory.

**Prevention:** The project outline already identifies the right approach: band count changes only take effect on engine reset (i.e., in `prepareToPlay()`). Implementation:

1. Pre-allocate filter and envelope arrays for the MAXIMUM band count (32) at all times.
2. Store the "active band count" as a simple integer.
3. When the band count parameter changes, update a pending value. Apply it in `prepareToPlay()` or at the start of the next `processBlock()` with a crossfade.
4. The simplest safe approach: read the band count atomically at the top of `processBlock()`, only use that many bands from the pre-allocated arrays, recalculate filter coefficients if changed.

```cpp
// Pre-allocate for max bands
static constexpr int MAX_BANDS = 32;
std::array<BandFilter, MAX_BANDS> modulatorFilters;
std::array<BandFilter, MAX_BANDS> carrierFilters;
std::array<EnvelopeFollower, MAX_BANDS> envelopes;

// In processBlock:
int activeBands = static_cast<int>(bandCountParam->load());
activeBands = juce::jlimit(8, MAX_BANDS, activeBands);
// Only iterate up to activeBands
```

The filter coefficients should be recalculated when band count changes, but this can be done at the start of processBlock (coefficient calculation is just math, no allocation) rather than in a separate thread.

**Detection:** Automate the Bands parameter in the DAW to sweep from 8 to 32 during playback. Listen for pops, watch for crashes.

**Confidence:** HIGH -- this is explicitly called out in the project outline as "the trickiest one."

**Phase:** Phase 2 (Filter Bank architecture) -- the pre-allocation pattern must be decided before building the filter bank.

---

### Pitfall 8: IIR Filter Coefficient Recalculation Causing Clicks

**What goes wrong:** When filter coefficients are recalculated (due to formant shift, band count change, or sample rate change), the filter output produces a transient click or pop.

**Why it happens:** Abruptly changing IIR filter coefficients while the filter has internal state (delay line values) causes a discontinuity in the output signal. The new coefficients interpret the old state differently, producing a spike.

**Prevention:**
- For the Formant Shift parameter: smooth the shift value with `SmoothedValue`, and recalculate coefficients gradually per-sample or per-block (per-block is usually sufficient and much cheaper).
- When recalculating coefficients, do NOT reset the filter state (delay lines). Let the filter "ring" through the transition naturally. Resetting state to zero causes an even worse transient.
- For band count changes at engine reset: reset filter states to zero in `prepareToPlay()` (acceptable because there is no audio playing during reset).
- Consider using `juce::dsp::IIR::Filter::snapToZero()` after coefficient changes to quickly kill any ringing artifacts without a hard reset.

**Detection:** Automate the Formant Shift parameter to sweep continuously during playback. Listen for periodic clicks at the coefficient update rate.

**Confidence:** HIGH -- fundamental IIR filter behavior.

**Phase:** Phase 2 (Filter Bank) and Phase 5 (Formant Shift parameter).

---

### Pitfall 9: DAW-Specific Sidechain Routing Incompatibilities

**What goes wrong:** Sidechain works in Reaper but not in Ableton. Or it works in Ableton but the sidechain signal appears on the wrong channels in FL Studio. Each DAW has its own quirks.

**Why it happens:** DAWs implement VST3 sidechain routing differently:

- **Ableton Live:** Requires the sidechain bus to be declared as `false` (not enabled by default). The sidechain dropdown only appears after the plugin is loaded and the user clicks the plugin's wrench icon to reveal the sidechain panel. If the bus is `true` by default, Ableton may not show the routing option at all.

- **Logic Pro:** Logic primarily uses AU format, not VST3. If targeting Logic, you need an AU build (out of scope for now, but important to know). Logic's VST3 support exists but is less mature than its AU support.

- **FL Studio:** FL routes sidechain audio to additional input channels. The sidechain bus mapping can differ from other DAWs. Test that `getBusBuffer(buffer, true, 1)` actually returns the sidechain signal and not silence.

- **Reaper:** Most flexible -- lets the user manually route any channels. Sidechain appears as additional input pins. Usually works fine if the bus declaration is correct.

- **Bitwig:** Similar to Ableton in sidechain routing. Generally well-behaved with VST3 sidechains.

**Consequences:** Plugin appears broken in specific DAWs. Users on those DAWs cannot use the plugin.

**Prevention:**
- Test in ALL target DAWs (Ableton, FL Studio, Reaper, at minimum) during Phase 1, before writing any DSP.
- Use the `false` default for the sidechain bus in BusesProperties.
- Accept both stereo and disabled layouts for the sidechain in `isBusesLayoutSupported()`.
- Document DAW-specific routing instructions for end users.

**Detection:** Load the compiled VST3 in each target DAW. Verify the sidechain routing option appears. Route a simple signal (sine wave) into the sidechain and verify it arrives in `processBlock()` with non-zero samples.

**Confidence:** MEDIUM -- behavior varies by DAW version and may change with updates. The core advice (use `false` default, test in all DAWs) is HIGH confidence.

**Phase:** Phase 1 (validation) and Phase 6 (polish/compatibility testing).

---

### Pitfall 10: GUI Timer Callback Blocking the Message Thread

**What goes wrong:** The band visualizer update (30fps timer) causes the GUI to become sluggish, or the DAW's UI thread to stall, because the timer callback does too much work.

**Why it happens:** JUCE's `Timer` callback runs on the message thread. If the callback reads large amounts of data, performs complex calculations, or triggers a full repaint of a large component, it blocks the message thread. At 30fps, that is 33ms between callbacks -- not much headroom.

**Prevention:**
- The audio thread should write envelope values to a lock-free structure (e.g., `std::array<std::atomic<float>, MAX_BANDS>`).
- The timer callback reads these atomics and calls `repaint()` only on the visualizer component, not the entire plugin window.
- Keep the `paint()` method of the visualizer lightweight -- draw rectangles, not complex paths.
- Consider using `juce::Component::repaint(Rectangle<int>)` to limit the repaint region.

**Detection:** Open the plugin GUI and play audio. If the DAW feels sluggish or the GUI stutters, the timer callback or paint method is too heavy.

**Confidence:** HIGH -- standard JUCE GUI performance pattern.

**Phase:** Phase 5 (GUI).

---

## Minor Pitfalls

### Pitfall 11: Forgetting to Call prepareToPlay on DSP Sub-Objects

**What goes wrong:** Filters or envelope followers produce garbage output, silence, or NaN values because they were never initialized with the correct sample rate and block size.

**Prevention:** In `prepareToPlay()`, propagate the `ProcessSpec` to every DSP object:
```cpp
void prepareToPlay(double sampleRate, int samplesPerBlock) override
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32>(samplesPerBlock);
    spec.numChannels = 2;

    for (auto& filter : modulatorFilters)
        filter.prepare(spec);
    for (auto& filter : carrierFilters)
        filter.prepare(spec);
    // ... envelope followers, etc.
}
```

**Confidence:** HIGH.

**Phase:** Phase 2 (Filter Bank).

---

### Pitfall 12: Plugin State Save/Restore Losing Parameters

**What goes wrong:** User saves a DAW project, reopens it, and all plugin parameters are reset to defaults. Or worse, the plugin crashes on state restore because the XML is malformed or the band count was changed.

**Prevention:**
- Implement `getStateInformation()` and `setStateInformation()` using APVTS's `copyState()` and `replaceState()` methods.
- Always validate restored values (clamp to valid ranges).
- Handle version mismatches gracefully -- if a future version adds parameters, old saved states should still load without crashing.

```cpp
void getStateInformation(juce::MemoryBlock& destData) override
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void setStateInformation(const void* data, int sizeInBytes) override
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
```

**Confidence:** HIGH.

**Phase:** Phase 5 (Parameters) and Phase 6 (Preset system).

---

### Pitfall 13: Mono/Stereo Bus Mismatch Crashes

**What goes wrong:** Plugin crashes or produces distorted output when a DAW sends a mono signal on the main input or sidechain.

**Prevention:** In `isBusesLayoutSupported()`, also accept mono inputs:
```cpp
// Accept mono OR stereo for sidechain
const auto& scLayout = layouts.getChannelSet(true, 1);
if (!scLayout.isDisabled()
    && scLayout != juce::AudioChannelSet::stereo()
    && scLayout != juce::AudioChannelSet::mono())
    return false;
```

In `processBlock()`, always check `getNumChannels()` before accessing channel pointers. If mono, duplicate to both channels or process mono.

**Confidence:** HIGH.

**Phase:** Phase 1 (bus layout) and Phase 6 (edge cases).

---

### Pitfall 14: VST3 Unique ID Collision

**What goes wrong:** Plugin conflicts with another plugin in the DAW's plugin database. DAW may load the wrong plugin, refuse to load, or corrupt the plugin cache.

**Prevention:** Ensure the plugin code ("CVoc") and manufacturer ID form a unique combination. In CMakeLists.txt:
```cmake
juce_add_plugin(Canary
    PLUGIN_MANUFACTURER_CODE  Xxxx    # Use a unique 4-char code
    PLUGIN_CODE               CVoc
    # ...
)
```

The manufacturer code must be globally unique. Register it if distributing commercially.

**Confidence:** HIGH.

**Phase:** Phase 1 (Project Setup).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Phase 1: Setup & Sidechain | Sidechain bus invisible in DAW (Pitfall 1) | Declare in BOTH constructor and isBusesLayoutSupported; use `false` default; test in all target DAWs before proceeding |
| Phase 1: Setup & Sidechain | getBusBuffer crash on no sidechain (Pitfall 5) | Null-check sidechain buffer channel count on every processBlock call |
| Phase 1: Setup & Sidechain | DAW-specific routing issues (Pitfall 9) | Test in Ableton, Reaper, FL Studio immediately after first build |
| Phase 2: Filter Bank | Denormals from IIR filters (Pitfall 3) | ScopedNoDenormals as first line of processBlock; DC offset in envelope followers |
| Phase 2: Filter Bank | Allocation in processBlock (Pitfall 2) | Pre-allocate for MAX_BANDS (32); never resize arrays in processBlock |
| Phase 2: Filter Bank | Band count change crash (Pitfall 7) | Pre-allocate max; use atomic active count; recalculate coefficients at block boundaries |
| Phase 3: Envelope Follower | Denormal accumulation in feedback loop (Pitfall 3) | Add tiny DC offset (1e-18f) to prevent filter settling at zero |
| Phase 4: Vocoder Engine | Sidechain buffer crash (Pitfall 5) | Defensive check at top of processBlock before any sidechain access |
| Phase 5: Parameters | APVTS thread safety (Pitfall 4) | Cache getRawParameterValue pointers; only use atomic load in processBlock |
| Phase 5: Parameters | SmoothedValue sweep on reset (Pitfall 6) | Call setCurrentAndTargetValue after reset in prepareToPlay |
| Phase 5: Parameters | Filter coefficient clicks (Pitfall 8) | Smooth formant shift parameter; do not reset filter state on coefficient change |
| Phase 5: GUI | Timer callback blocking UI (Pitfall 10) | Lock-free atomics for audio-to-GUI data; repaint only visualizer component |
| Phase 6: Polish | State save/restore failure (Pitfall 12) | Implement get/setStateInformation via APVTS; validate on restore |
| Phase 6: Polish | Mono input crash (Pitfall 13) | Accept mono in isBusesLayoutSupported; check channel count before access |

## Overall Confidence Assessment

| Pitfall Category | Confidence | Notes |
|-----------------|------------|-------|
| Sidechain bus declaration | HIGH | Most common JUCE sidechain failure mode; well-documented |
| Real-time audio safety | HIGH | Fundamental constraint; universally agreed upon |
| Denormals | HIGH | Classic DSP pitfall; especially relevant for multi-band filter plugins |
| APVTS thread safety | HIGH | Well-documented JUCE pattern |
| Band count changes | HIGH | Project outline already identified this as critical |
| DAW compatibility | MEDIUM | Varies by DAW version; core advice is stable but details may shift |
| GUI performance | HIGH | Standard JUCE pattern |
| Filter coefficient clicks | HIGH | Fundamental IIR behavior |

## Sources

- JUCE framework API documentation (AudioProcessor, AudioProcessorValueTreeState, dsp::IIR::Filter)
- JUCE tutorials on plugin sidechain configuration
- VST3 SDK bus arrangement documentation
- Established real-time audio programming best practices (Ross Bencina's "Real-time audio programming 101")
- JUCE community forums (common sidechain and denormal discussions)

Note: Web search was unavailable during research. All findings are based on training data covering well-established, long-standing JUCE/VST3 patterns. These are architectural constraints that have been stable for years. Confidence levels reflect this -- the core patterns have not changed, but DAW-specific behavior (Pitfall 9) should be verified against current DAW versions during development.
