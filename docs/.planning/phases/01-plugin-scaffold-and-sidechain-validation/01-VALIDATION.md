---
phase: 1
slug: plugin-scaffold-and-sidechain-validation
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-20
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Manual DAW testing (no automated unit tests for Phase 1) |
| **Config file** | None — Phase 1 is pure integration testing in DAWs |
| **Quick run command** | `cmake --build build --config Release` |
| **Full suite command** | Build + sign + load in all 4 DAWs |
| **Estimated runtime** | ~2–5 minutes per DAW |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --config Release` — verify .vst3 is produced
- **After every plan wave:** Load in at least one DAW and verify sidechain option appears
- **Before `/gsd:verify-work`:** All four DAWs tested with sidechain visible and passthrough confirmed
- **Max feedback latency:** Build must succeed within 60 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 1-01-01 | 01 | 1 | INFRA-01 | build | `cmake --build build --config Release` | ❌ W0 | ⬜ pending |
| 1-01-02 | 01 | 1 | INFRA-02 | manual | Load in Ableton, verify sidechain dropdown | ❌ W0 | ⬜ pending |
| 1-01-03 | 01 | 1 | INFRA-03 | manual | Route audio, verify passthrough unmodified | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

All files below are created from scratch in this phase (no existing code in repo):

- [ ] `Canary/CMakeLists.txt` — entire build system
- [ ] `Canary/src/PluginProcessor.h` — processor header with bus config
- [ ] `Canary/src/PluginProcessor.cpp` — processor with sidechain bus declaration
- [ ] `Canary/src/PluginEditor.h` — editor header
- [ ] `Canary/src/PluginEditor.cpp` — minimal editor implementation

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Sidechain routing option appears in DAW | INFRA-02 | Requires visual DAW inspection | 1. Insert Canary on a track. 2. Open sidechain routing panel. 3. Verify dropdown lists other tracks. |
| Audio passes through unmodified | INFRA-03 | Requires audio playback in DAW | 1. Route audio to plugin. 2. Play audio. 3. Confirm output matches input with no artifacts. |
| Plugin loads in Ableton Live | INFRA-01 | DAW integration test | Insert on audio track, verify no error on load, verify sidechain dropdown appears |
| Plugin loads in FL Studio | INFRA-01 | DAW integration test | Load on mixer insert, verify sidechain routing matrix shows plugin inputs |
| Plugin loads in Reaper | INFRA-01 | DAW integration test | Add to FX chain, verify sidechain input channels 3-4 available |
| Plugin loads in Logic Pro | INFRA-01 | DAW integration test | Insert on Audio FX slot, verify sidechain dropdown in plugin header bar |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: build passes after each task
- [ ] Wave 0 covers all MISSING references (all files created from scratch)
- [ ] No watch-mode flags
- [ ] Feedback latency < 60s (build time)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
