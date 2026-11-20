#include "ff6/battle_actor_background.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
} // namespace
const Tile& battle_character_background_tile(const BattlePublishedCharacterFrames& frames,
                                             unsigned tile) {
    require(tile < 32, "Character background references an unowned published tile");
    // UpdateCharGfxBuf publishes pairs 0/1, 4/5 in the upper row and pairs
    // 2/3, 6/7 in the lower row. TfrCharGfx preserves this interleaving.
    const unsigned column = tile & 15U;
    return frames[column / 4][(column & 1U) + ((column & 2U) ? 4U : 0U) + (tile >= 16 ? 2U : 0U)];
}
const Tile& battle_actor_background_tile(const BattlePublishedCharacterFrames& frames,
    const BattlePublishedStatusTiles& statuses, unsigned tile) {
    require(tile < 64, "Actor background references an unowned published tile");
    return tile < 32 ? battle_character_background_tile(frames, tile) : statuses[tile - 32];
}
BattleMonsterBackgroundSource
make_battle_monster_background_source(const BattleActorSceneAssets& assets,
                                      const BattleActorPublication& publication) {
    BattleMonsterBackgroundSource result;
    result.tiles = publication.monster_tiles ? *publication.monster_tiles : assets.monsters.tiles;
    for (unsigned slot = 0; slot < 6; ++slot)
        if (!assets.monster_pieces[0][slot].empty())
            result.first_tiles[slot] = assets.monster_pieces[0][slot].front().first_tile;
    return result;
}
BattleMonsterBackground
copy_battle_monsters_to_background(const BattleMonsterBackgroundSource& source,
                                   const BattleMonsterBackgroundRequest& request) {
    BattleMonsterBackground result;
    result.copied = request.selected & request.shown & request.displayed & request.visible;
    // A sixth destination crosses the original background palette range and
    // reads a retained unrelated bank. Require its separate owner, not black.
    require(result.copied.count() <= 5, "Six monster copies need retained background palette zero");
    auto& image = result.image;
    image.columns = image.rows = 32;
    image.cells.resize(32 * 32);
    image.tiles.assign(source.tiles.begin(), source.tiles.end());
    image.tiles.emplace_back(); // Explicit transparent clear tile.
    image.palettes.emplace_back();
    for (unsigned cell = 0; cell < image.cells.size(); ++cell) {
        image.cells[cell].tile = 256;
        image.cells[cell].available = cell < 32 * 19;
    }
    for (unsigned slot = 0; slot < 6; ++slot) {
        if (!result.copied[slot])
            continue;
        require(source.first_tiles[slot].has_value(), "Monster background image has no tile owner");
        const unsigned width = request.widths[slot], height = request.heights[slot];
        const unsigned x = (request.positions[slot].x & 255U) / 8U;
        const unsigned y = (request.positions[slot].y & 255U) / 8U;
        require(width && height && width <= 16 && height <= 16 && y < 19,
                "Invalid monster background rectangle");
        const unsigned palette = request.palette_slots[slot];
        require(palette < request.colors.size(), "Unowned monster background palette");
        image.palettes.push_back(request.colors[palette]);
        const bool flip = ((request.positions[slot].flip ^ request.facing[slot]) & 1U) != 0;
        for (unsigned row = 0; row < std::min(height, 19U - y); ++row)
            for (unsigned column = 0; column < std::min(width, 32U - x); ++column) {
                const unsigned tile =
                    *source.first_tiles[slot] + row * 16U + (flip ? width - 1U - column : column);
                require(tile < source.tiles.size(), "Monster background leaves its tile owner");
                // Later slots overwrite earlier rectangles, including holes.
                image.cells[(y + row) * 32 + x + column] = {
                    tile, static_cast<std::uint32_t>(image.palettes.size() - 1), 8, flip, false};
            }
    }
    return result;
}
} // namespace ff6
