#include "ff6/battle_cursors.hpp"
#include <algorithm>
#include "rom-materials/battle_cursor_hand_origin.macros.inc"
#include "rom-materials/battle_cursor_roulette_tile.macros.inc"
#include "rom-materials/battle_cursor_indicator_x.macros.inc"
#include "rom-materials/battle_cursor_indicator_y.macros.inc"
#include "rom-materials/battle_cursor_target_limit.macros.inc"
#include "rom-materials/battle_cursor_image_schedule.macros.inc"
#include "rom-materials/battle_cursor_hand_priority.macros.inc"
#include "rom-materials/battle_cursor_indicator_priority.macros.inc"
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
// Native cursor ranks keep the indicator above the actor plane and hands
// above the background planes; original priorities are ROM-selected inputs.
constexpr auto hand_depth = 3 + 2 * FF6_ROM_VALUE_BATTLE_CURSOR_HAND_PRIORITY_0;
constexpr auto indicator_depth = 3 + 2 * FF6_ROM_VALUE_BATTLE_CURSOR_INDICATOR_PRIORITY_0;
bool outside(std::uint16_t x, std::uint16_t y) {
    // Original coordinates wrap at nine bits; higher bits are not bounds tests.
    return ((x | y) & 256U) != 0;
}
RasterSprite sprite(BattleHandCursor cursor, std::span<const Tile, 4> tiles,
                    const std::array<Rgb555, 4>& colors, std::uint16_t depth) {
    RasterSprite out;
    out.x = cursor.x;
    out.y = cursor.y >= 224 ? int{cursor.y} - 256 : cursor.y;
    out.width = out.height = 16;
    out.depth = depth;
    out.flip_x = cursor.flip_x;
    std::ranges::copy(colors, out.colors.begin() + 12);
    out.pixels.resize(256);
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x)
            out.pixels[y * 16 + x] = tiles[(y / 8) * 2 + x / 8][(y % 8) * 8 + x % 8];
    return out;
}
} // namespace
BattleCursorAssets load_battle_cursor_assets(const ContentPack& pack) {
    BattleCursorAssets out;
    const auto commands = pack.get("battle_command_cursor_positions");
    const auto columns = pack.get("battle_magitek_cursor_columns");
    const auto rows = pack.get("battle_list_cursor_rows");
    const auto palette = pack.get("battle_character_status_palette");
    require(commands.size() == 24 && columns.size() == 2 && rows.size() == 4 &&
                palette.size() == 32,
            "Invalid battle cursor resource extent");
    constexpr std::array<unsigned, 3> layouts{1, 2, 0};
    for (unsigned layout = 0; layout < 3; ++layout)
        for (unsigned row = 0; row < 4; ++row) {
            const auto at = layouts[layout] * 8 + row * 2;
            out.commands[layout][row] = {commands[at], commands[at + 1]};
        }
    std::ranges::copy(columns, out.magitek_columns.begin());
    std::ranges::copy(rows, out.list_rows.begin());
    const auto decoded = decompress_lzss(pack.get("menu_graphics_status_gfx_4bpp_lz"));
    require(decoded.size() == 8192, "Invalid battle status graphics extent");
    const auto tiles = decode_tiles(decoded, 4);
    out.roulette = tiles[FF6_ROM_VALUE_BATTLE_CURSOR_ROULETTE_TILE_0];
    for (unsigned part = 0; part < 4; ++part)
        out.hand[part] = tiles[FF6_ROM_VALUE_BATTLE_CURSOR_HAND_ORIGIN_0 + (part / 2) * 16 + part % 2];
    // The four top halves are identical in the supported resource. The last
    // lower half deliberately returns to the first phase (shared status data).
    constexpr std::array<unsigned, 4> top_tiles{
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_0,
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_1,
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_2,
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_3};
    constexpr std::array<unsigned, 4> bottom_tiles{
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_4,
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_5,
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_6,
        FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_7};
    for (unsigned part = 0; part < 2; ++part) {
        out.indicator_top[part] = tiles[top_tiles[0] + part];
        for (unsigned phase = 0; phase < 4; ++phase) {
            require(tiles[top_tiles[phase] + part] == out.indicator_top[part],
                    "Unsupported changing battle indicator top image");
            out.indicator_bottom[phase][part] = tiles[bottom_tiles[phase] + part];
        }
    }
    auto validate = [](const Tile& tile) {
        require(std::ranges::all_of(tile, [](auto value) { return value == 0 || value >= 12; }),
                "Battle cursor uses an unowned palette color");
    };
    for (const auto& tile : out.hand)
        validate(tile);
    for (const auto& tile : out.indicator_top)
        validate(tile);
    for (const auto& phase : out.indicator_bottom)
        for (const auto& tile : phase)
            validate(tile);
    for (unsigned i = 0; i < 4; ++i) {
        out.colors[i] = read16(palette, i * 2);
        require(out.colors[i] <= 32767, "Invalid battle cursor color");
    }
    return out;
}
BattleScreenPoint battle_command_cursor_position(const BattleCursorAssets& a,
                                                 BattleCommandLayout layout, std::uint8_t row) {
    const auto kind = static_cast<unsigned>(layout);
    require(kind < 3 && row < 4, "Invalid battle command cursor");
    return a.commands[kind][row];
}
BattleScreenPoint battle_magitek_cursor_position(const BattleCursorAssets& a,
                                                 BattleListCursor cursor) {
    require(cursor.column < 2 && cursor.row < 4, "Invalid battle Magitek cursor");
    return {a.magitek_columns[cursor.column], a.list_rows[cursor.row]};
}
BattleCursorFrame prepare_battle_cursors(const BattleCursorRequest& request,
                                         const BattleSceneAnchors& anchors,
                                         const BattleMonsterPositions& positions,
                                         const BattleCharacterViewData& view,
                                         const std::array<BattleCharacterPose, 4>& poses,
                                         const std::array<std::uint32_t, 4>& shown_status) {
    BattleCursorFrame out;
    out.menu = request.menu;
    if (request.active_character) {
        const auto slot = *request.active_character;
        require(slot < 4, "Invalid active battle cursor character");
        const auto& a = anchors[slot];
        if (!request.hide_characters && !request.hide_indicator &&
            !outside(a.center_x, a.center_y)) {
            const auto offset =
                battle_character_y_offset(view, shown_status[slot], poses[slot], request.magitek);
            out.indicator = BattleHandCursor{
                wrap8(unsigned{a.center_x} - (FF6_ROM_VALUE_BATTLE_CURSOR_INDICATOR_X_0 + 0U)),
                wrap8(static_cast<std::uint32_t>(int{a.center_y} - FF6_ROM_VALUE_BATTLE_CURSOR_INDICATOR_Y_0 + offset)), false};
        }
    }
    // In both-group selection, even frames show character hands, odd frames
    // monster hands. Selection is tested before visibility, with no fallback.
    const bool characters =
        request.targets.multiple ? (request.clock & 1U) == 0 : request.targets.characters.any();
    for (unsigned slot = 0; slot < (characters ? 4U : 6U); ++slot) {
        const bool selected =
            characters ? request.targets.characters[slot] && request.visible.characters[slot]
                       : request.targets.monsters[slot] && request.visible.monsters[slot];
        const auto& a = anchors[(characters ? 0U : 4U) + slot];
        if (!selected || outside(a.cursor_x, a.cursor_y) || wrap8(a.cursor_y) >= FF6_ROM_VALUE_BATTLE_CURSOR_TARGET_LIMIT_0)
            continue;
        const auto front = characters ? positions.character_cursor_front[slot]
                                      : positions.positions[slot].cursor_front;
        // Only the low byte is consulted by the original facing test.
        out.targets[slot] =
            BattleHandCursor{wrap8(a.cursor_x), wrap8(a.cursor_y), wrap8(front) != 0};
    }
    return out;
}
void publish_battle_cursors(BattleCursorPublication& state, const BattleCursorAssets& assets,
                            std::uint8_t clock, bool hide_characters) {
    state.visible = state.prepared;
    if (hide_characters)
        return;
    if ((clock % FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_8) == FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_9)
        std::ranges::copy(assets.indicator_top, state.indicator_image.begin());
    else if ((clock % FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_8) == FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_10)
        std::ranges::copy(assets.indicator_bottom[(clock / FF6_ROM_VALUE_BATTLE_CURSOR_IMAGE_SCHEDULE_8) % 4U],
                          state.indicator_image.begin() + 2);
}
BattleCursorSprites make_battle_cursor_sprites(const BattleCursorPublication& state,
                                               const BattleCursorAssets& assets) {
    BattleCursorSprites out;
    for (const auto& hand : state.visible.targets)
        if (hand)
            out.before_actors.push_back(sprite(*hand, assets.hand, assets.colors, hand_depth));
    if (state.visible.indicator)
        out.before_actors.push_back(
            sprite(*state.visible.indicator, state.indicator_image, assets.colors, indicator_depth));
    if (state.visible.menu)
        out.after_actors.push_back(sprite(*state.visible.menu, assets.hand, assets.colors, hand_depth));
    return out;
}
std::optional<RasterSprite> make_battle_roulette_sprite(std::uint8_t target,
    const BattleSceneAnchors& anchors, const BattleMonsterPositions& positions,
    const BattleCursorAssets& assets) {
    require(target < 10, "Invalid automatic Roulette target");
    const auto& anchor = anchors[target];
    if (outside(anchor.cursor_x, anchor.cursor_y)) return {};
    RasterSprite out;
    out.x = wrap8(anchor.cursor_x);
    out.y = wrap8(anchor.cursor_y);
    out.width = out.height = 8;
    out.depth = hand_depth;
    out.flip_x = wrap8(target < 4 ? positions.character_cursor_front[target]
                                 : positions.positions[target - 4].cursor_front) != 0;
    std::ranges::copy(assets.colors, out.colors.begin() + 12);
    out.pixels.assign(assets.roulette.begin(), assets.roulette.end());
    return out;
}
} // namespace ff6
