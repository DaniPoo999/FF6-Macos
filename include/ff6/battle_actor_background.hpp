#pragma once
#include "ff6/battle_actor_scene.hpp"

namespace ff6 {
using BattlePublishedCharacterFrames = std::array<BattleCharacterFrame, 4>;
using BattlePublishedStatusTiles = std::array<Tile, 32>;
// Character background frames alias the actor owner's published image. Their
// layout differs from the two row-major pieces used by ordinary actor drawing.
const Tile& battle_character_background_tile(const BattlePublishedCharacterFrames& frames,
                                             unsigned tile);
// The adjacent status strip is a separate, live actor publication. Specter's
// original enemy-caster selectors can cross from character images into it.
const Tile& battle_actor_background_tile(const BattlePublishedCharacterFrames& frames,
    const BattlePublishedStatusTiles& statuses, unsigned tile);
struct BattleMonsterBackgroundSource {
    std::array<Tile, 256> tiles{};
    std::array<std::optional<std::uint8_t>, 6> first_tiles;
};
BattleMonsterBackgroundSource
make_battle_monster_background_source(const BattleActorSceneAssets& assets,
                                      const BattleActorPublication& publication = {});
struct BattleMonsterBackgroundRequest {
    std::bitset<6> selected, shown, displayed, visible;
    std::array<BattleMonsterPosition, 6> positions;
    std::array<std::uint8_t, 6> widths{}, heights{}, facing{}, palette_slots{};
    BattleMonsterPaletteBanks colors{};
};
struct BattleMonsterBackground {
    TileLayer image;
    std::bitset<6> copied;
};
// DrawBG1Monster's tile-aligned, clipped rectangle and palette snapshot. This
// owns only the nineteen initialized rows; sampling other rows must fail.
BattleMonsterBackground
copy_battle_monsters_to_background(const BattleMonsterBackgroundSource& source,
                                   const BattleMonsterBackgroundRequest& request);
} // namespace ff6
