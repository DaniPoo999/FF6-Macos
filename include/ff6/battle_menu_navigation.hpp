#pragma once
#include "ff6/battle_state.hpp"
#include "ff6/input.hpp"

namespace ff6 {
// Ready-menu order is separate from combat slots, text order and action queues.
// Rank four is retained during a cycle until the next compaction pass.
struct BattleMenuOrder {
    std::array<std::optional<std::uint8_t>, 4> ranks;
    std::bitset<4> controlled_monsters;
    bool operator==(const BattleMenuOrder&) const = default;
};
struct BattleMenuQueueWork {
    std::uint32_t cycles = 0, fast_rom_clocks = 0;
};
// Optional source-work accounting follows the same first-match searches as
// the mutation. It includes the compaction routine's return, not its caller.
void compact_battle_menu_order(BattleMenuOrder& order, BattleMenuQueueWork* work = nullptr);
void request_battle_menu(BattleMenuOrder& order, BattleMenuRequest request, bool opening_allowed);
std::optional<std::uint8_t> ready_battle_menu(BattleMenuOrder& order, bool opening_allowed);
enum class BattleMenuCycle { consumed, next, previous };
void finish_battle_menu(BattleMenuOrder& order, std::uint8_t character, BattleMenuCycle cycle);

struct BattleMenuSounds {
    std::uint8_t move = 0, select = 0, invalid = 0;
    bool operator==(const BattleMenuSounds&) const = default;
};
enum class BattleCommandLayout { window, short_form, control };
enum class BattleMenuDecision {
    none,
    close,
    next_character,
    previous_character,
    command,
    row,
    defend,
    cancel,
    attack
};
struct BattleMenuNavigationResult {
    BattleMenuDecision decision = BattleMenuDecision::none;
    BattleMenuSounds sounds;
    std::optional<PreparedBattleCommand> command;
    std::optional<BattlePlayerAttack> attack;
    // Targeting policy is game data, distinct from an already selected target mask.
    std::uint8_t targeting = 0;
    // No value means this phase preserves the caller's current timing gate.
    std::optional<bool> wait_for_menu;
};
// Pure decision phase, before command-specific window/target initialization.
// Input is already mapped/sampled; rendering and transition waits belong to the caller.
BattleMenuNavigationResult navigate_battle_commands(std::uint8_t& cursor,
                                                    const BattleCommandList& commands,
                                                    BattleCommandLayout layout, Buttons repeating,
                                                    bool closing);
struct BattleListCursor {
    std::uint8_t column = 0, row = 0;
    bool operator==(const BattleListCursor&) const = default;
};
struct BattleListMovement {
    BattleListCursor cursor;
    // -1: scroll up; +1: scroll down; -65: left from the top-left corner.
    std::int16_t scroll = 0;
    bool sound = false;
};
BattleListMovement move_battle_list_cursor(BattleListCursor cursor, std::uint8_t columns,
                                           Buttons repeating);
struct BattleMagitekChoices {
    std::array<std::uint8_t, 8> targeting{}, terra{}, other{};
};
BattleMagitekChoices load_battle_magitek_choices(const ContentPack& pack);
BattleMenuNavigationResult navigate_battle_magitek(BattleListCursor& cursor,
                                                   const BattleMagitekChoices& choices,
                                                   std::uint8_t displayed_identity,
                                                   Buttons repeating, Buttons pressed,
                                                   bool closing);
} // namespace ff6
