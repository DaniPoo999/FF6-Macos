#include "ff6/main_menu.hpp"
#include "rom-materials/main_menu_sidebar_positions.macros.inc"
#include "rom-materials/main_menu_name_positions.macros.inc"
#include "rom-materials/main_menu_name_row_step.macros.inc"
#include "rom-materials/main_menu_portrait_x.macros.inc"
#include "rom-materials/main_menu_portrait_loop_origin.macros.inc"
#include "rom-materials/main_menu_portrait_pitch.macros.inc"
#include "rom-materials/main_menu_target_scroll.macros.inc"
#include "rom-materials/main_menu_target_portrait_x.macros.inc"
#include "rom-materials/main_menu_body_scroll.macros.inc"
#include "rom-materials/main_menu_body_label_palette.macros.inc"
#include "rom-materials/main_menu_cursor_palette_bytes.macros.inc"
#include "rom-materials/party_label_palette.macros.inc"
#include "rom-materials/main_menu_wounded_palette.macros.inc"
#include "rom-materials/main_menu_hidden_mp_offset.macros.inc"
#include "rom-materials/main_menu_hidden_mp_count.macros.inc"
#include "rom-materials/main_menu_cursor_priority.macros.inc"
#include "rom-materials/main_menu_portrait_priority.macros.inc"
#include "rom-materials/main_menu_cursor_first_color.macros.inc"
#include "rom-materials/main_menu_disabled_palette.macros.inc"
#include "rom-materials/main_menu_primary_font_slot.macros.inc"
#include "rom-materials/main_menu_paired_font_slots.macros.inc"
#include <algorithm>

namespace ff6 {
// The menu compositor's upper sprite bands straddle the two high background
// bands: their native ranks are 3 * priority + 1. Lower bands need a different
// ordering; reject them here instead of silently applying this conversion.
static_assert(FF6_ROM_VALUE_MAIN_MENU_CURSOR_PRIORITY_0 >= 2 && FF6_ROM_VALUE_MAIN_MENU_CURSOR_PRIORITY_0 <= 3);
static_assert(FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_PRIORITY_0 >= 2 && FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_PRIORITY_0 <= 3);
// C3/33307 and C3/3331F put name/status on the same row; all slots agree.
static_assert(FF6_ROM_VALUE_MAIN_MENU_NAME_POSITIONS_1 == FF6_ROM_VALUE_MAIN_MENU_NAME_POSITIONS_3);
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
MenuTextPosition position(std::uint16_t encoded) {
    MenuTextPlane plane;
    std::uint32_t base;
    if (encoded >= 0x3849 && encoded < 0x4849) {
        plane = MenuTextPlane::body;
        base = 0x3849;
    } else {
        require(encoded >= 0x7849 && encoded < 0x8849, "Invalid menu text position");
        plane = MenuTextPlane::sidebar;
        base = 0x7849;
    }
    require((encoded - base) % 2 == 0, "Unaligned menu text position");
    const auto cell = (encoded - base) / 2;
    return {plane, static_cast<std::uint8_t>((cell / 1024) * 32 + cell % 32),
            static_cast<std::uint8_t>((cell % 1024) / 32)};
}
MenuLabel label(ByteView bytes, std::size_t& cursor) {
    const auto location = position(read16(bytes, cursor));
    cursor += 2;
    Bytes glyphs;
    while (cursor < bytes.size() && bytes[cursor] != 0) {
        require(glyphs.size() < 32, "Menu label exceeds row width");
        glyphs.push_back(bytes[cursor++]);
    }
    require(cursor < bytes.size() && !glyphs.empty(), "Missing menu label terminator/text");
    ++cursor;
    return {location, std::move(glyphs)};
}
std::vector<RasterLine> plain_rows(const MenuWindowAssets& windows) {
    std::vector<RasterLine> rows(windows.rows.begin(), windows.rows.end());
    for (auto& row : rows) {
        row.blend = ColorBlend::none;
        row.operand = 0;
    }
    return rows;
}
TileLayer image_layer(std::span<const Tile> tiles, std::uint16_t columns,
                      const std::array<Rgb555, 16>& palette, std::int32_t x, std::int32_t y,
                      std::uint16_t depth, const MenuWindowAssets& windows) {
    TileLayer layer;
    require(columns > 0 && tiles.size() % columns == 0 && tiles.size() / columns <= 64,
            "Invalid menu image dimensions");
    layer.columns = columns;
    layer.rows = static_cast<std::uint16_t>(tiles.size() / columns);
    layer.wrap_x = layer.wrap_y = false;
    layer.tiles.assign(tiles.begin(), tiles.end());
    layer.palettes.push_back(palette);
    for (std::size_t i = 0; i < tiles.size(); ++i)
        layer.cells.push_back({static_cast<std::uint32_t>(i), 0, depth, false, false});
    layer.lines = plain_rows(windows);
    for (auto& row : layer.lines) {
        row.offset_x = -x;
        row.offset_y = -y;
    }
    return layer;
}
} // namespace

MainMenuAssets load_main_menu(const ContentPack& pack) {
    MainMenuAssets assets{};
    assets.windows = load_menu_windows(pack);
    const auto font = pack.get("menu_graphics_small_font_2bpp");
    require(font.size() == 4096, "Invalid menu font length");
    assets.sidebar_font = decode_tiles(font, 2);
    assets.body_font = assets.sidebar_font;
    const auto solids = pack.get("menu_font_solid_patterns");
    require(solids.size() == 32, "Invalid solid font pattern length");
    Bytes expanded;
    for (std::size_t tile = 0; tile < 8; ++tile)
        for (std::size_t pair = 0; pair < 2; ++pair)
            for (unsigned y = 0; y < 8; ++y) {
                expanded.push_back(solids[tile * 4 + pair * 2]);
                expanded.push_back(solids[tile * 4 + pair * 2 + 1]);
            }
    const auto solid_tiles = decode_tiles(expanded, 4);
    std::copy(solid_tiles.begin(), solid_tiles.end(), assets.body_font.begin());
    const auto colors = pack.get("interface_palettes_menu_pal_pal");
    require(colors.size() == 200, "Invalid menu palette length");
    for (std::size_t i = 0; i < assets.text_colors.size(); ++i)
        assets.text_colors[i] = read16(colors, i * 2) & 0x7fffU;
    // This resource supplies four colors; its destination origin is selected by the ROM.
    static_assert(FF6_ROM_VALUE_MAIN_MENU_CURSOR_FIRST_COLOR_0 + 4 <= 16);
    for (std::size_t i = FF6_ROM_VALUE_MAIN_MENU_CURSOR_FIRST_COLOR_0; i < FF6_ROM_VALUE_MAIN_MENU_CURSOR_FIRST_COLOR_0 + 4; ++i)
        assets.cursor_colors[i] = read16(colors, FF6_ROM_VALUE_MAIN_MENU_CURSOR_PALETTE_BYTES_0 + (i - FF6_ROM_VALUE_MAIN_MENU_CURSOR_FIRST_COLOR_0) * 2) & 0x7fffU;
    const auto cursor_graphics = decode_tiles(pack.get("menu_graphics_menu_sprite_4bpp"), 4);
    // The original transfer spans 160 tiles, but this resource owns only 64.
    // The cursor uses four of those; adjacent transfer bytes are not its assets.
    require(cursor_graphics.size() == 64, "Invalid menu sprite resource");
    for (std::size_t i = 0; i < 4; ++i) {
        assets.cursor_tiles[i] = cursor_graphics[(i / 2) * 16 + i % 2];
        for (const auto pixel : assets.cursor_tiles[i])
            require(pixel == 0 || pixel >= FF6_ROM_VALUE_MAIN_MENU_CURSOR_FIRST_COLOR_0, "Cursor references an unowned color");
    }
    const auto cursor_positions = pack.get("menu_main_cursor_positions");
    require(cursor_positions.size() == 14, "Invalid menu cursor positions");
    for (std::size_t i = 0; i < 7; ++i)
        assets.cursor_positions[i] = {cursor_positions[i * 2], cursor_positions[i * 2 + 1]};
    const auto character_positions = pack.get("menu_character_cursor_positions");
    require(character_positions.size() == 8, "Invalid character cursor positions");
    for (std::size_t i = 0; i < 4; ++i)
        assets.character_cursor_positions[i] = {character_positions[i * 2],
                                                character_positions[i * 2 + 1]};
    assets.portraits = decode_tiles(pack.get("menu_graphics_portrait_gfx_4bpp"), 4);
    require(assets.portraits.size() == 19 * 25, "Invalid portrait graphics length");
    assets.portrait_order = decode_portrait_order(pack.get("field_portrait_tile_order"));
    const auto portrait_colors = pack.get("menu_graphics_portrait_pal_pal");
    require(portrait_colors.size() == 19 * 32, "Invalid portrait palettes length");
    for (std::size_t p = 0; p < 19; ++p)
        for (std::size_t c = 0; c < 16; ++c)
            assets.portrait_colors[p][c] = read16(portrait_colors, p * 32 + c * 2) & 0x7fffU;
    const auto graphic_aliases = pack.get("menu_portrait_graphic_aliases");
    const auto palette_aliases = pack.get("menu_portrait_palette_aliases");
    require(graphic_aliases.size() == 54 && palette_aliases.size() == 27,
            "Invalid portrait alias lengths");
    for (std::size_t i = 0; i < 27; ++i) {
        const auto offset = read16(graphic_aliases, i * 2);
        require(offset % 800 == 0 && offset / 800 < 19 && palette_aliases[i] < 19,
                "Invalid portrait alias");
        assets.portrait_graphics[i] = static_cast<std::uint8_t>(offset / 800);
        assets.portrait_palettes[i] = palette_aliases[i];
    }
    const auto labels = pack.get("menu_main_labels");
    std::size_t cursor = 0;
    for (auto& item : assets.labels)
        item = label(labels, cursor);
    require(cursor == labels.size(), "Trailing main menu labels");
    cursor = 0;
    const auto order = pack.get("menu_main_order_label");
    assets.order_label = label(order, cursor);
    require(cursor == order.size(), "Trailing order label");
    const auto wounded = pack.get("menu_main_wounded_label");
    require(wounded.size() == 8 && std::find(wounded.begin(), wounded.end(), 0) == wounded.end(),
            "Invalid wounded label");
    assets.wounded_label.assign(wounded.begin(), wounded.end());
    for (std::size_t slot = 0; slot < 4; ++slot) {
        const auto positions = pack.get("menu_main_number_positions_" + std::to_string(slot + 1));
        require(positions.size() == 10, "Invalid character number positions");
        for (std::size_t number = 0; number < 5; ++number)
            assets.number_positions[slot][number] = position(read16(positions, number * 2));
    }
    const auto scroll = pack.get("menu_main_text_scroll_bands");
    cursor = 0;
    std::size_t row = 0;
    std::uint16_t value = 0;
    while (cursor < scroll.size() && scroll[cursor] != 0) {
        const auto count = scroll[cursor++];
        require(count <= 127 && count <= 224 - row, "Invalid menu text scroll band");
        value = read16(scroll, cursor);
        cursor += 2;
        require(value <= 255, "Menu text scroll outside its layout");
        for (unsigned i = 0; i < count; ++i)
            assets.sidebar_scroll[row++] = value;
    }
    require(cursor + 1 == scroll.size() && row == 167 && value == 0,
            "Invalid menu text scroll extent");
    // This resource ends early; its final zero offset persists to the last row.
    std::fill(assets.sidebar_scroll.begin() + static_cast<std::ptrdiff_t>(row),
              assets.sidebar_scroll.end(), value);
    return assets;
}

Bytes menu_number(std::uint32_t value, unsigned source_digits, unsigned visible_digits,
                  bool leading_zeroes) {
    require(source_digits > 0 && source_digits <= 8 && visible_digits > 0 &&
                visible_digits <= source_digits,
            "Invalid menu number width");
    Bytes digits(source_digits);
    for (std::size_t i = digits.size(); i > 0; --i) {
        digits[i - 1] = static_cast<std::uint8_t>(0xb4U + value % 10U);
        value /= 10;
    }
    require(value == 0, "Menu number exceeds source width");
    if (!leading_zeroes)
        for (std::size_t i = 0; i + 1 < digits.size() && digits[i] == 0xb4; ++i)
            digits[i] = 0xff;
    return Bytes(digits.end() - visible_digits, digits.end());
}

static RasterScene make_party_menu(const MainMenuAssets& assets, const MainMenuView& view,
                                   bool target) {
    require(view.cursor < 7 && view.intensity <= 15 && view.steps <= 0xffffff &&
                view.gil <= 9999999,
            "Invalid main menu display state");
    auto colors = assets.text_colors;
    const auto font_color = view.font_color.value_or(colors[FF6_ROM_VALUE_MAIN_MENU_PRIMARY_FONT_SLOT_0]);
    require(font_color <= 0x7fff, "Invalid menu font color");
    // InitFontColor owns these entries, shared by different font palette groups.
    for (const auto index : {static_cast<unsigned>(FF6_ROM_VALUE_MAIN_MENU_PRIMARY_FONT_SLOT_0),
                             static_cast<unsigned>(FF6_ROM_VALUE_MAIN_MENU_PAIRED_FONT_SLOTS_0),
                             static_cast<unsigned>(FF6_ROM_VALUE_MAIN_MENU_PAIRED_FONT_SLOTS_1)})
        colors[index] = font_color;
    TileLayer body, sidebar;
    body.columns = sidebar.columns = 64;
    body.rows = sidebar.rows = 32;
    body.tiles = assets.body_font;
    sidebar.tiles = assets.sidebar_font;
    require(body.tiles.size() == 256 && sidebar.tiles.size() == 256, "Invalid menu fonts");
    // Clear cells use an explicitly empty tile independent of glyph zero.
    body.tiles.emplace_back();
    sidebar.tiles.emplace_back();
    body.cells.assign(2048, {256, 0, 9 * 256, false, false});
    sidebar.cells.assign(2048, {256, 0, 11 * 256, false, false});
    for (std::size_t p = 0; p < 5; ++p) {
        std::array<Rgb555, 16> group{};
        std::copy_n(colors.begin() + static_cast<std::ptrdiff_t>(p * 16), 16, group.begin());
        body.palettes.push_back(group);
    }
    for (std::size_t p = 0; p < 8; ++p) {
        std::array<Rgb555, 16> group{};
        std::copy_n(colors.begin() + static_cast<std::ptrdiff_t>(p * 4), 4, group.begin());
        sidebar.palettes.push_back(group);
    }
    body.lines = sidebar.lines = plain_rows(assets.windows);
    for (std::size_t y = 0; y < 224; ++y) {
        body.lines[y].offset_y = FF6_ROM_VALUE_MAIN_MENU_BODY_SCROLL_0 + 1;
        body.lines[y].offset_x = target ? static_cast<std::int32_t>(FF6_ROM_VALUE_MAIN_MENU_TARGET_SCROLL_0) - 65536 : 0;
        sidebar.lines[y].offset_y = std::int32_t{assets.sidebar_scroll[y]} + 1;
    }
    const auto text = [&](MenuTextPosition position, ByteView glyphs, unsigned palette) {
        auto& layer = position.plane == MenuTextPlane::body ? body : sidebar;
        require(position.column < 64 && position.row < 32 && palette < layer.palettes.size(),
                "Invalid menu text placement");
        const auto start =
            (std::size_t{position.column} / 32) * 1024 + position.row * 32 + position.column % 32;
        require(glyphs.size() <= 2048 - start, "Menu text extends outside canvas");
        for (std::size_t i = 0; i < glyphs.size() && glyphs[i] != 0; ++i) {
            const auto cell = start + i;
            const auto at = (cell % 1024) / 32 * 64 + (cell / 1024) * 32 + cell % 32;
            layer.cells[at].tile = glyphs[i];
            layer.cells[at].palette = palette;
        }
    };
    const auto draw_label = [&](const MenuLabel& item, unsigned palette) {
        text(item.position, item.glyphs, palette);
    };
    // The label sequence is a resource schema; original strings stay in the pack.
    for (std::size_t i = 20; i < 27; ++i)
        draw_label(assets.labels[i], i == 26 && !view.save_enabled ? FF6_ROM_VALUE_MAIN_MENU_DISABLED_PALETTE_0 : 0U);
    for (const auto i : {27U, 29U, 30U})
        draw_label(assets.labels[i], FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0);
    draw_label(assets.labels[28], 0);
    draw_label(assets.order_label, FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0);
    text({MenuTextPlane::sidebar, FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_0,
          FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_1}, menu_number(view.steps, 8, 7), 0);
    text({MenuTextPlane::sidebar, FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_2,
          FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_3}, menu_number(view.gil, 8, 7), 0);
    text({MenuTextPlane::sidebar, FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_4,
          FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_5}, menu_number(view.hours, 3, 2), 0);
    text({MenuTextPlane::sidebar, FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_6,
          FF6_ROM_VALUE_MAIN_MENU_SIDEBAR_POSITIONS_7}, menu_number(view.minutes, 3, 2, true), 0);
    for (std::size_t slot = 0; slot < view.party.size(); ++slot) {
        if (!view.party[slot])
            continue;
        const auto& character = *view.party[slot];
        require(character.maximum_hp <= 9999 && character.hp <= character.maximum_hp &&
                    character.maximum_mp <= 999 && character.mp <= character.maximum_mp &&
                    character.portrait < 27,
                "Invalid character display values");
        const unsigned palette = character.wounded ? FF6_ROM_VALUE_MAIN_MENU_WOUNDED_PALETTE_0 : 0U;
        for (std::size_t label = 0; label < 5; ++label)
            draw_label(assets.labels[slot * 5 + label], label < 3 ? FF6_ROM_VALUE_MAIN_MENU_BODY_LABEL_PALETTE_0 : palette);
        const auto row = static_cast<std::uint8_t>(FF6_ROM_VALUE_MAIN_MENU_NAME_POSITIONS_1 + slot * FF6_ROM_VALUE_MAIN_MENU_NAME_ROW_STEP_0);
        text({MenuTextPlane::body, FF6_ROM_VALUE_MAIN_MENU_NAME_POSITIONS_0, row}, character.name, palette);
        if (character.wounded)
            text({MenuTextPlane::body, FF6_ROM_VALUE_MAIN_MENU_NAME_POSITIONS_2, row}, assets.wounded_label, palette);
        const auto& positions = assets.number_positions[slot];
        text(positions[0], menu_number(character.level, 3, 2), palette);
        text(positions[1], menu_number(character.hp, 5, 4), palette);
        text(positions[2], menu_number(character.maximum_hp, 5, 4), palette);
        if (character.show_mp) {
            text(positions[3], menu_number(character.mp, 5, 4), palette);
            text(positions[4], menu_number(character.maximum_mp, 5, 4), palette);
        } else {
            auto location = positions[3];
            require(location.column >= FF6_ROM_VALUE_MAIN_MENU_HIDDEN_MP_OFFSET_0, "Invalid hidden MP position");
            location.column = static_cast<std::uint8_t>(location.column - FF6_ROM_VALUE_MAIN_MENU_HIDDEN_MP_OFFSET_0);
            text(location, Bytes(FF6_ROM_VALUE_MAIN_MENU_HIDDEN_MP_COUNT_0, 0xff), palette);
        }
    }
    RasterScene scene;
    scene.intensity.assign(224, view.intensity);
    scene.layers.push_back(std::move(body));
    if (!target) {
        scene.layers.push_back(make_main_menu_windows(assets.windows, view.window));
        scene.layers.push_back(std::move(sidebar));
    }
    if (!target && view.sidebar_cursor_visible) {
        const auto cursor = assets.cursor_positions[view.cursor];
        scene.layers.push_back(image_layer(assets.cursor_tiles, 2, assets.cursor_colors, cursor[0],
                                           cursor[1], (3 * FF6_ROM_VALUE_MAIN_MENU_CURSOR_PRIORITY_0 + 1) * 256, assets.windows));
    }
    if (!target && view.character_cursor) {
        require(*view.character_cursor < 4 && view.party[*view.character_cursor].has_value(),
                "Character cursor must select an occupied slot");
        const auto cursor = assets.character_cursor_positions[*view.character_cursor];
        scene.layers.push_back(image_layer(assets.cursor_tiles, 2, assets.cursor_colors, cursor[0],
                                           cursor[1], (3 * FF6_ROM_VALUE_MAIN_MENU_CURSOR_PRIORITY_0 + 1) * 256, assets.windows));
    }
    for (std::size_t slot = 0; slot < view.party.size(); ++slot) {
        if (!view.party[slot])
            continue;
        const auto& character = *view.party[slot];
        const auto image = assets.portrait_graphics[character.portrait];
        const auto palette = assets.portrait_palettes[character.portrait];
        require(image < 19 && palette < 19 && assets.portraits.size() == 19 * 25,
                "Invalid resolved portrait identity");
        std::array<Tile, 25> tiles{};
        for (std::size_t i = 0; i < 25; ++i) {
            require(assets.portrait_order[i] < 25, "Invalid portrait tile order");
            tiles[i] = assets.portraits[image * 25 + assets.portrait_order[i]];
        }
        // InitPortraitRowPos tests the target-page flag before battle row.
        // The target page applies its selected scroll to the shared row origin.
        const auto x = target ? FF6_ROM_VALUE_MAIN_MENU_TARGET_PORTRAIT_X_0 : character.back_row ? FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_X_0 : FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_X_1;
        scene.layers.push_back(
            image_layer(tiles, 5, assets.portrait_colors[palette], x,
                        (FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_LOOP_ORIGIN_0 - 4 * FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_PITCH_0) +
                        static_cast<std::int32_t>(slot) * FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_PITCH_0, (3 * FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_PRIORITY_0 + 1) * 256, assets.windows));
    }
    return scene;
}
RasterScene make_main_menu_scene(const MainMenuAssets& assets, const MainMenuView& view) {
    return make_party_menu(assets, view, false);
}
RasterScene make_menu_target_party_scene(const MainMenuAssets& assets, const MainMenuView& view) {
    return make_party_menu(assets, view, true);
}
} // namespace ff6
