#pragma once
#include "rom-materials/battle_sprite_default_priority.macros.inc"
#include "ff6/battle_monster_view.hpp"
namespace ff6 {
struct BattleMonsterAnchorInput {
    std::uint16_t x = 0, y = 0, overlap = 0, cursor_front = 0;
    std::uint8_t width = 0, height = 0;
};
struct BattleCharacterAnchorInput {
    std::uint16_t x = 0, y = 0, offset_x = 0, offset_y = 0, animation_x = 0, cursor_front = 0;
};
struct BattleActorAnchors {
    std::uint16_t center_x = 0, center_y = 0, bottom_y = 0, draw_y = 0, cursor_x = 0, cursor_y = 0;
    bool operator==(const BattleActorAnchors&) const = default;
};
// Characters first, then monsters; semantic anchors used by cursors, effects and
// ordering. Computes the complete coordinate prefix before target-selection work.
using BattleSceneAnchors = std::array<BattleActorAnchors, 10>;
BattleSceneAnchors
calculate_battle_anchors(const std::array<BattleMonsterAnchorInput, 6>& monsters,
                         const std::array<BattleCharacterAnchorInput, 4>& characters);
struct BattleSpriteStyle {
    std::uint8_t palette = 0, priority = FF6_ROM_VALUE_BATTLE_SPRITE_DEFAULT_PRIORITY_0;
    bool flip_x = false, flip_y = false;
    bool operator==(const BattleSpriteStyle&) const = default;
};
using BattleMonsterStyle = BattleSpriteStyle;
struct BattleScreenPoint {
    std::int16_t x = 0, y = 0;
    bool operator==(const BattleScreenPoint&) const = default;
};
// Shared game projection: nine-bit wrapping and the battlefield y=151 sentinel.
BattleScreenPoint project_battle_piece(std::uint16_t x, std::uint16_t y, std::int16_t offset_x,
                                       std::int16_t offset_y);
struct BattleMonsterStyleMask {
    std::uint8_t palette = 7, priority = 3;
    bool flip_x = true, flip_y = true;
    bool operator==(const BattleMonsterStyleMask&) const = default;
};
struct BattleDrawTarget {
    std::uint16_t bottom_y = 0;
    std::optional<std::uint8_t> participant;
    bool operator==(const BattleDrawTarget&) const = default;
};
struct BattleDrawOrderWork {
    std::uint32_t cycles = 0, fast_rom_clocks = 0;
    unsigned passes = 0, exchanges = 0, ties = 0;
    bool operator==(const BattleDrawOrderWork&) const = default;
};
struct BattleDrawOrder {
    std::array<BattleDrawTarget, 10> staged{}, published{};
    std::array<BattleMonsterStyleMask, 6> monster_masks{};
    bool awaiting_publication = false;
    // Most recent staging attempt, including a blocked attempt. This measures
    // original algorithm work, not the native host's execution time.
    BattleDrawOrderWork preparation_work;
};
// A pending order blocks restaging (including mask reset) until publication.
bool stage_battle_draw_order(BattleDrawOrder& state, const BattleSceneAnchors& anchors);
bool publish_battle_draw_order(BattleDrawOrder& state);
struct BattleMonsterDrawPiece {
    std::int16_t x = 0, y = 0;
    std::uint8_t first_tile = 0;
    BattleMonsterStyle style;
    bool operator==(const BattleMonsterDrawPiece&) const = default;
};
// Uses selected image orientation, retained facing and appearance independently.
// Y=151 is the original rejected-position sentinel, retained for the scene's
// battlefield clipping owner. This function does not perform sprite-budget selection.
std::vector<BattleMonsterDrawPiece>
project_battle_monster(const MonsterSpriteVariants& variants, std::uint8_t slot,
                       const BattleMonsterPosition& position, std::uint8_t retained_facing,
                       BattleMonsterStyle style, BattleMonsterStyleMask mask);
RasterSprite make_battle_monster_sprite(const BattleMonsterDrawPiece& piece,
                                        const std::array<Tile, 256>& tiles,
                                        const std::array<Rgb555, 16>& colors);
} // namespace ff6
