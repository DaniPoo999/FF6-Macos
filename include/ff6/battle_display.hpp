#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
// Shared semantic publication: names/counts, escape rules and character gauges.
// No sprite, device-register or video-memory interface is exposed here.
void update_battle_display(BattleState& state);
// Damage/status presentation can update names and gauges while counters are
// still queued. Escape admission retains the last battle-loop publication.
void update_battle_action_readouts(BattleState& state);
// Initial announcement, publication, then returning Jump actions, in that order.
// Stops before scene construction. Invalid inputs/capacity leave state unchanged.
void prepare_battle_presentation(BattleState& state, const BattleFormation& formation);
} // namespace ff6
