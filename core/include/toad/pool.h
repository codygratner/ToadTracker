#pragma once

#include "types.h"

namespace toad {

inline int8_t evaluateNotePool(InstrumentNotePool& pool, int8_t base_note) {
    if (pool.mode == POOL_OFF || pool.slot_count == 0) return base_note;

    if (pool.mode == POOL_WEIGHTED_RND) {
        uint32_t total_weight = 0;
        for (uint8_t i = 0; i < pool.slot_count; ++i) {
            total_weight += pool.slots[i].weight;
        }
        if (total_weight == 0) return base_note;

        // Linear congruential pseudo-random number generator (zero library calls)
        static uint32_t lcg_seed = 123456789;
        lcg_seed = lcg_seed * 1664525u + 1013904223u;
        uint32_t roll = lcg_seed % total_weight;
        uint32_t cumulative = 0;

        for (uint8_t i = 0; i < pool.slot_count; ++i) {
            if (pool.slots[i].weight == 0) continue;
            cumulative += pool.slots[i].weight;
            if (roll < cumulative) {
                pool.last_selected = i;
                return base_note + pool.slots[i].semitone_offset;
            }
        }
    } else if (pool.mode == POOL_CYCLE) {
        for (uint8_t i = 0; i < pool.slot_count; ++i) {
            pool.last_selected = (pool.last_selected + 1) % pool.slot_count;
            if (pool.slots[pool.last_selected].weight > 0) {
                return base_note + pool.slots[pool.last_selected].semitone_offset;
            }
        }
    }
    return base_note;
}

} // namespace toad
