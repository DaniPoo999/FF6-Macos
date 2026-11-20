#include "ff6/battle_monster_view.hpp"
#include "rom-materials/battle_character_cursor_back_offset.macros.inc"
#include "rom-materials/battle_monster_approach_geometry.macros.inc"
#include "rom-materials/battle_colosseum_monster_bound.macros.inc"
#include "rom-materials/battle_colosseum_monster_inset.macros.inc"
#include <algorithm>
#include <numeric>
namespace ff6 {
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
std::uint16_t replace_low(std::uint16_t word, std::uint8_t low) {
    return static_cast<std::uint16_t>((word & 0xff00U) | low);
}
std::int16_t mirror_piece_offset(std::int16_t offset, std::uint8_t tiles) {
    // Reflect a 32-pixel piece within the image, then retain the original
    // signed-byte displacement. Widen before subtracting; narrow explicitly.
    const auto reflected = std::int32_t{tiles} * 8 - 32 - offset;
    return static_cast<std::int16_t>(signed8(wrap8(static_cast<std::uint32_t>(reflected))));
}
void validate_sizes(const std::array<MonsterDimensions, 6>& sizes) {
    for (const auto& size : sizes)
        require(size.width <= 16 && size.height <= 16, "Invalid monster display size");
}
} // namespace
BattleMonsterViewData decode_battle_monster_view_data(ByteView maps, ByteView pieces,
                                                      ByteView alignments, ByteView offsets) {
    require(maps.size() == 485 && pieces.size() == 64 && alignments.size() == 256 &&
                offsets.size() == 5,
            "Invalid monster view resource extent");
    BattleMonsterViewData result;
    std::array<MonsterSpritePiece, 16> templates;
    for (unsigned i = 0; i < 16; ++i) {
        const auto at = i * 4;
        const auto tile = read16(pieces, at + 2);
        require(tile >= 256 && tile < 512 && (tile & 15U) <= 12 && (tile & 255U) / 16 <= 12,
                "Invalid monster sprite tile selection");
        templates[i] = {pieces[at], pieces[at + 1], static_cast<std::uint8_t>(tile & 255U)};
    }
    for (unsigned layout = 0; layout < 13; ++layout) {
        const auto pointer = read16(maps, layout * 12);
        require(pointer >= 0xc540, "Invalid monster sprite layout pointer");
        std::size_t at = pointer - 0xc4a4U;
        // Only the first pointer in each six-entry group is used. The caller
        // walks six consecutive variable-length lists, including following groups.
        for (auto& slot : result.layouts[layout]) {
            const auto origin = slice(maps, at, 2);
            at += 2;
            while (true) {
                const auto id = slice(maps, at++, 1)[0];
                if ((id & 128U) != 0)
                    break;
                require(id < templates.size() && slot.size() < 16,
                        "Invalid monster sprite piece list");
                const auto p = templates[id];
                const auto x = wrap8(static_cast<std::uint32_t>(p.x) + 256U - origin[0]);
                const auto y = wrap8(static_cast<std::uint32_t>(p.y) + 256U - origin[1]);
                require(x != 255, "Monster sprite offset collides with list terminator");
                slot.push_back({static_cast<std::int16_t>(signed8(x)),
                                static_cast<std::int16_t>(signed8(y)), p.first_tile});
            }
        }
    }
    for (unsigned i = 0; i < 256; ++i) {
        require(alignments[i] < 5, "Invalid monster vertical alignment");
        result.alignments[i] = alignments[i];
    }
    for (unsigned i = 0; i < 5; ++i)
        result.alignment_offsets[i] = signed8(offsets[i]);
    return result;
}
BattleMonsterViewData load_battle_monster_view_data(const ContentPack& pack) {
    return decode_battle_monster_view_data(
        pack.get("monster_sprite_maps"), pack.get("monster_sprite_pieces"),
        pack.get("data_ece800_monster_align"), pack.get("monster_alignment_offsets"));
}
MonsterSpriteVariants make_monster_sprite_variants(const BattleMonsterViewData& data,
                                                   std::uint8_t layout,
                                                   const std::array<MonsterDimensions, 6>& sizes) {
    require(layout < data.layouts.size(), "Invalid monster sprite layout");
    validate_sizes(sizes);
    MonsterSpriteVariants result;
    for (unsigned orientation = 0; orientation < 4; ++orientation) {
        result[orientation] = data.layouts[layout];
        for (unsigned slot = 0; slot < 6; ++slot)
            for (auto& piece : result[orientation][slot]) {
                if ((orientation & 1U) != 0)
                    piece.x = mirror_piece_offset(piece.x, sizes[slot].width);
                if ((orientation & 2U) != 0 && piece.y != -1)
                    piece.y = mirror_piece_offset(piece.y, sizes[slot].height);
            }
    }
    return result;
}
BattleMonsterPositions initialize_monster_positions(const BattleMonsterViewData& data,
                                                    const BattleMonsterPositionRequest& request) {
    require(static_cast<unsigned>(request.approach) < 4, "Invalid battle approach");
    validate_sizes(request.sizes);
    BattleMonsterPositions result;
    result.positions = request.positions;
    result.character_cursor_front = request.character_cursor_front;
    auto& groups = result.groups;
    for (std::uint8_t i = 0; i < 4; ++i) {
        if (request.approach == BattleApproach::side && i < 2)
            groups.characters_right.push_back(i);
        else
            groups.characters_left.push_back(i);
        if (request.approach == BattleApproach::back ||
            (request.approach == BattleApproach::side && i >= 2))
            result.character_cursor_front[i] = replace_low(
                result.character_cursor_front[i], FF6_ROM_VALUE_BATTLE_CHARACTER_CURSOR_BACK_OFFSET_0);
    }
    for (std::uint8_t i = 0; i < 6; ++i) {
        require(request.retained_facing[i] < 4, "Invalid retained monster facing");
        auto& p = result.positions[i];
        p.cursor_front = 0;
        p.flip = 0;
        const auto width = unsigned{request.sizes[i].width} * 8U;
        const auto x = unsigned{wrap8(p.x)};
        bool left = false, flip = false;
        switch (request.approach) {
        case BattleApproach::normal:
            left = true;
            p.cursor_front = static_cast<std::uint16_t>(width);
            break;
        case BattleApproach::back:
            p.x = replace_low(p.x, wrap8(0U - x - width));
            flip = true;
            break;
        case BattleApproach::pincer:
            left = wrap8(x + width) < FF6_ROM_VALUE_BATTLE_MONSTER_APPROACH_GEOMETRY_0;
            if (left)
                p.cursor_front = static_cast<std::uint16_t>(width);
            else {
                p.x = replace_low(p.x, wrap8(FF6_ROM_VALUE_BATTLE_MONSTER_APPROACH_GEOMETRY_1 - x - width));
                flip = true;
            }
            break;
        case BattleApproach::side:
            p.x = replace_low(p.x, wrap8(x + FF6_ROM_VALUE_BATTLE_MONSTER_APPROACH_GEOMETRY_2));
            flip = (wrap8(unsigned{wrap8(p.x)} + width / 2) & 128U) == 0;
            if (!flip)
                p.cursor_front = static_cast<std::uint16_t>(width);
            break;
        }
        if (flip)
            p.flip = request.retained_facing[i] ^ 1U;
        (left ? groups.monsters_left : groups.monsters_right).push_back(i);
    }
    for (auto i : groups.monsters_left)
        groups.monsters_on_left.set(i);
    for (auto i : groups.monsters_right)
        groups.monsters_on_right.set(i);
    for (auto i : groups.characters_left)
        groups.characters_on_left.set(i);
    for (auto i : groups.characters_right)
        groups.characters_on_right.set(i);
    for (unsigned axis = 0; axis < 4; ++axis) {
        auto& order = result.monster_cursor_order[axis];
        std::iota(order.begin(), order.end(), std::uint8_t{0});
        const auto edge = [&](unsigned i) {
            const auto& p = result.positions[i];
            const auto& size = request.sizes[i];
            switch (axis) {
            case 0:
                return p.x;
            case 1:
                return wrap16(unsigned{p.x} + unsigned{size.width} * 8U);
            case 2:
                return p.y;
            default:
                return wrap16(unsigned{p.y} + unsigned{size.height} * 8U);
            }
        };
        std::stable_sort(order.begin(), order.end(),
                         [&](auto a, auto b) { return edge(a) < edge(b); });
        if ((axis & 1U) != 0)
            std::reverse(order.begin(), order.end());
        for (unsigned i = 0; i < 4; ++i)
            result.character_cursor_order[axis][i] =
                static_cast<std::uint8_t>((axis & 1U) != 0 ? 3 - i : i);
    }
    // Original colosseum placement follows grouping/sorting. Do not reorder
    // cursor candidates after this override or substitute a full identity index.
    if (request.colosseum_monster) {
        auto& p = result.positions[0];
        const auto alignment = data.alignments[*request.colosseum_monster];
        require(alignment < 5, "Invalid colosseum alignment");
        p.y = wrap8(FF6_ROM_VALUE_BATTLE_COLOSSEUM_MONSTER_BOUND_0 -
                      unsigned{request.sizes[0].height} * 8U);
        if (alignment == 0)
            p.y = 0;
        else
            p.y = wrap8(unsigned{p.y} +
                        static_cast<std::uint32_t>(data.alignment_offsets[alignment]));
        p.x = static_cast<std::uint16_t>(
            (wrap8(FF6_ROM_VALUE_BATTLE_COLOSSEUM_MONSTER_BOUND_0 -
                   unsigned{request.sizes[0].width} * 8U) >> 1U) +
            FF6_ROM_VALUE_BATTLE_COLOSSEUM_MONSTER_INSET_0);
    }
    return result;
}
} // namespace ff6
