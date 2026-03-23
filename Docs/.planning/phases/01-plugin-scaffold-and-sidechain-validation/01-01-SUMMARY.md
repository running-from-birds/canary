---
phase: 01-plugin-scaffold-and-sidechain-validation
plan: 01
subsystem: infra
tags: [juce, vst3, cmake, fetchcontent, sidechain, audio-plugin, cpp17]

# Dependency graph
requires:
  - phase: none
    provides: first phase, no dependencies
provides:
  - CMake build system with JUCE 8.0.12 FetchContent
  - VST3 plugin scaffold with sidechain bus configuration
  - Passthrough processBlock with sidechain guard
  - Minimal plugin editor
  - Ad-hoc signed .vst3 bundle installed to ~/Library/Audio/Plug-Ins/VST3/
affects: [01-02-PLAN, phase-02-vocoder-dsp-engine]

# Tech tracking
tech-stack:
  added: [JUCE 8.0.12, CMake 3.24+, C++17, AppleClang 17]
  patterns: [FetchContent for JUCE, BusesProperties sidechain declaration, isBusesLayoutSupported dual-site validation]

key-files:
  created:
    - Canary/CMakeLists.txt
    - Canary/src/PluginProcessor.h
    - Canary/src/PluginProcessor.cpp
    - Canary/src/PluginEditor.h
    - Canary/src/PluginEditor.cpp
    - Canary/.gitignore
  modified: []

key-decisions:
  - "Added C language to CMakeLists.txt LANGUAGES for CMake 4.x compatibility (JUCE enables C internally)"
  - "Sidechain bus default enabled = false for Ableton compatibility"
  - "Used Unix Makefiles generator (Xcode generator incompatible with current toolchain)"

patterns-established:
  - "Sidechain declared in BOTH BusesProperties constructor AND isBusesLayoutSupported()"
  - "Disabled sidechain accepted via isDisabled() check in isBusesLayoutSupported()"
  - "getBusBuffer(buffer, true, 1) pattern for sidechain access with getNumChannels() guard"
  - "Ad-hoc code signing after build for macOS compatibility"

requirements-completed: [INFRA-01, INFRA-02, INFRA-03]

# Metrics
duration: 10min
completed: 2026-03-21
---

# Phase 1 Plan 1: Plugin Scaffold Summary

**VST3 plugin scaffold with JUCE 8.0.12 FetchContent, sidechain bus in BusesProperties and isBusesLayoutSupported, passthrough processBlock, and minimal editor -- builds and installs to VST3 folder**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-21T03:19:37Z
- **Completed:** 2026-03-21T03:29:44Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- CMake build system fetching JUCE 8.0.12 via FetchContent, producing a VST3 bundle
- Sidechain bus correctly declared in both BusesProperties constructor and isBusesLayoutSupported
- Passthrough processBlock with ScopedNoDenormals and sidechain guard (getBusBuffer + getNumChannels check)
- Plugin built, installed to ~/Library/Audio/Plug-Ins/VST3/, and ad-hoc code signed

## Task Commits

Each task was committed atomically:

1. **Task 1: Create CMakeLists.txt and PluginProcessor** - `73be187` (feat)
2. **Task 2: Create PluginEditor and verify full build** - `1353bdc` (feat)

## Files Created/Modified
- `Canary/CMakeLists.txt` - JUCE CMake build config with FetchContent, VST3 format, Canary metadata
- `Canary/src/PluginProcessor.h` - AudioProcessor subclass with sidechainConnected atomic flag
- `Canary/src/PluginProcessor.cpp` - Bus config, isBusesLayoutSupported, passthrough processBlock with sidechain guard
- `Canary/src/PluginEditor.h` - Minimal editor declaration
- `Canary/src/PluginEditor.cpp` - Dark background editor with centered "Canary" label
- `Canary/.gitignore` - Excludes build/ directory

## Decisions Made
- Added C language to CMakeLists.txt `LANGUAGES C CXX` because CMake 4.x (installed via Homebrew) requires explicit C language declaration when JUCE internally enables C compilation. Without this, CMake generation fails with missing CMAKE_C_COMPILE_OBJECT.
- Used Unix Makefiles generator instead of Xcode because Xcode generator reported "Xcode 1.5 not supported" (version detection issue with current toolchain).
- Sidechain bus default enabled set to `false` per research recommendation for better Ableton compatibility.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] CMake 4.x requires C language declaration**
- **Found during:** Task 2 (build step)
- **Issue:** CMake 4.3.0 (latest Homebrew version) fails generation with "Missing variable: CMAKE_C_COMPILE_OBJECT" when project declares only CXX but JUCE internally enables C
- **Fix:** Changed `LANGUAGES CXX` to `LANGUAGES C CXX` in CMakeLists.txt
- **Files modified:** Canary/CMakeLists.txt
- **Verification:** cmake configure completes successfully
- **Committed in:** 1353bdc (Task 2 commit)

**2. [Rule 3 - Blocking] CMake not installed**
- **Found during:** Task 2 (build step)
- **Issue:** cmake binary not available on PATH or in Homebrew
- **Fix:** Installed cmake via `brew install cmake`
- **Files modified:** none (system tool installation)
- **Verification:** `/opt/homebrew/bin/cmake --version` returns 4.3.0

---

**Total deviations:** 2 auto-fixed (2 blocking)
**Impact on plan:** Both auto-fixes necessary to complete the build. The LANGUAGES change is a minor CMakeLists.txt adjustment that doesn't affect functionality. No scope creep.

## Issues Encountered
- CMake 4.3.0 (latest Homebrew) has compatibility issues with JUCE's cmake integration. Required adding C to LANGUAGES declaration and using Unix Makefiles generator instead of Xcode.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All 5 source files exist and compile into a working VST3 plugin
- .vst3 bundle installed and ad-hoc signed at ~/Library/Audio/Plug-Ins/VST3/Canary.vst3
- Ready for 01-02-PLAN: DAW sidechain validation (human checkpoint to verify sidechain appears in DAWs)
- Sidechain bus configuration follows all best practices from research (dual declaration, disabled acceptance, getBusBuffer guard)

## Self-Check: PASSED

All source files verified present. Both task commits found in git log. VST3 bundle exists at install location.

---
*Phase: 01-plugin-scaffold-and-sidechain-validation*
*Completed: 2026-03-21*
