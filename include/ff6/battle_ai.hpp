#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
struct BattleAiData {
    Bytes scripts;
    std::array<std::uint8_t, 256> random{};
    std::array<std::uint8_t, 11> first_attacks{}, commands{};
};
BattleAiData load_battle_ai(const ContentPack& pack);
std::uint8_t battle_ai_attack_command(const BattleAiData& data, std::uint8_t attack);

// Extra inputs owned by formation setup, targeting and scripted battle events.
// Variables are the AI language's variables, not a CPU memory image. Queueing
// a variable change does not immediately change what later conditions see.
struct BattleAiEnvironment {
    std::bitset<10> allocated, selectable{1023};
    std::array<std::uint8_t, 36> variables{};
    std::array<std::uint8_t, 10> personal_variables{};
    std::array<std::optional<std::uint8_t>, 10> preferred_targets;
    // IF_HIT reads a separate retained target, not the pending counterattacker
    // candidate used by target selector 45.
    std::array<std::optional<std::uint8_t>, 10> last_hit_targets;
    std::uint16_t battle_identity = 0;
    bool gau_recruited = false;
};
// Unfiltered selectors used by AI control commands (CheckAITarget with carry set).
std::uint16_t battle_ai_control_targets(std::uint8_t selector, std::uint8_t participant,
                                        const BattleAiData& data, const BattleState& battle,
                                        BattleAiEnvironment& environment, BattleRandom& random);
struct BattleAiProgress {
    std::uint16_t resume = 0;
    std::uint8_t block = 255;
    bool operator==(const BattleAiProgress&) const = default;
};
// A turn starts at the script entry, rechecks conditions, then resumes at its
// saved wait only if the same conditional block is still applicable.
struct BattleAiExecution {
    std::uint8_t participant = 0, block = 0, resume_block = 255;
    std::uint16_t cursor = 0, initial_targets = 0, targets = 0;
    BattleAiProgress progress;
    bool initially_unresponsive = false, unresponsive = false, complete = false;
    bool operator==(const BattleAiExecution&) const = default;
};
BattleAiExecution begin_battle_ai(std::uint8_t participant, std::uint16_t entry,
                                 BattleAiProgress progress = {}, std::uint16_t targets = 0,
                                 bool unresponsive = false);
struct BattleAiAction {
    BattleQueuedCommand command;
    // Ordinary commands need target preparation and command delay before queue
    // insertion. Script/event requests bypass those.
    bool prepare_attack = false;
    bool operator==(const BattleAiAction&) const = default;
};
// Executes one documented data-language instruction. The caller handles an
// emitted request before the next step, preserving target-preparation RNG order.
// End/wait commit progress; no attacks, event requests or variable writes are
// silently executed here. Invalid data rolls back execution, RNG and environment.
std::optional<BattleAiAction> step_battle_ai(BattleAiExecution& execution,
                                           const BattleAiData& data,
                                           const BattleState& battle,
                                           BattleAiEnvironment& environment,
                                           BattleRandom& random);
} // namespace ff6
