#pragma once
#include "ff6/party.hpp"
#include <bitset>

namespace ff6 {
struct BattleSkills {
    // Bushido's gauge limit is count-1, not the highest learned bit. Zero
    // remains an explicit count instead of exposing an underflowed sentinel.
    std::uint8_t bushido_count = 0, blitz_count = 0;
    std::bitset<8> dances, learned_blitzes;
    std::vector<std::uint8_t> rages;
    bool operator==(const BattleSkills&) const = default;
};
BattleSkills initialize_battle_skills(const LearnedAbilities& abilities);
} // namespace ff6
