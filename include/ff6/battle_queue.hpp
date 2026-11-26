#pragma once
#include "ff6/battle_action_preparation.hpp"
#include "ff6/battle_ai.hpp"

namespace ff6 {
struct BattleQueueData {
    BattleAiData ai;
    BattleActionPreparationData preparation;
    std::array<std::uint8_t, 256> spell_mp{};
    std::array<std::array<std::uint8_t, 2>, 256> rage_attacks{};
    std::array<std::array<std::uint8_t, 4>, 8> dance_attacks{};
    std::array<std::uint8_t, 3> dance_thresholds{};
};
BattleQueueData load_battle_queue(const ContentPack& pack);
// After a physical block, Interceptor may counter the remembered enemy. The
// caller keeps the block identity local to the completed action.
bool queue_battle_retort(BattleState&, std::uint8_t target, bool physical);
bool queue_battle_interceptor(BattleState&, const BattleQueueData&, std::uint8_t target);
// Admit a deferred counter decision, never evaluate its script against the
// current hit snapshot. Earlier counters may revive or remove its targets.
bool queue_battle_ai_retaliation(BattleState&, std::uint8_t target);
enum class BattleCommandChain { normal, retaliation };
struct BattleQueueContext {
    BattleAiEnvironment ai;
    BattleTargetEnvironment targets;
    std::array<BattleAiProgress, 10> normal_progress, retaliation_progress;
    std::optional<std::uint8_t> continuing_action, continuing_retaliation;
    std::bitset<10> retarget_pending;
    // One AI response per participant until the entire counter chain drains.
    std::bitset<10> retaliation_evaluated;
};
// C2/0049..007D publishes escape rules only after immediate/counter work,
// immediately before checking battle termination or selecting a normal turn.
void publish_battle_loop_display(BattleState&, const BattleQueueContext&);
// Admission combines gameplay lifecycle with the graphics owner's explicit
// targetability mask. That mask is not inferred from actor coordinates/pixels.
std::bitset<10> battle_target_admission(const BattleState& battle,
                                       std::bitset<6> monster_targets, bool allow_away = false);

// The caller controls when AI runs. Normal monsters enqueue RANDOM during
// advance preparation; the action dispatcher expands it only at execution.
// Requests are prepared and inserted between AI instructions, preserving RNG
// order. Does not execute attacks/events or consume an existing RANDOM request.
// Failure rolls back all queues, continuation, target preferences and RNG.
std::size_t queue_battle_ai_turn(BattleState& battle, BattleQueueContext& context,
                                const BattleQueueData& data, std::uint8_t participant,
                                BattleCommandChain chain = BattleCommandChain::normal,
                                std::uint16_t initial_targets = 0, bool unresponsive = false);

// Shared MP-cost rule for normal/retaliation insertion. The insertion owner
// consumes waive_next_action_mp after successfully allocating a request.
std::uint8_t battle_queued_mp_cost(const BattleState& battle, std::uint8_t participant,
                                  BattleQueuedCommand command,
                                  const std::array<std::uint8_t, 256>& spell_mp);

// The advance-queue caller has already removed its schedule entry. Blocked turns
// retain a reschedule request. Otherwise this prepares existing commands, runs
// scripted character AI, or queues a deferred monster decision. Unimplemented
// automatic character selection is an explicit, atomic rejection.
bool prepare_battle_advance(BattleState& battle, BattleQueueContext& context,
                            const BattleQueueData& data, std::uint8_t participant);

// Called at an ordinary victory boundary. The eligibility bit is consumed once,
// even when the probability or living-party check declines the appearance.
bool queue_veldt_gau_appearance(BattleState&, BattleQueueContext&,
                                const std::array<std::uint8_t, 256>& random);
// After Gau's appearance check, Shadow may leave an ordinary victory. The
// authored event runs before rewards; the return owner commits roster changes.
bool queue_shadow_departure(BattleState&, const BattleQueueContext&,
                            const std::array<std::uint8_t, 256>& random);
enum class BattleConclusion { ongoing, escaped, defeated, victorious, interrupted, engulfed,
                              scripted_return };
// Query at every completed presentation/queue boundary, including after an
// immediate pose/status action. Death of both sides resolves as party defeat.
BattleConclusion battle_conclusion_at_rest(const BattleState&, const BattleQueueContext&);
// Preserve the source's persistent battle-result bits for the resuming event.
void record_battle_conclusion(BattleQueueContext&, BattleConclusion);

enum class BattleQueueWorkKind { idle, immediate, retaliation, advance, action };
struct BattleQueueWork {
    BattleQueueWorkKind kind = BattleQueueWorkKind::idle;
    std::optional<std::uint8_t> participant;
    bool operator==(const BattleQueueWork&) const = default;
};
// Queue arbitration only. The battle-loop owner must service lifecycle/end-of-
// battle work at the corresponding boundary and must finish presentation before
// asking for more work. Selection advances schedule cursors, not command chains.
// The immediate owner explicitly consumes its front request after accepting it.
BattleQueueWork take_battle_queue_work(BattleState& battle, BattleQueueContext& context);
} // namespace ff6
