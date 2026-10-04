```markdown

\# TRACKER\_SPEC.md: ToadTracker ("The Toad") Architecture Specification



\*\*Project Codename:\*\* ToadTracker  

\*\*Casual Moniker:\*\* "The Toad"  

\*\*Mascot / Iconography:\*\* Tsathoggua (The Sleeper of N'kai)  

\*\*Standard License:\*\* GNU General Public License v3.0 (GPLv3)  

\*\*Target Environments:\*\* Embedded Hardware (dadamachines TBD-16 / CTAG TBD on ESP32-P4/S3), Desktop (Standalone \& VST3 Plugin via JUCE 8/9 on macOS/Linux/Windows), and Mobile (iOS/iPadOS, Android).



\---



\## 1. Project Overview \& Architectural Principles



ToadTracker is an ergonomic, open-source, multi-platform step tracker combining M8/LSDJ tracker workflows, Renoise-inspired algorithmic composition tools, and native digital signal processing (DSP) synthesis.



\### 1.1 Non-Negotiable Engineering Guardrails

1\. \*\*Zero Dynamic Allocation in Hot Audio Paths:\*\* `malloc`, `free`, `new`, `delete`, and resizing containers (`std::vector::push\_back`, `std::string`) are strictly prohibited inside all audio render callbacks, table tick processors, and DSP loops. All memory pools, delay lines, and voice buffers must be statically allocated or reserved at initialization (`prepareToPlay` / boot time).

2\. \*\*Lock-Free Concurrency:\*\* Inter-thread communication between UI tasks (Core 0 / GUI faceplate) and the real-time Audio Engine (Core 1 / audio callback) must exclusively utilize Single-Producer Single-Consumer (SPSC) lock-free ring buffers or atomic variables (`std::atomic`). Thread-blocking primitives (mutexes, locks, condition variables) are forbidden in the audio domain.

3\. \*\*Decoupled Architecture:\*\* Pure C++20 core engine logic (`/core`) must remain strictly isolated from platform abstractions (`/hal`). The core contains zero platform-specific headers (no ESP-IDF, no JUCE, no Win32/POSIX audio calls).

4\. \*\*Strict Hexadecimal Cell Representation:\*\* All pattern cells, table values, velocities, modulation offsets, and command arguments must be represented and manipulated as two-digit hexadecimal bytes (`00`–`FF` or `00`–`7F`). Decimal is reserved strictly for human-tempo units (BPM), groove tick multipliers, and audio sample rates.

5\. \*\*Deterministic Sample-Accurate Timing:\*\* All sequencer subdivisions, groove patterns, primary tables, and auxiliary tables advance strictly via sample counting. System wall-clocks (`juce::Timer`, `std::chrono`, FreeRTOS system ticks) must never drive sequencing math.

6\. \*\*Master Output Protection:\*\* A zero-latency, branchless rational saturation stage (`fastTanh`) must be hardwired immediately prior to the physical DAC buffer and VST audio bus to guarantee that excessive resonance or wavefolding never clips beyond $0\\text{ dBFS}$ ($1.0\\text{f}$).



\---



\## 2. Hardware Topology \& Controller Abstraction



\### 2.1 Hardware Specification (dadamachines TBD-16)

\- \*\*Host MCU:\*\* ESP32-P4 / ESP32-S3 (Dual-Core Xtensa/RISC-V with PSRAM).

\- \*\*Core Allocation:\*\* Core 0 (UI, screen rendering, file I/O, USB stack); Core 1 (Audio DMA buffer synthesis, table engine).

\- \*\*Display:\*\* 240×240 ST7789 SPI LCD.

\- \*\*Surface Controls:\*\* 16 Tactile RGB Step Buttons (driven via RMT/SPI DMA), 4 Push-Rotary Encoders, D-pad, A/B/X/Y tactical switches, Left/Right Screen Page buttons, dedicated illuminated Record button, and Play/Stop transport switches.



\### 2.2 Standard Gamepad \& M8-Baseline Navigation Mapping

The tracker core maps all operations to an 8-button minimum controller standard, allowing full functionality on standard gamepads (Xbox, PlayStation, 8BitDo) and touch overlays without requiring dedicated hardware encoders:



| Logical Input | Desktop Gamepad | TBD-16 Physical Hardware | Functionality |

| :--- | :--- | :--- | :--- |

| \*\*PAGE\_PREV\*\* | `L1` (Left Bumper) | Screen Left Button | Cycles back through view stack |

| \*\*PAGE\_NEXT\*\* | `R1` (Right Bumper) | Screen Right Button | Cycles forward through view stack |

| \*\*NAV\_PAD\*\* | D-Pad (Up/Down/Left/Right) | D-Pad / Enc 1 \& 2 Rotation | Grid cursor movement |

| \*\*BTN\_A\*\* | `A` Button | Enc 3 \& 4 Turn / Click | Edit value / Increment / Confirm |

| \*\*BTN\_B\*\* | `B` Button | Back / Stop Switch | Back / Cancel / Stop transport |

| \*\*BTN\_OPT\*\* | `X` Button | Screen Select Switch | Context action / Sub-screen jump |

| \*\*BTN\_EDIT\*\* | `Y` Button | Step Pad Tap | Audition note / Trigger preview |

| \*\*TRANSPORT\*\* | `Start` | Play Switch | Start / Pause playback |

| \*\*RECORD\*\* | `Select` | Record Switch | Arm recording / Parameter lock |

| \*\*VIEW\_MOD\*\* | `L2` + `L1`/`R1` | Dedicated Screen Jump | Fast toggle (SONG $\\longleftrightarrow$ PHRASE) |

| \*\*SUB\_MOD\*\* | `R2` + `L1`/`R1` | Dedicated Sound Jump | Jump (SYNTH $\\longleftrightarrow$ TABLE $\\longleftrightarrow$ POOL) |



\---



\## 3. Memory Layout \& Core Data Structures



All runtime song and table configurations reside in fixed memory structures (allocated in PSRAM on ESP32 targets, standard heap on desktop initialization):



```cpp

\#pragma once

\#include <cstdint>

\#include <cstddef>



namespace toad {



constexpr size\_t MAX\_TRACKS       = 8;

constexpr size\_t PHRASE\_STEPS     = 16;

constexpr size\_t TABLE\_ROWS       = 16;

constexpr size\_t TOTAL\_CHAINS     = 255;

constexpr size\_t TOTAL\_PHRASES    = 255;

constexpr size\_t TOTAL\_TABLES     = 64;

constexpr size\_t TOTAL\_INSTRUMENTS = 64;

constexpr size\_t MAX\_POOL\_SLOTS   = 8;



// --- COMMAND ENUMS (Strict Hex Values) ---

enum TrackerCommand : uint8\_t {

&#x20;   CMD\_NONE = 0x00,

&#x20;   CMD\_ARPG = 0x01, // Arpeggio offset (x = semitone 1, y = semitone 2)

&#x20;   CMD\_FCUT = 0x02, // Universal Filter Cutoff (00..FF)

&#x20;   CMD\_FRES = 0x03, // Universal Filter Resonance (00..FF)

&#x20;   CMD\_FOLD = 0x04, // Wavefolder Drive Amount (00..FF)

&#x20;   CMD\_PWM\_ = 0x05, // Pulse Width Modulation (00..FF)

&#x20;   CMD\_DISP = 0x06, // Allpass Disperser Frequency (00..FF)

&#x20;   CMD\_WPOS = 0x07, // Wavetable Frame Position Scan (00..FF)

&#x20;   CMD\_WAMT = 0x08, // Wavetable Warp Intensity (00..FF)

&#x20;   CMD\_FDRV = 0x09, // Filter Saturation Drive (00..FF)

&#x20;   CMD\_SMP1 = 0x0A, // Sample Start Offset Point (00..FF)

&#x20;   CMD\_SLOP = 0x0B, // Sample Loop Mode (00 = Fwd, 01 = PingPong, 02 = Off)

&#x20;   CMD\_ATBL = 0x10, // Trigger Aux Table (Val = Table ID 00..3F)

&#x20;   CMD\_HOP\_ = 0x20, // Table Loop/Jump Target (Val = Step 00..0F)

&#x20;   CMD\_MCC1 = 0x30, // Send MIDI Macro CC 1 (00..7F)

&#x20;   CMD\_MCC2 = 0x31, // Send MIDI Macro CC 2 (00..7F)

&#x20;   CMD\_MCC3 = 0x32, // Send MIDI Macro CC 3 (00..7F)

&#x20;   CMD\_MCC4 = 0x33, // Send MIDI Macro CC 4 (00..7F)

&#x20;   CMD\_MPCH = 0x34, // Send MIDI Program Change (00..7F)

&#x20;   CMD\_PBND = 0x35  // Send MIDI Pitch Bend (-64..+63 -> 14-bit)

};



// --- SEQUENCER ROW UNITS ---

struct StepEffect {

&#x20;   TrackerCommand cmd;

&#x20;   uint8\_t        val;

};



struct PhraseStep {

&#x20;   uint8\_t    note;       // 00..7F (0xFF = Empty/No Trigger)

&#x20;   uint8\_t    instrument; // 00..3F (0xFF = Empty)

&#x20;   uint8\_t    volume;     // 00..FF

&#x20;   StepEffect fx\[3];      // 3 Stackable FX columns per step (M8 style)

};



struct Phrase {

&#x20;   PhraseStep steps\[PHRASE\_STEPS];

};



struct ChainStep {

&#x20;   uint8\_t phrase\_id;     // 00..FE (0xFF = End of Chain)

&#x20;   int8\_t  transpose;     // Signed semitone transposition (-7F..7F)

};



struct Chain {

&#x20;   ChainStep steps\[16];

};



struct SongRow {

&#x20;   uint8\_t chain\_ids\[MAX\_TRACKS]; // Chain ID per track (0xFF = Empty)

};



// --- TABLE MODULATION UNITS ---

struct TableRow {

&#x20;   int8\_t         transpose; // Semitone offset (-80..7F)

&#x20;   uint8\_t        volume;    // Volume scaling (00 = ignore, 01..FF)

&#x20;   TrackerCommand cmd1;

&#x20;   uint8\_t        val1;

&#x20;   TrackerCommand cmd2;

&#x20;   uint8\_t        val2;

};



struct Table {

&#x20;   TableRow rows\[TABLE\_ROWS];

&#x20;   uint8\_t  speed; // Ticks per row (01 = audio-rate tick, 06 = standard)

&#x20;   bool     loop;  // Loop execution flag

};



struct TablePlayer {

&#x20;   uint8\_t table\_id;

&#x20;   int8\_t  current\_row;

&#x20;   uint8\_t tick\_counter;

&#x20;   uint8\_t speed;

&#x20;   bool    active;



&#x20;   inline void trigger(uint8\_t id, uint8\_t default\_speed = 1) {

&#x20;       table\_id = id;

&#x20;       current\_row = 0;

&#x20;       tick\_counter = 0;

&#x20;       speed = default\_speed;

&#x20;       active = true;

&#x20;   }



&#x20;   inline void stop() {

&#x20;       active = false;

&#x20;       current\_row = -1;

&#x20;   }

};



// --- PROBABILISTIC NOTE POOL (RENOISE Yxx) ---

enum PoolMode : uint8\_t {

&#x20;   POOL\_OFF          = 0x00,

&#x20;   POOL\_WEIGHTED\_RND = 0x01,

&#x20;   POOL\_CYCLE        = 0x02,

&#x20;   POOL\_SHUFFLE      = 0x03

};



struct PoolSlot {

&#x20;   int8\_t  semitone\_offset; // Relative semitone (-24..+24)

&#x20;   uint8\_t weight;          // 00 = Disabled, 01..FF relative weight

&#x20;   uint8\_t velocity\_scale;  // Dynamics scaling (00..FF)

};



struct InstrumentNotePool {

&#x20;   PoolMode mode;

&#x20;   uint8\_t  slot\_count;

&#x20;   PoolSlot slots\[MAX\_POOL\_SLOTS];

&#x20;   uint8\_t  last\_selected;

};



// --- SCALE QUANTIZATION ENGINE ---

enum SnapMode : uint8\_t {

&#x20;   SNAP\_DOWN    = 0x00,

&#x20;   SNAP\_UP      = 0x01,

&#x20;   SNAP\_NEAREST = 0x02

};



struct ScaleDefinition {

&#x20;   char     name\[12];

&#x20;   uint8\_t  root\_note; // 00 = C ... 0B = B

&#x20;   uint16\_t note\_mask; // 12-bit active semitone mask (Bit 0 = Root)

&#x20;   SnapMode snap\_mode;

&#x20;   bool     enabled;

};



// --- UNIFIED FILTER \& SATURATION DEFINITIONS ---

enum FilterType : uint8\_t {

&#x20;   FLT\_BYPASS      = 0x00,

&#x20;   FLT\_LP12\_SVF    = 0x01,

&#x20;   FLT\_LP24\_LADDER = 0x02,

&#x20;   FLT\_HP12\_SVF    = 0x03,

&#x20;   FLT\_BP12\_SVF    = 0x04,

&#x20;   FLT\_NOTCH\_SVF   = 0x05,

&#x20;   FLT\_PEAK\_SVF    = 0x06,

&#x20;   FLT\_COMB\_POS    = 0x07,

&#x20;   FLT\_COMB\_NEG    = 0x08

};



enum WarpMode : uint8\_t {

&#x20;   WARP\_OFF     = 0x00,

&#x20;   WARP\_PWM     = 0x01,

&#x20;   WARP\_SYNC    = 0x02,

&#x20;   WARP\_BEND    = 0x03,

&#x20;   WARP\_FOLD    = 0x04,

&#x20;   WARP\_FORMANT = 0x05,

&#x20;   WARP\_BITCR   = 0x06

};



// --- INSTRUMENT DEFINITIONS ---

enum InstrumentType : uint8\_t {

&#x20;   INST\_TYPE\_INTERNAL\_SYNTH = 0x00,

&#x20;   INST\_TYPE\_WAVETABLE      = 0x01,

&#x20;   INST\_TYPE\_SAMPLE\_PLAYER  = 0x02,

&#x20;   INST\_TYPE\_SF2\_MULTISAMPLE= 0x03,

&#x20;   INST\_TYPE\_MIDI\_OUT       = 0x04

};



struct MidiConfig {

&#x20;   uint8\_t midi\_channel;

&#x20;   uint8\_t default\_program;

&#x20;   uint8\_t bank\_msb;

&#x20;   uint8\_t bank\_lsb;

&#x20;   uint8\_t cc\_assignments\[4];

};



struct SynthConfig {

&#x20;   float pulse\_width;

&#x20;   float fold\_drive;

&#x20;   float disperser\_freq;

&#x20;   uint8\_t disperser\_stages;

};



struct WavetableConfig {

&#x20;   uint8\_t  table\_index;

&#x20;   float    position;

&#x20;   WarpMode warp\_mode;

&#x20;   float    warp\_amount;

};



struct Instrument {

&#x20;   char               name\[12];

&#x20;   InstrumentType     type;

&#x20;   uint8\_t            table\_id;

&#x20;   InstrumentNotePool note\_pool;

&#x20;   ScaleDefinition    scale;

&#x20;   FilterType         filter\_type;

&#x20;   float              filter\_cutoff;

&#x20;   float              filter\_resonance;

&#x20;   float              drive\_amount;



&#x20;   union {

&#x20;       SynthConfig     synth;

&#x20;       WavetableConfig wavetable;

&#x20;       MidiConfig      midi;

&#x20;   };

};



} // namespace toad



```



\---



\## 4. Performance Optimization Core (`FastMath.h`)



To eliminate audio callback underruns on the ESP32 and accelerate non-realtime rendering in JUCE, `<cmath>` transcendentals (`std::sin`, `std::tanh`, `std::pow`) are strictly banned from per-sample DSP calculations.



```cpp

\#pragma once

\#include <cstdint>

\#include <cstring>



namespace toad {

namespace FastMath {



// Branchless 32-bit Phase Accumulator Step Calculation

inline uint32\_t calculatePhaseIncrement(float frequencyHz, float sampleRateHz) {

&#x20;   return static\_cast<uint32\_t>((frequencyHz / sampleRateHz) \* 4294967296.0f);

}



// Fast Rational Tanh Approximation for Soft-Clipping \& Drive Stages

// Maximum error < 1%, zero denormals, fully branchless

inline float fastTanh(float x) {

&#x20;   float x2 = x \* x;

&#x20;   return x \* (27.0f + x2) / (27.0f + 9.0f \* x2);

}



// Fast Hard Soft-Clipper for Master Output Protection

inline float masterClip(float x) {

&#x20;   if (x > 1.5f) return 1.0f;

&#x20;   if (x < -1.5f) return -1.0f;

&#x20;   return fastTanh(x);

}



// Fast 2^x Pitch-to-Frequency Conversion (IEEE 754 bit-cast approximation)

inline float fast2Exp(float p) {

&#x20;   float clp = (p < -126.0f) ? -126.0f : p;

&#x20;   union { float f; uint32\_t i; } v;

&#x20;   v.i = static\_cast<uint32\_t>((1 << 23) \* (clp + 126.94269504f));

&#x20;   return v.f;

}



// Semitone Offset to Pitch Multiplier (12-Tone Equal Temperament)

inline float semitoneToRatio(float semitones) {

&#x20;   return fast2Exp(semitones \* 0.08333333333f);

}



// High-speed Parabolic Sine Approximation (-1.0 to 1.0 phase input)

inline float fastSin(float normalizedPhase) {

&#x20;   float x = normalizedPhase;

&#x20;   return 4.0f \* x \* (1.0f - ((x < 0.0f) ? -x : x));

}



} // namespace FastMath

} // namespace toad



```



\---



\## 5. Algorithmic Processing Pipelines



\### 5.1 Step Trigger Pipeline



Upon arriving at an active step boundary during sample-accurate countdown:



```

\[ Step Event: Note + Inst + FX ]

&#x20;              │

&#x20;              ▼

\[ 1. Instrument Note Pool ] ──► Resolve probabilistic semitone offset (Yxx)

&#x20;              │

&#x20;              ▼

\[ 2. Scale Quantizer ]     ──► Restrict pitch to active scale mask

&#x20;              │

&#x20;              ▼

\[ 3. De-Click Crossfader ] ──► Apply 2ms anti-click ramp down if voice is active

&#x20;              │

&#x20;              ▼

\[ 4. Voice Trigger ]       ──► Dispatch noteOn to DSP Voice or MIDI Sink

&#x20;              │

&#x20;              ├─► \[ 5. Primary Table ] ──► Reset tick counter, execute Row 0

&#x20;              │

&#x20;              └─► \[ 6. Aux Table ]     ──► Dynamic execution triggered via ATBL command



```



\### 5.2 Probabilistic Pitch Resolution Algorithm



```cpp

namespace toad {



inline int8\_t evaluateNotePool(InstrumentNotePool\& pool, int8\_t base\_note) {

&#x20;   if (pool.mode == POOL\_OFF || pool.slot\_count == 0) return base\_note;



&#x20;   if (pool.mode == POOL\_WEIGHTED\_RND) {

&#x20;       uint32\_t total\_weight = 0;

&#x20;       for (uint8\_t i = 0; i < pool.slot\_count; ++i) {

&#x20;           total\_weight += pool.slots\[i].weight;

&#x20;       }

&#x20;       if (total\_weight == 0) return base\_note;



&#x20;       // Linear congruential pseudo-random number generator (zero library calls)

&#x20;       static uint32\_t lcg\_seed = 123456789;

&#x20;       lcg\_seed = lcg\_seed \* 1664525u + 1013904223u;

&#x20;       uint32\_t roll = lcg\_seed % total\_weight;

&#x20;       uint32\_t cumulative = 0;



&#x20;       for (uint8\_t i = 0; i < pool.slot\_count; ++i) {

&#x20;           if (pool.slots\[i].weight == 0) continue;

&#x20;           cumulative += pool.slots\[i].weight;

&#x20;           if (roll < cumulative) {

&#x20;               pool.last\_selected = i;

&#x20;               return base\_note + pool.slots\[i].semitone\_offset;

&#x20;           }

&#x20;       }

&#x20;   } else if (pool.mode == POOL\_CYCLE) {

&#x20;       for (uint8\_t i = 0; i < pool.slot\_count; ++i) {

&#x20;           pool.last\_selected = (pool.last\_selected + 1) % pool.slot\_count;

&#x20;           if (pool.slots\[pool.last\_selected].weight > 0) {

&#x20;               return base\_note + pool.slots\[pool.last\_selected].semitone\_offset;

&#x20;           }

&#x20;       }

&#x20;   }

&#x20;   return base\_note;

}



inline int8\_t quantizeToScale(const ScaleDefinition\& scale, int8\_t raw\_note) {

&#x20;   if (!scale.enabled || scale.note\_mask == 0x0FFF || scale.note\_mask == 0) {

&#x20;       return raw\_note;

&#x20;   }



&#x20;   int8\_t octave = raw\_note / 12;

&#x20;   int8\_t semitone = raw\_note % 12;

&#x20;   if (semitone < 0) { semitone += 12; octave -= 1; }



&#x20;   int8\_t rel\_semitone = (semitone - scale.root\_note + 12) % 12;

&#x20;   if ((scale.note\_mask >> rel\_semitone) \& 1) {

&#x20;       return raw\_note;

&#x20;   }



&#x20;   int8\_t lower\_dist = 0, higher\_dist = 0;

&#x20;   for (int d = 1; d <= 6; ++d) {

&#x20;       if (lower\_dist == 0 \&\& ((scale.note\_mask >> ((rel\_semitone - d + 12) % 12)) \& 1)) {

&#x20;           lower\_dist = d;

&#x20;       }

&#x20;       if (higher\_dist == 0 \&\& ((scale.note\_mask >> ((rel\_semitone + d) % 12)) \& 1)) {

&#x20;           higher\_dist = d;

&#x20;       }

&#x20;       if (lower\_dist != 0 \&\& higher\_dist != 0) break;

&#x20;   }



&#x20;   int8\_t offset = 0;

&#x20;   if (scale.snap\_mode == SNAP\_DOWN) {

&#x20;       offset = -lower\_dist;

&#x20;   } else if (scale.snap\_mode == SNAP\_UP) {

&#x20;       offset = higher\_dist;

&#x20;   } else {

&#x20;       offset = (lower\_dist <= higher\_dist) ? -lower\_dist : higher\_dist;

&#x20;   }



&#x20;   return (octave \* 12) + scale.root\_note + rel\_semitone + offset;

}



} // namespace toad



```



\---



\## 6. Synthesis, Wavetable \& Multi-Sample Engines



\### 6.1 Native DSP Voices (Alpha Juno PWM, Wavefolder, Allpass Disperser)



\* \*\*Alpha Juno Style Saw/Pulse:\*\* Dual-edge phase accumulator providing variable-slope saw and pulse-width wave modifications.

\* \*\*Wavefolder:\*\* Non-linear symmetric and asymmetric foldback waveshaper driven by `CMD\_FOLD`.

\* \*\*Allpass Disperser:\*\* Multi-stage cascaded allpass delay structure ($1$ to $8$ stages) creating spectral phase smear for kicks, snares, and metallic transients.



\### 6.2 Stripped-Down Single-Oscillator Wavetable Synth



\* \*\*Frame Topology:\*\* Standard 2048-sample single-cycle frames (matching Serum/Vital export structures); up to 64 frames per `.ttw` table, stored in PSRAM.

\* \*\*Dual-Axis Interpolation:\*\* Cubic Hermite interpolation between intra-frame samples; linear morphing across table positions (`CMD\_WPOS`).

\* \*\*Phase-Warp Operators (`CMD\_WAMT`):\*\* Branchless phase-distortion remapping implementing Sync, Squeeze/PWM, Bend, and Mirror folding without runtime FFT overhead.



\### 6.3 SoundFont 2 (`.sf2`) Multi-Sample Engine



\* \*\*Direct Zone Streaming:\*\* Parses standard `.sf2` RIFF structures directly from storage/PSRAM without runtime ZIP decompression.

\* \*\*Key \& Velocity Splitting:\*\* Up to 64 active key/velocity zones per instrument. Linear interpolation phase accumulators handle arbitrary pitch transposition.

\* \*\*Universal Filter Integration:\*\* Every multi-sample voice passes through the shared `UniversalFilter` module (Ladder, SVF, Comb) and dynamic table sweeps.



\---



\## 7. Deterministic Audio Pipeline \& Offline Rendering



\### 7.1 Sample-Accurate Timing Model



The master timeline advances strictly by integer sample increments:





$$\\text{SamplesPerTick} = \\frac{\\text{SampleRate} \\times 60.0}{\\text{BPM} \\times 24 \\times \\text{GrooveMultiplier}}$$



Inside the audio callback (`processBlock` in JUCE or I2S DMA in ESP-IDF), incoming buffers of arbitrary size $N$ are sliced into sub-segments that land precisely on tick and step boundaries:



```cpp

void renderBlockDeterministic(float\* outputBuffer, size\_t totalSamples) {

&#x20;   size\_t samplesProcessed = 0;



&#x20;   while (samplesProcessed < totalSamples) {

&#x20;       size\_t samplesUntilTick = engine.getSamplesUntilNextTick();

&#x20;       size\_t slice = (totalSamples - samplesProcessed < samplesUntilTick) 

&#x20;                      ? (totalSamples - samplesProcessed) 

&#x20;                      : samplesUntilTick;



&#x20;       // Render DSP and dispatch MIDI up to boundary

&#x20;       engine.renderVoices(outputBuffer + samplesProcessed, slice);

&#x20;       engine.dispatchMidiEvents(slice);



&#x20;       samplesProcessed += slice;

&#x20;       engine.advanceSampleClock(slice);

&#x20;   }

}



```



\### 7.2 Non-Realtime / Offline Detection



When running inside JUCE:



\* Query `playHead->getPosition()->getIsNonRealtime()`.

\* When non-realtime rendering is active, disable all GUI repainting, throttle atomic buffer posting, and execute DSP at maximum CPU throughput.

\* Enforce `juce::ScopedNoDenormals` to prevent floating-point CPU performance penalties.



\---



\## 8. Cross-Format Interchange \& Multi-Sample Auto-Sampler



\### 8.1 Auto-Sampler / Instrument Bouncer (`BakeInstrument`)



To export native procedural DSP synths to sample-only targets (picoTracker, NullPerator, FT2/XM, IT):



1\. \*\*Automated Sampling:\*\* The engine triggers chromatic notes ($3$ or $6$ semitone increments from C1 to C7) through the native DSP voice at 50x offline speed.

2\. \*\*Phase-Aligned Loop Detection:\*\* Scans the sustain tail for zero-crossing phase alignment to generate seamless forward loop points (`loop\_start`, `loop\_end`).

3\. \*\*Container Compilation:\*\* Writes raw linear 16-bit PCM multi-sample banks mapped into destination formats.



\### 8.2 Import / Export Structural Transpilation



\* \*\*Phrase Slicing:\*\* 64-row patterns from incoming `.xm` or `.it` files are split into four contiguous 16-step `.ttp` Phrases, mapped to a single `.ttc` Chain.

\* \*\*Envelope Translation:\*\* Multi-point envelope curves are sampled into 16-row Primary Tables with looped jump markers (`CMD\_HOP\_`).

\* \*\*Macro Unrolling:\*\* When exporting to classic formats lacking dual Aux Tables, table parameter automation sweeps are baked directly into the destination pattern effect columns.

\* \*\*Channel Compaction:\*\* Classic modules with >8 or >16 tracks are conditionally collapsed by merging non-overlapping sparse channels.



\---



\## 9. File System, State Serialization \& Extension Grammar



\### 9.1 File Extension Grammar



All assets are structured with consistent, 3-letter extensions:



\* `.tts`: \*\*T\*\*oad\*\*T\*\*racker \*\*S\*\*ong (Master JSON/CBOR timeline project)

\* `.tti`: \*\*T\*\*oad\*\*T\*\*racker \*\*I\*\*nstrument (Engine config, wavetable refs, or MIDI descriptors)

\* `.ttc`: \*\*T\*\*oad\*\*T\*\*racker \*\*C\*\*hain (16-phrase container)

\* `.ttp`: \*\*T\*\*oad\*\*T\*\*racker \*\*P\*\*hrase (16-step note and FX block)

\* `.ttt`: \*\*T\*\*oad\*\*T\*\*racker \*\*T\*\*able (Primary or Aux modulation table)

\* `.ttw`: \*\*T\*\*oad\*\*T\*\*racker \*\*W\*\*avetable (2048-sample multi-frame file)

\* `.ttz`: \*\*T\*\*oad\*\*T\*\*racker \*\*Z\*\*one (SF2 instrument preset configuration)



\### 9.2 Atomic Save Pattern



To prevent song corruption from sudden power cutoffs on hardware:



1\. Serialize the project state to `/PROJECTS/<NAME>/song.tmp`.

2\. Flush buffers and synchronize the file descriptor to physical storage.

3\. Atomically rename `song.tmp` to `song.tts`.



\---



\## 10. Repository Architecture \& Automated Dependencies



\### 10.1 Workspace Layout



```text

ToadTracker/

├── CMakeLists.txt              # Root build orchestrator (FetchContent)

├── TRACKER\_SPEC.md             # This document

├── .gitignore

├── core/                       # Pure C++20 platform-agnostic engine

│   ├── include/toad/           # Core API \& types

│   │   ├── types.h             # Song, Chain, Phrase, Table definitions

│   │   ├── fast\_math.h         # Branchless math \& fast saturation

│   │   ├── engine.h            # Deterministic sequencing core

│   │   ├── pool.h              # Probabilistic note pool runner

│   │   ├── scale.h             # Scale quantizer engine

│   │   ├── synth\_voice.h       # PWM saw, wavefolder, disperser

│   │   ├── wavetable\_voice.h   # Vital/Serum-style single-osc synth

│   │   ├── sf2\_player.h        # SoundFont 2 zone reader

│   │   └── format\_bridge.h     # M8, picoTracker, IT/XM converters

│   └── src/                    # Core implementations

├── hal/

│   ├── common/                 # ITrackerHAL \& IMidiOutputSink interfaces

│   ├── esp32/                  # Target: dadamachines TBD-16 (ESP-IDF/FreeRTOS)

│   └── juce/                   # Target: Desktop VST3 / Standalone \& Gamepad

├── test/                       # Headless verification harness (Catch2)

│   ├── CMakeLists.txt

│   ├── test\_main.cpp

│   ├── test\_fast\_math.cpp

│   ├── test\_tables.cpp

│   └── test\_scale\_pool.cpp

└── assets/                     # Default Tsathoggua bitmaps, palettes, presets



```



\### 10.2 Root `CMakeLists.txt`



```cmake

cmake\_minimum\_required(VERSION 3.22)

project(ToadTracker VERSION 0.1.0 LANGUAGES C CXX)



set(CMAKE\_CXX\_STANDARD 20)

set(CMAKE\_CXX\_STANDARD\_REQUIRED ON)

set(CMAKE\_EXPORT\_COMPILE\_COMMANDS ON)



include(FetchContent)



\# 1. Catch2 for Headless Core Tests

FetchContent\_Declare(

&#x20;   Catch2

&#x20;   GIT\_REPOSITORY \[https://github.com/catchorg/Catch2.git](https://github.com/catchorg/Catch2.git)

&#x20;   GIT\_TAG        v3.5.2

)



\# 2. JUCE Framework (GPLv3)

option(BUILD\_JUCE\_TARGET "Build Desktop Standalone and VST3 Plugin" ON)

if(BUILD\_JUCE\_TARGET)

&#x20;   FetchContent\_Declare(

&#x20;       JUCE

&#x20;       GIT\_REPOSITORY \[https://github.com/juce-framework/JUCE.git](https://github.com/juce-framework/JUCE.git)

&#x20;       GIT\_TAG        8.0.4

&#x20;   )

endif()



\# 3. libopenmpt (For Classic Tracker Format Import)

option(ENABLE\_MODULE\_IMPORT "Enable MOD/XM/IT/S3M Import Engine" ON)

if(ENABLE\_MODULE\_IMPORT)

&#x20;   FetchContent\_Declare(

&#x20;       libopenmpt

&#x20;       GIT\_REPOSITORY \[https://github.com/OpenMPT/openmpt.git](https://github.com/OpenMPT/openmpt.git)

&#x20;       GIT\_TAG        libopenmpt-0.7.8

&#x20;   )

endif()



FetchContent\_MakeAvailable(Catch2)

if(BUILD\_JUCE\_TARGET)

&#x20;   FetchContent\_MakeAvailable(JUCE)

endif()



add\_subdirectory(core)

add\_subdirectory(test)

if(BUILD\_JUCE\_TARGET)

&#x20;   add\_subdirectory(hal/juce)

endif()



```



\---



\## 11. Phased Execution Roadmap



\### Phase 1: Repository Foundation \& Math Verification (Current Milestone)



\* Setup Git tracking, `.gitignore`, and the directory hierarchy.

\* Write root `CMakeLists.txt` and resolve Catch2 dependencies.

\* Implement `core/include/toad/types.h` and `core/include/toad/fast\_math.h`.

\* Write Catch2 tests verifying `fastTanh`, phase accumulation, and pitch calculation accuracy.



\### Phase 2: Core Sequencer \& Modulators



\* Implement `toad::quantizeToScale` and `toad::evaluateNotePool`.

\* Implement dual-table runners (`toad::TablePlayer`) with loop markers and `CMD\_HOP\_`.

\* Verify deterministic sample countdown and pattern stepping via headless unit tests.



\### Phase 3: DSP \& Sound Synthesis



\* Implement Alpha Juno PWM saw, wavefolder, and allpass disperser.

\* Implement Vital/Serum-style single-oscillator wavetable voice with warp modes.

\* Implement SF2 RIFF parser and multi-zone PCM player.

\* Add `UniversalFilter` (Ladder, SVF, Comb) and hardwired master `fastTanh` soft clipper.



\### Phase 4: Desktop JUCE Harness \& Gamepad UI



\* Wrap engine inside `juce::AudioProcessor` with sample-accurate `processBlock` slicing.

\* Implement virtual TBD-16 240×240 UI faceplate and gamepad input mapper (L1/R1, D-pad, ABXY).

\* Wire `IMidiOutputSink` to populate `juce::MidiBuffer` with sample-accurate timestamps.



\### Phase 5: Hardware Port (CTAG TBD / ESP32)



\* Map I2S DMA buffers to Core 1 audio callback.

\* Wire ST7789 SPI LCD and NeoPixel RMT drivers to Core 0 UI task.

\* Map physical rotary encoders, D-pad, and illuminated tactile buttons.

\* Connect hardware UART MIDI and USB-MIDI interfaces.



\### Phase 6: Interchange \& Format Bridges



\* Integrate `libopenmpt` to import `.mod`, `.xm`, `.s3m`, and `.it` files.

\* Implement `FormatBridge` for two-way Dirtywave M8 (`.m8s`) and picoTracker project import/export.

\* Implement the `BakeInstrument` auto-sampler to bounce internal C++ DSP voices to multi-sample WAV banks.



```



```

