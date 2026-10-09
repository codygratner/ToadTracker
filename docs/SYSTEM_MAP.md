# 🗺️ ToadTracker System Map

This is the central directory for ToadTracker system architecture, hardware HALs, and specifications.

### 1. 📜 Core Architecture & Governance
- [DOCS_CATALOG.json](DOCS_CATALOG.json) — Master index and navigation manifest.
- [GLOSSARY.md](GLOSSARY.md) — Canonical tracker terminology (Song, Chain, Phrase, Table, P-Lock, HAL).
- [BACKLOG.md](../BACKLOG.md) — Milestone roadmap and hardware feature queue.

### 2. 🔬 Engineering Specifications
- [Tracker Core Engine Spec](specs/tracker_core_engine.md) — 16-step playback loop, command columns, parameter locks, and audio rendering.
- [Hardware HAL: dadamachines TBD-16](specs/hardware_hal_tbd16.md) — ESP32-P4 DSP + RP2350B UI/OLED driver, SPI DMA bus, encoder mapping.
- [Multi-Target Deployment Spec](specs/desktop_and_embedded_targets.md) — Desktop SDL2, Steam Deck SteamOS, and Raspberry Pi ALSA target harnesses.

### 3. 🧭 Shared Field Guides & Lore
- [Audio DSP Field Guide](history/audio_dsp_field_guide.md) — Hard real-time audio thread invariants and FastMath rules.
- [Vibe-Coder's Field Guide](history/vibe_coding_field_guide.md) — AI workflow protocols, model tiers, and escalation gates.
- [Mayor Toad Lore Chronicle](lore/README.md) — Tsathoggua, City Hall, and the great mayoral coup.

## 🏛️ The Four-Pillar Ecosystem
- **The Klang Suite** (`c:\Dev\TheKlangSuite`): Polyphonic synthesizers and VST3 plugins.
- **ToadTracker** (`c:\Dev\ToadTracker`): 16-step embedded hardware groovebox.
- **N'kai** (`c:\Dev\nkai`): Asymmetric sidecar framework & interactive triage lab ([github.com/codygratner/nkai](https://github.com/codygratner/nkai)).
- **The Klang Research** (`c:\Dev\Research`): Centralized research repository, exploratory canvases, and soul harvests ([github.com/codygratner/TheKlangResearch](https://github.com/codygratner/TheKlangResearch)).
