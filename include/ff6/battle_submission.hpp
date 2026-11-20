#pragma once
#include "ff6/battle_time.hpp"

namespace ff6 {
struct BattleSubmissionData {
    std::array<std::uint8_t, 256> random{};
    std::array<std::uint8_t, 5> attack_commands{}, attack_offsets{};
    std::bitset<32> retarget_commands;
    std::array<std::uint8_t, 8> slot_attacks{};
    std::array<std::uint8_t, 2> joker_targets{};
    std::array<std::uint8_t, 3> dance_thresholds{};
    std::array<std::array<std::uint8_t, 4>, 8> dance_attacks{};
    std::array<std::array<std::uint8_t, 2>, 256> rage_attacks{};
};
// Shared by initial menu submission and subsequent automatic Rage turns.
std::uint8_t choose_battle_rage_attack(BattleState&, unsigned slot,
    const std::array<std::array<std::uint8_t, 2>, 256>& attacks,
    const std::array<std::uint8_t, 256>& random);
std::uint8_t choose_battle_dance_attack(BattleState&, unsigned slot,
    const std::array<std::array<std::uint8_t, 4>, 8>& attacks,
    const std::array<std::uint8_t, 3>& thresholds,
    const std::array<std::uint8_t, 256>& random);
BattleSubmissionData load_battle_submission_data(const ContentPack& pack);
// Menu-to-battle handoff. Full queues and malformed selections are explicit
// failures; the producer never overwrites a pending command.
void submit_battle_player_command(BattleState& state, BattlePlayerCommand command);
// Consume consecutive occupied queue positions, stopping at the first hole.
// Command conversion, RNG, queue changes and costs commit together.
std::size_t consume_battle_player_commands(BattleState& state, const BattleSubmissionData& data);
struct BattleTimeResult {
    std::size_t commands = 0;
    bool stepped = false;
};
// Whole input/time boundary: submitted commands are consumed even when Wait
// mode or a short clock interval prevents timer advancement.
BattleTimeResult service_battle_time(BattleState& state, BattleTimeInput input,
                                     const BattleSubmissionData& data);
} // namespace ff6
