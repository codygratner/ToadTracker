# Future Backlog & Reminders

> [!WARNING]
> ## 🚨 CRITICAL HARDWARE SPEC CORRECTION: OFFICIAL DADAMACHINES TBD-16
> The previous `TRACKER_SPEC.md` and `tbd16_emulator` assumed a fictional 240x240 LCD. The **OFFICIAL** production hardware specs from dadamachines / CTAG TBD are:
> - **Display:** **2.4-inch OLED Display with 128 × 64 pixels** (NOT 240x240 LCD!).
> - **Encoders:** **4 high-quality endless rotary push-encoders** (KNOB 1–4) + dedicated volume wheel.
> - **Buttons:** **30 tactile buttons with RGB backlighting** + 19 RGB LEDs + D-Pad (center-left).
> - **Tri-Core Processors:** **ESP32-P4** (400MHz Audio DSP) + **RP2350** (150MHz UI/Sequencer) + **ESP32-C6** (WiFi / Ableton Link).
> - **Official Source Repositories:**
>   - [`dadamachines/ctag-tbd`](https://github.com/dadamachines/ctag-tbd): Primary TBD-16 firmware & adaptations repo.
>   - [`dadamachines/dada-tbd-app-template`](https://github.com/dadamachines/dada-tbd-app-template): Official app template for RP2350 UI apps.
>   - [`ctag-fh-kiel/ctag-tbd`](https://github.com/ctag-fh-kiel/ctag-tbd): Upstream core CTAG audio platform (Robert Manzke).
> - **Action Required when resuming ToadTracker:** Refactor `DisplayEngine` and `tbd16_emulator` to render for 128x64 OLED instead of 240x240.

This file indexes active roadmap items for the automated task runner (`execute-task`). For complete detailed technical specifications, see [`BACKLOG.md`](BACKLOG.md).

---

## Prioritized Work Items

### Item 1: BACKLOG-01 - Dedicated Project Screen (`VIEW_PROJECT`) & File Operations
- **Status:** Ready
- **Scope:** `core`, `apps/desktop`, `hal`
- **Summary:** Implement dedicated `VIEW_PROJECT` screen (`[0,0]` in 3x3 map) for project metadata (Title, BPM, Steps Per Beat, Master Transpose), global Scale Quantizer setup (Root Note, Scale Presets: Major, Minor, Dorian, Pentatonic, Hirajoshi, etc.), and atomic Disk I/O operations (`song.tts` Save, Load, New Project) with `Ctrl+S`, `Ctrl+O`, `Ctrl+N` hotkeys.

### Item 2: BACKLOG-02 - Dedicated Instrument Screen (`VIEW_INSTRUMENT`) & Parameter Sliders
- **Status:** Ready
- **Scope:** `core`, `apps/desktop`, `hal`
- **Summary:** Separate `VIEW_INSTRUMENT` (`[0,2]`) from `VIEW_SYNTH` (`[1,2]`). Implement dedicated instrument property controls: Instrument Name & Type, Volume & Panning, ADSR Amplitude Envelopes (with bar sliders), Universal Multi-Mode Filter (Bypass, LP12, LP24, HP12, BP12, Notch, Peak, Comb) + Cutoff & Resonance, Table routing, and Renoise-style probabilistic note pools (`Yxx`).

### Item 3: BACKLOG-03 - Offline Song WAV Exporter (`.wav`)
- **Status:** Ready
- **Scope:** `core`, `apps/cli`, `apps/desktop`
- **Summary:** Deterministic, headless offline audio renderer that bounces full songs or chains to 16-bit / 44.1 kHz stereo RIFF WAV files faster than real-time. Accessible via CLI (`toad_cli export`) and desktop UI (`Ctrl+E` / Project screen).

### Item 4: BACKLOG-04 - Instantaneous Hold-to-View Navigation Modal
- **Status:** Ready
- **Scope:** `core`, `apps/desktop`, `apps/tbd16_emulator`, `apps/steamdeck_runner`
- **Summary:** Make the 2D Navigation Map modal hold-to-view: pops up immediately on Left-Shift / Gamepad Left Trigger press, navigates with D-Pad/Arrows, and drops instantly on release with zero delay (eliminating the lingering 60-frame countdown).

### Item 5: BACKLOG-05 - TBD-16 Step-Button Encoder Parameter Locks (P-Locks) & Klang Engine Telemetry
- **Status:** Ready
- **Scope:** `core`, `apps/tbd16_emulator`, `hal`
- **Summary:** Implement Elektron-style parameter locks: holding a step button (`00`–`0F`) and turning any of the 4 encoders immediately locks that parameter value to that step. The step LED indicates locked state in amber, and writes the command/hex value into the tracker pattern columns (e.g. `12 7F`). On the TBD-16 hardware, the RP2350 transmits a 4-byte `StepLockPacket` via 20MHz SPI DMA directly to the ESP32-P4 audio engine.

---

## Reminders & Operational Guardrails
- **Zero Dynamic Allocation:** Preserve zero dynamic allocation in hot DSP and UI rendering loops.
- **Branching Policy:** All new feature work must be performed on dedicated feature branches (e.g. `feature/project-screen-and-file-ops`) and merged locally into `main`.
- **Local Commits Only:** Never push commits or tags to remote GitHub until explicitly directed by the user.
