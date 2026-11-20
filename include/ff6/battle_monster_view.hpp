#pragma once
#include "ff6/battle_monster_graphics.hpp"
#include "ff6/battle_state.hpp"

namespace ff6 {
struct MonsterSpritePiece {
    // Signed offsets after the original byte wrap, relative to the monster.
    std::int16_t x = 0, y = 0;
    std::uint8_t first_tile = 0; // Top-left tile of a 32x32 piece in the shared selection.
    bool operator==(const MonsterSpritePiece&) const = default;
};
using MonsterSpriteLayout = std::array<std::vector<MonsterSpritePiece>, 6>;
struct BattleMonsterViewData {
    std::array<MonsterSpriteLayout, 13> layouts;
    std::array<std::uint8_t, 256> alignments{};
    std::array<std::int32_t, 5> alignment_offsets{};
};
BattleMonsterViewData decode_battle_monster_view_data(ByteView maps, ByteView pieces,
                                                      ByteView alignments, ByteView offsets);
BattleMonsterViewData load_battle_monster_view_data(const ContentPack& pack);
// Four orientations: ordinary, horizontal, vertical, both. These are reusable
// scene pieces, not device sprite descriptors or an emulated upload buffer.
using MonsterSpriteVariants = std::array<MonsterSpriteLayout, 4>;
MonsterSpriteVariants make_monster_sprite_variants(const BattleMonsterViewData& data,
                                                   std::uint8_t layout,
                                                   const std::array<MonsterDimensions, 6>& sizes);
struct BattleMonsterPosition {
    std::uint16_t x = 0, y = 0, cursor_front = 0;
    // Display flip request; drawing combines this with the retained facing bits.
    std::uint8_t flip = 0;
    bool operator==(const BattleMonsterPosition&) const = default;
};
struct BattleMonsterPositionRequest {
    BattleApproach approach = BattleApproach::normal;
    std::array<BattleMonsterPosition, 6> positions{};
    std::array<MonsterDimensions, 6> sizes{};
    std::array<std::uint8_t, 6> retained_facing{};
    std::array<std::uint16_t, 4> character_cursor_front{};
    // Colosseum alignment deliberately indexes by the low byte of identity.
    std::optional<std::uint8_t> colosseum_monster;
};
struct BattleTargetGroups {
    std::vector<std::uint8_t> monsters_left, characters_left, monsters_right, characters_right;
    std::bitset<6> monsters_on_left, monsters_on_right;
    std::bitset<4> characters_on_left, characters_on_right;
};
struct BattleMonsterPositions {
    std::array<BattleMonsterPosition, 6> positions{};
    std::array<std::uint16_t, 4> character_cursor_front{};
    BattleTargetGroups groups;
    // Left edge ascending, right edge descending, top ascending, bottom descending.
    std::array<std::array<std::uint8_t, 6>, 4> monster_cursor_order{};
    std::array<std::array<std::uint8_t, 4>, 4> character_cursor_order{};
};
// Complete InitMonsterPos target grouping, approach placement and cursor sorting.
// All six slots participate independently of visibility/presence. Sprite draw
// ordering is a separate owner; cursor ordering must not replace it.
BattleMonsterPositions initialize_monster_positions(const BattleMonsterViewData& data,
                                                    const BattleMonsterPositionRequest& request);
} // namespace ff6
