# ToadTracker ("The Toad") - Feature Backlog & Roadmap

This document tracks prioritized feature specifications, architectural requirements, and acceptance criteria for upcoming ToadTracker engineering cycles.

---

## Prioritized Feature Backlog

| Priority | ID | Feature | Target Scope | Status |
| :---: | :---: | :--- | :--- | :---: |
| **P1** | **BACKLOG-01** | [Dedicated Project Screen (`VIEW_PROJECT`) & File Operations](#backlog-01-dedicated-project-screen-view_project--file-operations) | `core`, `desktop`, `hal` | **Ready** |
| **P2** | **BACKLOG-02** | [Dedicated Instrument Screen (`VIEW_INSTRUMENT`) & Parameter Sliders](#backlog-02-dedicated-instrument-screen-view_instrument--parameter-sliders) | `core`, `desktop`, `hal` | **Ready** |
| **P3** | **BACKLOG-03** | [Offline Song WAV Exporter (`.wav`)](#backlog-03-offline-song-wav-exporter-wav) | `core`, `desktop`, `cli` | **Ready** |

---

### BACKLOG-01: Dedicated Project Screen (`VIEW_PROJECT`) & File Operations

#### 1. Overview & Objective
In the 3x3 Spatial Navigation Map, cell `[0, 0]` is dedicated to `VIEW_PROJECT` (Meta Tier). Currently, `VIEW_PROJECT` falls back to rendering the default Song view in [`core/include/toad/display_engine.h`](core/include/toad/display_engine.h). This task implements a dedicated, authentic Project screen for project-wide metadata, scale quantization, and atomic disk I/O operations.

#### 2. Functional Requirements
- **Project Metadata Display & Editing:**
  - Song Title / Name (8–16 ASCII chars)
  - Master BPM (`song.bpm`, 32..255)
  - Groove / Steps Per Beat (`song.steps_per_beat`, 1..16)
  - Master Transpose (`song.master_transpose`, -24..+24 semitones)
  - FastTanh Saturation Drive / Master Level (`song.master_volume`)
- **Scale Quantizer Configuration:**
  - Root Note (`ScaleDefinition.root_note`: C, C#, D, ..., B)
  - Active Scale Preset (`ScaleDefinition.note_mask`):
    - Chromatic (All 12 semitones)
    - Major / Ionian (`101011010101b`)
    - Natural Minor / Aeolian (`101101011010b`)
    - Harmonic Minor (`101101011001b`)
    - Dorian (`101101010110b`)
    - Pentatonic Major (`101010010100b`)
    - Pentatonic Minor (`100101010010b`)
    - Blues (`100111110010b`)
    - Hirajoshi (`101100011000b`)
  - Snap Mode (`SNAP_DOWN`, `SNAP_UP`, `SNAP_NEAREST`)
  - Enable/Bypass toggle
- **Interactive Disk Operations:**
  - **Save Project:** Atomically serializes the active `Song` struct via [`Serializer::saveSong`](core/include/toad/serializer.h) to `song.tts` (with `.tmp` atomic replacement).
  - **Load Project:** Reads and deserializes `song.tts` via [`Serializer::loadSong`](core/include/toad/serializer.h), hot-reloading the song into [`Engine`](core/include/toad/engine.h).
  - **New Project / Reset:** Resets `Song` to clean default template with confirmation prompt.
- **Desktop Keyboard Shortcuts:**
  - `Ctrl + S`: Quick-save project to active file (`song.tts`).
  - `Ctrl + O`: Quick-load project from active file.
  - `Ctrl + N`: New blank project.

#### 3. Acceptance Criteria
- [ ] Dedicated `renderProjectView()` implemented in `DisplayEngine` displaying Project Title, BPM, Scale, and File Ops.
- [ ] Cursor navigation and value editing for all project fields.
- [ ] Save and Load actions invoke `Serializer` atomically without audio hiccups.
- [ ] `Ctrl+S`, `Ctrl+O`, and `Ctrl+N` wired in `apps/desktop/main.cpp`.
- [ ] Unit tests in `test/test_project_view.cpp` verifying scale switching, BPM adjustments, and serialization round-trip.

---

### BACKLOG-02: Dedicated Instrument Screen (`VIEW_INSTRUMENT`) & Parameter Sliders

#### 1. Overview & Objective
Currently, `VIEW_INSTRUMENT` (`[0, 2]` in the 3x3 Sound Tier) reuses `renderSynthView()`. In the ToadTracker architecture specification ([`TRACKER_SPEC.md`](TRACKER_SPEC.md#L383)), each of the 16 instruments owns high-level sound sculpting properties including ADSR amplitude envelopes, Universal Filter topology, panning, table assignment, and Renoise-inspired probabilistic note pools.

#### 2. Functional Requirements
- **Instrument Identification & Type Selection:**
  - Instrument Index `00..0F` (switchable via cursor or `Left/Right` on ID)
  - Instrument Name (up to 12 chars)
  - Instrument Engine Type:
    - `INST_TYPE_INTERNAL_SYNTH` (Alpha Juno PWM / Wavefolder / Disperser)
    - `INST_TYPE_WAVETABLE` (Serum/Vital-style single-oscillator wavetable)
    - `INST_TYPE_SF2_PLAYER` (Multi-zone SoundFont 2 sample player)
    - `INST_TYPE_MIDI_OUT` (External hardware / USB MIDI sink)
- **Amplitude ADSR Envelope:**
  - Attack (0..255 $\rightarrow$ 0.1ms .. 5000ms)
  - Decay (0..255 $\rightarrow$ 1ms .. 5000ms)
  - Sustain (0..255 $\rightarrow$ 0.0 .. 1.0 amplitude)
  - Release (0..255 $\rightarrow$ 1ms .. 5000ms)
  - Horizontal bar/slider rendering for intuitive visual feedback.
- **Universal Multi-Mode Filter:**
  - Filter Type: `FLT_BYPASS`, `FLT_LP12_SVF`, `FLT_LP24_LADDER`, `FLT_HP12_SVF`, `FLT_BP12_SVF`, `FLT_NOTCH_SVF`, `FLT_PEAK_SVF`, `FLT_COMB_POS`, `FLT_COMB_NEG`
  - Cutoff Frequency (Hex `00..FF` mapped exponentially 20 Hz .. 20 kHz)
  - Resonance (Hex `00..FF` mapped 0.0 .. 0.98Q)
- **Table Modulation Routing:**
  - Assigned Table ID (`00..1F` or `--` Disabled)
  - Auto-trigger on note-on toggle
- **Probabilistic Note Pool (`Yxx` Renoise Mode):**
  - Pool Mode: `POOL_OFF`, `POOL_WEIGHTED_RND`, `POOL_CYCLE`, `POOL_SHUFFLE`
  - Slot Count and relative weights (`01..FF`)

#### 3. Acceptance Criteria
- [ ] Dedicated `renderInstrumentView()` in `DisplayEngine` distinct from `renderSynthView()`.
- [ ] Clean navigation between `INST` (`[0,2]`), `SYNTH` (`[1,2]`), and `TABLE` (`[2,2]`).
- [ ] Interactive editing of ADSR envelope, Filter mode, Cutoff, Resonance, and Table assignment.
- [ ] Real-time synthesis reflecting envelope and filter changes during live auditioning via Virtual Piano Keyboard.
- [ ] Catch2 tests in `test/test_instrument_view.cpp` verifying parameter bounds, type switching, and zero heap allocation.

---

### BACKLOG-03: Offline Song WAV Exporter (`.wav`)

#### 1. Overview & Objective
Musicians need to bounce their songs or phrases directly to audio files without relying on real-time stereo mixdown recording or external capture software. This task implements a deterministic, headless, non-realtime audio exporter that renders the song to an uncompressed 16-bit or 24-bit stereo RIFF `.wav` file at 44.1 kHz / 48.0 kHz.

#### 2. Functional Requirements
- **Deterministic Offline Rendering Pipeline:**
  - Resets playback state and playhead to Song Row 0.
  - Renders audio in sample-accurate blocks (e.g. 512 frames) via `Engine::renderBlockDeterministic()`.
  - Advances until:
    - Song row exceeds `TOTAL_SONG_ROWS` or hits an explicit end marker.
    - Specified number of pattern loops complete (default: 1 full song pass + reverb/delay tail decay).
- **RIFF WAV File Writer:**
  - Standard 44-byte RIFF/WAVE header (`fmt ` chunk, `data` chunk, PCM format tag `0x0001`).
  - Stereo 2-channel, 16-bit signed integer or 24-bit PCM.
  - Streaming file write to avoid large buffer heap spikes.
- **Triggering & UI Integration:**
  - CLI command: `toad_cli export <input.tts> --output <output.wav> [--bpm <N>] [--tail <seconds>]`
  - Desktop UI: Export option in `VIEW_PROJECT` screen and `Ctrl + E` hotkey with progress bar or status indication.

#### 3. Acceptance Criteria
- [ ] `WavExporter` class implemented in `core/include/toad/wav_exporter.h` (or `format_bridge.h`).
- [ ] Supports 16-bit 44.1 kHz and 48.0 kHz stereo RIFF WAV writing.
- [ ] Non-realtime rendering executes faster than real-time (e.g. >10x real-time speed).
- [ ] CLI export option added to `apps/cli/main.cpp`.
- [ ] Desktop shortcut `Ctrl + E` triggers export with status bar feedback.
- [ ] Catch2 unit tests in `test/test_wav_exporter.cpp` verifying generated WAV header integrity, PCM sample ranges, and non-clipping via `fastTanh`.
