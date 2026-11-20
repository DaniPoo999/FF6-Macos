#include "ff6/battle_monster_graphics.hpp"
#include "rom-materials/battle_monster_imp_graphic.macros.inc"
#include "rom-materials/battle_train_dimensions.macros.inc"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
std::vector<Tile> decode_monster_tiles(ByteView source, bool three_bit) {
    if (!three_bit)
        return decode_tiles(source, 4);
    require(source.size() % 24 == 0, "Partial three-bit monster tile");
    Bytes expanded(source.size() / 24 * 32);
    for (std::size_t i = 0; i < source.size() / 24; ++i) {
        std::ranges::copy(source.subspan(i * 24, 16), std::span(expanded).subspan(i * 32).begin());
        for (unsigned row = 0; row < 8; ++row)
            expanded[i * 32 + 16 + row * 2] = source[i * 24 + 16 + row];
    }
    return decode_tiles(expanded, 4);
}
} // namespace
MonsterGraphicsAllocations decode_monster_graphics_allocations(ByteView bytes) {
    require(bytes.size() == 218, "Invalid monster allocation table extent");
    MonsterGraphicsAllocations result;
    for (unsigned layout = 0; layout < result.size(); ++layout) {
        const auto pointer = read16(bytes, layout * 2);
        require(pointer >= 0xd034U && (pointer - 0xd034U) % 4 == 0,
                "Invalid monster allocation pointer");
        const auto start = std::size_t{pointer - 0xd01aU};
        // These variable-length groups have no padding. The original's six
        // slot reads may reach the following group's first allocation records.
        for (unsigned slot = 0; slot < 6; ++slot) {
            const auto record = slice(bytes, start + slot * 4, 4);
            const auto offset = read16(record, 0);
            const auto width = record[2], height = record[3];
            require(offset % 32 == 0 && offset < 8192 && width > 0 && height > 0 && width <= 16 &&
                        height <= 16 && offset / 32 % 16 + width <= 16 &&
                        offset / 512 + height <= 16,
                    "Invalid monster graphics allocation");
            result[layout][slot] = {static_cast<std::uint16_t>(offset / 32), width, height};
        }
    }
    return result;
}
MonsterGraphic decode_monster_graphic(ByteView property, ByteView graphics, ByteView small_stencils,
                                      ByteView large_stencils, ByteView palettes) {
    require(property.size() == 5, "Invalid monster graphics property extent");
    const auto encoded = read16(property, 0);
    const bool three_bit = (encoded & 0x8000U) != 0;
    const bool large = (property[2] & 0x80U) != 0;
    const auto stencil = slice(large ? large_stencils : small_stencils,
                               std::size_t{property[4]} * (large ? 32 : 8), large ? 32 : 8);
    MonsterGraphic result;
    result.stencil_extent = large ? 16 : 8;
    result.palette_identity =
        static_cast<std::uint16_t>(((unsigned{property[2]} << 8U) | property[3]) & 1023U);
    const auto colors = slice(palettes, std::size_t{result.palette_identity} * 16, 32);
    for (unsigned i = 0; i < 16; ++i) {
        result.colors[i] = read16(colors, i * 2) & 0x7fffU;
        if (large)
            result.stencil[i] =
                static_cast<std::uint16_t>((unsigned{stencil[i * 2]} << 8U) | stencil[i * 2 + 1]);
        else if (i < 8)
            result.stencil[i] = static_cast<std::uint16_t>(unsigned{stencil[i]} << 8U);
    }
    unsigned combined = 0, count = 0;
    for (const auto row : result.stencil) {
        if (row == 0)
            break;
        ++result.dimensions.height;
        combined |= row;
        count += static_cast<unsigned>(std::popcount(row));
    }
    // Empty source stencils leave scratch-dependent dimensions in the original;
    // supported resources are nonempty. Malformed data has a defined failure.
    require(combined != 0, "Empty monster stencil has no defined extent");
    for (unsigned x = 0; x < 16; ++x)
        if ((combined & (0x8000U >> x)) != 0)
            result.dimensions.width = static_cast<std::uint8_t>(x + 1);
    result.packed_tiles = decode_monster_tiles(
        slice(graphics, std::size_t{encoded & 0x7fffU} * 8, count * (three_bit ? 24 : 32)),
        three_bit);
    return result;
}
BattleMonsterGraphicsData load_battle_monster_graphics_data(const ContentPack& pack) {
    const auto properties = pack.get("monster_tables_properties");
    const auto overlap = pack.get("auxiliary_monster_overlap");
    require(properties.size() == 416 * 5 && overlap.size() == 384,
            "Invalid monster graphics table extent");
    BattleMonsterGraphicsData result;
    for (unsigned i = 0; i < result.records.size(); ++i) {
        result.records[i] = decode_monster_graphic(
            slice(properties, i * 5, 5), pack.get("monster_graphics_monster_gfx_stc"),
            pack.get("monster_graphics_monster_stencil_small_stn"),
            pack.get("monster_graphics_monster_stencil_large_stn"),
            pack.get("monster_graphics_monster_pal_pal"));
        if (i < overlap.size())
            result.records[i].dimensions.overlap_y = overlap[i];
    }
    result.allocations =
        decode_monster_graphics_allocations(pack.get("monster_graphics_allocations"));
    const auto offsets = pack.get("monster_imp_offsets");
    require(offsets.size() == 68, "Invalid imp offset table extent");
    for (unsigned i = 0; i < 17; ++i) {
        const auto x = read16(offsets, i * 2), y = read16(offsets, 34 + i * 2);
        require(x % 32 == 0 && y % 512 == 0 && x <= 12 * 32 && y <= 12 * 512,
                "Invalid imp centering offset");
        result.imp_offset_x[i] = static_cast<std::uint16_t>(x / 32);
        result.imp_offset_y[i] = static_cast<std::uint16_t>(y / 32);
    }
    result.train_tiles = load_battle_train_graphics(pack).tiles;
    return result;
}
BattleMonsterGraphics
initialize_battle_monster_graphics(const BattleMonsterGraphicsData& data,
                                   const BattleMonsterGraphicsRequest& request) {
    require(request.layout < data.allocations.size(), "Invalid monster graphics layout");
    BattleMonsterGraphics result;
    result.dimensions = request.retained_dimensions;
    std::size_t palette_count = 0;
    for (unsigned slot = 0; slot < 6; ++slot) {
        const auto identity = request.identities[slot];
        if (!identity)
            continue;
        require(*identity < 384, "Invalid battle monster graphics identity");
        const auto& original = data.records[*identity];
        const auto& graphic = request.imp[slot] ? data.records[FF6_ROM_VALUE_BATTLE_MONSTER_IMP_GRAPHIC_0] : original;
        const auto allocation = data.allocations[request.layout][slot];
        const auto width = std::min(graphic.dimensions.width, allocation.width);
        const auto height = std::min(graphic.dimensions.height, allocation.height);
        require(width > 0 && height > 0 && width <= 16 && height <= 16,
                "Empty or invalid monster graphics load");
        unsigned first = allocation.first_tile;
        if (request.imp[slot]) {
            const auto previous = request.retained_dimensions[slot];
            require(previous.width <= 16 && previous.height <= 16,
                    "Invalid retained monster size for imp graphics");
            first += data.imp_offset_x[previous.width] + data.imp_offset_y[previous.height];
        } else {
            result.dimensions[slot] = {width, height, original.dimensions.overlap_y};
            if (*identity == 262) {
                result.dimensions[slot].width = FF6_ROM_VALUE_BATTLE_TRAIN_DIMENSIONS_0 & 255U;
                result.dimensions[slot].height = FF6_ROM_VALUE_BATTLE_TRAIN_DIMENSIONS_0 >> 8U;
            }
        }
        std::size_t tile = 0;
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) {
                if ((graphic.stencil[y] & (0x8000U >> x)) == 0)
                    continue;
                require(tile < graphic.packed_tiles.size() &&
                            first + y * 16 + x < result.tiles.size(),
                        "Monster graphics load outside owned resources");
                // Only set bits visited inside the clipped allocation consume a
                // packed tile. Cropping an already expanded image is different.
                result.tiles[first + y * 16 + x] = graphic.packed_tiles[tile++];
            }
        // This caller follows every loaded identity, including an imp substitute.
        // A later slot may then replace part of the assembled train selection.
        if (*identity == 262)
            result.tiles = data.train_tiles;
        result.palette_identities[slot] = original.palette_identity;
        auto found = std::find(result.distinct_palettes.begin(), result.distinct_palettes.end(),
                               std::optional<std::uint16_t>{original.palette_identity});
        std::size_t index = static_cast<std::size_t>(found - result.distinct_palettes.begin());
        if (found == result.distinct_palettes.end()) {
            index = palette_count++;
            result.distinct_palettes[index] = original.palette_identity;
            if (index < result.colors.size())
                result.colors[index] = original.colors;
        }
    }
    // All six slots participate, even absent ones: an absent palette matches the
    // first unused entry among the three loaded selections. Extra colors share 0.
    for (unsigned slot = 0; slot < 6; ++slot)
        for (std::uint8_t index = 0; index < 3; ++index)
            if (result.palette_identities[slot] == result.distinct_palettes[index]) {
                result.palette_slots[slot] = index;
                break;
            }
    return result;
}
} // namespace ff6
