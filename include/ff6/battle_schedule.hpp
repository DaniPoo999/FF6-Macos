#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
// C2/0A91 cancels living, present allies' ordinary actions for Gau's arrival.
void cancel_battle_actions_for_gau(BattleState& state);
// Complete AfterAction2 mechanics. Menus and queued actions remain typed
// requests for their native consumers; this pass does not execute an attack.
void refresh_battle_actions(BattleState& state, const BattleCommandData& commands,
                            const std::array<std::uint8_t, 256>& random_table,
                            std::uint8_t attacker_level);
} // namespace ff6
