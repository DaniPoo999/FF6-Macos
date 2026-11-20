#pragma once
#include "ff6/menu_windows.hpp"

namespace ff6 {
enum class MenuTextPlane { body, sidebar };
struct MenuTextPosition {
    MenuTextPlane plane;
    std::uint8_t column, row;
};
struct MenuLabel {
    MenuTextPosition position;
    Bytes glyphs;
};
struct MainMenuAssets {
    MenuWindowAssets windows;
    std::vector<Tile> body_font, sidebar_font, portraits;
    std::array<Rgb555, 80> text_colors;
    std::array<std::array<Rgb555, 16>, 19> portrait_colors;
    std::array<Rgb555, 16> cursor_colors;
    std::array<Tile, 4> cursor_tiles;
    PortraitOrder portrait_order;
    std::array<std::uint8_t, 27> portrait_graphics, portrait_palettes;
    std::array<std::array<std::uint8_t, 2>, 7> cursor_positions;
    std::array<std::array<std::uint8_t, 2>, 4> character_cursor_positions;
    std::array<MenuLabel, 31> labels;
    MenuLabel order_label;
    Bytes wounded_label;
    std::array<std::array<MenuTextPosition, 5>, 4> number_positions;
    std::array<std::uint16_t, 224> sidebar_scroll;
};
struct MenuCharacterView {
    std::array<std::uint8_t, 6> name{}; // Original glyph encoding; zero terminates.
    std::uint8_t level = 1, portrait = 0;
    std::uint16_t hp = 0, maximum_hp = 0, mp = 0, maximum_mp = 0;
    bool show_mp = false, wounded = false, back_row = false;
};
struct MainMenuView {
    std::array<std::optional<MenuCharacterView>, 4> party;
    MenuWindowAppearance window;
    std::optional<Rgb555> font_color;
    std::uint32_t steps = 0, gil = 0;
    std::uint8_t hours = 0, minutes = 0, cursor = 0, intensity = 15;
    bool save_enabled = false;
    bool sidebar_cursor_visible = true;
    std::optional<std::uint8_t> character_cursor;
};

// Display values are already resolved by game rules. This API owns no original
// RAM, clock, HP-boost calculation, MP-eligibility or status-animation state.
MainMenuAssets load_main_menu(const ContentPack& pack);
RasterScene make_main_menu_scene(const MainMenuAssets& assets, const MainMenuView& view);
// Item/Magic target pages share character blocks shifted eight tiles right.
// Portrait alignment is fixed there; battle row remains a gameplay property.
// The caller supplies the target windows and selection cursor(s).
RasterScene make_menu_target_party_scene(const MainMenuAssets& assets, const MainMenuView& view);
// Convert the full source width, suppress leading zeroes, THEN take the visible
// suffix. This preserves e.g. a three-digit level displayed in two columns.
Bytes menu_number(std::uint32_t value, unsigned source_digits, unsigned visible_digits,
                  bool leading_zeroes = false);
} // namespace ff6
