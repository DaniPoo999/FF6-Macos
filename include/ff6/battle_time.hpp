#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
struct BattleTimeInput {
    std::uint8_t clock = 0;
    bool menu_wait = false, escape_held = false;
};
struct BattleEscapeResult {
    std::bitset<4> departing;
    bool forbidden = false;
};
// C2/20DE: readiness is rechecked when the queued request executes, not when
// its probability counter first succeeds. Presentation owns the departure.
BattleEscapeResult consume_battle_escape(BattleState& state, bool held);
// Source-work observations follow the actual native decisions. Queue storage
// and ready-menu presentation are separate owners, resolved by the scheduling
// profile before this work may be used as an elapsed deadline.
enum class BattleTimeWorkKind { execution, immediate_action, retaliation_action, menu_request };
struct BattleTimeWorkItem {
    BattleTimeWorkKind kind = BattleTimeWorkKind::execution;
    std::uint32_t cycles = 0, fast_rom_clocks = 0;
    std::uint8_t character = 0;
    std::size_t chain_length = 0;
    bool waived = false, controlled = false;
};
using BattleTimeWorkTrace = std::vector<BattleTimeWorkItem>;
// Timing portion of UpdateBattleTime, after player command submission. The caller
// must process submitted commands first, even during Wait mode. This owner queues
// timer/ready/menu work; it does not execute attacks or interpret controller input.
// One call advances at most one step. Failure leaves the complete state unchanged.
bool advance_battle_time(BattleState& state, BattleTimeInput input,
                         const std::array<std::uint8_t, 256>& random_table,
                         BattleTimeWorkTrace* work = nullptr);
} // namespace ff6
