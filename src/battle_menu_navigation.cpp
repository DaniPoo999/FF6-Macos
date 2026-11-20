#include "ff6/battle_menu_navigation.hpp"
#include <algorithm>

namespace ff6 {
namespace {
constexpr Buttons directions =
    button(Button::up) | button(Button::down) | button(Button::left) | button(Button::right);
constexpr Buttons secondary =
    button(Button::a) | button(Button::x) | button(Button::l) | button(Button::r);
constexpr Buttons primary = button(Button::b) | button(Button::y) | button(Button::start) |
                            button(Button::select) | directions;
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
void validate(const BattleMenuOrder& order) {
    for (auto rank : order.ranks)
        require(!rank || *rank <= 4, "Invalid ready-menu rank");
}
} // namespace
void compact_battle_menu_order(BattleMenuOrder& order, BattleMenuQueueWork* work) {
    validate(order);
    unsigned next = 0;
    // C10286-C102C9: setup, five compaction visits and cleanup termination.
    // Search work depends on the first matching character, including duplicate
    // ranks. All working and stack bytes use the slow-memory profile.
    std::uint32_t cycles = 139, slow = 32;
    const auto search = [&](unsigned old, std::optional<std::uint8_t> replacement) {
        unsigned slot = 0;
        for (auto& rank : order.ranks) {
            if (rank && *rank == old) {
                rank = replacement;
                cycles += 31U + slot * 18U;
                slow += 6U + slot * 2U;
                return true;
            }
            ++slot;
        }
        cycles += 83;
        slow += 10;
        return false;
    };
    // Preserve the original first-slot search and its behavior with duplicate
    // ranks; a general sort changes which duplicate survives the cleanup pass.
    for (unsigned old = 0; old < 5; ++old) {
        if (search(old, static_cast<std::uint8_t>(next))) {
            ++next;
            cycles += 4;
            slow += 2;
        }
    }
    for (unsigned old = next; old < 5; ++old) {
        cycles += 26;
        slow += 6;
        search(old, {});
    }
    if (work) *work = {cycles, 6U * cycles + 2U * slow};
}
void request_battle_menu(BattleMenuOrder& order, BattleMenuRequest request, bool opening_allowed) {
    require(request.character < 4, "Invalid battle menu character");
    validate(order);
    if (!request.open) {
        order.ranks[request.character].reset();
        return;
    }
    if (!opening_allowed)
        return;
    order.controlled_monsters[request.character] = request.control;
    if (!order.ranks[request.character]) {
        compact_battle_menu_order(order);
        order.ranks[request.character] = 3;
    }
}
std::optional<std::uint8_t> ready_battle_menu(BattleMenuOrder& order, bool opening_allowed) {
    compact_battle_menu_order(order);
    if (opening_allowed)
        for (unsigned slot = 0; slot < 4; ++slot)
            if (order.ranks[slot] == 0)
                return static_cast<std::uint8_t>(slot);
    return {};
}
void finish_battle_menu(BattleMenuOrder& order, std::uint8_t character, BattleMenuCycle cycle) {
    require(character < 4, "Invalid closing menu character");
    validate(order);
    if (cycle == BattleMenuCycle::previous) {
        for (auto rank : order.ranks)
            require(!rank || *rank < 4, "Previous-menu cycle requires compacted readiness");
        // Rank increments occur before selection; equal maxima choose the
        // highest combat slot, as in the original ascending search.
        unsigned selected = 0, maximum = 0;
        for (unsigned i = 0; i < 4; ++i)
            if (order.ranks[i]) {
                const auto rank = ++*order.ranks[i];
                if (rank >= maximum) {
                    maximum = rank;
                    selected = i;
                }
            }
        if (order.ranks[selected])
            order.ranks[selected] = 0;
    } else if (cycle == BattleMenuCycle::next) {
        if (order.ranks[character])
            order.ranks[character] = 4;
    } else if (cycle == BattleMenuCycle::consumed) {
        order.ranks[character].reset();
    } else
        throw std::invalid_argument("Invalid battle menu cycle");
}
BattleMenuNavigationResult navigate_battle_commands(std::uint8_t& cursor,
                                                    const BattleCommandList& commands,
                                                    BattleCommandLayout layout, Buttons repeating,
                                                    bool closing) {
    require(layout == BattleCommandLayout::window || layout == BattleCommandLayout::short_form ||
                layout == BattleCommandLayout::control,
            "Invalid battle command layout");
    BattleMenuNavigationResult result;
    result.wait_for_menu = false;
    if (closing) {
        result.decision = BattleMenuDecision::close;
        return result;
    }
    if ((repeating & secondary) == button(Button::x)) {
        result.decision = BattleMenuDecision::next_character;
        result.sounds.move = 1;
        return result;
    }
    if ((repeating & primary) == button(Button::y)) {
        result.decision = BattleMenuDecision::previous_character;
        result.sounds.move = 1;
        return result;
    }
    const auto enabled = [&](unsigned i) {
        return commands.slots[i] && !commands.slots[i]->disabled;
    };
    require(std::ranges::any_of(commands.slots, [](const auto& c) { return c && !c->disabled; }),
            "No selectable battle commands");
    const auto step = [&](int delta) {
        for (unsigned count = 0; count < 4; ++count) {
            cursor = static_cast<std::uint8_t>((unsigned{cursor} + (delta < 0 ? 3U : 1U)) & 3U);
            if (enabled(cursor))
                return;
        }
    };
    cursor &= 3U;
    if (layout == BattleCommandLayout::short_form) {
        if (!enabled(cursor))
            step(1);
        for (const auto [key, selected] : {std::pair{Button::up, 0U},
                                           {Button::down, 3U},
                                           {Button::left, 1U},
                                           {Button::right, 2U}})
            if (held(repeating, key) && enabled(selected) && cursor != selected) {
                cursor = static_cast<std::uint8_t>(selected);
                ++result.sounds.move;
            }
        const auto shoulders = repeating & (button(Button::l) | button(Button::r));
        if (shoulders == button(Button::l) || shoulders == button(Button::r)) {
            ++result.sounds.move;
            result.decision = shoulders == button(Button::l) ? BattleMenuDecision::row
                                                             : BattleMenuDecision::defend;
            return result;
        }
    } else {
        if (!enabled(cursor))
            step(1);
        else if (repeating & (button(Button::up) | button(Button::down))) {
            ++result.sounds.move;
            step(held(repeating, Button::up) ? -1 : 1);
        }
    }
    if (held(repeating, Button::a)) {
        result.decision = BattleMenuDecision::command;
        result.command = commands.slots[cursor];
        if (layout == BattleCommandLayout::control) {
            result.attack = BattlePlayerAttack{result.command->identity, 0};
            result.command->identity = 14;
        }
        result.targeting = commands.slots[cursor]->targeting;
        result.wait_for_menu = layout != BattleCommandLayout::control;
        ++result.sounds.select;
    } else if (layout == BattleCommandLayout::window) {
        const auto direction = repeating & directions;
        if (direction == button(Button::left) || direction == button(Button::right)) {
            ++result.sounds.move;
            result.decision = direction == button(Button::left) ? BattleMenuDecision::row
                                                                : BattleMenuDecision::defend;
        }
    }
    return result;
}
BattleListMovement move_battle_list_cursor(BattleListCursor cursor, std::uint8_t columns,
                                           Buttons repeating) {
    require(columns > 0 && columns <= 3 && cursor.column < columns && cursor.row < 4,
            "Invalid battle list cursor");
    BattleListMovement out{cursor};
    switch (repeating & directions) {
    case button(Button::up):
        if (cursor.row == 0)
            out.scroll = -1;
        else {
            --out.cursor.row;
            out.sound = true;
        }
        break;
    case button(Button::down):
        if (cursor.row == 3)
            out.scroll = 1;
        else {
            ++out.cursor.row;
            out.sound = true;
        }
        break;
    case button(Button::left):
        if (cursor.column != 0) {
            --out.cursor.column;
            out.sound = true;
        } else {
            out.cursor.column = static_cast<std::uint8_t>(columns - 1U);
            if (cursor.row == 0)
                out.scroll = -65;
            else {
                --out.cursor.row;
                out.sound = true;
            }
        }
        break;
    case button(Button::right):
        if (unsigned{cursor.column} + 1U != columns) {
            ++out.cursor.column;
            out.sound = true;
        } else {
            out.cursor.column = 0;
            if (cursor.row == 3)
                out.scroll = 1;
            else {
                ++out.cursor.row;
                out.sound = true;
            }
        }
        break;
    default:
        break;
    }
    return out;
}
BattleMagitekChoices load_battle_magitek_choices(const ContentPack& pack) {
    const auto bytes = pack.get("battle_magitek_choices");
    require(bytes.size() == 24, "Invalid Magitek choice resource");
    BattleMagitekChoices result;
    std::copy_n(bytes.begin(), 8, result.targeting.begin());
    std::copy_n(bytes.begin() + 8, 8, result.terra.begin());
    std::copy_n(bytes.begin() + 16, 8, result.other.begin());
    for (const auto* choices : {&result.terra, &result.other})
        require(std::ranges::all_of(*choices, [](auto id) { return id < 8 || id == 255; }),
                "Invalid Magitek attack choice");
    return result;
}
BattleMenuNavigationResult navigate_battle_magitek(BattleListCursor& cursor,
                                                   const BattleMagitekChoices& choices,
                                                   std::uint8_t displayed_identity,
                                                   Buttons repeating, Buttons pressed,
                                                   bool closing) {
    BattleMenuNavigationResult result;
    if (closing) {
        result.decision = BattleMenuDecision::close;
        return result;
    }
    const auto moved = move_battle_list_cursor(cursor, 2, repeating);
    // Fixed lists discard scrolling requests, including the provisional column
    // wrap. The original helper still returns that column to scrollable callers.
    if (!moved.scroll)
        cursor = moved.cursor;
    result.sounds.move = moved.sound;
    if (held(pressed, Button::b)) {
        result.decision = BattleMenuDecision::cancel;
        ++result.sounds.select;
        return result;
    }
    if (held(repeating, Button::a)) {
        const auto index = unsigned{cursor.row} * 2U + cursor.column;
        const auto attack = (displayed_identity == 0 ? choices.terra : choices.other)[index];
        if (attack >= 128) {
            ++result.sounds.invalid;
            return result;
        }
        require(attack < 8, "Invalid Magitek attack choice");
        ++result.sounds.select;
        result.decision = BattleMenuDecision::attack;
        result.attack = BattlePlayerAttack{attack, 0};
        result.targeting = choices.targeting[index];
    }
    return result;
}
} // namespace ff6
