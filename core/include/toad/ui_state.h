#pragma once

#include "toad/types.h"
#include "toad/engine.h"
#include "toad/input.h"
#include <cstdint>
#include <algorithm>

namespace toad {

// ============================================================================
// TRACKER VIEWS
// ============================================================================
enum TrackerView : uint8_t {
    VIEW_SONG = 0,
    VIEW_CHAIN,
    VIEW_PHRASE,
    VIEW_TABLE,
    VIEW_INSTRUMENT,
    VIEW_SYNTH,
    VIEW_PROJECT,
    VIEW_SETTINGS,
    VIEW_COUNT
};

// ============================================================================
// UI STATE MACHINE
// Manages cursor, navigation hierarchy, editing buffers, and parameter focus
// ============================================================================
class UIState {
public:
    UIState() {
        reset();
    }

    void reset() {
        current_view = VIEW_SONG;
        previous_view = VIEW_SONG;
        active_track = 0;
        cursor_row = 0;
        cursor_col = 0;

        selected_song_row = 0;
        selected_chain_id = 0;
        selected_chain_step = 0;
        selected_phrase_id = 0;
        selected_phrase_step = 0;
        selected_table_id = 0;
        selected_table_row = 0;
        selected_instrument_id = 0;
        selected_synth_param = 0;

        octave_offset = 0;
        edit_step = 1;
        clipboard_note = NOTE_EMPTY;
        clipboard_inst = 0;
        clipboard_vol = 0xFF;

        for (size_t i = 0; i < MAX_TRACKS; ++i) {
            track_muted[i] = false;
            track_soloed[i] = false;
            vu_levels[i] = 0.0f;
        }

        is_remapping = false;
        remap_action_index = 0;
        settings_scroll_row = 0;
    }

    // --- NAVIGATION HIERARCHY ---
    void pageNext(const Song& song) {
        switch (current_view) {
            case VIEW_SONG: {
                // Enter Chain under cursor
                uint8_t chain_id = song.rows[cursor_row].chain_ids[active_track];
                if (chain_id < TOTAL_CHAINS) {
                    selected_chain_id = chain_id;
                    current_view = VIEW_CHAIN;
                    cursor_row = 0;
                    cursor_col = 0;
                } else {
                    current_view = VIEW_CHAIN;
                }
                break;
            }
            case VIEW_CHAIN: {
                // Enter Phrase under cursor
                const Chain& chain = song.chains[selected_chain_id < TOTAL_CHAINS ? selected_chain_id : 0];
                uint8_t phrase_id = chain.steps[cursor_row < 16 ? cursor_row : 0].phrase_id;
                if (phrase_id < TOTAL_PHRASES) {
                    selected_phrase_id = phrase_id;
                    current_view = VIEW_PHRASE;
                    cursor_row = 0;
                    cursor_col = 0;
                } else {
                    current_view = VIEW_PHRASE;
                }
                break;
            }
            case VIEW_PHRASE: {
                // Enter Instrument under cursor
                const Phrase& ph = song.phrases[selected_phrase_id < TOTAL_PHRASES ? selected_phrase_id : 0];
                uint8_t inst_id = ph.steps[cursor_row < 16 ? cursor_row : 0].instrument;
                if (inst_id < TOTAL_INSTRUMENTS) {
                    selected_instrument_id = inst_id;
                }
                current_view = VIEW_INSTRUMENT;
                cursor_row = 0;
                cursor_col = 0;
                break;
            }
            case VIEW_INSTRUMENT: {
                const Instrument& inst = song.instruments[selected_instrument_id < TOTAL_INSTRUMENTS ? selected_instrument_id : 0];
                if (inst.table_id < TOTAL_TABLES) {
                    selected_table_id = inst.table_id;
                    current_view = VIEW_TABLE;
                } else {
                    current_view = VIEW_SYNTH;
                }
                cursor_row = 0;
                cursor_col = 0;
                break;
            }
            case VIEW_TABLE:
                current_view = VIEW_SYNTH;
                cursor_row = 0;
                cursor_col = 0;
                break;
            case VIEW_SYNTH:
                current_view = VIEW_PROJECT;
                cursor_row = 0;
                cursor_col = 0;
                break;
            case VIEW_PROJECT:
                current_view = VIEW_SETTINGS;
                cursor_row = 0;
                cursor_col = 0;
                break;
            case VIEW_SETTINGS:
                current_view = VIEW_SONG;
                cursor_row = selected_song_row;
                cursor_col = active_track;
                break;
            default:
                current_view = VIEW_SONG;
                break;
        }
    }

    void pagePrev() {
        switch (current_view) {
            case VIEW_SETTINGS:
                current_view = VIEW_PROJECT;
                cursor_row = 0;
                cursor_col = 0;
                break;
            case VIEW_PROJECT:
                current_view = VIEW_SYNTH;
                cursor_row = 0;
                cursor_col = 0;
                break;
            case VIEW_TABLE:
            case VIEW_SYNTH:
                current_view = VIEW_INSTRUMENT;
                cursor_row = 0;
                cursor_col = 0;
                break;
            case VIEW_INSTRUMENT:
                current_view = VIEW_PHRASE;
                cursor_row = selected_phrase_step;
                cursor_col = 0;
                break;
            case VIEW_PHRASE:
                current_view = VIEW_CHAIN;
                cursor_row = selected_chain_step;
                cursor_col = 0;
                break;
            case VIEW_CHAIN:
                current_view = VIEW_SONG;
                cursor_row = selected_song_row;
                cursor_col = active_track;
                break;
            case VIEW_SONG:
            default:
                break;
        }
    }

    // Fast Modifier Toggles
    void toggleViewMod() {
        // Song <-> Phrase rapid jump
        if (current_view == VIEW_SONG) {
            current_view = VIEW_PHRASE;
        } else {
            current_view = VIEW_SONG;
        }
    }

    void toggleSubMod() {
        // Synth <-> Table <-> Instrument cycle
        if (current_view == VIEW_SYNTH) {
            current_view = VIEW_TABLE;
        } else if (current_view == VIEW_TABLE) {
            current_view = VIEW_INSTRUMENT;
        } else {
            current_view = VIEW_SYNTH;
        }
    }

    // --- CURSOR MOVEMENT ---
    void moveCursor(int dRow, int dCol) {
        cursor_row = std::max(0, cursor_row + dRow);
        cursor_col = std::max(0, cursor_col + dCol);

        // Clamp depending on view
        switch (current_view) {
            case VIEW_SONG:
                cursor_row = std::min(cursor_row, static_cast<int>(TOTAL_SONG_ROWS - 1));
                cursor_col = std::min(cursor_col, static_cast<int>(MAX_TRACKS - 1));
                active_track = static_cast<uint8_t>(cursor_col);
                selected_song_row = static_cast<uint8_t>(cursor_row);
                break;

            case VIEW_CHAIN:
                cursor_row = std::min(cursor_row, 15);
                cursor_col = std::min(cursor_col, 1); // Col 0: Phrase ID, Col 1: Transpose
                selected_chain_step = static_cast<uint8_t>(cursor_row);
                break;

            case VIEW_PHRASE:
                cursor_row = std::min(cursor_row, 15);
                cursor_col = std::min(cursor_col, 8); // 0: Note, 1: Inst, 2: Vol, 3: FX1Cmd, 4: FX1Val, 5: FX2Cmd, 6: FX2Val, 7: FX3Cmd, 8: FX3Val
                selected_phrase_step = static_cast<uint8_t>(cursor_row);
                break;

            case VIEW_TABLE:
                cursor_row = std::min(cursor_row, 15);
                cursor_col = std::min(cursor_col, 5); // 0: Transpose, 1: Volume, 2: FX1Cmd, 3: FX1Val, 4: FX2Cmd, 5: FX2Val
                selected_table_row = static_cast<uint8_t>(cursor_row);
                break;

            case VIEW_INSTRUMENT:
                cursor_row = std::min(cursor_row, 7); // Instrument params
                cursor_col = 0;
                break;

            case VIEW_SYNTH:
                cursor_row = std::min(cursor_row, 7); // Synth params
                cursor_col = 0;
                selected_synth_param = static_cast<uint8_t>(cursor_row);
                break;

            case VIEW_PROJECT:
                cursor_row = std::min(cursor_row, 5);
                cursor_col = 0;
                break;

            case VIEW_SETTINGS:
                cursor_row = std::min(cursor_row, 17);
                cursor_col = 0;
                remap_action_index = cursor_row;
                break;

            default:
                break;
        }
    }

    // --- VALUE EDITING ---
    void editValue(Song& song, int delta, bool coarse = false) {
        int step_mult = coarse ? 16 : 1;
        int d = delta * step_mult;

        switch (current_view) {
            case VIEW_SONG: {
                uint8_t& val = song.rows[cursor_row].chain_ids[cursor_col];
                if (val == CHAIN_EMPTY && delta > 0) {
                    val = 0;
                } else if (val == 0 && delta < 0) {
                    val = CHAIN_EMPTY;
                } else if (val != CHAIN_EMPTY) {
                    int nv = static_cast<int>(val) + d;
                    if (nv < 0) val = CHAIN_EMPTY;
                    else if (nv >= static_cast<int>(TOTAL_CHAINS)) val = TOTAL_CHAINS - 1;
                    else val = static_cast<uint8_t>(nv);
                }
                break;
            }

            case VIEW_CHAIN: {
                Chain& ch = song.chains[selected_chain_id < TOTAL_CHAINS ? selected_chain_id : 0];
                if (cursor_col == 0) {
                    uint8_t& phrase = ch.steps[cursor_row].phrase_id;
                    if (phrase == PHRASE_EMPTY && delta > 0) {
                        phrase = 0;
                    } else if (phrase == 0 && delta < 0) {
                        phrase = PHRASE_EMPTY;
                    } else if (phrase != PHRASE_EMPTY) {
                        int nv = static_cast<int>(phrase) + d;
                        if (nv < 0) phrase = PHRASE_EMPTY;
                        else if (nv >= static_cast<int>(TOTAL_PHRASES)) phrase = TOTAL_PHRASES - 1;
                        else phrase = static_cast<uint8_t>(nv);
                    }
                } else if (cursor_col == 1) {
                    int8_t& trans = ch.steps[cursor_row].transpose;
                    int nt = static_cast<int>(trans) + (coarse ? delta * 12 : delta);
                    if (nt < -96) nt = -96;
                    if (nt > 96) nt = 96;
                    trans = static_cast<int8_t>(nt);
                }
                break;
            }

            case VIEW_PHRASE: {
                Phrase& ph = song.phrases[selected_phrase_id < TOTAL_PHRASES ? selected_phrase_id : 0];
                PhraseStep& step = ph.steps[cursor_row];

                if (cursor_col == 0) { // Note
                    if (step.note == NOTE_EMPTY && delta > 0) {
                        step.note = 60 + octave_offset * 12; // Default C-4
                    } else if (step.note != NOTE_EMPTY) {
                        int nn = static_cast<int>(step.note) + (coarse ? delta * 12 : delta);
                        if (nn < 0) step.note = NOTE_EMPTY;
                        else if (nn > 127) step.note = 127;
                        else step.note = static_cast<uint8_t>(nn);
                    }
                } else if (cursor_col == 1) { // Instrument
                    int ni = static_cast<int>(step.instrument) + d;
                    if (ni < 0) ni = 0;
                    if (ni >= static_cast<int>(TOTAL_INSTRUMENTS)) ni = TOTAL_INSTRUMENTS - 1;
                    step.instrument = static_cast<uint8_t>(ni);
                } else if (cursor_col == 2) { // Volume
                    int nv = static_cast<int>(step.volume) + d;
                    if (nv < 0) nv = 0;
                    if (nv > 255) nv = 255;
                    step.volume = static_cast<uint8_t>(nv);
                } else if (cursor_col == 3 || cursor_col == 5 || cursor_col == 7) { // FX Cmd
                    int fxIdx = (cursor_col - 3) / 2;
                    int ncmd = static_cast<int>(step.fx[fxIdx].cmd) + delta;
                    if (ncmd < 0) ncmd = 0;
                    if (ncmd > CMD_HOP_) ncmd = CMD_HOP_;
                    step.fx[fxIdx].cmd = static_cast<TrackerCommand>(ncmd);
                } else if (cursor_col == 4 || cursor_col == 6 || cursor_col == 8) { // FX Val
                    int fxIdx = (cursor_col - 4) / 2;
                    int nval = static_cast<int>(step.fx[fxIdx].val) + d;
                    if (nval < 0) nval = 0;
                    if (nval > 255) nval = 255;
                    step.fx[fxIdx].val = static_cast<uint8_t>(nval);
                }
                break;
            }

            case VIEW_TABLE: {
                Table& tb = song.tables[selected_table_id < TOTAL_TABLES ? selected_table_id : 0];
                TableRow& tr = tb.rows[cursor_row];
                if (cursor_col == 0) { // Transpose
                    int nt = static_cast<int>(tr.transpose) + (coarse ? delta * 12 : delta);
                    if (nt < -96) nt = -96;
                    if (nt > 96) nt = 96;
                    tr.transpose = static_cast<int8_t>(nt);
                } else if (cursor_col == 1) { // Volume
                    int nv = static_cast<int>(tr.volume) + d;
                    if (nv < 0) nv = 0;
                    if (nv > 255) nv = 255;
                    tr.volume = static_cast<uint8_t>(nv);
                } else if (cursor_col == 2 || cursor_col == 4) { // Cmd
                    int cIdx = (cursor_col - 2) / 2;
                    TrackerCommand& cmd = (cIdx == 0) ? tr.cmd1 : tr.cmd2;
                    int nc = static_cast<int>(cmd) + delta;
                    if (nc < 0) nc = 0;
                    if (nc > CMD_HOP_) nc = CMD_HOP_;
                    cmd = static_cast<TrackerCommand>(nc);
                } else if (cursor_col == 3 || cursor_col == 5) { // Val
                    int cIdx = (cursor_col - 3) / 2;
                    uint8_t& val = (cIdx == 0) ? tr.val1 : tr.val2;
                    int nv = static_cast<int>(val) + d;
                    if (nv < 0) nv = 0;
                    if (nv > 255) nv = 255;
                    val = static_cast<uint8_t>(nv);
                }
                break;
            }

            case VIEW_INSTRUMENT: {
                Instrument& inst = song.instruments[selected_instrument_id < TOTAL_INSTRUMENTS ? selected_instrument_id : 0];
                if (cursor_row == 0) { // Type
                    int nt = static_cast<int>(inst.type) + delta;
                    if (nt < 0) nt = 0;
                    if (nt > INST_TYPE_SF2_MULTISAMPLE) nt = INST_TYPE_SF2_MULTISAMPLE;
                    inst.type = static_cast<InstrumentType>(nt);
                } else if (cursor_row == 1) { // Filter Type
                    int nft = static_cast<int>(inst.filter_type) + delta;
                    if (nft < 0) nft = 0;
                    if (nft > FLT_COMB_NEG) nft = FLT_COMB_NEG;
                    inst.filter_type = static_cast<FilterType>(nft);
                } else if (cursor_row == 2) { // Table ID
                    int ntb = static_cast<int>(inst.table_id) + delta;
                    if (ntb < 0) ntb = 0;
                    if (ntb >= static_cast<int>(TOTAL_TABLES)) ntb = TOTAL_TABLES - 1;
                    inst.table_id = static_cast<uint8_t>(ntb);
                }
                break;
            }

            case VIEW_SYNTH: {
                Instrument& inst = song.instruments[selected_instrument_id < TOTAL_INSTRUMENTS ? selected_instrument_id : 0];
                if (inst.type == INST_TYPE_INTERNAL_SYNTH) {
                    if (cursor_row == 0) { // Pulse Width
                        float npw = inst.synth.pulse_width + static_cast<float>(delta) * (coarse ? 0.1f : 0.01f);
                        if (npw < 0.01f) npw = 0.01f;
                        if (npw > 0.99f) npw = 0.99f;
                        inst.synth.pulse_width = npw;
                    } else if (cursor_row == 1) { // Wavefolder Drive
                        float nfd = inst.synth.fold_drive + static_cast<float>(delta) * (coarse ? 0.2f : 0.02f);
                        if (nfd < 0.0f) nfd = 0.0f;
                        if (nfd > 4.0f) nfd = 4.0f;
                        inst.synth.fold_drive = nfd;
                    } else if (cursor_row == 2) { // Disperser Freq
                        float ndf = inst.synth.disperser_freq + static_cast<float>(delta) * (coarse ? 100.0f : 10.0f);
                        if (ndf < 20.0f) ndf = 20.0f;
                        if (ndf > 20000.0f) ndf = 20000.0f;
                        inst.synth.disperser_freq = ndf;
                    } else if (cursor_row == 3) { // Disperser Stages
                        int nds = static_cast<int>(inst.synth.disperser_stages) + delta;
                        if (nds < 1) nds = 1;
                        if (nds > 8) nds = 8;
                        inst.synth.disperser_stages = static_cast<uint8_t>(nds);
                    }
                } else if (inst.type == INST_TYPE_WAVETABLE) {
                    if (cursor_row == 0) { // Warp Mode
                        int nwm = static_cast<int>(inst.wavetable.warp_mode) + delta;
                        if (nwm < 0) nwm = 0;
                        if (nwm > WARP_BITCR) nwm = WARP_BITCR;
                        inst.wavetable.warp_mode = static_cast<WarpMode>(nwm);
                    } else if (cursor_row == 1) { // Warp Amount
                        float nwa = inst.wavetable.warp_amount + static_cast<float>(delta) * (coarse ? 0.1f : 0.01f);
                        if (nwa < 0.0f) nwa = 0.0f;
                        if (nwa > 1.0f) nwa = 1.0f;
                        inst.wavetable.warp_amount = nwa;
                    } else if (cursor_row == 2) { // Position
                        float npos = inst.wavetable.position + static_cast<float>(delta) * (coarse ? 4.0f : 0.5f);
                        if (npos < 0.0f) npos = 0.0f;
                        if (npos > 63.0f) npos = 63.0f;
                        inst.wavetable.position = npos;
                    }
                }
                break;
            }

            case VIEW_PROJECT: {
                if (cursor_row == 0) { // BPM
                    float nbpm = song.bpm + static_cast<float>(delta) * (coarse ? 10.0f : 1.0f);
                    if (nbpm < 20.0f) nbpm = 20.0f;
                    if (nbpm > 400.0f) nbpm = 400.0f;
                    song.bpm = nbpm;
                }
                break;
            }

            default:
                break;
        }
    }

    // --- FULL INPUT DISPATCHER ---
    void handleInput(const InputEvent& event, Song& song, Engine& engine) {
        if (!event.pressed && event.input != INPUT_JOG_CW && event.input != INPUT_JOG_CCW) {
            return;
        }

        // Quick modifiers
        if (event.modifier_view || event.input == INPUT_VIEW_MOD) {
            toggleViewMod();
            return;
        }
        if (event.modifier_sub || event.input == INPUT_SUB_MOD) {
            toggleSubMod();
            return;
        }

        switch (event.input) {
            // View Navigation
            case INPUT_PAGE_PREV:
                pagePrev();
                break;
            case INPUT_PAGE_NEXT:
                pageNext(song);
                break;

            // Cursor Movement
            case INPUT_UP:
                moveCursor(-1, 0);
                break;
            case INPUT_DOWN:
                moveCursor(1, 0);
                break;
            case INPUT_LEFT:
                moveCursor(0, -1);
                break;
            case INPUT_RIGHT:
                moveCursor(0, 1);
                break;

            // Editing
            case INPUT_BTN_A:
                editValue(song, 1, false);
                break;
            case INPUT_BTN_B:
                editValue(song, -1, false);
                break;
            case INPUT_JOG_CW:
                editValue(song, event.jog_delta > 0 ? event.jog_delta : 1, false);
                break;
            case INPUT_JOG_CCW:
                editValue(song, event.jog_delta < 0 ? event.jog_delta : -1, false);
                break;

            // Audition / Note Preview
            case INPUT_BTN_EDIT: {
                if (current_view == VIEW_PHRASE) {
                    const Phrase& ph = song.phrases[selected_phrase_id < TOTAL_PHRASES ? selected_phrase_id : 0];
                    uint8_t note = ph.steps[cursor_row].note;
                    if (note != NOTE_EMPTY) {
                        // Audition active note
                        engine.getTrackState(active_track).raw_note = note;
                        engine.getTrackState(active_track).effective_note = static_cast<int8_t>(note);
                    }
                }
                break;
            }

            // Transport Control
            case INPUT_TRANSPORT:
                if (engine.isPlaying()) {
                    engine.stop();
                } else {
                    engine.play(PLAY_SONG);
                }
                break;

            // Handheld / Steam Deck Grip Buttons
            case INPUT_OCTAVE_DOWN:
                if (octave_offset > -4) octave_offset--;
                break;
            case INPUT_OCTAVE_UP:
                if (octave_offset < 4) octave_offset++;
                break;
            case INPUT_TRACK_MUTE:
                track_muted[active_track] = !track_muted[active_track];
                break;
            case INPUT_TRACK_SOLO:
                track_soloed[active_track] = !track_soloed[active_track];
                break;

            default:
                break;
        }
    }

    // --- PUBLIC STATE VARIABLES ---
    TrackerView current_view{VIEW_SONG};
    TrackerView previous_view{VIEW_SONG};
    uint8_t active_track{0};
    int cursor_row{0};
    int cursor_col{0};

    uint8_t selected_song_row{0};
    uint8_t selected_chain_id{0};
    uint8_t selected_chain_step{0};
    uint8_t selected_phrase_id{0};
    uint8_t selected_phrase_step{0};
    uint8_t selected_table_id{0};
    uint8_t selected_table_row{0};
    uint8_t selected_instrument_id{0};
    uint8_t selected_synth_param{0};

    int8_t octave_offset{0};
    uint8_t edit_step{1};

    uint8_t clipboard_note{NOTE_EMPTY};
    uint8_t clipboard_inst{0};
    uint8_t clipboard_vol{0xFF};

    bool track_muted[MAX_TRACKS]{};
    bool track_soloed[MAX_TRACKS]{};
    float vu_levels[MAX_TRACKS]{};

    bool is_remapping{false};
    int  remap_action_index{0};
    int  settings_scroll_row{0};
};

} // namespace toad
