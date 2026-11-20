#include "ff6/battle_command_selection.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
void validate(const BattleCommandDraft& draft) {
    require(draft.selection.character < 4 && draft.selection.command < 30,
            "Invalid battle command draft");
    require(draft.phase == BattleCommandDraftPhase::first_target ||
                (draft.phase == BattleCommandDraftPhase::second_target &&
                 draft.selection.command == 23),
            "Battle command draft is not awaiting a target");
    require(!draft.selection.second, "Draft already contains a second cast");
}
void clear_cursor(BattleTargetCursor& cursor) {
    cursor.characters.reset();
    cursor.monsters.reset();
    cursor.multiple = false;
}
std::uint16_t selected_targets(const BattleTargetCursor& cursor, std::bitset<4> proxy) {
    auto characters = cursor.characters;
    auto monsters = cursor.monsters;
    if (proxy.any()) {
        if (monsters[5])
            characters |= proxy;
        monsters.reset(5);
    }
    return static_cast<std::uint16_t>(characters.to_ulong() | (monsters.to_ulong() << 8U));
}
void consume_item(BattleInventory& inventory, std::uint8_t character, std::uint8_t identity,
                  BattleItemSource source) {
    BattleItemStack* selected = nullptr;
    switch (source) {
    case BattleItemSource::inventory: {
        // Search by identity, not the selected visible row. Duplicate entries
        // consume the first one. Missing identities are an original no-op.
        auto found = std::ranges::find_if(inventory.items, [&](const auto& stack) {
            return (stack.item ? stack.item->identity : 255) == identity;
        });
        if (found != inventory.items.end())
            selected = &*found;
        break;
    }
    case BattleItemSource::right_hand:
    case BattleItemSource::left_hand:
        require(inventory.hands[character].has_value(), "Selected character has no hand inventory");
        selected = &(*inventory.hands[character])[source == BattleItemSource::left_hand ? 1 : 0];
        break;
    default:
        throw std::invalid_argument("Invalid battle item source");
    }
    if (selected) {
        if (selected->quantity >= 2)
            --selected->quantity;
        else
            *selected = {}; // Empty identity, disabled usage and cleared target/equipment metadata.
    }
}
} // namespace
BattleCommandDraft begin_battle_command(std::uint8_t character, std::uint8_t command) {
    BattleCommandDraft draft{{character, command, {255, 0}, {}}};
    validate(draft);
    return draft;
}
BattleConfirmation confirm_battle_command(BattleState& battle, BattleCommandDraft& draft,
                                          BattleTargetCursor& cursor, std::uint8_t attack,
                                          BattleConfirmationContext context) {
    validate(draft);
    auto next = draft;
    auto inventory = battle.inventory;
    const BattlePlayerAttack chosen{attack,
                                    selected_targets(cursor, context.enemy_character_proxy)};
    if (next.phase == BattleCommandDraftPhase::second_target)
        next.selection.second = chosen;
    else {
        next.selection.first = chosen;
        if (next.selection.command == 1 || next.selection.command == 8)
            consume_item(inventory, next.selection.character, attack, context.item_source);
        if (next.selection.command == 23) {
            next.phase = BattleCommandDraftPhase::second_target;
            draft = next;
            clear_cursor(cursor);
            return BattleConfirmation::choose_second_cast;
        }
    }
    // Submission can fail on a full queue. Commit consumption and draft state
    // only after it succeeds; callers can retry without losing an item.
    submit_battle_player_command(battle, next.selection);
    battle.inventory = std::move(inventory);
    if ((next.selection.command == 1 || next.selection.command == 8) &&
        context.item_source != BattleItemSource::inventory)
        battle.equipment_changed.set(next.selection.character);
    next.phase = BattleCommandDraftPhase::submitted;
    draft = next;
    clear_cursor(cursor);
    return BattleConfirmation::submitted;
}
void cancel_battle_command_target(BattleCommandDraft& draft, BattleTargetCursor& cursor) {
    validate(draft);
    draft.phase = BattleCommandDraftPhase::first_target;
    clear_cursor(cursor);
}
} // namespace ff6
