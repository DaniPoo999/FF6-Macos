#pragma once
#include "ff6/battle_submission.hpp"
#include "ff6/battle_target_selection.hpp"

namespace ff6 {
enum class BattleCommandDraftPhase { first_target, second_target, submitted };
struct BattleCommandDraft {
    BattlePlayerCommand selection;
    BattleCommandDraftPhase phase = BattleCommandDraftPhase::first_target;
    bool operator==(const BattleCommandDraft&) const = default;
};
BattleCommandDraft begin_battle_command(std::uint8_t character, std::uint8_t command);
enum class BattleItemSource { inventory, right_hand, left_hand };
struct BattleConfirmationContext {
    // The graphics caller's enemy-character proxy, distinct from the combat
    // participation mask. It stands in for monster cursor slot five.
    std::bitset<4> enemy_character_proxy;
    BattleItemSource item_source = BattleItemSource::inventory;
};
enum class BattleConfirmation { choose_second_cast, submitted };
// Confirmation includes target mapping, Item/Throw consumption, X-Magic staging
// and queue publication. A failed validation/publication leaves all owners intact.
// Cursor decisions already resolved cancellation, eligibility and auto-confirm.
BattleConfirmation confirm_battle_command(BattleState& battle, BattleCommandDraft& draft,
                                          BattleTargetCursor& cursor, std::uint8_t attack,
                                          BattleConfirmationContext context);
// Cancelling a target resets X-Magic to its first cast, retaining the unpublished
// first selection until it is replaced. It never consumes an item or publishes.
void cancel_battle_command_target(BattleCommandDraft& draft, BattleTargetCursor& cursor);
} // namespace ff6
