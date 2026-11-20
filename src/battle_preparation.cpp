#include "ff6/battle_preparation.hpp"
#include <algorithm>

namespace ff6 {
BattlePreparationData load_battle_preparation_data(const ContentPack& pack) {
    const auto random = pack.get("encounter_rng_tbl_bin");
    if (random.size() != 256)
        throw std::invalid_argument("Invalid battle random table extent");
    BattlePreparationData result{load_battle_commands(pack),
                                 load_battle_inventory(pack),
                                 load_monster_stats(pack),
                                 load_battle_party_data(pack),
                                 {}};
    std::ranges::copy(random, result.random.begin());
    return result;
}
BattleConfiguration initialize_battle_configuration(const GameMenuState& menu) {
    if (menu.battle_speed > 7)
        throw std::invalid_argument("Invalid battle speed");
    return {static_cast<std::uint8_t>(255U - 24U * menu.battle_speed), menu.battle_wait,
            menu.short_commands, !menu.hide_gauge};
}
std::array<std::uint8_t, 10>
initialize_battle_status_timers(const std::array<std::uint8_t, 256>& table, BattleRandom& random) {
    std::array<std::uint8_t, 10> result;
    // Characters 0..3 and monsters 4..9, visited high to low even when empty.
    for (std::size_t remaining = result.size(); remaining != 0; --remaining)
        result[remaining - 1] = random.next(table);
    return result;
}
namespace {
BattleMonsters load_monsters(const BattlePreparationData& data,
                                          const BattleFormationEntry& formation,
                                          std::uint16_t saved_doom_gaze_hp,
                                          std::optional<std::uint8_t> colosseum_monster,
                                          BattleRandom& random, const BattleMonsterRetention* retained) {
    auto next_random = random;
    BattleMonsters result;
    std::array<std::optional<std::uint16_t>, 6> loaded;
    for (std::size_t remaining = 6; remaining != 0; --remaining) {
        const auto slot = remaining - 1;
        const auto& placement = formation.formation.monsters[slot];
        result.present[slot] = placement.present;
        auto identity = placement.monster;
        if (identity && *identity >= data.monsters.monsters.size())
            throw std::invalid_argument("Invalid formation monster identity");
        if (colosseum_monster) {
            // The original retains the formation identity's high bit. Only
            // high-bit-clear slots receive the selected eight-bit opponent.
            identity = identity && *identity < 256
                           ? std::optional<std::uint16_t>{*colosseum_monster}
                           : std::nullopt;
        }
        if (!identity)
            continue;
        loaded[slot] = identity;
        MonsterCombatContext context;
        if (retained) {
            context.retained = retained->stats[slot];
            context.ai = retained->ai[slot];
            context.retain_vitals = retained->retain_vitals;
        }
        context.present = placement.present;
        context.battle_identity = formation.selection.battle_identity;
        context.saved_doom_gaze_hp = saved_doom_gaze_hp;
        context.loaded_monsters = loaded;
        auto monster = initialize_monster_combat_stats(data.monsters, data.party, data.random,
                                                       *identity, context, next_random);
        if (monster.doom_gaze_target)
            result.doom_gaze_target = static_cast<std::uint8_t>(slot);
        if (monster.first_strike_action)
            result.first_strike_queue.push_back(static_cast<std::uint8_t>(slot));
        result.slots[slot] = std::move(monster);
    }
    random = next_random;
    return result;
}
} // namespace
BattleMonsters initialize_battle_monsters(const BattlePreparationData& data,
                                          const BattleFormationEntry& formation,
                                          std::uint16_t saved_doom_gaze_hp,
                                          std::optional<std::uint8_t> colosseum_monster,
                                          BattleRandom& random) {
    return load_monsters(data, formation, saved_doom_gaze_hp, colosseum_monster, random, nullptr);
}
BattleMonsters reload_battle_monsters(const BattlePreparationData& data,
                                      const BattleFormationEntry& formation,
                                      std::uint16_t saved_doom_gaze_hp,
                                      const BattleMonsterRetention& retained, BattleRandom& random) {
    return load_monsters(data, formation, saved_doom_gaze_hp, {}, random, &retained);
}
BattlePreparation prepare_battle(const BattlePreparationData& data,
                                 const BattleFormationEntry& formation, const BattleParty& party,
                                 const PartyData& character_data, const EquipmentData& equipment,
                                 Roster& roster, const Inventory& inventory,
                                 const BattlePreparationContext& context) {
    if (party.colosseum != context.colosseum_monster.has_value())
        throw std::invalid_argument("Colosseum setup requires its selected opponent");
    auto next = roster;
    BattlePreparation result;
    result.available_characters = roster.recruited;
    result.random = formation.random;
    result.status_timers = initialize_battle_status_timers(data.random, result.random);
    result.characters =
        initialize_battle_characters(data.commands, character_data, equipment, party, next,
                                     context.menu.magic_order, context.menu.time, context.commands);
    result.configuration = initialize_battle_configuration(context.menu);
    result.inventory = initialize_battle_inventory(data.inventory, inventory, result.characters);
    result.skills = initialize_battle_skills(next.abilities);
    result.monsters = initialize_battle_monsters(data, formation, context.saved_doom_gaze_hp,
                                                 context.colosseum_monster, result.random);
    roster = std::move(next);
    return result;
}
} // namespace ff6
