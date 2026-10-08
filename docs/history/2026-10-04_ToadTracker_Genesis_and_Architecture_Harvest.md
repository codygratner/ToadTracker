# 🐸 Subagent Soul Harvest: ToadTracker Genesis & Architecture (v0.1.0 – v0.2.0)

**Date of Record:** 2026-10-04 (Harvested 2026-10-08)  
**Project:** ToadTracker ("The Toad")  
**Iconography / Mascot:** Tsathoggua (The Sleeper of N'kai)  
**License:** GNU General Public License v3.0 (GPLv3)  
**Source Conversation ID:** `3883592a-f4a6-4e2a-a11c-8e822ecfc51c` (1,678 steps, 20 explicit user directions)  
**Repository:** `c:\Dev\ToadTracker` (GitHub: `https://github.com/codygratner/ToadTracker`)  
**Target Environments:** 
- dadamachines TBD-16 (ESP32-P4 400MHz Audio DSP + RP2350 150MHz UI + ESP32-C6 WiFi, 128×64 mono OLED, 4 encoders, 30 RGB buttons)
- Valve Steam Deck (SteamOS / Linux x86_64 handheld)
- Raspberry Pi (Linux ARMv7 / AArch64)
- Desktop Standalone & VST3 (Windows / macOS / Linux via JUCE 8/9)

---

## 1. Executive Summary: The Genesis of "The Toad"

ToadTracker was born from the desire to create a modern, ergonomic, multi-platform musical tracker combining the legendary handheld ergonomics of **Dirtywave M8** and **Little Sound Dj (LSDj)** with **Renoise**-style algorithmic sequencing and high-fidelity native synthesis.

Over a single continuous engineering marathon (1,678 execution steps), the foundational architecture was drafted, verified against real hardware constraints, split into a 6-phase development pipeline, implemented in modern C++20 with 100% zero-allocation audio routines, and tagged for release at **v0.2.0**.

---

## 2. Non-Negotiable Core Engineering Invariants

The project enforces 6 strict invariants in `TRACKER_SPEC.md`:

1. **Zero Dynamic Allocation in Hot Audio Paths**: `malloc`, `free`, `new`, `delete`, and dynamic container resizing (`std::vector::push_back`, `std::string`) are strictly prohibited inside all audio render callbacks, table tick processors, and DSP loops. All memory pools, delay lines, and voice buffers are statically pre-allocated or reserved at boot time (`prepareToPlay`).
2. **Lock-Free Concurrency**: Inter-thread communication between UI tasks (Core 0 / GUI faceplate) and the real-time Audio Engine (Core 1 / audio callback) exclusively utilizes Single-Producer Single-Consumer (SPSC) lock-free ring buffers or atomic primitives (`std::atomic`). Mutexes, critical sections, and locks are strictly banned from audio code.
3. **Decoupled Architecture**: Pure C++20 core engine logic (`/core`) remains completely isolated from platform abstraction layers (`/hal`). The core contains zero platform-specific headers (no ESP-IDF, no JUCE, no Win32/POSIX audio calls).
4. **Strict Hexadecimal Cell Representation**: All pattern cells, table values, velocities, modulation offsets, and command arguments are represented and manipulated as two-digit hexadecimal bytes (`00`–`FF` or `00`–`7F`). Decimal is reserved strictly for human tempo (BPM), groove multipliers, and audio sample rates.
5. **Deterministic Sample-Accurate Timing**: Sequencer subdivisions, groove patterns, primary tables, and auxiliary tables advance strictly via sample counting. System wall-clocks (`juce::Timer`, `std::chrono`) never drive sequencing math.
6. **Master Output Protection**: A zero-latency, branchless rational saturation stage (`fastTanh`) is hardwired immediately prior to the physical DAC buffer and VST audio bus to guarantee that excessive resonance or wavefolding never clips beyond $0\text{ dBFS}$ ($1.0\text{f}$).

---

## 3. Hardware Architecture: The dadamachines TBD-16 Reality Check

A critical milestone during setup was vetting the target embedded hardware platform:
- **Corrected Display Spec**: Initial early sketches assumed a 240×240 color LCD. Codebase research across `dadamachines/ctag-tbd` confirmed the production specification: **2.4-inch monochrome OLED display with 128 × 64 pixels**.
- **Tri-Core Compute Architecture**:
  - **ESP32-P4 (400 MHz RISC-V)**: Audio DSP engine, synth synthesis, filter algorithms, DMA buffer transfers.
  - **RP2350 (150 MHz ARM Cortex-M33 / RISC-V)**: UI state machine, button debounce, encoder polling, sequencer timing.
  - **ESP32-C6 (160 MHz RISC-V)**: Low-power wireless sync, Ableton Link, MIDI over BLE / WiFi.
- **Physical Controls**: 30 tactile RGB backlit buttons, 4 endless push-encoders (Knobs 1–4) + dedicated volume wheel, 19 RGB status LEDs, and center-left D-Pad.

---

## 4. The 6-Phase Engineering Pipeline (v0.1.0 ➔ v0.2.0)

The entire foundational build was sequenced across 6 phases:

### Phase 1: Core Foundation & Types (`commit 246560e`)
- Pure C++20 core types (`Note`, `Step`, `HexByte`, `SPSCQueue`).
- Branchless `fast_math` approximations.
- Automated Catch2 test harness running in headless CI.

### Phase 2: Deterministic Sequencer & Step Engine (`commit 57e450d`)
- 8 monophonic tracks with sample-accurate step trigger pipeline.
- Dual-table modulation engines per voice running at audio rate or sub-tick tick rate.
- Micro-timing groove templates and swing accumulators.

### Phase 3: DSP Synthesis Engine (`commit 883cf9a`)
- **Alpha Juno Engine**: Modeled after Roland IR3R05 dual 2-pole cascaded SVF with localized resonance feedback (preserving low-end punch without bass dropout).
- **Wavetable Oscillator**: Bandlimited wavetables with linear interpolation and anti-aliasing.
- **UniversalFilter**: Zero-Delay Feedback (ZDF) State Variable Filter with Lowpass, Bandpass, Highpass, and Notch modes.
- **SoundFont Player (SF2)**: Sample-playback voice engine with linear envelope generators.
- **Master Stage**: Soft-clipping rational saturation (`fastTanh`).

### Phase 4: Virtual TBD-16 & Display Engine (`commit 899f8ad`)
- Hardware-accurate 128×64 pixel 1-bit OLED framebuffer renderer (`DisplayEngine`).
- Virtual TBD-16 faceplate emulator in JUCE with clickable RGB tactile buttons and knobs.
- Valve Steam Deck handheld harness with custom gamepad mappings.

### Phase 5: Hardware Abstraction Layer (`commit fe76cf0`)
- **ESP32 Target**: FreeRTOS dual-core task pin (Core 0: UI/USB; Core 1: I2S DMA Audio).
- **Raspberry Pi Linux ARM Target**: ALSA low-latency audio driver, DirectFB / DRM framebuffer blit.

### Phase 6: Serialization & Interchange Bridge (`commit 9e0ac92`)
- Compact binary song and patch serialization (`.toad`).
- `BakeInstrument` automated multi-velocity sample baker.
- **Dirtywave M8 Format Bridge**: Importer converting `.m8s` song files into native ToadTracker structures.

---

## 5. UI Ergonomics & Desktop Evolution

Following Phase 6, user feedback drove a major UI overhaul:
1. **Standalone Windows Desktop Runner (`commit a765047`)**: Built `ToadTracker.exe` (independent from the TBD-16 hardware emulator) for clean desktop production.
2. **Settings Page & Key Remapping (`commit 6610f12`)**:
   - In-engine configuration modal with persistent key bindings.
   - Built-in `m8.run` layout preset (mapping `A` and `S` keys as Left/Right page navigation bumpers).
   - Musical PC Keyboard: White keys mapped to `QWERTYUIOP[]` starting from C, black keys mapped to numbers `2, 3, 5, 6, 7, 9, 0, =`.
3. **2D Navigation Map (`commit 5859d86`)**:
   - Replaced flat top tab bar with an LSDj / M8 / LittleGPTracker-style 2D navigation matrix (`Song` $\leftrightarrow$ `Chain` $\leftrightarrow$ `Phrase` $\leftrightarrow$ `Instrument` $\leftrightarrow$ `Table` $\leftrightarrow$ `Groove` $\leftrightarrow$ `FX`).
   - Added corner mini-map overlay providing instant spatial orientation.

---

## 6. Active Roadmap: Backlog Roster (Items 1–4)

Documented in `c:\Dev\ToadTracker\BACKLOG.md`:

| ID | Title | Scope | Target |
| :--- | :--- | :--- | :--- |
| **BACKLOG-01** | MIDI In/Out & Clock Sync | DIN MIDI on TBD-16, USB-MIDI Class Compliant on PC/Deck, Clock Slave/Master. | v0.3.0 |
| **BACKLOG-02** | USB Storage & Preset Manager | FAT32/exFAT disk access on ESP32 USB-OTG and SD card filesystem. | v0.3.0 |
| **BACKLOG-03** | Sample Slicing & Playback Engine | Transient detection, slice markers, forward/reverse/ping-pong looping. | v0.4.0 |
| **BACKLOG-04** | Instantaneous Hold-to-View Navigation Modal | Holding Left-Shift (or Gamepad Back) temporarily displays 2D navigation map; releasing returns to active screen. | v0.3.0 |

---

## 7. Synergy with The Klang Suite & Next Steps

This harvest cements the shared lineage between **The Klang Suite** and **ToadTracker**:
1. **Shared Filter Lineage**: ToadTracker's Alpha Juno IR3R05 dual 2-pole filter directly informed The Klang Suite's filter DSP research.
2. **Decoupled DSP Engine (`libtks_dsp`)**: The Klang Suite will extract its synth voice modules into a pure C++20 static library (`libtks_dsp`) matching ToadTracker's zero-allocation, zero-JUCE `/core` architecture.
3. **Hardware Parity**: Both projects target the dadamachines TBD-16 hardware interface (4 encoders, 128×64 display) with seamless desktop-to-hardware migration.

---
*Archived permanently into institutional memory across The Klang Suite, ToadTracker, and The Klang Vault.*
