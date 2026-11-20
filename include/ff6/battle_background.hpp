#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
struct BattleBackgroundGraphics {
    std::uint8_t identity = 0;
    std::uint16_t tile_count = 128;
    bool operator==(const BattleBackgroundGraphics&) const = default;
};
struct BattleBackgroundDefinition {
    std::array<std::optional<BattleBackgroundGraphics>, 3> graphics;
    std::array<std::optional<std::uint8_t>, 2> layouts;
    std::uint8_t palette = 0;
    bool wave = false;
    bool operator==(const BattleBackgroundDefinition&) const = default;
};
struct BattleBackgroundCell {
    std::uint16_t character = 0;
    std::uint8_t palette = 0;
    bool priority = false, flip_x = false, flip_y = false;
    bool operator==(const BattleBackgroundCell&) const = default;
};
using BattleBackgroundLayout = std::array<BattleBackgroundCell, 1024>;
struct BattleBackground {
    std::uint8_t identity = 0;
    BattleBackgroundDefinition definition;
    // Decoded resource identities, not device storage. Unknown retained tiles
    // remain absent; an attempted view of one receives an explicit diagnostic.
    std::array<std::optional<Tile>, 1024> characters;
    std::array<std::optional<BattleBackgroundLayout>, 2> layouts;
    std::array<std::array<Rgb555, 16>, 3> colors;
    std::array<std::int16_t, 32> wave_samples{};
};
BattleBackgroundDefinition decode_battle_background_definition(ByteView record);
BattleBackgroundLayout decode_battle_background_layout(ByteView bytes);
struct BattleBackgroundGraphicsData {
    std::vector<Tile> tiles;
    // Compressed prefixes leave the previous workspace tail intact. Raw aliases
    // overwrite it; any portion beyond extracted graphics remains unknown.
    bool retains_tail = false;
};
BattleBackgroundGraphicsData load_battle_background_graphics(const ContentPack& pack,
                                                             std::uint8_t identity);
BattleBackgroundLayout load_battle_background_layout(const ContentPack& pack,
                                                     std::uint8_t identity);
// The two low terrain banks may be omitted by a background definition. The
// entering field supplies their 256 decoded tiles; absent input stays unknown.
BattleBackground load_battle_background(const ContentPack& pack, std::uint8_t identity,
                                        std::span<const std::optional<Tile>> retained_terrain = {});
struct BattleBackgroundView {
    std::int32_t x = 0, y = 0;
    std::uint16_t width = 256, height = 152;
};
// Build only the requested window of the ordinary background plane. This
// exposes static assets; animated scrolling, waves and palette changes belong
// to the battle presentation owner, and are not silently applied by a loader.
TileLayer make_battle_background_layer(const BattleBackground& background,
                                       BattleBackgroundView view = {});
// Retain the complete source plane for per-line motion. Equal layout halves
// collapse to a verified 256-pixel period. Unowned off-screen cells reject if
// scrolling later samples them; they are never replaced with invented pixels.
TileLayer make_battle_background_plane(const BattleBackground& background,
                                       BattleBackgroundView view = {});
RasterScene make_battle_background_study(const BattleBackground& background,
                                         BattleBackgroundView view = {});
// LoadTrainGfx uses definition 50 as a 256-tile assembly template for monster
// graphics. Its cells are nine-bit source selectors, not background tile cells.
struct BattleTrainGraphics {
    std::array<Tile, 256> tiles;
    std::array<Rgb555, 16> colors;
};
std::array<Tile, 256> assemble_battle_train_graphics(const BattleBackgroundLayout& layout,
                                                     std::span<const Tile> first,
                                                     std::span<const Tile> third);
BattleTrainGraphics load_battle_train_graphics(const ContentPack& pack);
// Show the assembled selection in the monster loader's 16-column arrangement.
// Actual scene placement, sprite ordering and animation belong to the scene owner.
RasterScene make_battle_train_study(const BattleTrainGraphics& graphics);
} // namespace ff6
