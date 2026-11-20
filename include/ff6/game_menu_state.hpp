#pragma once
#include "ff6/game_time.hpp"
#include "ff6/input.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
using WindowPalettes = std::array<std::array<Rgb555, 7>, 8>;
struct MenuCursorPosition {
    std::uint8_t x = 0, y = 0;
    bool operator==(const MenuCursorPosition&) const = default;
};
struct MenuListPosition {
    MenuCursorPosition position;
    std::uint8_t scroll = 0;
    bool operator==(const MenuListPosition&) const = default;
};
struct MenuCursorMemory {
    MenuCursorPosition main, character, rare_items, item_options;
    MenuListPosition items;
    std::array<MenuCursorPosition, 4> skills{};
    std::array<MenuListPosition, 4> magic{};
    bool operator==(const MenuCursorMemory&) const = default;
};
// Semantic options, not packed SRAM bytes. Configuration editing, elapsed-time
// advancement and save serialization remain separate owners.
struct GameMenuState {
    MenuCursorMemory cursors;
    GameTime time, saved_time;
    std::uint8_t time_frames = 0, battle_speed = 0, message_speed = 0;
    std::uint8_t magic_order = 0;
    std::uint16_t save_count = 0;
    bool battle_wait = false, short_commands = false, hide_gauge = false;
    bool mono = false, cursor_memory = false, remove_equipment = false;
    bool custom_buttons = false, multiplayer = false;
    std::uint8_t second_player_characters = 0;
    ButtonMap saved_buttons = default_button_map();
    bool operator==(const GameMenuState&) const = default;
};
// InitSaveSlot's defaults differ from cold InitNewGame. Speeds are zero-based.
inline GameMenuState new_game_menu_defaults(std::uint16_t speeds, bool wait,
                                            std::uint8_t skills_cursor) {
    GameMenuState result;
    result.battle_speed = static_cast<std::uint8_t>(speeds & 255U);
    result.message_speed = static_cast<std::uint8_t>(speeds >> 8U);
    result.battle_wait = wait;
    for (auto& cursor : result.cursors.skills)
        cursor.y = skills_cursor;
    return result;
}
} // namespace ff6
