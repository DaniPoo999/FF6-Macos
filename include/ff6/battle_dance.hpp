#pragma once
#include "ff6/battle_state.hpp"

namespace ff6 {
struct BattleDanceData {
    std::array<std::uint8_t, 64> terrain_dances{};
    std::array<std::uint8_t, 8> backgrounds{};
    std::bitset<56> preserved_backgrounds{};
};
BattleDanceData load_battle_dance(const ContentPack&);
struct BattleDanceStart {
    bool stumbled = false;
    std::optional<std::uint8_t> background;
};
// C2/177D: the already-selected attack keeps its command identity. Terrain is
// logical battle state; special scenes can preserve a different visible image.
BattleDanceStart begin_battle_dance(BattleState&, unsigned character,
    const BattleDanceData&, const std::array<std::uint8_t, 256>& random);
} // namespace ff6
