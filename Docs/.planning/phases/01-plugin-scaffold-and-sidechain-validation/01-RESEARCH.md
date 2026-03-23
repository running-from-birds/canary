# Phase 1: Plugin Scaffold and Sidechain Validation - Research

**Researched:** 2026-03-20
**Domain:** JUCE VST3 plugin scaffolding, CMake build system, sidechain bus configuration
**Confidence:** HIGH

## Summary

Phase 1 is the foundation of the entire Canary project. It produces a VST3 plugin that loads in DAWs, exposes a sidechain routing option, and passes audio through unmodified. No DSP processing occurs in this phase -- the goal is to validate that the build system, plugin metadata, bus configuration, and DAW compatibility are all correct before any signal processing code is written.

The critical technical challenge is the sidechain bus declaration. JUCE requires sidechain configuration in TWO separate places (constructor `BusesProperties` AND `isBusesLayoutSupported()` override), and if either is missing or inconsistent, the sidechain routing option silently fails to appear in DAWs. This is the single most common failure mode for JUCE sidechain plugins.

**Primary recommendation:** Build the minimal plugin scaffold with CMake + FetchContent pinned to JUCE 8.0.12, declare the sidechain bus correctly in both required locations, and test in all four target DAWs (Ableton, FL Studio, Reaper, Logic) before proceeding to Phase 2.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| INFRA-01 | Plugin compiles as VST3 and loads successfully in Ableton, FL Studio, Reaper, and Logic | CMake + FetchContent pattern with JUCE 8.0.12, `juce_add_plugin()` with `FORMATS VST3`, `COPY_PLUGIN_AFTER_BUILD TRUE`, ad-hoc code signing for macOS |
| INFRA-02 | DAW presents a sidechain input routing option when the plugin is inserted | Sidechain bus declared in constructor `BusesProperties` AND validated in `isBusesLayoutSupported()`; must accept disabled sidechain layout; DAW-specific routing instructions documented |
| INFRA-03 | Plugin passes main input audio through to output before any DSP is added (verifiable passthrough build) | `processBlock()` returns without modifying the main input buffer; sidechain guard check prevents crash when no sidechain is connected |
</phase_requirements>

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Audio plugin framework | Industry standard. Latest stable release (Dec 2025). Provides AudioProcessor, bus management, CMake API. |
| C++ | C++17 | Language standard | JUCE 7+ requires C++17 minimum. Provides structured bindings, std::optional. |
| CMake | 3.24+ | Build system | JUCE requires 3.22 minimum. 3.24+ recommended for better FetchContent and presets support. |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Xcode CLI Tools | 15+ (Apple Clang 15+) | macOS compiler | Required for macOS builds. `xcode-select --install` |
| Homebrew CMake | 3.24+ | CMake on macOS | `brew install cmake` if system cmake is too old |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| FetchContent | Git submodule | Acceptable but adds manual `git submodule update` steps. FetchContent is cleaner. |
| VST3 only | VST3 + AU | AU needed for Logic native support, but adds scope. Can add later with one CMake line (`FORMATS VST3 AU`). |

**Installation:**
```bash
xcode-select --install
brew install cmake
```

**Version verification:** JUCE 8.0.12 confirmed as latest stable release via GitHub releases page (published December 16, 2025).

## Architecture Patterns

### Recommended Project Structure

```
Canary/
├── CMakeLists.txt                    # JUCE + CMake configuration
├── src/
│   ├── PluginProcessor.h             # AudioProcessor subclass declaration
│   ├── PluginProcessor.cpp           # Bus config, processBlock (passthrough), state stubs
│   ├── PluginEditor.h                # Editor declaration (minimal for Phase 1)
│   └── PluginEditor.cpp              # Blank editor with plugin name label
```

Phase 1 includes ONLY the processor and editor. No Vocoder, FilterBank, or EnvelopeFollower files yet -- those come in Phase 2. The directory structure should be set up so future files can be added to `src/` without restructuring.

### Pattern 1: CMakeLists.txt with FetchContent

**What:** Pull JUCE via FetchContent pinned to a specific git tag for reproducible builds.

**Example:**
```cmake
cmake_minimum_required(VERSION 3.24)

project(Canary VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Canary
    COMPANY_NAME "Canary"
    PLUGIN_MANUFACTURER_CODE Cnry       # 4 chars, at least one uppercase
    PLUGIN_CODE CVoc                     # 4 chars, at least one uppercase
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
    FORMATS VST3
    PRODUCT_NAME "Canary"
)

target_sources(Canary
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

target_compile_definitions(Canary
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Canary
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags
)
```

Note: `juce_dsp` is NOT linked in Phase 1 because there is no DSP processing yet. It will be added in Phase 2 when the filter bank is built. This keeps the initial build lean and fast.

### Pattern 2: Sidechain Bus Declaration (CRITICAL)

**What:** Sidechain must be declared in TWO places or DAWs will not show the sidechain routing option.

**Place 1 -- Constructor BusesProperties:**
```cpp
CanaryProcessor::CanaryProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input",     juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",   juce::AudioChannelSet::stereo(), true))
{
}
```

The third argument to `withInput` for the sidechain controls whether the bus is enabled by default. The official JUCE NoiseGate example omits this parameter (defaulting to `true`), but using `false` is recommended for better DAW compatibility -- particularly with Ableton Live, which may not show sidechain routing for buses that are enabled by default. The DAW enables the bus when the user explicitly routes a signal to it.

**Place 2 -- isBusesLayoutSupported:**
```cpp
bool CanaryProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Main input must be mono or stereo
    const auto& mainInput = layouts.getMainInputChannelSet();
    if (mainInput != juce::AudioChannelSet::stereo()
        && mainInput != juce::AudioChannelSet::mono())
        return false;

    // Output must match main input
    if (layouts.getMainOutputChannelSet() != mainInput)
        return false;

    // Sidechain: accept stereo, mono, or disabled
    const auto& sidechain = layouts.getChannelSet(true, 1);
    if (!sidechain.isDisabled()
        && sidechain != juce::AudioChannelSet::stereo()
        && sidechain != juce::AudioChannelSet::mono())
        return false;

    return true;
}
```

**Critical detail:** The sidechain MUST be allowed to be disabled (`isDisabled()` returns true). Some DAWs probe with the sidechain disabled first, and if `isBusesLayoutSupported()` rejects that configuration, the plugin will fail to load entirely.

### Pattern 3: Passthrough processBlock

**What:** For Phase 1, processBlock does nothing -- the main input buffer IS the output buffer (in-place processing). Simply returning without modification achieves passthrough.

```cpp
void CanaryProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Guard: check sidechain availability (defensive, even for passthrough)
    auto sidechain = getBusBuffer(buffer, true, 1);
    sidechainConnected.store(sidechain.getNumChannels() > 0);

    // Phase 1: passthrough -- main input buffer IS the output buffer
    // No processing needed; buffer passes through unmodified
}
```

### Pattern 4: Minimal Editor

**What:** Phase 1 editor is just a window with the plugin name. No knobs, no visualizer.

```cpp
CanaryEditor::CanaryEditor(CanaryProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(450, 380);
}

void CanaryEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawFittedText("Canary", getLocalBounds(), juce::Justification::centred, 1);
}
```

### Anti-Patterns to Avoid

- **Missing isBusesLayoutSupported:** Declaring sidechain in BusesProperties but not validating in isBusesLayoutSupported causes silent sidechain failure.
- **Rejecting disabled sidechain:** Not accepting `isDisabled()` for the sidechain bus causes the plugin to fail to load in some DAWs.
- **Including DSP code in Phase 1:** Adding Vocoder/FilterBank/EnvelopeFollower files before sidechain is validated wastes effort if the bus configuration is wrong.
- **Skipping code signing on macOS:** On macOS Ventura+, unsigned plugins may not load. Ad-hoc signing is sufficient for dev testing.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CMake plugin setup | Manual compiler flags and linker config | `juce_add_plugin()` CMake API | JUCE's CMake API handles all platform-specific details, plugin format wrapping, and metadata embedding |
| VST3 SDK integration | Separate Steinberg SDK download | JUCE bundled VST3 headers | JUCE bundles its own VST3 SDK and handles all wrapping internally |
| Plugin copy to DAW folder | Manual post-build script | `COPY_PLUGIN_AFTER_BUILD TRUE` + `VST3_COPY_DIR` | JUCE CMake handles platform-specific install paths |
| macOS code signing | Custom signing script | `codesign --force --deep --sign -` (ad-hoc) | Ad-hoc signing is sufficient for development; no certificate needed |

## Common Pitfalls

### Pitfall 1: Sidechain Bus Not Appearing in DAW

**What goes wrong:** Plugin loads but DAW never shows sidechain input routing. The entire vocoder concept is dead on arrival.
**Why it happens:** Missing or inconsistent sidechain declaration between constructor and isBusesLayoutSupported.
**How to avoid:** Implement BOTH declaration points as shown in Architecture Patterns. Test in ALL four target DAWs immediately after first successful build.
**Warning signs:** Plugin loads and passes audio but no sidechain dropdown appears in any DAW.

### Pitfall 2: Plugin Fails to Load on macOS (Code Signing)

**What goes wrong:** DAW says "plugin failed to load" or "plugin could not be validated" on macOS Ventura or newer.
**Why it happens:** macOS requires code signing even for local development. Unsigned .vst3 bundles are rejected by Gatekeeper.
**How to avoid:** After building, ad-hoc sign the plugin:
```bash
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Canary.vst3
```
**Warning signs:** Plugin works on older macOS but fails on Ventura+.

### Pitfall 3: VST3 Unique ID Collision

**What goes wrong:** Plugin conflicts with another plugin in DAW's plugin database. DAW loads wrong plugin or refuses to scan.
**Why it happens:** Using generic manufacturer code or plugin code that collides with existing plugins.
**How to avoid:** Use a unique PLUGIN_MANUFACTURER_CODE (e.g., "Cnry" for Canary) and PLUGIN_CODE ("CVoc"). These form a composite unique identifier.
**Warning signs:** DAW shows duplicate plugin entries or fails to scan.

### Pitfall 4: getBusBuffer Crash When No Sidechain Connected

**What goes wrong:** Plugin crashes when no sidechain is routed, or when the sidechain source track is deleted mid-playback.
**Why it happens:** `getBusBuffer(buffer, true, 1)` returns a buffer with 0 channels when no sidechain is connected. Accessing channel 0 of a 0-channel buffer crashes.
**How to avoid:** Always check `sidechain.getNumChannels() > 0` before any access. In Phase 1, this is the passthrough guard.
**Warning signs:** Plugin works with sidechain but crashes without one.

### Pitfall 5: Logic Pro VST3 Sidechain Limitations

**What goes wrong:** Sidechain works in Ableton, FL Studio, and Reaper but not in Logic Pro.
**Why it happens:** Logic Pro primarily uses AU (Audio Units) format, not VST3. Logic's VST3 support exists but is less mature, and sidechain routing for VST3 plugins in Logic may be limited or absent.
**How to avoid:** For full Logic support, consider adding AU format in a future phase (`FORMATS VST3 AU` in CMake). For Phase 1, test VST3 in Logic but accept that full sidechain functionality may require AU format.
**Warning signs:** Sidechain works in all other DAWs but not Logic.

### Pitfall 6: DAW Plugin Cache Stale After Rebuild

**What goes wrong:** After rebuilding the plugin with changes, the DAW still loads the old version.
**Why it happens:** DAWs cache plugin metadata and sometimes the plugin binary itself. Simply overwriting the .vst3 file may not trigger a rescan.
**How to avoid:** Force DAW to rescan plugins after each rebuild:
- **Ableton:** Options > Preferences > Plug-Ins > Rescan
- **FL Studio:** Options > Manage plugins > Rescan
- **Reaper:** Options > Preferences > Plug-Ins > VST > Re-scan
- **Logic:** Logic automatically rescans on launch; restart Logic after rebuild
**Warning signs:** Changes to bus layout or metadata don't take effect.

## Code Examples

### Complete PluginProcessor.h Skeleton

```cpp
// Source: JUCE AudioProcessor API + project architecture research
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class CanaryProcessor : public juce::AudioProcessor
{
public:
    CanaryProcessor();
    ~CanaryProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Cross-thread flag for sidechain status (audio thread writes, GUI reads)
    std::atomic<bool> sidechainConnected { false };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanaryProcessor)
};
```

### Build and Test Commands

```bash
# First build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Ad-hoc code signing (macOS Ventura+ requirement)
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Canary.vst3

# Verify the .vst3 bundle exists
ls -la ~/Library/Audio/Plug-Ins/VST3/Canary.vst3
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Projucer for project setup | CMake with `juce_add_plugin()` | JUCE 6 (2020) | CMake is now the only recommended approach for new projects |
| Separate VST3 SDK download | JUCE bundles VST3 SDK internally | JUCE 5.4+ | No need to download or configure Steinberg SDK separately |
| VST2 format | VST3 format | Steinberg discontinued VST2 licensing (2018) | Set `JUCE_VST3_CAN_REPLACE_VST2=0` |
| JUCE 7.x | JUCE 8.0.12 | 2024 | JUCE 8 is the current major release line; use latest 8.0.x |

## Open Questions

1. **Logic Pro VST3 sidechain support**
   - What we know: Logic primarily uses AU format. VST3 sidechain support in Logic exists but may be limited.
   - What's unclear: Whether Logic Pro's current version reliably shows VST3 sidechain routing.
   - Recommendation: Test VST3 in Logic during Phase 1. If sidechain fails, add AU format (`FORMATS VST3 AU`) as a follow-up task. Do not block Phase 1 on this.

2. **Sidechain bus default enabled state**
   - What we know: The official JUCE NoiseGate example omits the third parameter (defaults to `true`). Community advice suggests `false` for better Ableton compatibility.
   - What's unclear: Whether `true` vs `false` actually affects modern Ableton Live (11/12).
   - Recommendation: Use `false` (disabled by default) as the safer choice. If sidechain doesn't appear in a specific DAW, try `true` as a fallback.

3. **Manufacturer code uniqueness**
   - What we know: PLUGIN_MANUFACTURER_CODE must be globally unique for distribution.
   - What's unclear: What code the team wants to use.
   - Recommendation: Use "Cnry" (for Canary) as placeholder. Can be changed before distribution.

## Validation Architecture

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Manual DAW testing (no automated unit tests for Phase 1) |
| Config file | None -- Phase 1 is pure integration testing in DAWs |
| Quick run command | `cmake --build build --config Release` |
| Full suite command | Build + sign + load in all 4 DAWs |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| INFRA-01 | Plugin compiles and loads in DAWs | manual | `cmake --build build --config Release && codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Canary.vst3` | N/A -- build system test |
| INFRA-02 | Sidechain routing option appears in DAW | manual | N/A -- requires visual DAW inspection | N/A -- manual test |
| INFRA-03 | Audio passes through unmodified | manual | N/A -- requires audio playback in DAW | N/A -- manual test |

### Sampling Rate

- **Per task commit:** Build and verify .vst3 is produced
- **Per wave merge:** Load in at least one DAW and verify sidechain appears
- **Phase gate:** All four DAWs tested with documented results

### Wave 0 Gaps

- [ ] `Canary/CMakeLists.txt` -- entire build system (does not exist yet)
- [ ] `Canary/src/PluginProcessor.h` -- processor header
- [ ] `Canary/src/PluginProcessor.cpp` -- processor implementation with bus config
- [ ] `Canary/src/PluginEditor.h` -- editor header
- [ ] `Canary/src/PluginEditor.cpp` -- minimal editor implementation

Note: Phase 1 is entirely about creating these files from scratch. There is no existing code in the repository -- only documentation in the Docs/ directory.

## DAW-Specific Sidechain Validation Steps

These are the exact steps to verify sidechain routing works in each target DAW after the plugin is built:

### Ableton Live
1. Insert Canary on a vocal/audio track
2. Click the plugin's wrench icon (or the sidechain panel toggle)
3. In the sidechain section, verify a dropdown appears listing other tracks
4. Select a synth/pad track as the sidechain source
5. Play both tracks -- verify the main audio passes through (Phase 1 passthrough)

### FL Studio
1. Load Canary on a mixer insert
2. Route another mixer insert (synth/pad track) to the plugin's sidechain input via the mixer routing matrix
3. Verify the sidechain signal arrives (in later phases; for Phase 1, verify the routing option exists)

### Reaper
1. Insert Canary on a vocal track
2. Open the plugin's I/O routing (click the "2 in 2 out" button on the FX panel)
3. Verify the plugin shows 4 input pins (2 main + 2 sidechain)
4. Route a send from a synth track to the plugin's inputs 3-4

### Logic Pro
1. Insert Canary (VST3) on an audio track
2. Look for sidechain dropdown in the plugin header bar
3. If absent, this confirms AU format is needed for full Logic sidechain support
4. Document the result either way

## Sources

### Primary (HIGH confidence)
- [JUCE GitHub Releases](https://github.com/juce-framework/JUCE/releases) - Verified JUCE 8.0.12 as latest stable (Dec 16, 2025)
- [JUCE CMake API docs](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md) - `juce_add_plugin()` options, minimum CMake 3.22
- [JUCE Plugin Examples](https://juce.com/tutorials/tutorial_plugin_examples) - NoiseGate sidechain example pattern
- [JUCE Bus Layouts Tutorial](https://juce.com/tutorials/tutorial_audio_bus_layouts/) - BusesProperties, isBusesLayoutSupported

### Secondary (MEDIUM confidence)
- [JUCE Forum - Sidechain discussions](https://forum.juce.com/t/sidechain-and-processcontextreplacing/44327) - Community patterns for sidechain default enabled state
- Project research files: STACK.md, ARCHITECTURE.md, PITFALLS.md (training-data-based, pre-verified against official docs where possible)

### Tertiary (LOW confidence)
- DAW-specific sidechain behavior details (varies by DAW version; must be validated hands-on)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - JUCE 8.0.12 version verified against GitHub releases; CMake API verified against official docs
- Architecture: HIGH - Sidechain pattern verified against official JUCE NoiseGate example and tutorials
- Pitfalls: HIGH - Sidechain declaration in two places is the most well-documented JUCE sidechain failure mode
- DAW compatibility: MEDIUM - General patterns are stable but DAW-specific behavior varies by version

**Research date:** 2026-03-20
**Valid until:** 2026-04-20 (JUCE release cadence is ~monthly; check for 8.0.13+ before starting)
