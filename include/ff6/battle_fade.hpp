#pragma once
#include "ff6/raster.hpp"
#include "rom-materials/battle_entry_fade_origin.macros.inc"
#include "rom-materials/battle_entry_fade_initial_color.macros.inc"
namespace ff6 {
// The fixed row buffer is the closed-battlefield interface. Its initial edge
// and subtraction color are separately extracted presentation choices.
struct BattleEntryFade {
    std::uint8_t brightness = 0, remaining = FF6_ROM_VALUE_BATTLE_ENTRY_FADE_ORIGIN_0;
    std::array<std::uint8_t, 152> subtraction = [] {
        std::array<std::uint8_t, 152> rows{};
        rows.fill(FF6_ROM_VALUE_BATTLE_ENTRY_FADE_INITIAL_COLOR_0);
        return rows;
    }();
    bool complete() const {
        return remaining == 0;
    }
    bool operator==(const BattleEntryFade&) const = default;
};
// Update the logical fade after publishing the current frame. The two opening
// edges move two rows per update and retain all rows they have already revealed.
void advance_battle_entry_fade(BattleEntryFade& fade);
// Apply to the canonical closed battlefield. Actor pieces declare their own
// fixed-color eligibility via CompositeBlend; monster palette groups 0..2 do
// not receive the character/armor subtraction. Global intensity affects all.
void apply_battle_entry_fade(const BattleEntryFade& fade, RasterScene& scene);
} // namespace ff6
