#pragma once
#include "ff6/battle_party.hpp"
#include "ff6/equipment.hpp"

namespace ff6 {
struct CombatVitals {
    std::uint16_t current = 0, maximum = 0;
    bool operator==(const CombatVitals&) const = default;
};
struct CombatHand {
    std::uint8_t attack = 0, hit_rate = 0, elements = 0, flags = 0;
    std::uint8_t item = 0, special = 0, spell = 0;
    bool operator==(const CombatHand&) const = default;
};
struct CombatStats {
    std::uint8_t level = 0, speed = 0, strength = 0, stamina = 0, magic_power = 0;
    // Nonzero original hit thresholds, not menu percentages.
    std::uint8_t evade_threshold = 255, magic_evade_threshold = 255;
    std::uint8_t defense = 0, magic_defense = 0;
    std::array<CombatHand, 2> hands{};
    CombatVitals hp, mp;
    std::uint8_t absorbed = 0, nullified = 0, weak = 0, halved = 0;
    // Bit set means blocked. Original serialized comparisons invert these.
    std::uint32_t blocked_status = 0, pending_status = 0;
    std::uint16_t equipment_status = 0;
    bool instant_death_protection = false;
    std::array<std::uint8_t, 4> relic_effects{}; // battle effects 1, 3, 4, 5
    std::uint8_t monster_flags = 0, retaliation_flags = 0, metamorph = 0;
    std::uint8_t monster_special = 255;
    bool operator==(const CombatStats&) const = default;
};
std::uint8_t battle_evade_threshold(std::uint8_t percent);
void fix_battle_immunity(CombatStats& stats);
// This later InitBattle stage follows pending-status application. It is not
// automatically run by the initial record loaders.
void normalize_battle_immunity(CombatStats& stats, bool permanent_morph);
struct CharacterCombatStats {
    CombatStats stats;
    Character persistent;
    std::uint16_t actor_mask = 0;
    std::uint8_t graphics = 0, run_factor = 0, party_relic_effects = 0;
    std::array<std::uint8_t, 2> relics{}, block_graphics{};
    bool genji_graphics = false, condemned_counter_update = true, magitek_cue = false;
};
// Returned persistent state owns equipment boost changes and scripted ailment
// clearing. Commands/spells and later status effects have separate owners.
CharacterCombatStats initialize_character_combat_stats(const Character& character,
                                                       const PartyData& party_data,
                                                       const EquipmentData& equipment,
                                                       std::uint16_t actor_mask,
                                                       std::uint8_t graphics, bool scripted);
struct MonsterStatsRecord {
    std::uint8_t speed = 0, attack = 0, hit_rate = 0, evade = 0, magic_evade = 0;
    std::uint8_t defense = 0, magic_defense = 0, magic_power = 0, level = 0;
    std::uint16_t hp = 0, mp = 0, experience = 0, gil = 0;
    std::uint8_t metamorph = 0, flags = 0, behavior = 0;
    std::uint32_t blocked_status = 0, initial_status = 0;
    std::uint8_t absorbed = 0, nullified = 0, weak = 0, attack_item = 0;
    std::uint8_t special = 0, special_animation = 0;
    std::array<std::uint8_t, 2> steal_items{}, drop_items{};
    std::array<std::uint8_t, 10> name{};
    // USA name grouping still uses eight-byte stride/keys on a ten-byte table.
    // Keep its lookup key separate from the actual display name.
    std::array<std::uint8_t, 8> grouping_key{};
};
struct MonsterStatsData {
    std::array<MonsterStatsRecord, 384> monsters;
};
MonsterStatsData decode_monster_stats(ByteView properties, ByteView items, ByteView names,
                                      ByteView special_animations);
MonsterStatsData load_monster_stats(const ContentPack& pack);
void apply_monster_rage_properties(CombatStats& stats, const MonsterStatsRecord& monster);
struct MonsterCombatContext {
    bool present = false, retain_vitals = false;
    std::uint16_t battle_identity = 0, saved_doom_gaze_hp = 0;
    CombatStats retained;
    BattleAiState ai;
    std::array<std::optional<std::uint16_t>, 6> loaded_monsters;
};
struct MonsterCombatStats {
    CombatStats stats;
    BattleAiState ai;
    std::uint16_t identity = 0, name_identity = 0, experience = 0, gil = 0;
    std::array<std::uint8_t, 2> steal_items{};
    std::uint8_t behavior = 0, special = 0, special_animation = 0;
    bool first_strike_action = false, doom_gaze_target = false;
};
// first_strike_action is an explicit intent for the future action-queue owner.
// Invalid inputs leave random unchanged; loaded_monsters includes this slot.
MonsterCombatStats
initialize_monster_combat_stats(const MonsterStatsData& data, const BattlePartyData& ai_data,
                                const std::array<std::uint8_t, 256>& random_table,
                                std::uint16_t identity, const MonsterCombatContext& context,
                                BattleRandom& random);
} // namespace ff6
