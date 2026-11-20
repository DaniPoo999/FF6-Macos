#pragma once
#include "ff6/battle_character_presentation.hpp"
#include "ff6/battle_readout.hpp"
#include "rom-materials/final_battle_viewport.macros.inc"
#include "rom-materials/battlefield_horizontal_window.macros.inc"

namespace ff6 {
void initialize_battle_background_motion(const ContentPack& pack, BattleBackground& background);
// Canonical closed-menu battlefield composition. Callers supply native pieces,
// never device registers, sprite descriptors, or transfer programs.
RasterScene compose_battle_scene(const BattleBackground& background,
                                 const BattleReadoutAssets& readout_assets,
                                 const BattleReadoutView& readout,
                                 std::span<const RasterSprite> pieces,
                                 BattleBackgroundView viewport = {0, 1, 256, (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1)},
                                 std::uint16_t clock = 0);
struct BattleSceneAppearance {
    std::uint8_t background = 0;
    MenuWindowAppearance window;
    Rgb555 font_color = 32767;
    bool green_soldier_disguise = false;
};
} // namespace ff6
