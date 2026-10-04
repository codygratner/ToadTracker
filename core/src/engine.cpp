#include <toad/engine.h>
#include <cmath>
#include <algorithm>

namespace toad {

Engine::Engine() {
    updateTimingCoefficients();
    for (auto& track : tracks_) {
        track.reset();
    }
    for (auto& v : synthVoices_) v.setSampleRate(sampleRate_);
    for (auto& v : wavetableVoices_) v.setSampleRate(sampleRate_);
    for (auto& v : sf2Voices_) v.setSampleRate(sampleRate_);
}

void Engine::setSampleRate(float sampleRateHz) {
    if (sampleRateHz <= 0.0f) return;
    sampleRate_ = sampleRateHz;
    updateTimingCoefficients();
    for (auto& v : synthVoices_) v.setSampleRate(sampleRate_);
    for (auto& v : wavetableVoices_) v.setSampleRate(sampleRate_);
    for (auto& v : sf2Voices_) v.setSampleRate(sampleRate_);
}

void Engine::setBpm(float bpm) {
    if (bpm <= 0.0f) return;
    bpm_ = bpm;
    updateTimingCoefficients();
}

void Engine::setGrooveMultiplier(float multiplier) {
    if (multiplier <= 0.0f) return;
    grooveMultiplier_ = multiplier;
    updateTimingCoefficients();
}

void Engine::setTicksPerStep(uint8_t ticks) {
    if (ticks == 0) return;
    ticksPerStep_ = ticks;
}

void Engine::updateTimingCoefficients() {
    samplesPerTick_ = FastMath::calculateSamplesPerTick(sampleRate_, bpm_, grooveMultiplier_);
    if (samplesUntilTick_ > samplesPerTick_ || samplesUntilTick_ <= 0.0f) {
        samplesUntilTick_ = samplesPerTick_;
    }
}

void Engine::loadSong(const Song& song) {
    song_ = song;
    setBpm(song_.bpm);
    setTicksPerStep(song_.groove > 0 ? song_.groove : 6);
    stop();
}

const TrackState& Engine::getTrackState(size_t track) const {
    static TrackState dummy;
    if (track < MAX_TRACKS) {
        return tracks_[track];
    }
    return dummy;
}

void Engine::play(PlaybackMode mode) {
    playbackMode_ = mode;
    transportState_ = TRANSPORT_PLAYING;
    tickCountdown_ = ticksPerStep_;
    samplesUntilTick_ = samplesPerTick_;

    if (playbackMode_ == PLAY_SONG) {
        loadSongRow(songRow_);
    } else if (playbackMode_ == PLAY_CHAIN) {
        if (tracks_[0].current_chain_id == CHAIN_EMPTY) {
            tracks_[0].current_chain_id = 0;
            tracks_[0].chain_step = 0;
        }
        tracks_[0].chain_ended = false;
        if (tracks_[0].current_chain_id < TOTAL_CHAINS) {
            const Chain& chain = song_.chains[tracks_[0].current_chain_id];
            tracks_[0].current_phrase_id = chain.steps[tracks_[0].chain_step].phrase_id;
            tracks_[0].chain_transpose = chain.steps[tracks_[0].chain_step].transpose;
            tracks_[0].phrase_step = 0;
        }
    } else if (playbackMode_ == PLAY_PHRASE) {
        if (tracks_[0].current_phrase_id == PHRASE_EMPTY) {
            tracks_[0].current_phrase_id = 0;
        }
        tracks_[0].phrase_step = 0;
        tracks_[0].chain_ended = false;
    }

    TrackerEvent ev;
    ev.type = TrackerEvent::EVENT_TRANSPORT_CHANGE;
    ev.param_value = static_cast<float>(transportState_);
    pushEvent(ev);

    // Immediately trigger step 0 across active tracks
    for (size_t t = 0; t < MAX_TRACKS; ++t) {
        triggerStep(t, 0);
    }
}

void Engine::pause() {
    if (transportState_ == TRANSPORT_PLAYING) {
        transportState_ = TRANSPORT_PAUSED;
        TrackerEvent ev;
        ev.type = TrackerEvent::EVENT_TRANSPORT_CHANGE;
        ev.param_value = static_cast<float>(transportState_);
        pushEvent(ev);
    }
}

void Engine::stop() {
    transportState_ = TRANSPORT_STOPPED;
    songRow_ = 0;
    tickCountdown_ = ticksPerStep_;
    samplesUntilTick_ = samplesPerTick_;

    for (auto& track : tracks_) {
        track.reset();
    }
    for (auto& v : synthVoices_) v.reset();
    for (auto& v : wavetableVoices_) v.reset();
    for (auto& v : sf2Voices_) v.reset();

    TrackerEvent ev;
    ev.type = TrackerEvent::EVENT_TRANSPORT_CHANGE;
    ev.param_value = static_cast<float>(transportState_);
    pushEvent(ev);
}

void Engine::setSongRow(uint8_t row) {
    songRow_ = row;
    if (isPlaying() && playbackMode_ == PLAY_SONG) {
        loadSongRow(songRow_);
    }
}

void Engine::setChainPosition(uint8_t track, uint8_t chainId, uint8_t step) {
    if (track >= MAX_TRACKS) return;
    TrackState& tr = tracks_[track];
    tr.current_chain_id = chainId;
    tr.chain_step = step % 16;
    tr.chain_ended = (chainId == CHAIN_EMPTY);

    if (!tr.chain_ended && chainId < TOTAL_CHAINS) {
        const Chain& chain = song_.chains[chainId];
        tr.current_phrase_id = chain.steps[tr.chain_step].phrase_id;
        tr.chain_transpose = chain.steps[tr.chain_step].transpose;
        tr.phrase_step = 0;
        if (tr.current_phrase_id == PHRASE_EMPTY) {
            tr.chain_ended = true;
        }
    }
}

void Engine::setPhrasePosition(uint8_t track, uint8_t phraseId, uint8_t step) {
    if (track >= MAX_TRACKS) return;
    TrackState& tr = tracks_[track];
    tr.current_phrase_id = phraseId;
    tr.phrase_step = step % PHRASE_STEPS;
    tr.chain_ended = (phraseId == PHRASE_EMPTY);
}

void Engine::loadSongRow(uint8_t row) {
    if (row >= TOTAL_SONG_ROWS) return;
    const SongRow& sr = song_.rows[row];

    for (size_t t = 0; t < MAX_TRACKS; ++t) {
        TrackState& tr = tracks_[t];
        tr.current_chain_id = sr.chain_ids[t];
        tr.chain_step = 0;
        tr.chain_ended = (sr.chain_ids[t] == CHAIN_EMPTY);

        if (!tr.chain_ended && tr.current_chain_id < TOTAL_CHAINS) {
            const Chain& chain = song_.chains[tr.current_chain_id];
            tr.current_phrase_id = chain.steps[0].phrase_id;
            tr.chain_transpose = chain.steps[0].transpose;
            tr.phrase_step = 0;
            if (tr.current_phrase_id == PHRASE_EMPTY) {
                tr.chain_ended = true;
            }
        }
    }
}

void Engine::pushEvent(const TrackerEvent& event) {
    if (eventCount_ < MAX_BLOCK_EVENTS) {
        eventBuffer_[eventCount_++] = event;
    }
}

void Engine::applyTableCommand(TrackState& track, TrackerCommand cmd, uint8_t val, uint32_t sampleOffset) {
    switch (cmd) {
        case CMD_FCUT:
            // 20 Hz to 20,000 Hz exponential sweep
            track.filter_cutoff = 20.0f * FastMath::fast2Exp(static_cast<float>(val) * (9.965784f / 255.0f));
            break;

        case CMD_FRES:
            track.filter_resonance = static_cast<float>(val) / 255.0f;
            break;

        case CMD_FOLD:
            track.wavefolder_drive = static_cast<float>(val) / 255.0f;
            break;

        case CMD_PWM_:
            track.pulse_width = 0.01f + 0.98f * (static_cast<float>(val) / 255.0f);
            break;

        case CMD_DISP:
            track.disperser_freq = 20.0f * FastMath::fast2Exp(static_cast<float>(val) * (8.965784f / 255.0f));
            break;

        case CMD_WPOS:
            track.wavetable_position = static_cast<float>(val) * (63.0f / 255.0f);
            break;

        case CMD_WAMT:
            track.wavetable_warp = static_cast<float>(val) / 255.0f;
            break;

        case CMD_FDRV:
            track.filter_drive = static_cast<float>(val) * (4.0f / 255.0f);
            break;

        case CMD_ATBL:
            if (val < TOTAL_TABLES) {
                track.aux_table.trigger(val, song_.tables[val].speed);
            }
            break;

        default:
            break;
    }

    TrackerEvent ev;
    ev.type = TrackerEvent::EVENT_PARAM_CHANGE;
    ev.track = static_cast<uint8_t>(&track - &tracks_[0]);
    ev.param_cmd = cmd;
    ev.param_value = static_cast<float>(val);
    ev.sample_offset = sampleOffset;
    pushEvent(ev);
}

void Engine::evaluateTableModulation(TrackState& track, uint32_t sampleOffset) {
    int8_t table_transpose = 0;
    float vol_mult = 1.0f;

    // 1. Primary Table Modulation
    if (track.primary_table.active && track.primary_table.current_row >= 0 &&
        track.primary_table.table_id < TOTAL_TABLES) {
        const Table& pt = song_.tables[track.primary_table.table_id];
        const TableRow& pr = pt.rows[track.primary_table.current_row];

        table_transpose += pr.transpose;
        if (pr.volume > 0) {
            vol_mult *= (static_cast<float>(pr.volume) / 255.0f);
        }
        if (pr.cmd1 != CMD_NONE) applyTableCommand(track, pr.cmd1, pr.val1, sampleOffset);
        if (pr.cmd2 != CMD_NONE) applyTableCommand(track, pr.cmd2, pr.val2, sampleOffset);
    }

    // 2. Aux Table Modulation
    if (track.aux_table.active && track.aux_table.current_row >= 0 &&
        track.aux_table.table_id < TOTAL_TABLES) {
        const Table& at = song_.tables[track.aux_table.table_id];
        const TableRow& ar = at.rows[track.aux_table.current_row];

        table_transpose += ar.transpose;
        if (ar.volume > 0) {
            vol_mult *= (static_cast<float>(ar.volume) / 255.0f);
        }
        if (ar.cmd1 != CMD_NONE) applyTableCommand(track, ar.cmd1, ar.val1, sampleOffset);
        if (ar.cmd2 != CMD_NONE) applyTableCommand(track, ar.cmd2, ar.val2, sampleOffset);
    }

    // Apply combined transposition and volume scaling
    int eff_note = track.resolved_note + table_transpose;
    if (eff_note < 0) eff_note = 0;
    if (eff_note > 127) eff_note = 127;
    track.effective_note = static_cast<int8_t>(eff_note);

    float eff_vol = static_cast<float>(track.raw_volume) * vol_mult;
    if (eff_vol < 0.0f) eff_vol = 0.0f;
    if (eff_vol > 255.0f) eff_vol = 255.0f;
    track.effective_volume = static_cast<uint8_t>(eff_vol);

    // Update DSP Voice Modulation Parameters
    size_t trackIdx = static_cast<size_t>(&track - &tracks_[0]);
    if (trackIdx < MAX_TRACKS) {
        const Instrument& inst = song_.instruments[track.instrument_id < TOTAL_INSTRUMENTS ? track.instrument_id : 0];
        if (inst.type == INST_TYPE_INTERNAL_SYNTH) {
            synthVoices_[trackIdx].setPitch(static_cast<float>(track.effective_note));
            synthVoices_[trackIdx].setFilterParameters(inst.filter_type, track.filter_cutoff, track.filter_resonance, track.filter_drive);
            synthVoices_[trackIdx].setPulseWidth(track.pulse_width);
            synthVoices_[trackIdx].setFoldDrive(track.wavefolder_drive);
            synthVoices_[trackIdx].setDisperser(track.disperser_freq);
        } else if (inst.type == INST_TYPE_WAVETABLE) {
            wavetableVoices_[trackIdx].setPitch(static_cast<float>(track.effective_note));
            wavetableVoices_[trackIdx].setFilterParameters(inst.filter_type, track.filter_cutoff, track.filter_resonance, track.filter_drive);
            wavetableVoices_[trackIdx].setPosition(track.wavetable_position);
            wavetableVoices_[trackIdx].setWarp(inst.wavetable.warp_mode, track.wavetable_warp);
        } else if (inst.type == INST_TYPE_SF2_MULTISAMPLE) {
            sf2Voices_[trackIdx].setPitch(static_cast<float>(track.effective_note));
            sf2Voices_[trackIdx].setFilterParameters(inst.filter_type, track.filter_cutoff, track.filter_resonance, track.filter_drive);
        }
    }
}

void Engine::triggerStep(size_t trackIndex, uint32_t sampleOffset) {
    if (trackIndex >= MAX_TRACKS) return;
    TrackState& track = tracks_[trackIndex];

    if (track.chain_ended || track.current_phrase_id >= TOTAL_PHRASES) return;

    const Phrase& phrase = song_.phrases[track.current_phrase_id];
    const PhraseStep& step = phrase.steps[track.phrase_step];

    // If step has no note, evaluate any FX in the step and return
    if (step.note == NOTE_EMPTY) {
        for (int i = 0; i < 3; ++i) {
            if (step.fx[i].cmd != CMD_NONE) {
                applyTableCommand(track, step.fx[i].cmd, step.fx[i].val, sampleOffset);
            }
        }
        return;
    }

    // --- STEP TRIGGER PIPELINE (Section 5.1) ---
    track.raw_note = step.note;
    track.instrument_id = step.instrument;
    track.raw_volume = (step.volume != 0) ? step.volume : 0xFF;

    const Instrument& inst = song_.instruments[step.instrument < TOTAL_INSTRUMENTS ? step.instrument : 0];

    // 1. Probabilistic Note Pool Resolution
    InstrumentNotePool poolCopy = inst.note_pool;
    int8_t pooled_note = evaluateNotePool(poolCopy, static_cast<int8_t>(step.note));

    // 2. Scale Quantization
    const ScaleDefinition& scale = inst.scale.enabled ? inst.scale : song_.master_scale;
    int8_t quantized_note = quantizeToScale(scale, pooled_note);

    // 3. Chain Semitone Transposition
    int8_t transposed_note = quantized_note + track.chain_transpose;
    if (transposed_note < 0) transposed_note = 0;
    if (transposed_note > 127) transposed_note = 127;

    track.resolved_note = transposed_note;
    track.effective_note = transposed_note;
    track.effective_volume = track.raw_volume;
    track.voice_active = true;

    // 4. Process Step FX Commands (Including CMD_ATBL trigger)
    for (int i = 0; i < 3; ++i) {
        if (step.fx[i].cmd != CMD_NONE) {
            applyTableCommand(track, step.fx[i].cmd, step.fx[i].val, sampleOffset);
        }
    }

    // 5. Trigger Instrument Primary Table
    if (inst.table_id < TOTAL_TABLES) {
        track.primary_table.trigger(inst.table_id, song_.tables[inst.table_id].speed);
    }

    // 6. Evaluate Row 0 of triggered tables immediately on the note trigger
    evaluateTableModulation(track, sampleOffset);

    // 7. Dispatch Note to Native DSP Voice Engine
    if (inst.type == INST_TYPE_INTERNAL_SYNTH) {
        synthVoices_[trackIndex].noteOn(static_cast<uint8_t>(track.effective_note), track.effective_volume, inst.synth, inst);
    } else if (inst.type == INST_TYPE_WAVETABLE) {
        wavetableVoices_[trackIndex].noteOn(static_cast<uint8_t>(track.effective_note), track.effective_volume, inst.wavetable, inst);
    } else if (inst.type == INST_TYPE_SF2_MULTISAMPLE) {
        sf2Voices_[trackIndex].noteOn(static_cast<uint8_t>(track.effective_note), track.effective_volume, inst);
    }

    // 8. Push NOTE_ON Event
    TrackerEvent ev;
    ev.type = TrackerEvent::EVENT_NOTE_ON;
    ev.track = static_cast<uint8_t>(trackIndex);
    ev.note = static_cast<uint8_t>(track.effective_note);
    ev.instrument = track.instrument_id;
    ev.volume = track.effective_volume;
    ev.sample_offset = sampleOffset;
    pushEvent(ev);
}

void Engine::advanceSequencerStep() {
    if (playbackMode_ == PLAY_PHRASE) {
        tracks_[0].phrase_step = (tracks_[0].phrase_step + 1) % PHRASE_STEPS;
        return;
    }

    if (playbackMode_ == PLAY_CHAIN) {
        TrackState& tr = tracks_[0];
        tr.phrase_step++;
        if (tr.phrase_step >= PHRASE_STEPS) {
            tr.phrase_step = 0;
            tr.chain_step = (tr.chain_step + 1) % 16;
            if (tr.current_chain_id < TOTAL_CHAINS) {
                const Chain& chain = song_.chains[tr.current_chain_id];
                if (chain.steps[tr.chain_step].phrase_id != PHRASE_EMPTY) {
                    tr.current_phrase_id = chain.steps[tr.chain_step].phrase_id;
                    tr.chain_transpose = chain.steps[tr.chain_step].transpose;
                } else {
                    tr.chain_step = 0;
                    tr.current_phrase_id = chain.steps[0].phrase_id;
                    tr.chain_transpose = chain.steps[0].transpose;
                }
            }
        }
        return;
    }

    // Full Song Traversal
    for (size_t t = 0; t < MAX_TRACKS; ++t) {
        TrackState& tr = tracks_[t];
        if (tr.chain_ended) continue;

        tr.phrase_step++;
        if (tr.phrase_step >= PHRASE_STEPS) {
            tr.phrase_step = 0;
            tr.chain_step++;

            if (tr.chain_step >= 16) {
                tr.chain_ended = true;
            } else if (tr.current_chain_id < TOTAL_CHAINS) {
                const Chain& chain = song_.chains[tr.current_chain_id];
                if (chain.steps[tr.chain_step].phrase_id == PHRASE_EMPTY) {
                    tr.chain_ended = true;
                } else {
                    tr.current_phrase_id = chain.steps[tr.chain_step].phrase_id;
                    tr.chain_transpose = chain.steps[tr.chain_step].transpose;
                }
            }
        }
    }

    // Check if song row should advance (driven by track 0 or all tracks ended)
    if (tracks_[0].chain_ended) {
        songRow_++;
        if (songRow_ >= TOTAL_SONG_ROWS) {
            songRow_ = 0;
        }
        loadSongRow(songRow_);
    }
}

void Engine::processTick(uint32_t sampleOffset) {
    // 1. Advance table players and evaluate per-tick table modulation
    for (size_t t = 0; t < MAX_TRACKS; ++t) {
        TrackState& track = tracks_[t];
        bool tableChanged = false;

        if (track.primary_table.active && track.primary_table.table_id < TOTAL_TABLES) {
            tableChanged |= track.primary_table.tick(song_.tables[track.primary_table.table_id]);
        }
        if (track.aux_table.active && track.aux_table.table_id < TOTAL_TABLES) {
            tableChanged |= track.aux_table.tick(song_.tables[track.aux_table.table_id]);
        }

        if (tableChanged) {
            evaluateTableModulation(track, sampleOffset);
        }
    }

    // 2. Countdown ticks until next phrase step boundary
    tickCountdown_--;
    if (tickCountdown_ == 0) {
        tickCountdown_ = ticksPerStep_;
        advanceSequencerStep();
        for (size_t t = 0; t < MAX_TRACKS; ++t) {
            triggerStep(t, sampleOffset);
        }
    }
}

size_t Engine::getSamplesUntilNextTick() const {
    if (samplesUntilTick_ <= 0.0f) return 1;
    return static_cast<size_t>(std::ceil(samplesUntilTick_));
}

void Engine::advanceSampleClock(size_t samples) {
    samplesUntilTick_ -= static_cast<float>(samples);
}

void Engine::processBlock(size_t totalSamples) {
    clearEvents();
    if (transportState_ != TRANSPORT_PLAYING) return;

    size_t samplesProcessed = 0;

    while (samplesProcessed < totalSamples) {
        size_t samplesUntilTick = getSamplesUntilNextTick();
        size_t slice = (totalSamples - samplesProcessed < samplesUntilTick)
                       ? (totalSamples - samplesProcessed)
                       : samplesUntilTick;

        samplesProcessed += slice;
        advanceSampleClock(slice);

        if (samplesUntilTick_ <= 0.0f) {
            processTick(static_cast<uint32_t>(samplesProcessed));
            samplesUntilTick_ += samplesPerTick_;
        }
    }
}

void Engine::renderVoices(float* outLeft, float* outRight, size_t numFrames) {
    if (!outLeft || !outRight || numFrames == 0) return;

    for (size_t i = 0; i < numFrames; ++i) {
        float mix = 0.0f;
        for (size_t t = 0; t < MAX_TRACKS; ++t) {
            const auto& track = tracks_[t];
            if (track.voice_active) {
                const Instrument& inst = song_.instruments[track.instrument_id < TOTAL_INSTRUMENTS ? track.instrument_id : 0];
                float sample = 0.0f;
                switch (inst.type) {
                    case INST_TYPE_INTERNAL_SYNTH:  sample = synthVoices_[t].process(); break;
                    case INST_TYPE_WAVETABLE:       sample = wavetableVoices_[t].process(); break;
                    case INST_TYPE_SF2_MULTISAMPLE: sample = sf2Voices_[t].process(); break;
                    default: break;
                }
                mix += sample * (static_cast<float>(track.effective_volume) / 255.0f);
            }
        }
        // Master Output Protection: zero-latency branchless fastTanh / masterClip
        float safeSample = FastMath::masterClip(mix);
        outLeft[i] = safeSample;
        outRight[i] = safeSample;
    }
}

void Engine::renderVoicesInterleaved(float* outInterleavedStereo, size_t numFrames) {
    if (!outInterleavedStereo || numFrames == 0) return;

    for (size_t i = 0; i < numFrames; ++i) {
        float mix = 0.0f;
        for (size_t t = 0; t < MAX_TRACKS; ++t) {
            const auto& track = tracks_[t];
            if (track.voice_active) {
                const Instrument& inst = song_.instruments[track.instrument_id < TOTAL_INSTRUMENTS ? track.instrument_id : 0];
                float sample = 0.0f;
                switch (inst.type) {
                    case INST_TYPE_INTERNAL_SYNTH:  sample = synthVoices_[t].process(); break;
                    case INST_TYPE_WAVETABLE:       sample = wavetableVoices_[t].process(); break;
                    case INST_TYPE_SF2_MULTISAMPLE: sample = sf2Voices_[t].process(); break;
                    default: break;
                }
                mix += sample * (static_cast<float>(track.effective_volume) / 255.0f);
            }
        }
        float safeSample = FastMath::masterClip(mix);
        outInterleavedStereo[2 * i] = safeSample;
        outInterleavedStereo[2 * i + 1] = safeSample;
    }
}

void Engine::renderBlockDeterministic(float* outInterleavedStereo, size_t totalFrames) {
    if (!outInterleavedStereo || totalFrames == 0) return;

    if (transportState_ != TRANSPORT_PLAYING) {
        std::fill(outInterleavedStereo, outInterleavedStereo + totalFrames * 2, 0.0f);
        return;
    }

    size_t samplesProcessed = 0;

    while (samplesProcessed < totalFrames) {
        size_t samplesUntilTick = getSamplesUntilNextTick();
        size_t slice = (totalFrames - samplesProcessed < samplesUntilTick)
                       ? (totalFrames - samplesProcessed)
                       : samplesUntilTick;

        renderVoicesInterleaved(outInterleavedStereo + samplesProcessed * 2, slice);

        samplesProcessed += slice;
        advanceSampleClock(slice);

        if (samplesUntilTick_ <= 0.0f) {
            processTick(static_cast<uint32_t>(samplesProcessed));
            samplesUntilTick_ += samplesPerTick_;
        }
    }
}

void Engine::renderBlockDeterministic(float* outLeft, float* outRight, size_t totalFrames) {
    if (!outLeft || !outRight || totalFrames == 0) return;

    if (transportState_ != TRANSPORT_PLAYING) {
        std::fill(outLeft, outLeft + totalFrames, 0.0f);
        std::fill(outRight, outRight + totalFrames, 0.0f);
        return;
    }

    size_t samplesProcessed = 0;

    while (samplesProcessed < totalFrames) {
        size_t samplesUntilTick = getSamplesUntilNextTick();
        size_t slice = (totalFrames - samplesProcessed < samplesUntilTick)
                       ? (totalFrames - samplesProcessed)
                       : samplesUntilTick;

        renderVoices(outLeft + samplesProcessed, outRight + samplesProcessed, slice);

        samplesProcessed += slice;
        advanceSampleClock(slice);

        if (samplesUntilTick_ <= 0.0f) {
            processTick(static_cast<uint32_t>(samplesProcessed));
            samplesUntilTick_ += samplesPerTick_;
        }
    }
}

const SynthVoice& Engine::getSynthVoice(size_t track) const {
    return synthVoices_[track < MAX_TRACKS ? track : 0];
}

const WavetableVoice& Engine::getWavetableVoice(size_t track) const {
    return wavetableVoices_[track < MAX_TRACKS ? track : 0];
}

const SF2Voice& Engine::getSF2Voice(size_t track) const {
    return sf2Voices_[track < MAX_TRACKS ? track : 0];
}

} // namespace toad

