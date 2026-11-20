#pragma once
#include "ff6/battle_inventory.hpp"
#include "ff6/battle_skills.hpp"
#include "ff6/game_menu_state.hpp"

namespace ff6 {
struct BattlePreparationData {
    BattleCommandData commands;
    BattleInventoryData inventory;
    MonsterStatsData monsters;
    BattlePartyData party;
    std::array<std::uint8_t, 256> random;
};
BattlePreparationData load_battle_preparation_data(const ContentPack& pack);
struct BattleConfiguration {
    std::uint8_t speed_constant = 255;
    bool wait_mode = false, short_commands = false, show_gauge = true;
    bool operator==(const BattleConfiguration&) const = default;
};
BattleConfiguration initialize_battle_configuration(const GameMenuState& menu);
std::array<std::uint8_t, 10>
initialize_battle_status_timers(const std::array<std::uint8_t, 256>& table, BattleRandom& random);
struct BattleMonsters {
    std::array<std::optional<MonsterCombatStats>, 6> slots;
    // Presence comes from the formation independently of loaded identity.
    std::bitset<6> present;
    std::optional<std::uint8_t> doom_gaze_target;
    // Monster slot identities in original insertion order. First-strike setup
    // stops their ATB, fills their gauge and starts an advance wait of 255.
    // This is pending work for the action owner, not an executed AI command.
    std::vector<std::uint8_t> first_strike_queue;
};
// Initial battle entry, with empty prior monster state. Reinforcement/reload
// callers retain more state and must not substitute this fresh initializer.
BattleMonsters initialize_battle_monsters(const BattlePreparationData& data,
                                          const BattleFormationEntry& formation,
                                          std::uint16_t saved_doom_gaze_hp,
                                          std::optional<std::uint8_t> colosseum_monster,
                                          BattleRandom& random);
// LoadMonsterProp retains elemental/status owners and AI continuation when a
// command changes formation inside an existing battle.
struct BattleMonsterRetention {
    std::array<CombatStats, 6> stats;
    std::array<BattleAiState, 6> ai;
    bool retain_vitals = false;
};
BattleMonsters reload_battle_monsters(const BattlePreparationData&, const BattleFormationEntry&,
                                      std::uint16_t saved_doom_gaze_hp,
                                      const BattleMonsterRetention&, BattleRandom&);
struct BattlePreparation {
    BattleRandom random;
    std::uint16_t available_characters = 0;
    std::array<std::uint8_t, 10> status_timers;
    BattleCharacters characters;
    BattleConfiguration configuration;
    BattleInventory inventory;
    BattleSkills skills;
    BattleMonsters monsters;
};
struct BattlePreparationContext {
    GameMenuState menu;
    BattleCommandRules commands;
    std::uint16_t saved_doom_gaze_hp = 0;
    std::optional<std::uint8_t> colosseum_monster;
};
// Ordered setup through InitMonsters. The next stage is pending-status
// application; battle type, ATB initialization and action execution follow it.
// All persistent character changes publish together after successful preparation.
BattlePreparation prepare_battle(const BattlePreparationData& data,
                                 const BattleFormationEntry& formation, const BattleParty& party,
                                 const PartyData& character_data, const EquipmentData& equipment,
                                 Roster& roster, const Inventory& inventory,
                                 const BattlePreparationContext& context);
} // namespace ff6
