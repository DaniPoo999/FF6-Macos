#pragma once
#include "ff6/battle_background.hpp"
#include <bitset>

namespace ff6 {
struct MonsterDimensions {
    std::uint8_t width = 0, height = 0, overlap_y = 0;
    bool operator==(const MonsterDimensions&) const = default;
};
struct MonsterGraphic {
    std::uint16_t palette_identity = 0;
    std::array<Rgb555, 16> colors{};
    // Sixteen rows, most significant bit on the left. Small stencils have
    // eight meaningful rows/columns. The first empty row terminates loading.
    std::array<std::uint16_t, 16> stencil{};
    MonsterDimensions dimensions;
    std::uint8_t stencil_extent = 8; // Encoded layout, independent of occupied bounds.
    std::vector<Tile> packed_tiles;
};
struct MonsterGraphicsAllocation {
    std::uint16_t first_tile = 0;
    std::uint8_t width = 0, height = 0;
};
using MonsterGraphicsAllocations = std::array<std::array<MonsterGraphicsAllocation, 6>, 13>;
MonsterGraphicsAllocations decode_monster_graphics_allocations(ByteView bytes);
MonsterGraphic decode_monster_graphic(ByteView property, ByteView graphics, ByteView small_stencils,
                                      ByteView large_stencils, ByteView palettes);
struct BattleMonsterGraphicsData {
    std::array<MonsterGraphic, 416> records;
    MonsterGraphicsAllocations allocations;
    std::array<std::uint16_t, 17> imp_offset_x{}, imp_offset_y{};
    std::array<Tile, 256> train_tiles{};
};
BattleMonsterGraphicsData load_battle_monster_graphics_data(const ContentPack& pack);
struct BattleMonsterGraphicsRequest {
    std::uint8_t layout = 0;
    // Graphics loading uses identity independently of the battle visibility mask.
    std::array<std::optional<std::uint16_t>, 6> identities;
    std::bitset<6> imp;
    // Absent slots and imp substitutions preserve these previous display sizes
    // and overlap offsets. The caller owns their actual initialization.
    std::array<MonsterDimensions, 6> retained_dimensions{};
};
struct BattleMonsterGraphics {
    // Ordered decoded resources shared by the monster images. No device memory
    // or upload API; later selected resources may overwrite earlier selections.
    std::array<Tile, 256> tiles{};
    std::array<MonsterDimensions, 6> dimensions{};
    std::array<std::optional<std::uint16_t>, 6> palette_identities;
    std::array<std::optional<std::uint16_t>, 6> distinct_palettes;
    std::array<std::optional<std::array<Rgb555, 16>>, 3> colors;
    std::array<std::uint8_t, 6> palette_slots{};
};
// Corresponds to the complete InitMonsterGfx + LoadMonsterPal resource owners.
// It does not determine visibility, position, sprite order or entrance animation.
BattleMonsterGraphics
initialize_battle_monster_graphics(const BattleMonsterGraphicsData& data,
                                   const BattleMonsterGraphicsRequest& request);
} // namespace ff6
