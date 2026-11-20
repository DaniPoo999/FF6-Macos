#include "ff6/opening_landscape.hpp"
#include <algorithm>
#include "rom-materials/opening_landscape_directory_offsets.macros.inc"
#include "rom-materials/opening_landscape_second_slice.macros.inc"
#include "rom-materials/opening_landscape_page_row_shift.macros.inc"
#include "rom-materials/opening_landscape_page_row_mask.macros.inc"
#include "rom-materials/opening_landscape_page_column_bits.macros.inc"

namespace ff6 {
AffineLayer load_opening_landscape(const ContentPack& pack) {
    const auto graphics = pack.get("map_graphics_map_gfx_4bpp");
    const auto directory = pack.get("map_graphics_map_gfx_ptr_bin");
    const auto data = decompress_lzss(pack.get("cutscene_assets_title_opening_bin_lz"));
    const auto assignments = pack.get("opening_landscape_tile_palettes");
    const auto palettes = pack.get("opening_landscape_palettes");
    if (directory.size() != 82 * 3 || assignments.size() != 224 || palettes.size() != 96)
        throw std::invalid_argument("Invalid opening landscape resource extent");

    // InitOpeningGfx selects records through paired low-word/high-byte reads.
    // Directory origin and three-byte record layout are format metadata; the
    // selected entries and nonzero subrange come from the user's ROM.
    static_assert(FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_1 ==
                  FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_0 + 2U);
    static_assert((FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_0 - 0xda00U) % 3U == 0);
    static_assert(FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_3 ==
                  FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_2 + 2U);
    static_assert((FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_2 - 0xda00U) % 3U == 0);
    static_assert(FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_5 ==
                  FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_4 + 2U);
    static_assert((FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_4 - 0xda00U) % 3U == 0);
    Bytes planar;
    for (const auto [identity, skip] :
         {std::pair{(FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_0 - 0xda00U) / 3U, 0U},
          std::pair{(FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_2 - 0xda00U) / 3U,
                    FF6_ROM_VALUE_OPENING_LANDSCAPE_SECOND_SLICE_0 - 0xdb00U},
          std::pair{(FF6_ROM_VALUE_OPENING_LANDSCAPE_DIRECTORY_OFFSETS_4 - 0xda00U) / 3U, 0U}})
        append(planar, slice(graphics, read24(directory, identity * 3) + skip, 0x800));
    append(planar, slice(data, 0x940, 0x400));

    AffineLayer layer;
    layer.columns = layer.rows = 128;
    layer.tiles = decode_tiles(planar, 4);
    for (std::size_t tile = 0; tile < layer.tiles.size(); ++tile) {
        for (auto& pixel : layer.tiles[tile])
            if (pixel != 0)
                // Preserve the source mask: upper assignment bits are present
                // in real data but do not select extra palette banks.
                pixel = static_cast<std::uint8_t>(pixel | ((assignments[tile] & 3U) << 4U));
    }
    // Normalize the ROM's repeated page grid to row-major cells. The extractor
    // validates every source/destination pair in opening.asm's scrdata1 table.
    // Page dimensions are format metadata; page selection is private ROM data.
    constexpr auto pages_per_row = 32U >> FF6_ROM_VALUE_OPENING_LANDSCAPE_PAGE_ROW_SHIFT_0;
    constexpr auto repeating_rows = FF6_ROM_VALUE_OPENING_LANDSCAPE_PAGE_ROW_MASK_0 / pages_per_row + 1U;
    constexpr auto repeating_columns = 1U << FF6_ROM_VALUE_OPENING_LANDSCAPE_PAGE_COLUMN_BITS_0;
    for (unsigned y = 0; y < 128; ++y)
        for (unsigned x = 0; x < 128; ++x) {
            const auto page = ((y / 32) % repeating_rows) * pages_per_row + (x / 32) % repeating_columns;
            const auto tile = slice(data, 0xbc0 + page * 1024 + (y % 32) * 32 + x % 32, 1)[0];
            // The original also converts 32 tiles from retained scratch beyond
            // its prepared graphics/palette table. None is addressed by this
            // landscape. Reject such references rather than invent their data.
            if (tile >= layer.tiles.size())
                throw std::invalid_argument("Opening landscape references an unprepared tile");
            layer.cells.push_back(tile);
        }
    for (std::size_t i = 0; i < 48; ++i)
        layer.colors[16 + i] = read16(palettes, i * 2) & 0x7fffU;
    layer.lines.resize(224);
    for (auto& line : layer.lines)
        line.enabled = false;
    return layer;
}
} // namespace ff6
