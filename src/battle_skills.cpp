#include "ff6/battle_skills.hpp"
#include <bit>

namespace ff6 {
BattleSkills initialize_battle_skills(const LearnedAbilities& abilities) {
    BattleSkills result;
    result.bushido_count = static_cast<std::uint8_t>(std::popcount(abilities.bushido));
    result.blitz_count = static_cast<std::uint8_t>(std::popcount(abilities.blitz));
    result.dances = abilities.dances;
    result.learned_blitzes = abilities.blitz;
    for (unsigned rage = 0; rage < 255; ++rage)
        if ((abilities.rages[rage / 8] & (1U << (rage % 8))) != 0)
            result.rages.push_back(static_cast<std::uint8_t>(rage));
    return result;
}
} // namespace ff6
