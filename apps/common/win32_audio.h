#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmeapi.h>
#pragma comment(lib, "winmm.lib")

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
#include <algorithm>
#include <functional>
#include <array>

namespace toad {

// ============================================================================
// WIN32 LOW-LATENCY AUDIO DEVICE (WAVEOUT WITH ZERO RUNTIME ALLOCATIONS)
// ============================================================================
class Win32AudioDevice {
public:
    static constexpr size_t NUM_BUFFERS = 4;
    static constexpr size_t BUFFER_FRAMES = 512; // 11.6 ms latency at 44.1 kHz
    static constexpr size_t BUFFER_CHANNELS = 2; // Stereo
    static constexpr size_t BUFFER_SAMPLES = BUFFER_FRAMES * BUFFER_CHANNELS;
    static constexpr size_t SCOPE_BUFFER_SIZE = 1024; // Samples for oscilloscope visualization

    using AudioCallback = std::function<void(float* interleavedStereo, size_t numFrames)>;

    Win32AudioDevice() : hWaveOut_(nullptr), hWakeupEvent_(nullptr), hThread_(nullptr), running_(false) {
        scopeBuffer_.fill(0.0f);
        peakLeft_.store(0.0f);
        peakRight_.store(0.0f);
    }

    ~Win32AudioDevice() {
        stop();
    }

    bool start(uint32_t sampleRate, AudioCallback callback) {
        if (running_) return true;

        callback_ = std::move(callback);
        sampleRate_ = sampleRate;

        // Configure 16-bit Stereo PCM format
        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = static_cast<WORD>(BUFFER_CHANNELS);
        wfx.nSamplesPerSec = sampleRate;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8);
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        hWakeupEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!hWakeupEvent_) return false;

        MMRESULT res = waveOutOpen(&hWaveOut_, WAVE_MAPPER, &wfx,
                                   reinterpret_cast<DWORD_PTR>(hWakeupEvent_),
                                   0, CALLBACK_EVENT);
        if (res != MMSYSERR_NOERROR) {
            CloseHandle(hWakeupEvent_);
            hWakeupEvent_ = nullptr;
            return false;
        }

        // Initialize wave headers and buffers
        for (size_t i = 0; i < NUM_BUFFERS; ++i) {
            ZeroMemory(&headers_[i], sizeof(WAVEHDR));
            headers_[i].lpData = reinterpret_cast<LPSTR>(pcmBuffers_[i].data());
            headers_[i].dwBufferLength = static_cast<DWORD>(BUFFER_SAMPLES * sizeof(int16_t));
            headers_[i].dwUser = static_cast<DWORD_PTR>(i);
            waveOutPrepareHeader(hWaveOut_, &headers_[i], sizeof(WAVEHDR));
        }

        running_ = true;
        hThread_ = CreateThread(nullptr, 0, threadProcStatic, this, 0, nullptr);
        if (!hThread_) {
            waveOutClose(hWaveOut_);
            CloseHandle(hWakeupEvent_);
            running_ = false;
            return false;
        }

        SetThreadPriority(hThread_, THREAD_PRIORITY_TIME_CRITICAL);

        // Pre-fill and prime all buffers
        for (size_t i = 0; i < NUM_BUFFERS; ++i) {
            fillBuffer(i);
            waveOutWrite(hWaveOut_, &headers_[i], sizeof(WAVEHDR));
        }

        return true;
    }

    void stop() {
        if (!running_) return;

        running_ = false;
        if (hWakeupEvent_) {
            SetEvent(hWakeupEvent_);
        }

        if (hThread_) {
            WaitForSingleObject(hThread_, 2000);
            CloseHandle(hThread_);
            hThread_ = nullptr;
        }

        if (hWaveOut_) {
            waveOutReset(hWaveOut_);
            for (size_t i = 0; i < NUM_BUFFERS; ++i) {
                waveOutUnprepareHeader(hWaveOut_, &headers_[i], sizeof(WAVEHDR));
            }
            waveOutClose(hWaveOut_);
            hWaveOut_ = nullptr;
        }

        if (hWakeupEvent_) {
            CloseHandle(hWakeupEvent_);
            hWakeupEvent_ = nullptr;
        }
    }

    [[nodiscard]] bool isRunning() const noexcept { return running_; }
    [[nodiscard]] uint32_t getSampleRate() const noexcept { return sampleRate_; }

    // Read latest oscilloscope waveform samples (UI thread)
    void getScopeSnapshot(float* outScope, size_t count) const {
        if (!outScope || count == 0) return;
        size_t writeIdx = scopeHead_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            size_t idx = (writeIdx + SCOPE_BUFFER_SIZE - count + i) % SCOPE_BUFFER_SIZE;
            outScope[i] = scopeBuffer_[idx];
        }
    }

    [[nodiscard]] float getPeakLeft() const noexcept { return peakLeft_.load(std::memory_order_relaxed); }
    [[nodiscard]] float getPeakRight() const noexcept { return peakRight_.load(std::memory_order_relaxed); }

private:
    static DWORD WINAPI threadProcStatic(LPVOID param) {
        auto* self = static_cast<Win32AudioDevice*>(param);
        self->threadProc();
        return 0;
    }

    void threadProc() {
        while (running_) {
            WaitForSingleObject(hWakeupEvent_, 20);

            if (!running_) break;

            for (size_t i = 0; i < NUM_BUFFERS; ++i) {
                if (headers_[i].dwFlags & WHDR_DONE) {
                    waveOutUnprepareHeader(hWaveOut_, &headers_[i], sizeof(WAVEHDR));
                    fillBuffer(i);
                    waveOutPrepareHeader(hWaveOut_, &headers_[i], sizeof(WAVEHDR));
                    waveOutWrite(hWaveOut_, &headers_[i], sizeof(WAVEHDR));
                }
            }
        }
    }

    void fillBuffer(size_t index) {
        if (callback_) {
            callback_(floatBuffer_.data(), BUFFER_FRAMES);
        } else {
            floatBuffer_.fill(0.0f);
        }

        // Convert float (-1.0 .. 1.0) to 16-bit PCM and calculate peaks
        float maxL = 0.0f;
        float maxR = 0.0f;
        size_t head = scopeHead_.load(std::memory_order_relaxed);

        for (size_t f = 0; f < BUFFER_FRAMES; ++f) {
            float l = floatBuffer_[f * 2];
            float r = floatBuffer_[f * 2 + 1];

            float absL = std::abs(l);
            float absR = std::abs(r);
            if (absL > maxL) maxL = absL;
            if (absR > maxR) maxR = absR;

            // Update oscilloscope buffer with mono sum
            scopeBuffer_[head] = (l + r) * 0.5f;
            head = (head + 1) % SCOPE_BUFFER_SIZE;

            // Branchless clamp to int16
            int32_t sl = static_cast<int32_t>(l * 32767.0f);
            int32_t sr = static_cast<int32_t>(r * 32767.0f);
            if (sl > 32767) sl = 32767; else if (sl < -32768) sl = -32768;
            if (sr > 32767) sr = 32767; else if (sr < -32768) sr = -32768;

            pcmBuffers_[index][f * 2] = static_cast<int16_t>(sl);
            pcmBuffers_[index][f * 2 + 1] = static_cast<int16_t>(sr);
        }

        scopeHead_.store(head, std::memory_order_relaxed);
        peakLeft_.store(maxL, std::memory_order_relaxed);
        peakRight_.store(maxR, std::memory_order_relaxed);
    }

    HWAVEOUT hWaveOut_{nullptr};
    HANDLE   hWakeupEvent_{nullptr};
    HANDLE   hThread_{nullptr};
    std::atomic<bool> running_{false};
    uint32_t sampleRate_{44100};

    AudioCallback callback_{nullptr};

    std::array<std::array<int16_t, BUFFER_SAMPLES>, NUM_BUFFERS> pcmBuffers_{};
    std::array<WAVEHDR, NUM_BUFFERS> headers_{};
    std::array<float, BUFFER_SAMPLES> floatBuffer_{};

    // Oscilloscope & VU Meters (lock-free)
    alignas(64) std::array<float, SCOPE_BUFFER_SIZE> scopeBuffer_{};
    alignas(64) std::atomic<size_t> scopeHead_{0};
    alignas(64) std::atomic<float> peakLeft_{0.0f};
    alignas(64) std::atomic<float> peakRight_{0.0f};
};

} // namespace toad
