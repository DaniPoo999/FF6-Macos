#include "ff6/menu_windows.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void validate_placement(const MenuWindowPlacement& window) {
    require(window.page < 2 && window.column < 32 && window.row < 32 &&
                window.interior_columns > 0 && window.interior_columns <= 30 &&
                window.interior_rows > 0 && window.interior_rows <= 30,
            "Invalid window dimensions");
    const auto start = std::size_t{window.page} * 1024 + window.row * 32 + window.column;
    require(start + (std::size_t{window.interior_rows} + 1) * 32 + window.interior_columns + 1 <
                2048,
            "Window extends beyond menu canvas");
}

// These three resources contain only bounded runs of menu color/clip values.
// Accept literal runs, a bounded extent and one final terminator. Reject repeat
// flags, indirection, truncation and trailing bytes: no generic HDMA execution.
template <std::size_t Width, std::size_t Rows>
std::array<std::array<std::uint8_t, Width>, Rows> expand_menu_runs(ByteView bytes,
                                                               bool retain_final = false) {
    std::array<std::array<std::uint8_t, Width>, Rows> output{};
    std::size_t cursor = 0, row = 0;
    while (cursor < bytes.size()) {
        const auto count = bytes[cursor++];
        if (count == 0) {
            require(cursor == bytes.size() && (row == Rows || (retain_final && row)),
                    "Invalid menu band extent");
            if (retain_final && row < Rows)
                std::fill(output.begin() + row, output.end(), output[row - 1]);
            return output;
        }
        require(count <= 127 && count <= Rows - row, "Invalid menu band length");
        const auto value = slice(bytes, cursor, Width);
        cursor += Width;
        for (std::size_t i = 0; i < count; ++i)
            std::copy(value.begin(), value.end(), output[row++].begin());
    }
    throw std::runtime_error("Unterminated menu bands");
}
} // namespace

MenuWindowSkins load_menu_window_skins(const ContentPack& pack) {
    MenuWindowSkins skins{};
    const auto graphics = pack.get("menu_graphics_window_gfx_4bpp");
    const auto palettes = pack.get("menu_graphics_window_pal_pal");
    require(graphics.size() == 8 * 28 * 32 && palettes.size() == 8 * 32,
            "Invalid menu window skin resources");
    const auto tiles = decode_tiles(graphics, 4);
    for (std::size_t s = 0; s < skins.size(); ++s) {
        auto& skin = skins[s];
        for (std::size_t t = 0; t < skin.tiles.size(); ++t) {
            skin.tiles[t] = tiles[s * 28 + t];
            for (const auto pixel : skin.tiles[t])
                require(pixel <= 7, "Window pixel references an unowned palette color");
        }
        for (std::size_t c = 0; c < skin.colors.size(); ++c)
            // The stored high bit has no color meaning. Preserve source bytes
            // in the pack, but expose only RGB555 in the native appearance.
            skin.colors[c] = read16(palettes, s * 32 + (c + 1) * 2) & 0x7fffU;
    }
    return skins;
}
MenuWindowAssets load_menu_windows(const ContentPack& pack) {
    MenuWindowAssets assets{};
    assets.skins = load_menu_window_skins(pack);
    const auto pattern = pack.get("menu_window_tile_pattern");
    require(pattern.size() == 56, "Invalid menu window pattern length");
    for (std::size_t i = 0; i < assets.pattern.size(); ++i) {
        const auto value = read16(pattern, i * 2);
        require(value >= 0x180 && value < 0x19c, "Invalid menu window pattern tile");
        assets.pattern[i] = static_cast<std::uint8_t>(value - 0x180U);
    }
    const auto layout = pack.get("menu_main_window_layout");
    require(layout.size() == 28, "Invalid main menu layout length");
    for (std::size_t i = 0; i < assets.placements.size(); ++i) {
        const auto address = read16(layout, i * 4);
        // Storage-specific position encoding ends here. The native layer owns
        // coordinates, not a copy of the original screen buffer or its address.
        require(address >= 0x5849 && address < 0x6849 && (address - 0x5849) % 2 == 0,
                "Invalid menu layout position");
        const auto cell = (address - 0x5849U) / 2U;
        assets.placements[i] = {
            static_cast<std::uint8_t>(cell / 1024), static_cast<std::uint8_t>(cell % 32),
            static_cast<std::uint8_t>((cell % 1024) / 32), layout[i * 4 + 2], layout[i * 4 + 3]};
        validate_placement(assets.placements[i]);
    }
    const auto shades = expand_menu_runs<1, 240>(pack.get("menu_main_window_shade_bands"));
    const auto blends = expand_menu_runs<1, 224>(pack.get("menu_main_window_blend_bands"));
    const auto clips = expand_menu_runs<2, 223>(pack.get("menu_window_clip_bands"));
    for (const auto& shade : shades)
        require((shade[0] & 0xe0U) == 0xe0U, "Window gradient requires gray operands");
    for (std::size_t y = 0; y < assets.rows.size(); ++y) {
        auto& row = assets.rows[y];
        row.offset_y = 1; // Visible row zero samples the next tile row pixel.
        require(blends[y][0] == 0x02 || blends[y][0] == 0x82, "Invalid window blend band");
        row.blend = blends[y][0] == 0x02 ? ColorBlend::add : ColorBlend::subtract;
        row.operand = static_cast<Rgb555>((shades[y][0] & 31U) * 1057U);
        row.left = row.right = 0;
        // The menu disables all layers on its last row. Earlier reversed
        // endpoints represent an empty clip, not wrapped horizontal coverage.
        if (y < clips.size() && clips[y][0] <= clips[y][1]) {
            row.left = clips[y][0];
            row.right = static_cast<std::uint16_t>(std::uint16_t{clips[y][1]} + 1U);
        }
    }
    return assets;
}

void set_menu_window_gradient(MenuWindowAssets& assets, ByteView shade_data, ByteView blend_data) {
    const auto shades = expand_menu_runs<1, 240>(shade_data, true);
    const auto blends = expand_menu_runs<1, 240>(blend_data, true);
    for (unsigned y = 0; y < shades.size(); ++y) {
        require((shades[y][0] & 0xe0U) == 0xe0U, "Window gradient requires gray operands");
        require(blends[y][0] == 2 || blends[y][0] == 130, "Invalid window blend band");
    }
    for (unsigned y = 0; y < assets.rows.size(); ++y) {
        assets.rows[y].blend = blends[y][0] == 2 ? ColorBlend::add : ColorBlend::subtract;
        assets.rows[y].operand = static_cast<Rgb555>((shades[y][0] & 31U) * 1057U);
    }
}

TileLayer make_menu_windows(const MenuWindowAssets& assets,
                            std::span<const MenuWindowPlacement> placements,
                            const MenuWindowAppearance& appearance) {
    require(appearance.skin < assets.skins.size(), "Invalid menu window skin");
    const auto& skin = assets.skins[appearance.skin];
    TileLayer layer;
    layer.columns = 64;
    layer.rows = 32;
    layer.tiles.assign(skin.tiles.begin(), skin.tiles.end());
    for (const auto& tile : layer.tiles)
        for (const auto pixel : tile)
            require(pixel <= 7, "Window pixel references an unowned palette color");
    layer.tiles.emplace_back(); // Clear cells are transparent, not theme tile zero.
    std::array<Rgb555, 16> palette{};
    const auto& colors = appearance.colors ? *appearance.colors : skin.colors;
    for (std::size_t c = 0; c < colors.size(); ++c) {
        require(colors[c] <= 0x7fff, "Invalid custom window color");
        palette[c + 1] = colors[c];
    }
    layer.palettes.push_back(palette);
    constexpr std::uint16_t window_depth = 5 * 256;
    layer.cells.assign(64 * 32, {28, 0, window_depth, false, false});
    layer.lines.assign(assets.rows.begin(), assets.rows.end());
    for (const auto tile : assets.pattern)
        require(tile < 28, "Invalid window pattern tile");

    const auto draw = [&](const MenuWindowPlacement& window) {
        validate_placement(window);
        const auto start = std::size_t{window.page} * 1024 + window.row * 32 + window.column;
        const auto width = std::size_t{window.interior_columns},
                   height = std::size_t{window.interior_rows};
        for (std::size_t y = 0; y < height + 2; ++y)
            for (std::size_t x = 0; x < width + 2; ++x) {
                std::size_t part;
                if (y == 0 || y == height + 1) {
                    const auto bottom = y == 0 ? 0U : 1U;
                    part = x == 0           ? 24 + bottom * 2
                           : x == width + 1 ? 25 + bottom * 2
                                            : 16 + bottom * 2 + (x - 1) % 2;
                } else {
                    const auto row = y - 1;
                    part = x == 0           ? 20 + (row % 2) * 2
                           : x == width + 1 ? 21 + (row % 2) * 2
                                            : (row % 4) * 4 + (x - 1) % 4;
                }
                // Preserve the original page-edge row carry; geometric clipping
                // would lose the small offscreen order-window fragments.
                const auto cell = start + y * 32 + x;
                const auto column = (cell / 1024) * 32 + cell % 32;
                const auto row = (cell % 1024) / 32;
                layer.cells[row * 64 + column].tile = assets.pattern[part];
            }
    };
    for (const auto& placement : placements) draw(placement);
    return layer;
}
TileLayer make_main_menu_windows(const MenuWindowAssets& assets,
                                 const MenuWindowAppearance& appearance) {
    // Painter order is behavior of DrawMainMenu/_c33170, not content bytes.
    std::array<MenuWindowPlacement, 6> ordered;
    unsigned cursor = 0;
    for (const auto index : {5U, 6U, 3U, 0U, 1U, 2U}) ordered[cursor++] = assets.placements[index];
    return make_menu_windows(assets, ordered, appearance);
}
} // namespace ff6
