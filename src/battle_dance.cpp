#include "ff6/battle_dance.hpp"
#include <algorithm>
#include <stdexcept>
namespace ff6 {
BattleDanceData load_battle_dance(const ContentPack& pack) {
    const auto terrain = pack.get("data_ed8e5b_battle_bg_dance");
    const auto backgrounds = pack.get("data_d1f9ab_dance_bg");
    const auto preserved = pack.get("battle_dance_preserved_backgrounds");
    if (terrain.size() != 64 || backgrounds.size() != 10 || preserved.size() != 56)
        throw std::invalid_argument("Invalid Dance terrain tables");
    BattleDanceData data;
    std::copy(terrain.begin(), terrain.end(), data.terrain_dances.begin());
    std::copy_n(backgrounds.begin(), 8, data.backgrounds.begin());
    for (unsigned i = 0; i < 56; ++i) {
        if (preserved[i] > 1) throw std::invalid_argument("Invalid Dance background flag");
        data.preserved_backgrounds[i] = preserved[i] != 0;
    }
    for (auto dance : data.terrain_dances)
        if (dance != 255 && dance >= 8) throw std::invalid_argument("Invalid terrain Dance identity");
    for (auto background : data.backgrounds)
        if (background >= 56) throw std::invalid_argument("Invalid Dance background identity");
    return data;
}
BattleDanceStart begin_battle_dance(BattleState& state, unsigned character,
    const BattleDanceData& data, const std::array<std::uint8_t, 256>& random) {
    if (character >= 4 || !state.terrain_background || *state.terrain_background >= 64)
        throw std::invalid_argument("Dance requires a character and battle terrain");
    auto dance = state.selections.dances[character];
    if (dance == 255) dance = state.selections.last_dance;
    if (dance >= 8) throw std::invalid_argument("Invalid selected Dance");
    state.selections.dances[character] = dance;
    auto& status = state.status.targets[character].active;
    status |= status_mask(BattleStatus::dance);
    if (data.terrain_dances[*state.terrain_background] == dance) return {};
    // RandCarry C2/4B53 uses bit zero. Matching terrain consumes no draw.
    if (!(state.random.next(random) & 1U)) {
        status &= ~status_mask(BattleStatus::dance);
        return {true, {}};
    }
    state.terrain_background = data.backgrounds[dance];
    return {false, state.terrain_background};
}
} // namespace ff6
