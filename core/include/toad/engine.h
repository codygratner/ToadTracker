#pragma once

#include "types.h"
#include "fast_math.h"
#include "scale.h"
#include "pool.h"

#include <cstdint>
#include <cstddef>
#include <array>

namespace toad {

// Maximum events buffered per audio render block (lock-free fixed ring buffer)
constexpr size_t MAX_BLOCK_EVENTS = 256;

// Event structure for sample-accurate voice / MIDI dispatch
struct TrackerEvent {
    enum Type : uint8_t {
        EVENT_NOTE_ON,
        EVENT_NOTE_OFF,
        EVENT_PARAM_CHANGE,
        EVENT_TABLE_TICK,
        EVENT_STEP_TRIGGER,
        EVENT_TRANSPORT_CHANGE
    } type{EVENT_NOTE_ON};

    uint8_t        track{0};
    uint8_t        note{0};            // Effective note (after pool + scale + transpose + tables)
    uint8_t        instrument{0};
    uint8_t        volume{0};          // Effective volume (after table scaling)
    TrackerCommand param_cmd{CMD_NONE};
    float          param_value{0.0f};
    uint32_t       sample_offset{0};   // Offset within the current audio block
};

enum TransportState : uint8_t {
    TRANSPORT_STOPPED = 0,
    TRANSPORT_PLAYING = 1,
    TRANSPORT_PAUSED  = 2
};

enum PlaybackMode : uint8_t {
    PLAY_SONG   = 0, // Full song row traversal
    PLAY_CHAIN  = 1, // Loop current chain on track 0
    PLAY_PHRASE = 2  // Loop current phrase on track 0
};

struct TrackState {
    uint8_t current_chain_id{CHAIN_EMPTY};
    uint8_t chain_step{0};              // 0..15
    uint8_t current_phrase_id{PHRASE_EMPTY};
    int8_t  chain_transpose{0};         // Signed semitones from ChainStep
    uint8_t phrase_step{0};             // 0..15
    bool    chain_ended{false};

    // Voice & Pitch Resolution State
    bool    voice_active{false};
    uint8_t raw_note{NOTE_EMPTY};       // From PhraseStep
    int8_t  resolved_note{-1};          // After Pool + Scale + Chain Transpose (-1 = Empty)
    int8_t  effective_note{-1};         // After Tables (-1 = Empty)
    uint8_t instrument_id{INST_EMPTY};
    uint8_t raw_volume{0};              // From PhraseStep (00..FF)
    uint8_t effective_volume{0};        // After Tables

    // Dual Table Modulation Players
    TablePlayer primary_table;
    TablePlayer aux_table;

    // Track Modulated Parameters
    float filter_cutoff{20000.0f};
    float filter_resonance{0.0f};
    float wavefolder_drive{0.0f};
    float pulse_width{0.5f};
    float disperser_freq{1000.0f};
    float wavetable_position{0.0f};
    float wavetable_warp{0.0f};
    float filter_drive{0.0f};

    void reset() {
        current_chain_id = CHAIN_EMPTY;
        chain_step = 0;
        current_phrase_id = PHRASE_EMPTY;
        chain_transpose = 0;
        phrase_step = 0;
        chain_ended = false;

        voice_active = false;
        raw_note = NOTE_EMPTY;
        resolved_note = -1;
        effective_note = -1;
        instrument_id = INST_EMPTY;
        raw_volume = 0;
        effective_volume = 0;

        primary_table.stop();
        aux_table.stop();

        filter_cutoff = 20000.0f;
        filter_resonance = 0.0f;
        wavefolder_drive = 0.0f;
        pulse_width = 0.5f;
        disperser_freq = 1000.0f;
        wavetable_position = 0.0f;
        wavetable_warp = 0.0f;
        filter_drive = 0.0f;
    }
};

class Engine {
public:
    Engine();

    // Setup and Timing Configuration
    void setSampleRate(float sampleRateHz);
    float getSampleRate() const { return sampleRate_; }

    void setBpm(float bpm);
    float getBpm() const { return bpm_; }

    void setGrooveMultiplier(float multiplier);
    float getGrooveMultiplier() const { return grooveMultiplier_; }

    void setTicksPerStep(uint8_t ticks);
    uint8_t getTicksPerStep() const { return ticksPerStep_; }

    // Transport Control
    void play(PlaybackMode mode = PLAY_SONG);
    void pause();
    void stop();
    bool isPlaying() const { return transportState_ == TRANSPORT_PLAYING; }
    bool isPaused() const { return transportState_ == TRANSPORT_PAUSED; }
    bool isStopped() const { return transportState_ == TRANSPORT_STOPPED; }
    TransportState getTransportState() const { return transportState_; }
    PlaybackMode getPlaybackMode() const { return playbackMode_; }

    // Position Control
    void setSongRow(uint8_t row);
    uint8_t getSongRow() const { return songRow_; }

    void setChainPosition(uint8_t track, uint8_t chainId, uint8_t step = 0);
    void setPhrasePosition(uint8_t track, uint8_t phraseId, uint8_t step = 0);

    // Song Data Binding
    void loadSong(const Song& song);
    Song& getSong() { return song_; }
    const Song& getSong() const { return song_; }

    // Track State Query
    const TrackState& getTrackState(size_t track) const;

    // Deterministic Timing & Slicing
    size_t getSamplesUntilNextTick() const;
    void advanceSampleClock(size_t samples);

    // Block Processing (Accumulates Sample-Accurate Events)
    void processBlock(size_t totalSamples);

    // Event Buffer Inspection
    size_t getEventCount() const { return eventCount_; }
    const TrackerEvent* getEvents() const { return eventBuffer_.data(); }
    void clearEvents() { eventCount_ = 0; }

    // Step Trigger & Tick Processors
    void triggerStep(size_t trackIndex, uint32_t sampleOffset = 0);
    void processTick(uint32_t sampleOffset = 0);

private:
    void updateTimingCoefficients();
    void applyTableCommand(TrackState& track, TrackerCommand cmd, uint8_t val, uint32_t sampleOffset);
    void evaluateTableModulation(TrackState& track, uint32_t sampleOffset);
    void pushEvent(const TrackerEvent& event);
    void advanceSequencerStep();
    void loadSongRow(uint8_t row);

    // Core Song Memory (Zero-allocation fixed structure)
    Song song_{};

    // Transport & Playback
    TransportState transportState_{TRANSPORT_STOPPED};
    PlaybackMode   playbackMode_{PLAY_SONG};
    uint8_t        songRow_{0};

    // Tracks
    std::array<TrackState, MAX_TRACKS> tracks_{};

    // Timing
    float   sampleRate_{44100.0f};
    float   bpm_{120.0f};
    float   grooveMultiplier_{1.0f};
    uint8_t ticksPerStep_{6}; // Standard 6 ticks per step

    // Sample-accurate Countdown
    float   samplesPerTick_{918.75f};
    float   samplesUntilTick_{918.75f};
    uint8_t tickCountdown_{6};

    // Fixed-Size Event Buffer
    std::array<TrackerEvent, MAX_BLOCK_EVENTS> eventBuffer_{};
    size_t eventCount_{0};
};

} // namespace toad
