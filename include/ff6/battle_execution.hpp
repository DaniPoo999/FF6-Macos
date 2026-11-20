#pragma once
#include "ff6/battle_queue.hpp"

namespace ff6 {
// Palidor replaces queued actions with one airborne Jump per successful target.
void begin_battle_summon_jumps(BattleState&, BattleQueueContext&, std::bitset<10>);

struct BattleExecutionData {
    BattleQueueData queue;
    std::array<std::uint8_t, 32> command_properties{};
    std::array<std::array<std::uint8_t, 4>, 384> monster_choices{};
};
// Applies the selected monster properties and returns the underlying attack
// command. Rage retains the caller's free MP cost and automatic turn ownership.
BattleQueuedCommand begin_battle_rage(BattleState&, std::uint8_t participant,
    BattleQueuedCommand, const MonsterStatsData&, const BattleAiData&);
BattleExecutionData load_battle_execution(const ContentPack& pack);
struct BattleInitializedCommand {
    BattleQueuedCommand command;
    // RANDOM/Jump dispatch tests use the queued identity, even if Imp changed
    // the command that the attack consumer will execute.
    std::uint8_t queued_identity = 0;
};
BattleInitializedCommand initialize_battle_command(BattleState& battle, BattleQueueContext& queue,
                                                    const BattleExecutionData& data,
                                                    std::uint8_t participant,
                                                    BattleQueuedCommand command,
                                                    bool retarget_changed_status);
struct BattleMimicMemory {
    BattleQueuedCommand first{18, 0, 0, 0};
    std::optional<BattleQueuedCommand> second, jump;
};
struct BattleExecutionContext {
    BattleMimicMemory mimic;
    // Supplied by the formation/transform owner, independently of display names.
    std::array<std::optional<std::uint16_t>, 6> monster_identities;
    std::optional<std::uint8_t> executing;
    bool sonic_dive = false, normal_in_progress = false, refresh_monsters = false;
};
enum class BattleExecutionDisposition { execute, absent, blocked, jump_wait, no_action };
struct BattleExecutionRequest {
    BattleExecutionDisposition disposition = BattleExecutionDisposition::execute;
    BattleQueuedCommand command{18, 0, 0, 0};
};
// Resolves Mimic/deferred decisions, applies status conversion, admits/removes
// one action and hands it to a native command consumer. No damage or graphics
// execute here. Blocked and first-stage Jump requests remain queued.
// Malformed/unsupported inputs leave all three owners unchanged.
BattleExecutionRequest begin_battle_normal_action(BattleState& battle, BattleQueueContext& queue,
                                                  BattleExecutionContext& execution,
                                                  const BattleExecutionData& data,
                                                  std::uint8_t participant);
// Call after the command consumer and its after-action work/presentation. Supply
// its final command/targets for Mimic, or no command when the actor was absent.
// Returns true when the caller must play the shared return presentation; false
// retains the actor for its next command. Does not clear newly queued work.
bool finish_battle_normal_action(BattleState& battle, BattleQueueContext& queue,
                                 BattleExecutionContext& execution,
                                 const BattleExecutionData& data,
                                 std::optional<BattleQueuedCommand> performed);
// Called after preparing an advance; true requests the character takeoff.
bool begin_battle_jump_advance(BattleState&, BattleExecutionContext&, unsigned participant);
// C2/021E is also called after a character counter. Non-copyable commands
// and monster actions leave history intact; null means no action executed.
void remember_battle_mimic_action(BattleExecutionContext&, const BattleExecutionData&,
    unsigned participant, std::optional<BattleQueuedCommand> performed);
// Counter chains have priority over ordinary turns and do not reset their ATB.
// Deferred AI decisions observe the battlefield when consumed. A scheduled
// visit may outlive its chain, or a decision may emit nothing. no_action releases its continuation
// without starting an execution; the caller must not finish or present it.
BattleExecutionRequest begin_battle_retaliation(BattleState& battle, BattleQueueContext& queue,
    BattleExecutionContext& execution, const BattleExecutionData& data, std::uint8_t participant);
void finish_battle_retaliation(BattleState& battle, BattleQueueContext& queue,
    BattleExecutionContext& execution);
// Semantic AI variable/switch/timer commands. Other commands remain with their
// presentation owners. Invalid operands leave state unchanged.
bool execute_battle_ai_control(BattleState& battle, BattleAiEnvironment& ai,
                               const BattleAiData& data, std::uint8_t participant,
                               BattleQueuedCommand command);
} // namespace ff6
