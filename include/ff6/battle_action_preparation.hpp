#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
struct BattleActionSpellTarget {
    std::uint8_t targeting = 0, flags = 0;
};
struct BattleActionItemTarget {
    std::uint8_t targeting = 0, spell = 0, statuses = 0;
};
struct BattleActionPreparationData {
    std::array<std::uint8_t, 256> random{};
    std::array<std::uint8_t, 32> delays{}, targeting{};
    std::array<std::uint8_t, 30> command_flags{};
    std::array<std::uint8_t, 5> tool_items{}, tool_offsets{};
    std::array<BattleActionSpellTarget, 256> spells;
    std::array<BattleActionItemTarget, 256> items;
};
BattleActionPreparationData load_battle_action_preparation(const ContentPack& pack);
std::uint8_t battle_action_delay(const BattleActionPreparationData& data, std::uint8_t command,
                                 std::uint8_t duration);

struct BattleTargetPolicy {
    bool exclude_attacker = false, prefer_previous_attacker = false;
    bool no_retarget = false, allow_dead = false, reflected = false;
    bool use_backup = false, force_retarget = false, exclude_characters = false;
    bool operator==(const BattleTargetPolicy&) const = default;
};
struct BattleAttackTargetRules {
    BattleTargetPolicy policy;
    std::uint8_t targeting = 0;
    // Requests for the action owner, not mutations of battle-wide scratch state.
    std::optional<std::uint8_t> spell, item;
    bool break_item = false;
    bool operator==(const BattleAttackTargetRules&) const = default;
};
BattleAttackTargetRules battle_attack_target_rules(const BattleActionPreparationData& data,
                                                   std::uint8_t command, std::uint8_t attack);
struct BattleTargetEnvironment {
    // Combined admission supplied by the caller: allocated/active/arrival,
    // Seize and Jump restrictions. Present/dead filtering is separate below.
    std::bitset<10> selectable;
    std::uint16_t backup_targets = 0;
    std::optional<std::uint8_t> previous_attacker;
    bool skip_next_filter = false;
    bool operator==(const BattleTargetEnvironment&) const = default;
};
// Uses semantic participant/status/formation state. Invalid inputs leave the
// caller's one-use filter and RNG unchanged. No display or hardware clocks.
std::uint16_t choose_battle_targets(const BattleState& battle, std::uint8_t attacker,
                                    std::uint16_t targets, const BattleAttackTargetRules& rules,
                                    BattleTargetEnvironment& environment,
                                    const std::array<std::uint8_t, 256>& random_table,
                                    BattleRandom& random);
struct BattlePreparedAction {
    BattleQueuedCommand command;
    std::uint8_t duration = 0;
    std::optional<BattleAttackTargetRules> targeting;
};
// Adds the command delay and chooses targets only when none were supplied.
// The turn owner initializes a disabled duration (255) before this call.
// Commands >= 30 carry non-target operands and bypass this preparation.
BattlePreparedAction prepare_battle_action(const BattleActionPreparationData& data,
                                            const BattleState& battle, std::uint8_t attacker,
                                            BattleQueuedCommand command, std::uint8_t duration,
                                            BattleTargetEnvironment& environment,
                                            BattleRandom& random);
} // namespace ff6
