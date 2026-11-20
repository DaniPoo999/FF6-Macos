#pragma once
#include "ff6/battle_menu_navigation.hpp"
#include "ff6/battle_monster_view.hpp"

namespace ff6 {
// Eligibility already combines presence, visibility and condition restrictions.
// It can change while the player is choosing a target.
struct BattleTargetEligibility {
    std::bitset<4> characters;
    std::bitset<6> monsters;
};
struct BattleTargetCursor {
    std::uint8_t policy = 0, group = 0, index = 0;
    std::bitset<4> characters;
    std::bitset<6> monsters;
    bool multiple = false;
    std::uint8_t roulette_index = 0, roulette_countdown = 0;
    bool roulette_slowing = false, roulette_stopped = false;
    bool operator==(const BattleTargetCursor&) const = default;
};
// C1/B3AD: automatic (enemy/AI) Roulette is independent of the player menu.
struct BattleAutomaticRoulette {
    std::uint8_t index = 0, fast_wait = 0, wait = 0;
    bool slowing = false, stopped = false, complete = false;
    std::optional<std::uint8_t> target;
};
BattleAutomaticRoulette begin_battle_automatic_roulette(
    const std::array<std::uint8_t, 256>& random, std::uint8_t& index);
std::optional<std::uint8_t> advance_battle_automatic_roulette(BattleAutomaticRoulette&,
    BattleTargetEligibility, std::uint8_t clock,
    const std::array<std::uint8_t, 256>& random, std::uint8_t& index);
// Returns false if a single-enemy choice has no eligible enemy. Such a menu
// must return to its caller, without executing an invalid target search.
bool initialize_battle_target_cursor(BattleTargetCursor& cursor, std::uint8_t policy,
                                     std::uint8_t character,
                                     const BattleMonsterPositions& positions,
                                     BattleTargetEligibility eligible);
enum class BattleTargetDirection { up, down, left, right };
void move_battle_target_cursor(BattleTargetCursor& cursor, BattleTargetDirection direction,
                               BattleApproach approach, const BattleMonsterPositions& positions,
                               BattleTargetEligibility eligible);
enum class BattleTargetDecision { none, confirm, cancel };
struct BattleTargetInput {
    Buttons repeating = 0, pressed = 0;
    std::uint8_t clock = 0;
    bool closing = false;
};
struct BattleTargetResult {
    BattleTargetDecision decision = BattleTargetDecision::none;
    BattleMenuSounds sounds;
};
// This is the cursor decision phase. Confirm retains selected masks for the
// command owner to consume; inventory, X-Magic and window closure are separate.
// Graphics randomness reads then increments its independent byte index; battle
// mechanics randomness increments before reading and must not be substituted.
BattleTargetResult navigate_battle_targets(BattleTargetCursor& cursor, BattleTargetInput input,
                                           BattleApproach approach,
                                           const BattleMonsterPositions& positions,
                                           BattleTargetEligibility eligible,
                                           const std::array<std::uint8_t, 256>& random_table,
                                           std::uint8_t& graphics_random_index);
} // namespace ff6
