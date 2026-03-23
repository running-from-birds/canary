# Technology Stack

**Project:** Canary (VST3 Vocoder Plugin)
**Researched:** 2026-03-20
**Overall Confidence:** MEDIUM -- versions verified against training data through May 2025; live source verification was unavailable. Verify JUCE version against https://github.com/juce-framework/JUCE/releases before starting.

## Recommended Stack

### Core Framework

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| JUCE | 8.0.x (latest 8.x) | Audio plugin framework | Industry standard for cross-platform audio plugins. CMake-first since JUCE 6. JUCE 8 is the current major line with active development. Use the latest 8.0.x patch release. | MEDIUM |
| C++ | C++17 | Language standard | JUCE 7+ requires C++17 minimum. C++20 is usable but not required and adds no critical benefit for this project. C++17 gives structured bindings, std::optional, and if-constexpr which are all useful. | HIGH |
| CMake | 3.22+ (recommend 3.24+) | Build system | JUCE's CMake API requires 3.22 minimum. Use 3.24+ for better presets support and FetchContent improvements. | HIGH |

### VST3 SDK

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| VST3 SDK | Bundled with JUCE | Plugin format SDK | JUCE bundles its own VST3 SDK headers and handles all VST3 wrapping internally. Do NOT download the Steinberg VST3 SDK separately -- JUCE's `juce_add_plugin()` handles everything. Set `JUCE_VST3_CAN_REPLACE_VST2 OFF` in CMake. | HIGH |

### Build Toolchain

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Xcode / Apple Clang | Xcode 15+ (Apple Clang 15+) | macOS compiler | Required for macOS builds. Xcode command line tools are sufficient for CLI builds; full Xcode IDE optional. Targets macOS 10.15+ (Catalina) for broad compatibility. | HIGH |
| MSVC | Visual Studio 2022 (v17.x) | Windows compiler | The standard Windows toolchain for JUCE. MinGW is not recommended -- JUCE's CMake integration targets MSVC, and many DAW hosts expect MSVC-compiled binaries. | HIGH |
| GCC / Clang | GCC 11+ or Clang 14+ | Linux compiler | For Linux builds if needed. Lower priority since the project targets VST3 in major DAWs. | HIGH |

### JUCE Modules Required

| Module | Purpose | Why Needed |
|--------|---------|------------|
| `juce_audio_processors` | Plugin hosting/wrapping, AudioProcessor base class, APVTS | Core of any JUCE plugin. Provides processBlock(), bus management, parameter system. Non-negotiable. |
| `juce_audio_basics` | AudioBuffer, MidiBuffer, sample rate utilities | Foundation types used everywhere. Dependency of audio_processors. |
| `juce_audio_utils` | AudioProcessorEditor helpers | Simplifies editor creation. Provides GenericAudioProcessorEditor for rapid prototyping. |
| `juce_audio_formats` | Audio format reading (optional) | Not strictly needed for a pure effect plugin, but pulled in transitively. |
| `juce_dsp` | IIR filters, ProcessorChain, SmoothedValue, ProcessSpec | Critical for the vocoder. Provides `juce::dsp::IIR::Filter`, `ProcessorDuplicator`, `ProcessSpec`. This is where the filter bank lives. |
| `juce_gui_basics` | Component, LookAndFeel, Slider, Label | Required for the custom UI with knobs and band visualizer. |
| `juce_gui_extra` | Optional -- extra GUI components | Only if needed for advanced UI features. Can skip initially. |
| `juce_graphics` | 2D rendering, Path, Colour | Required for the band visualizer rendering. Dependency of gui_basics. |
| `juce_core` | Strings, Files, XML, base utilities | Foundation module. Everything depends on it. |
| `juce_data_structures` | ValueTree, UndoManager | Required by APVTS (AudioProcessorValueTreeState). |
| `juce_events` | Timer, MessageManager, async callbacks | Required for GUI timer-based visualizer refresh (~30fps). |

**Modules to NOT include:**

| Module | Why Skip |
|--------|----------|
| `juce_audio_devices` | For standalone apps only. VST3 host provides the audio device. |
| `juce_midi_ci` | MIDI Capability Inquiry -- not needed, no MIDI in this plugin. |
| `juce_opengl` | Overkill for a simple 2D band visualizer. Software rendering is fine at 450x380px. |
| `juce_osc` | OSC networking -- irrelevant. |
| `juce_video` | Video playback -- irrelevant. |
| `juce_analytics` | Telemetry -- not appropriate. |

### Supporting Libraries

| Library | Purpose | When to Use | Confidence |
|---------|---------|-------------|------------|
| Catch2 or GoogleTest | Unit testing DSP logic | Test filter bank coefficients, envelope follower math, parameter ranges outside the plugin host | MEDIUM |
| FRUT (optional) | Projucer-to-CMake migration | Do NOT use -- starting fresh with CMake, no migration needed | HIGH |

## CMake Integration Pattern

### Use FetchContent (recommended over git submodules)

FetchContent is the modern CMake pattern for pulling JUCE. It pins to a specific git tag, is reproducible, and requires no manual submodule management.

```cmake
cmake_minimum_required(VERSION 3.24)

project(Canary VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0  # Pin to specific release tag -- verify latest at GitHub
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Canary
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Mfgr
    PLUGIN_CODE CVoc
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
        src/Vocoder.cpp
        src/FilterBank.cpp
        src/EnvelopeFollower.cpp
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
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags
)
```

### Why FetchContent over Git Submodules

| Approach | Recommendation | Rationale |
|----------|----------------|-----------|
| FetchContent | USE THIS | Clean, version-pinned, no submodule state to manage, works with CI/CD out of the box |
| Git submodule | Acceptable alternative | Works but adds git submodule update steps. Use if you prefer vendoring deps in-repo. |
| System-installed JUCE | DO NOT USE | Version mismatches, not reproducible across machines |
| Projucer | DO NOT USE | Legacy tool. JUCE team recommends CMake for all new projects since JUCE 6. Projucer generates non-portable build files and doesn't support modern CMake features. |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Framework | JUCE 8.x | iPlug2 | iPlug2 is lighter but smaller community, fewer DSP primitives, less DAW testing coverage. JUCE is the industry standard. |
| Framework | JUCE 8.x | DPF (DISTRHO) | Good for simple plugins but lacks the DSP module richness (IIR filters, ProcessorChain) that a vocoder needs. |
| Build | CMake + FetchContent | Projucer | Legacy. Generates IDE projects instead of using CMake natively. JUCE's own docs recommend CMake. |
| Build | CMake + FetchContent | Meson / Bazel | No first-party JUCE support. CMake is the only officially supported build system. |
| Language | C++17 | Rust (via nih-plug) | nih-plug is promising but immature ecosystem, harder to find vocoder DSP references, JUCE's C++ DSP module provides exactly what's needed. |
| Plugin Format | VST3 only | VST3 + AU + AAX | AU (Audio Units) for Logic can be added later with one line in CMake (`FORMATS VST3 AU`). AAX requires iLok signing. Start with VST3 to keep scope tight. |

## Platform-Specific Notes

### macOS
- **Minimum deployment target:** macOS 10.15 (Catalina) -- set via `CMAKE_OSX_DEPLOYMENT_TARGET`
- **Code signing:** Required for macOS Ventura+ to load unsigned plugins in some DAWs. For dev testing, use `codesign --force --deep --sign - Canary.vst3`
- **Universal binaries:** Build for both arm64 and x86_64 with `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for distribution
- **Install path:** `~/Library/Audio/Plug-Ins/VST3/`
- **AU format:** Adding `AU` to FORMATS later gives Logic Pro native support

### Windows
- **Use MSVC (Visual Studio 2022)** -- not MinGW. JUCE and most DAW plugin APIs expect MSVC ABI.
- **Install path:** `C:\Program Files\Common Files\VST3\`
- **cmake -G:** Use `"Visual Studio 17 2022"` generator or Ninja with MSVC toolchain

### Linux
- **Lower priority** -- few commercial DAWs on Linux (Reaper and Bitwig are the main ones)
- **Dependencies:** `libasound2-dev`, `libfreetype6-dev`, `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libgl1-mesa-dev`

## Key CMake Variables and Flags

| Variable | Value | Purpose |
|----------|-------|---------|
| `JUCE_WEB_BROWSER` | 0 | Disable web browser component (unneeded, avoids WebKit dependency) |
| `JUCE_USE_CURL` | 0 | Disable CURL (unneeded, avoids libcurl dependency) |
| `JUCE_VST3_CAN_REPLACE_VST2` | 0 | No VST2 compatibility needed |
| `JUCE_DISPLAY_SPLASH_SCREEN` | 0 | Valid for GPLv3 use or with a JUCE license |
| `COPY_PLUGIN_AFTER_BUILD` | TRUE | Auto-copies .vst3 to system plugin dir for immediate DAW testing |

## JUCE Licensing Note

JUCE has dual licensing:
- **GPLv3** -- free, but your plugin source must also be GPLv3
- **Commercial license** -- required for closed-source distribution, starts at free tier for revenue under $50k/year (Personal license)

For development and learning, GPLv3 is fine. If distributing commercially, acquire the appropriate license from https://juce.com/get-juce/.

## Installation / First Build

```bash
# macOS prerequisites
xcode-select --install  # Xcode command line tools
brew install cmake       # or download from cmake.org

# Clone project and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# The .vst3 bundle will be copied to ~/Library/Audio/Plug-Ins/VST3/ automatically
# Open your DAW and scan for new plugins
```

## Version Verification Checklist

Before starting development, verify these versions are current:

- [ ] Check latest JUCE release: https://github.com/juce-framework/JUCE/releases
- [ ] Update the `GIT_TAG` in FetchContent to match
- [ ] Confirm CMake minimum version in JUCE's own CMakeLists.txt
- [ ] Check if JUCE 8.x introduced breaking changes vs 7.x (API is stable but worth scanning changelog)

## Sources

- JUCE GitHub repository: https://github.com/juce-framework/JUCE (primary source)
- JUCE CMake API documentation: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- JUCE module documentation: https://docs.juce.com/master/modules.html
- JUCE tutorials (AudioPlugin): https://docs.juce.com/master/tutorial_create_projucer_basic_plugin.html
- Confidence note: Versions are based on training data through May 2025. JUCE 8.0.x was the current major release line as of that date. Verify against live GitHub releases before pinning a version.
