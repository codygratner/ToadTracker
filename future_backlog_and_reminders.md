# Future Backlog & Reminders

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

---

## Reminders & Operational Guardrails
- **Zero Dynamic Allocation:** Preserve zero dynamic allocation in hot DSP and UI rendering loops.
- **Branching Policy:** All new feature work must be performed on dedicated feature branches (e.g. `feature/project-screen-and-file-ops`) and merged locally into `main`.
- **Local Commits Only:** Never push commits or tags to remote GitHub until explicitly directed by the user.
