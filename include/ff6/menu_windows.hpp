#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"
#include <optional>

namespace ff6 {
struct MenuWindowPlacement {
    // Two adjacent 32-column pages; drawing past a page's right edge continues
    // on its next row. This matters for the partially hidden order windows.
    std::uint8_t page, column, row, interior_columns, interior_rows;
};
struct MenuWindowSkin {
    std::array<Tile, 28> tiles;
    std::array<Rgb555, 7> colors; // Editable opaque colors; index zero is transparent.
};
using MenuWindowSkins = std::array<MenuWindowSkin, 8>;
MenuWindowSkins load_menu_window_skins(const ContentPack& pack);
struct MenuWindowAssets {
    MenuWindowSkins skins;
    std::array<std::uint8_t, 28> pattern;
    // Options, time, steps, party, save choice, outer order, edge order.
    std::array<MenuWindowPlacement, 7> placements;
    std::array<RasterLine, 224> rows;
};
struct MenuWindowAppearance {
    std::uint8_t skin = 0;
    std::optional<std::array<Rgb555, 7>> colors;
};

// Decode scoped ROM content into ordinary geometry, tiles, colors and row effects.
// There is no register stream, DMA executor or video-memory input in this API.
MenuWindowAssets load_menu_windows(const ContentPack& pack);
// Replace only the native per-row color recipe; clip geometry stays owned by
// the menu. These assets contain literal color runs, never arbitrary DMA code.
void set_menu_window_gradient(MenuWindowAssets&, ByteView shades, ByteView blends);
TileLayer make_main_menu_windows(const MenuWindowAssets& assets,
                                 const MenuWindowAppearance& appearance = {});
TileLayer make_menu_windows(const MenuWindowAssets& assets,
                            std::span<const MenuWindowPlacement> placements,
                            const MenuWindowAppearance& appearance = {});
} // namespace ff6
