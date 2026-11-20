#include "ff6/battle_drawing.hpp"
#include "rom-materials/battle_sprite_clip_bottom.macros.inc"
#include "rom-materials/battle_sprite_wrap_top.macros.inc"
#include "rom-materials/battle_monster_anchor_offset.macros.inc"
#include "rom-materials/battle_character_anchor_offsets.macros.inc"
#include <algorithm>
namespace ff6 {
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
void valid_style(BattleMonsterStyle style) {
    require(style.palette < 8 && style.priority < 4, "Invalid monster appearance");
}
std::uint16_t project_axis(std::uint16_t position, std::int16_t offset) {
    require(offset >= -128 && offset <= 127, "Invalid monster piece displacement");
    return static_cast<std::uint16_t>((unsigned{position} + static_cast<std::uint32_t>(offset)) &
                                      511U);
}
} // namespace
BattleScreenPoint project_battle_piece(std::uint16_t x, std::uint16_t y, std::int16_t offset_x,
                                       std::int16_t offset_y) {
    const auto px = project_axis(x, offset_x), py = project_axis(y, offset_y);
    const auto sx = px >= 256 ? std::int32_t{px} - 512 : std::int32_t{px};
    // All publication paths select the same visible edge and rejected-position
    // sentinel. The upper-half origin is the nine-bit coordinate format.
    const auto sy = py < FF6_ROM_VALUE_BATTLE_SPRITE_CLIP_BOTTOM_0 ? std::int32_t{py}
        : py >= 256U + FF6_ROM_VALUE_BATTLE_SPRITE_WRAP_TOP_0 ? std::int32_t{py} - 512
        : std::int32_t{FF6_ROM_VALUE_BATTLE_SPRITE_CLIP_BOTTOM_0};
    return {static_cast<std::int16_t>(sx), static_cast<std::int16_t>(sy)};
}
BattleSceneAnchors
calculate_battle_anchors(const std::array<BattleMonsterAnchorInput, 6>& monsters,
                         const std::array<BattleCharacterAnchorInput, 4>& characters) {
    BattleSceneAnchors out;
    for (unsigned i = 0; i < 6; ++i) {
        const auto m = monsters[i];
        require(m.width <= 16 && m.height <= 16, "Invalid monster anchor dimensions");
        auto& a = out[i + 4];
        a.center_x = wrap16(unsigned{m.x} + unsigned{m.width} * 4U);
        a.center_y = a.cursor_y = wrap16(unsigned{m.y} + unsigned{m.height} * 4U);
        a.bottom_y = wrap16(unsigned{m.y} + unsigned{m.height} * 8U -
                            FF6_ROM_VALUE_BATTLE_MONSTER_ANCHOR_OFFSET_0);
        a.draw_y = wrap16(unsigned{a.bottom_y} + m.overlap);
        a.cursor_x = wrap16(unsigned{m.x} - FF6_ROM_VALUE_BATTLE_MONSTER_ANCHOR_OFFSET_0 +
                            m.cursor_front);
    }
    for (unsigned i = 0; i < 4; ++i) {
        const auto c = characters[i];
        auto& a = out[i];
        const auto x = wrap16(unsigned{c.x} + c.offset_x + c.animation_x);
        a.cursor_x = wrap16(unsigned{x} - FF6_ROM_VALUE_BATTLE_CHARACTER_ANCHOR_OFFSETS_0 +
                            c.cursor_front);
        a.center_x = wrap16(unsigned{x} + FF6_ROM_VALUE_BATTLE_CHARACTER_ANCHOR_OFFSETS_1);
        const auto y = wrap16(unsigned{c.y} + c.offset_y);
        const unsigned center = unsigned{y} + FF6_ROM_VALUE_BATTLE_CHARACTER_ANCHOR_OFFSETS_2;
        a.center_y = a.cursor_y = wrap16(center);
        // The center-to-bottom addition retains carry from the center addition.
        a.bottom_y = a.draw_y = wrap16(unsigned{a.center_y} +
            FF6_ROM_VALUE_BATTLE_CHARACTER_ANCHOR_OFFSETS_3 + (center > 65535U ? 1U : 0U));
    }
    return out;
}
bool stage_battle_draw_order(BattleDrawOrder& state, const BattleSceneAnchors& anchors) {
    state.preparation_work = {};
    auto& work = state.preparation_work;
    if (state.awaiting_publication) {
        work.cycles = 12;
        work.fast_rom_clocks = 78;
        return false;
    }
    std::array<BattleDrawTarget, 10> order;
    for (std::uint8_t i = 0; i < 10; ++i)
        order[i] = {anchors[i].draw_y, i};
    // Ten entries make the original stable adjacent-exchange sort inexpensive.
    // Keep its complete nine-comparison passes: their data-dependent work
    // delays the next task pass. An optimized shrinking bound would lose that
    // evidence. Ties preserve character-before-monster and original slot order.
    bool changed;
    do {
        changed = false;
        ++work.passes;
        for (std::size_t i = 1; i < order.size(); ++i) {
            if (order[i - 1].bottom_y == order[i].bottom_y) ++work.ties;
            else if (order[i - 1].bottom_y < order[i].bottom_y) {
                std::swap(order[i - 1], order[i]);
                ++work.exchanges;
                changed = true;
            }
        }
    } while (changed);
    // Includes initialization, the final no-exchange pass, publication flag
    // and routine return. See BATTLE_TASK_CLOCK.md for the reviewed boundaries.
    work.cycles = 613U + 293U * work.passes + 72U * work.exchanges - 2U * work.ties;
    const auto slow_bytes = 133U + 40U * work.passes + 28U * work.exchanges;
    work.fast_rom_clocks = 6U * work.cycles + 2U * slow_bytes;
    state.staged = order;
    state.monster_masks.fill({});
    state.awaiting_publication = true;
    return true;
}
bool publish_battle_draw_order(BattleDrawOrder& state) {
    if (!state.awaiting_publication)
        return false;
    state.published = state.staged;
    state.awaiting_publication = false;
    return true;
}
std::vector<BattleMonsterDrawPiece>
project_battle_monster(const MonsterSpriteVariants& variants, std::uint8_t slot,
                       const BattleMonsterPosition& position, std::uint8_t retained_facing,
                       BattleMonsterStyle style, BattleMonsterStyleMask mask) {
    require(slot < 6 && position.flip < 4 && retained_facing < 4,
            "Invalid monster projection selection");
    valid_style(style);
    require(mask.palette < 8 && mask.priority < 4, "Invalid monster appearance mask");
    const unsigned orientation = unsigned{position.flip} ^ retained_facing;
    const auto& pieces = variants[orientation][slot];
    require(!pieces.empty() && pieces.size() <= 16, "Invalid selected monster sprite count");
    style.palette &= mask.palette;
    style.priority &= mask.priority;
    style.flip_x = (style.flip_x && mask.flip_x) != ((orientation & 1U) != 0);
    style.flip_y = (style.flip_y && mask.flip_y) != ((orientation & 2U) != 0);
    std::vector<BattleMonsterDrawPiece> out;
    out.reserve(pieces.size());
    for (const auto& piece : pieces) {
        const auto point = project_battle_piece(position.x, position.y, piece.x, piece.y);
        out.push_back({point.x, point.y, piece.first_tile, style});
    }
    return out;
}
RasterSprite make_battle_monster_sprite(const BattleMonsterDrawPiece& piece,
                                        const std::array<Tile, 256>& tiles,
                                        const std::array<Rgb555, 16>& colors) {
    valid_style(piece.style);
    require(piece.first_tile % 16 <= 12 && piece.first_tile / 16 <= 12,
            "Monster piece extends outside shared tiles");
    RasterSprite out;
    out.x = piece.x;
    out.y = piece.y;
    out.width = out.height = 32;
    constexpr std::array<std::uint16_t, 4> depths{2, 3, 6, 9};
    out.depth = depths[piece.style.priority];
    out.colors = colors;
    out.flip_x = piece.style.flip_x;
    out.flip_y = piece.style.flip_y;
    out.pixels.reserve(32 * 32);
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 32; ++x)
            out.pixels.push_back(
                tiles[piece.first_tile + (y / 8) * 16 + x / 8][(y % 8) * 8 + x % 8]);
    return out;
}
} // namespace ff6
