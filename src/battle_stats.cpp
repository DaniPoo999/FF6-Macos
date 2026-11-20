#include "ff6/battle_stats.hpp"
#include "rom-materials/battle_character_vital_caps.macros.inc"
#include "rom-materials/battle_moogle_suit_graphic.macros.inc"
#include "rom-materials/battle_monster_stamina_parameters.macros.inc"
#include "rom-materials/battle_monster_strength_parameters.macros.inc"
#include <algorithm>

namespace ff6 {
std::uint8_t battle_evade_threshold(std::uint8_t percent) {
    const unsigned doubled = std::min(unsigned{percent} * 2U, 255U);
    return static_cast<std::uint8_t>(doubled == 0 ? 255 : 256U - doubled);
}
void fix_battle_immunity(CombatStats& stats) {
    stats.instant_death_protection = (stats.blocked_status & 128U) != 0;
    stats.blocked_status &= ~std::uint32_t{128};
    if ((stats.nullified & 8U) != 0)
        stats.blocked_status |= 4U;
}
void normalize_battle_immunity(CombatStats& stats, bool permanent_morph) {
    if ((stats.equipment_status & 256U) != 0)
        stats.blocked_status |= 0x80000000U;
    if (permanent_morph)
        stats.blocked_status |= 0x08000000U;
    // Dance/stop/sleep/condemned/near-fatal/image aren't permanent protections.
    stats.blocked_status |= (std::uint32_t{stats.equipment_status} & 0xee78U) << 8U;
    if ((stats.blocked_status & 0x024000U) != 0)
        stats.blocked_status |= 0x024000U; // regen and sap
    if ((stats.blocked_status & 0x0c0000U) != 0)
        stats.blocked_status |= 0x0c0000U; // slow and haste
}
CharacterCombatStats initialize_character_combat_stats(const Character& character,
                                                       const PartyData& party_data,
                                                       const EquipmentData& equipment,
                                                       std::uint16_t actor_mask,
                                                       std::uint8_t graphics, bool scripted) {
    CharacterCombatStats result;
    result.persistent = character;
    const auto gear = calculate_equipment(character, party_data, equipment);
    auto& persistent = result.persistent;
    persistent.hp.boost = gear.hp_boost;
    persistent.mp.boost = gear.mp_boost;
    auto& stats = result.stats;
    stats.hp = {character.hp.current, maximum_vital(persistent.hp, FF6_ROM_VALUE_BATTLE_CHARACTER_VITAL_CAPS_0)};
    stats.mp = {character.mp.current, maximum_vital(persistent.mp, FF6_ROM_VALUE_BATTLE_CHARACTER_VITAL_CAPS_1)};
    if (scripted) {
        stats.hp.current = stats.hp.maximum;
        stats.mp.current = stats.mp.maximum;
        persistent.status &= 0x2dU;
    }
    result.graphics = character.equipment[3] == 159
                          ? FF6_ROM_VALUE_BATTLE_MOOGLE_SUIT_GRAPHIC_0 : graphics;
    result.run_factor = gear.run_factor;
    result.genji_graphics = (gear.relic_effects[3] & 16U) != 0;
    result.party_relic_effects = gear.relic_effects[1];
    result.relics = {character.equipment[4], character.equipment[5]};
    result.block_graphics = {gear.physical_block_graphics, gear.magic_block_graphics};
    auto relic4 = gear.relic_effects[3];
    if (gear.paired_weapons)
        relic4 &= 0xefU;
    // Three shifts select Merit Award (bit 5), independently of Genji Glove.
    result.actor_mask =
        static_cast<std::uint16_t>((actor_mask & 0x7fffU) | ((relic4 & 32U) != 0 ? 0x8000U : 0U));
    stats.relic_effects = {
        static_cast<std::uint8_t>((gear.relic_effects[0] & 127U) | (gear.relic_effects[1] & 128U)),
        gear.relic_effects[2], relic4, gear.relic_effects[4]};
    stats.level = character.level;
    stats.strength = static_cast<std::uint8_t>(std::min(unsigned{gear.attributes[0]} * 2U, 255U));
    stats.speed = gear.attributes[1];
    stats.stamina = gear.attributes[2];
    stats.magic_power = gear.attributes[3];
    stats.evade_threshold = battle_evade_threshold(gear.evade);
    stats.magic_evade_threshold = battle_evade_threshold(gear.magic_evade);
    stats.defense = gear.defense;
    stats.magic_defense = gear.magic_defense;
    stats.monster_flags = static_cast<std::uint8_t>(16U | (gear.relic_effects[4] & 128U));
    stats.absorbed = gear.absorbed;
    stats.nullified = gear.nullified;
    stats.weak = gear.weak;
    stats.halved = gear.halved;
    stats.blocked_status = gear.status_immunity;
    stats.equipment_status =
        static_cast<std::uint16_t>(gear.status2 | (unsigned{gear.status3} << 8U));
    const auto status4 = static_cast<std::uint8_t>((character.persistent_effects & 192U) |
                                                   ((gear.status3 & 1U) != 0 ? 128U : 0U));
    stats.pending_status = persistent.status | (std::uint32_t{gear.status2} << 8U) |
                           (std::uint32_t{gear.status3 & 254U} << 16U) |
                           (std::uint32_t{status4} << 24U);
    result.condemned_counter_update = (gear.status2 & 1U) == 0;
    result.magitek_cue = (persistent.status & 8U) != 0;
    for (unsigned i = 0; i < 2; ++i) {
        const auto& source = gear.hands[i];
        stats.hands[i] = {source.attack,  source.hit_rate,        source.elements,
                          source.effects, character.equipment[i], gear.block_animations[i],
                          source.spell};
        if ((relic4 & 8U) == 0)
            stats.hands[i].flags &= 0xbfU;
    }
    fix_battle_immunity(stats);
    return result;
}
MonsterStatsData decode_monster_stats(ByteView properties, ByteView items, ByteView names,
                                      ByteView special_animations) {
    if (properties.size() != 384 * 32 || items.size() != 384 * 4 || names.size() != 384 * 10 ||
        special_animations.size() != 384)
        throw std::invalid_argument("Invalid monster statistic table extent");
    MonsterStatsData result;
    for (std::size_t i = 0; i < result.monsters.size(); ++i) {
        const auto p = properties.subspan(i * 32, 32);
        auto& m = result.monsters[i];
        m.speed = p[0];
        m.attack = p[1];
        m.hit_rate = p[2];
        m.evade = p[3];
        m.magic_evade = p[4];
        m.defense = p[5];
        m.magic_defense = p[6];
        m.magic_power = p[7];
        m.hp = read16(p, 8);
        m.mp = read16(p, 10);
        m.experience = read16(p, 12);
        m.gil = read16(p, 14);
        m.level = p[16];
        m.metamorph = p[17];
        m.flags = p[18];
        m.behavior = p[19];
        m.blocked_status = read16(p, 20) | (std::uint32_t{p[22]} << 16U);
        m.absorbed = p[23];
        m.nullified = p[24];
        m.weak = p[25];
        m.attack_item = p[26];
        m.initial_status = read32(p, 27);
        m.special = p[31];
        m.special_animation = special_animations[i];
        m.steal_items = {items[i * 4], items[i * 4 + 1]};
        m.drop_items = {items[i * 4 + 2], items[i * 4 + 3]};
        std::ranges::copy(names.subspan(i * 10, 10), m.name.begin());
        std::ranges::copy(names.subspan(i * 8, 8), m.grouping_key.begin());
    }
    return result;
}
MonsterStatsData load_monster_stats(const ContentPack& pack) {
    return decode_monster_stats(
        pack.get("data_cf0000_monster_prop"), pack.get("data_cf3000_monster_items"),
        pack.get("text_monster_name_bin"), pack.get("data_cf37c0_monster_special_anim"));
}
void apply_monster_rage_properties(CombatStats& stats, const MonsterStatsRecord& m) {
    stats.monster_special = m.special;
    stats.weak |= m.weak;
    stats.blocked_status |= m.blocked_status;
    auto pending = m.initial_status;
    if ((pending & 0x010000U) != 0)
        pending |= 0x80000000U;
    stats.pending_status = pending & ~std::uint32_t{0x7b010000};
    if ((m.initial_status & 0x01000000U) != 0)
        stats.relic_effects[2] |= 64U;
    stats.equipment_status |= static_cast<std::uint16_t>((m.initial_status >> 8U) & 65535U);
    stats.absorbed |= m.absorbed;
    stats.nullified |= m.nullified;
    stats.metamorph = m.metamorph;
    stats.monster_flags = m.flags;
}
MonsterCombatStats
initialize_monster_combat_stats(const MonsterStatsData& data, const BattlePartyData& ai_data,
                                const std::array<std::uint8_t, 256>& random_table,
                                std::uint16_t identity, const MonsterCombatContext& context,
                                BattleRandom& random) {
    if (identity >= data.monsters.size())
        throw std::invalid_argument("Invalid monster identity");
    for (const auto loaded : context.loaded_monsters)
        if (loaded && *loaded >= data.monsters.size())
            throw std::invalid_argument("Invalid loaded monster identity");
    const auto& m = data.monsters[identity];
    MonsterCombatStats result;
    auto& stats = result.stats;
    stats = context.retained;
    result.identity = result.name_identity = identity;
    for (std::size_t i = context.loaded_monsters.size(); i-- > 0;) {
        const auto loaded = context.loaded_monsters[i];
        if (loaded && data.monsters[*loaded].grouping_key == m.grouping_key) {
            result.name_identity = *loaded;
            break;
        }
    }
    result.ai = context.ai;
    initialize_battle_ai(result.ai, ai_data.ai[identity]);
    result.steal_items = m.steal_items;
    result.experience = m.experience;
    result.gil = m.gil;
    stats.defense = m.defense;
    stats.magic_defense = m.magic_defense;
    if (!context.retain_vitals) {
        stats.hp = {m.hp, m.hp};
        stats.mp = {m.mp, m.mp};
        if (context.battle_identity == 463) {
            result.doom_gaze_target = true;
            if (context.saved_doom_gaze_hp != 0)
                stats.hp.current = context.saved_doom_gaze_hp;
        }
    }
    const auto fraction = stats.hp.maximum >> 9U;
    // CMP's carry survives the capped immediate load and contributes to ADC.
    stats.stamina = static_cast<std::uint8_t>(
        fraction >= FF6_ROM_VALUE_BATTLE_MONSTER_STAMINA_PARAMETERS_0
            ? FF6_ROM_VALUE_BATTLE_MONSTER_STAMINA_PARAMETERS_1 +
                  FF6_ROM_VALUE_BATTLE_MONSTER_STAMINA_PARAMETERS_2 + 1
            : fraction + FF6_ROM_VALUE_BATTLE_MONSTER_STAMINA_PARAMETERS_2);
    stats.hands[0].attack = m.attack;
    stats.hands[0].hit_rate = m.hit_rate;
    stats.hands[0].item = m.attack_item;
    stats.evade_threshold = battle_evade_threshold(m.evade);
    stats.magic_evade_threshold = battle_evade_threshold(m.magic_evade);
    stats.level = m.level;
    stats.speed = m.speed;
    stats.magic_power =
        static_cast<std::uint8_t>(std::min(unsigned{m.magic_power} + (m.magic_power >> 1U), 255U));
    stats.retaliation_flags |= static_cast<std::uint8_t>((m.initial_status >> 24U) & 130U);
    result.behavior = m.behavior;
    result.special = m.special;
    result.special_animation = m.special_animation;
    apply_monster_rage_properties(stats, m);
    result.first_strike_action = context.present && (m.behavior & 2U) != 0;
    stats.strength = static_cast<std::uint8_t>(
        FF6_ROM_VALUE_BATTLE_MONSTER_STRENGTH_PARAMETERS_1 +
        (random.next(random_table) & FF6_ROM_VALUE_BATTLE_MONSTER_STRENGTH_PARAMETERS_0));
    fix_battle_immunity(stats);
    return result;
}
} // namespace ff6
