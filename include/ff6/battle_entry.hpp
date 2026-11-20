#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
struct BattleApproachWeights {
    // Effective weights in normal/back/pincer/side order, including source carry.
    std::array<std::uint16_t, 4> values{};
};
BattleApproachWeights decode_battle_approach_weights(ByteView bytes);
BattleApproachWeights load_battle_approach_weights(const ContentPack& pack);
void update_battle_participation(BattleState& state);
// ChangeBattle chooses the new approach without resetting party rows or gauges.
void change_battle_approach(BattleState&, const BattleFormation&, std::uint8_t relic_effects,
                           const BattleApproachWeights&, const std::array<std::uint8_t, 256>&);
// Initial caller after AfterAction2: participation, approach, immunity, rows,
// preemptive and gauges. Preserves fractional counters and commits atomically.
void initialize_battle_entry(BattleState& state, const BattleFormation& formation,
                             std::uint8_t party_relic_effects, const BattleApproachWeights& weights,
                             const std::array<std::uint8_t, 256>& random_table);
} // namespace ff6
