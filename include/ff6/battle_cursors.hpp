#pragma once
#include "ff6/battle_character_view.hpp"
#include "ff6/battle_target_selection.hpp"

namespace ff6 {
struct BattleCursorAssets {
    std::array<std::array<BattleScreenPoint, 4>, 3> commands;
    std::array<std::uint8_t, 2> magitek_columns{};
    std::array<std::uint8_t, 4> list_rows{};
    std::array<Tile, 4> hand{};
    Tile roulette{};
    std::array<Tile, 2> indicator_top{};
    std::array<std::array<Tile, 2>, 4> indicator_bottom{};
    std::array<Rgb555, 4> colors{};
};
BattleCursorAssets load_battle_cursor_assets(const ContentPack& pack);
BattleScreenPoint battle_command_cursor_position(const BattleCursorAssets& assets,
                                                 BattleCommandLayout layout, std::uint8_t row);
BattleScreenPoint battle_magitek_cursor_position(const BattleCursorAssets& assets,
                                                 BattleListCursor cursor);
struct BattleHandCursor {
    // Eight-bit screen positions; drawing wraps y at the bottom of the display.
    std::uint8_t x = 0, y = 0;
    bool flip_x = false;
    bool operator==(const BattleHandCursor&) const = default;
};
struct BattleCursorFrame {
    std::array<std::optional<BattleHandCursor>, 6> targets;
    std::optional<BattleHandCursor> indicator, menu;
    bool operator==(const BattleCursorFrame&) const = default;
};
struct BattleCursorRequest {
    std::optional<BattleHandCursor> menu;
    std::optional<std::uint8_t> active_character;
    BattleTargetCursor targets;
    // Drawing eligibility includes visibility and excludes characters acting
    // as enemies. It is separate from the selected masks and attack eligibility.
    BattleTargetEligibility visible;
    std::uint8_t clock = 0;
    bool hide_characters = false, hide_indicator = false, magitek = false;
};
BattleCursorFrame prepare_battle_cursors(const BattleCursorRequest& request,
                                         const BattleSceneAnchors& anchors,
                                         const BattleMonsterPositions& positions,
                                         const BattleCharacterViewData& view,
                                         const std::array<BattleCharacterPose, 4>& poses,
                                         const std::array<std::uint32_t, 4>& shown_status);
std::optional<RasterSprite> make_battle_roulette_sprite(std::uint8_t target,
    const BattleSceneAnchors&, const BattleMonsterPositions&, const BattleCursorAssets&);
struct BattleCursorPublication {
    BattleCursorFrame prepared, visible;
    std::array<Tile, 4> indicator_image{};
};
// Publish the prior descriptions before preparing new input. Status-image
// cadence is independent: the indicator's halves update on phases 6 and 7.
void publish_battle_cursors(BattleCursorPublication& state, const BattleCursorAssets& assets,
                            std::uint8_t clock, bool hide_characters);
struct BattleCursorSprites {
    // Preserve object-plane order: target hands, indicator, actors, menu hand.
    std::vector<RasterSprite> before_actors, after_actors;
};
BattleCursorSprites make_battle_cursor_sprites(const BattleCursorPublication& state,
                                               const BattleCursorAssets& assets);
} // namespace ff6
