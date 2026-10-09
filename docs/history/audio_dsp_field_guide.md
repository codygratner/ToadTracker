# 🎛️ The Audio DSP & C++ Field Guide

> **Status:** Canonical source hosted at [**`TheKlangResearch/guides/audio_dsp_field_guide.md`**](https://github.com/codygratner/TheKlangResearch/blob/main/guides/audio_dsp_field_guide.md)  
> **Local Reference Path:** [`C:\Dev\Research\guides\audio_dsp_field_guide.md`](file:///C:/Dev/Research/guides/audio_dsp_field_guide.md)  
> **Vault Mirror:** `TheKlangVault/Research/guides/audio_dsp_field_guide.md`

This guide has graduated to the centralized **The Klang Research Hub** to serve as the shared standard across `TheKlangSuite`, `ToadTracker`, and `N'kai`.

---

## ⚡ The Four Hard Real-Time Invariants (Executive Summary)

For real-time audio threads (`processBlock`, `processStereo`, voice rendering):
1. **Zero Heap Allocations:** Never call `new`, `malloc`, `free`, or resize dynamic containers (`std::vector::push_back`, `juce::String` concatenation) on the audio path. Everything must be pre-allocated in `prepareToPlay()`.
2. **Zero Locks:** Never acquire a `std::mutex`, `juce::CriticalSection`, or wait on thread synchronization primitives. Audio-to-UI communication must use lock-free atomics (`std::atomic<float>`) or single-reader single-writer FIFOs (`juce::AbstractFifo`).
3. **Zero Blocking I/O:** Never call filesystem operations, network sockets, or console output (`std::cout`, `DBG()`, `printf`) on the audio thread.
4. **SIMD & FastMath:** Standard CRT transcendentals (`std::pow`, `std::sin`, `std::tanh`) take 50–120 CPU cycles. In hot voice loops, use rational Padé or polynomial approximations (`TbdAudio::FastMath`).

*For full filter derivations, oversampling math, and circuit models, see the canonical [Audio DSP Field Guide](https://github.com/codygratner/TheKlangResearch/blob/main/guides/audio_dsp_field_guide.md).*
