#pragma once
#include "ff6/esper.hpp"
#include "ff6/battle_state.hpp"
namespace ff6 {
struct BattleRewardItem {
    std::uint8_t item = 255, quantity = 0;
};
struct BattleRewardData {
    EsperData espers{};
    std::array<std::uint8_t, 64> terrain_dances = [] {
        std::array<std::uint8_t, 64> result{}; result.fill(255); return result;
    }();
    std::array<std::uint8_t, 512> magic_points{};
    std::array<std::array<std::uint8_t, 2>, 256> item_magic{}; // rate, spell
};
BattleRewardData load_battle_reward_data(const ContentPack&);
struct BattleLearnedSpell { std::uint8_t character = 0, spell = 0; };
struct BattleRewards {
    std::uint8_t magic_points = 0;
    bool announce_magic_points = false, learned_dance = false;
    bool dispelled_cursed_shield = false;
    // Ordinary victories publish this into the retained AI-variable owner.
    // Coliseum/scripted returns do not perform equipped-item learning.
    std::optional<std::uint8_t> cursed_shield_battles;
    std::vector<BattleLearnedSpell> learned_spells;
    std::vector<std::uint8_t> learned_lores; // Attack identities, in announcement order.
    std::uint32_t experience = 0, gil = 0;
    std::vector<BattleRewardItem> items;
    std::vector<std::uint8_t> levelled_characters;
};
// Ordinary victory commits vitals, inventory, AP learning and level bonuses together.
// Shared persistent return state. Scripted exits use this without EXP/gil/drop
// awards; victory applies the same state before calculating rewards.
// C2/4A07 learns every eligible original monster, including hidden slots.
void learn_battle_rages(LearnedAbilities&, const BattleFormation&);
void retain_battle_party(const BattleState& battle, const BattleParty& party,
                        Roster& roster, Inventory& inventory);
BattleRewards award_colosseum_victory(const BattleState&, const BattleParty&, Roster&,
                                       Inventory&, std::uint8_t prize);
BattleRewards award_battle_victory(BattleState& battle, const BattleParty& party,
                                    const BattlePreparation& preparation,
                                    const MonsterStatsData& monsters, const PartyData& growth,
                                    const std::array<std::uint8_t, 256>& random,
                                    Roster& roster, Inventory& inventory, bool on_veldt = false,
                                    const BattleRewardData& learning = {},
                                    std::uint16_t formation = 512, bool morph_available = false,
                                    std::uint8_t cursed_shield_battles = 0);
} // namespace ff6
