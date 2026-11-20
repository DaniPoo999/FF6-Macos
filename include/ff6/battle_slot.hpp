#pragma once
#include "ff6/battle_menu_transition.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
struct BattleSlotData {
    std::array<std::array<std::uint8_t, 16>, 3> reels{};
    std::array<std::uint8_t, 6> rates{};
    std::array<std::uint8_t, 6> tile_bases{};
    std::array<Rgb555, 8> gradient{};
    std::array<Rgb555, 12> colors{};
};
BattleSlotData load_battle_slot(const ContentPack&);
std::uint8_t battle_slot_outcome(std::array<std::uint8_t, 3> symbols);
struct BattleSlotState {
    std::array<std::uint8_t, 3> positions{}, visible_positions{};
    std::array<bool, 3> committed{}, stopped{};
    std::uint8_t roll = 0, desired_second = 255, desired_third = 255, slip_remaining = 0;
    bool can_cancel() const { return !committed[0]; }
    // One A-button visit. Only the first consumes graphics RNG; the fourth
    // accepts the result after all three reels have actually stopped.
    std::optional<std::uint8_t> confirm(const BattleSlotData&, bool joker_allowed,
        const std::array<std::uint8_t, 256>& random, std::uint8_t& random_index);
    void advance(const BattleSlotData&);
    std::array<std::uint8_t, 3> symbols(const BattleSlotData&) const;
};
// Reels are typed scrolling pictures. Their shaded composition uses the
// renderer's ordinary layers, never a PPU register or offset-per-tile program.
std::vector<TileLayer> make_battle_slot_layers(const BattleSlotData&,
    const std::array<Tile, 64>&, const TileLayer& panel, const BattleSlotState&,
    const BattleMenuTransition&, unsigned fade = 0);
} // namespace ff6
