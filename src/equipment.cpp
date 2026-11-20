#include "ff6/equipment.hpp"
#include <algorithm>

namespace ff6 {
namespace {
std::int8_t signed_magnitude(unsigned nibble) {
    const auto magnitude = static_cast<int>(nibble & 7U);
    return static_cast<std::int8_t>((nibble & 8U) != 0 ? -magnitude : magnitude);
}
std::uint16_t add_bonus(std::uint16_t value, std::int16_t bonus) {
    return wrap16(static_cast<std::uint32_t>(std::int32_t{value} + bonus + 65536));
}
std::uint16_t clamp_stat_byte(std::uint16_t value) {
    // Original clamp replaces ONLY the low byte. Retained high bits participate
    // in the subsequent vigor bonus, even after an underflow or overflow.
    if (value < 256)
        return value;
    return static_cast<std::uint16_t>((value & 0xff00U) | ((value & 0x8000U) ? 0U : 255U));
}
std::uint8_t capped_sum(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>(std::min(unsigned{a} + b, 255U));
}
VitalBoost vital_boost(std::uint8_t effects, unsigned half) {
    if ((effects & half) != 0)
        return VitalBoost::half;
    if ((effects & (half >> 1U)) != 0)
        return VitalBoost::quarter;
    if ((effects & (half << 1U)) != 0)
        return VitalBoost::eighth;
    return VitalBoost::none;
}
void finish_hands(EquipmentStats& stats) {
    const auto right = stats.hands[0].kind, left = stats.hands[1].kind;
    const bool right_weapon = right == EquippedHandKind::weapon;
    const bool left_weapon = left == EquippedHandKind::weapon;
    const bool right_empty = right == EquippedHandKind::unarmed;
    const bool left_empty = left == EquippedHandKind::unarmed;
    stats.paired_weapons = right_weapon && left_weapon;
    // A weapon plus an empty opposite hand retains two-hand eligibility.
    // Two weapons, two empty hands and a single attacking hand do not.
    const bool clear_two_hand =
        stats.paired_weapons || (right_empty && left_empty) ||
        ((right_weapon || right_empty) && left == EquippedHandKind::other) ||
        ((left_weapon || left_empty) && right == EquippedHandKind::other);
    if (clear_two_hand)
        for (auto& hand : stats.hands)
            hand.effects &= 0xbfU;
    if (left_weapon && (right_empty || right == EquippedHandKind::other))
        stats.hands[0].attack = 0;
    else if (left_empty && right == EquippedHandKind::other)
        stats.hands[0].attack = 0;
    else if ((right_weapon && !left_weapon) || (right_empty && !left_weapon))
        stats.hands[1].attack = 0;
}
} // namespace

EquipmentData load_equipment_data(const ContentPack& pack) {
    const auto items = pack.get("data_d85000_item_prop");
    const auto evade = pack.get("equipment_evasion_modifiers");
    if (items.size() != 256 * 30 || evade.size() != 22)
        throw std::invalid_argument("Invalid equipment resource length");
    EquipmentData result{};
    for (std::size_t i = 0; i < result.items.size(); ++i) {
        const auto record = slice(items, i * 30, 30);
        const auto type = record[0] & 7U;
        const auto evade_index = record[26] & 15U, magic_index = unsigned{record[26]} >> 4U;
        if (type > 6 || evade_index >= 11 || magic_index >= 11)
            throw std::invalid_argument("Invalid item type/evasion modifier index");
        auto& item = result.items[i];
        item.weapon = type == 1;
        item.imp_equipment = (record[2] & 0x40U) != 0;
        item.field_effects = record[5];
        item.status_immunity = read16(record, 6);
        item.status3 = record[8];
        std::ranges::copy(record.subspan(9, 5), item.relic_effects.begin());
        item.attack_elements_or_halved = record[15];
        const auto bonuses = read16(record, 16);
        for (unsigned attribute = 0; attribute < 4; ++attribute)
            item.attribute_bonuses[attribute] = signed_magnitude(bonuses >> (attribute * 4U));
        item.spell = record[18];
        item.weapon_effects = record[19];
        item.power = record[20];
        item.hit_rate_or_magic_defense = record[21];
        item.absorbed = record[22];
        item.nullified = record[23];
        item.weak = record[24];
        item.status2 = record[25];
        item.evade_bonus = static_cast<std::int16_t>(signed16(read16(evade, evade_index * 2U)));
        item.magic_evade_bonus =
            static_cast<std::int16_t>(signed16(read16(evade, magic_index * 2U)));
        item.block_animation = record[27];
    }
    return result;
}
EquipmentStats calculate_equipment(const Character& character, const PartyData& party_data,
                                   const EquipmentData& equipment) {
    if (!character.properties || *character.properties >= party_data.characters.size())
        throw std::invalid_argument("Equipment requires assigned character properties");
    const auto& base = party_data.characters[*character.properties];
    if (base.run_factor > 3)
        throw std::invalid_argument("Invalid character escape factor");
    EquipmentStats stats{};
    std::array<std::uint16_t, 6> totals{};
    std::ranges::copy(character.attributes, totals.begin());
    totals[4] = base.combat_attributes[3];
    totals[5] = base.combat_attributes[4];
    stats.defense = base.combat_attributes[1];
    stats.magic_defense = base.combat_attributes[2];
    stats.run_factor = static_cast<std::uint8_t>(5U - base.run_factor);
    for (auto& hand : stats.hands)
        hand.attack = base.combat_attributes[0];
    for (std::size_t slot = character.equipment.size(); slot-- > 0;) {
        const auto identity = character.equipment[slot];
        const auto& item = equipment.items[identity];
        stats.field_effects |= item.field_effects;
        stats.status_immunity |= item.status_immunity;
        stats.status3 |= item.status3;
        for (std::size_t i = 0; i < stats.relic_effects.size(); ++i)
            stats.relic_effects[i] |= item.relic_effects[i];
        for (std::size_t i = 0; i < 4; ++i) {
            if (item.attribute_bonuses[i] < -7 || item.attribute_bonuses[i] > 7)
                throw std::invalid_argument("Invalid equipment attribute bonus");
            totals[i] = add_bonus(totals[i], item.attribute_bonuses[i]);
        }
        totals[4] = add_bonus(totals[4], item.evade_bonus);
        totals[5] = add_bonus(totals[5], item.magic_evade_bonus);
        const bool imp = (character.status & 0x20U) != 0;
        const auto power = static_cast<std::uint8_t>(imp == item.imp_equipment ? item.power : 1U);
        if (item.weapon) {
            if (slot < 2) {
                auto& hand = stats.hands[slot];
                hand.kind = identity == 255 ? EquippedHandKind::unarmed : EquippedHandKind::weapon;
                hand.attack = capped_sum(hand.attack, power);
                hand.hit_rate = item.hit_rate_or_magic_defense;
                hand.elements = item.attack_elements_or_halved;
                hand.absorbed = item.absorbed;
                hand.spell = item.spell;
                hand.effects = item.weapon_effects;
            }
        } else {
            stats.status2 |= item.status2;
            stats.halved |= item.attack_elements_or_halved;
            stats.weak |= item.weak;
            stats.absorbed |= item.absorbed;
            stats.nullified |= item.nullified;
            stats.defense = capped_sum(stats.defense, power);
            stats.magic_defense = capped_sum(stats.magic_defense, item.hit_rate_or_magic_defense);
        }
        if ((stats.relic_effects[0] & 2U) != 0) {
            stats.relic_effects[0] &= 0xfdU;
            if ((stats.relic_effects[2] & 2U) != 0)
                stats.relic_effects[0] |= 2U;
            stats.relic_effects[2] |= 2U;
        }
        stats.block_animations[slot] = item.block_animation;
        const auto graphic = static_cast<std::uint8_t>(1U << (item.block_animation & 3U));
        if ((item.block_animation & 4U) != 0)
            stats.physical_block_graphics |= graphic;
        if ((item.block_animation & 8U) != 0)
            stats.magic_block_graphics |= graphic;
    }
    stats.hp_boost = vital_boost(stats.relic_effects[0], 8);
    stats.mp_boost = vital_boost(stats.relic_effects[0], 64);
    for (auto& value : totals)
        value = clamp_stat_byte(value);
    finish_hands(stats);
    if ((stats.relic_effects[2] & 0x80U) != 0)
        totals[0] = wrap16(std::uint32_t{totals[0]} + (totals[0] >> 1U));
    for (std::size_t i = 0; i < 4; ++i)
        stats.attributes[i] = wrap8(totals[i]);
    stats.evade = wrap8(totals[4]);
    stats.magic_evade = wrap8(totals[5]);
    return stats;
}
EquipmentRefresh refresh_equipment(Roster& roster, const PartyData& party_data,
                                   const EquipmentData& equipment,
                                   std::span<const std::uint8_t> identities) {
    EquipmentRefresh result{};
    for (const auto identity : identities) {
        if (identity >= roster.characters.size() || result.characters[identity])
            throw std::invalid_argument("Invalid or repeated equipment refresh identity");
        const auto& character = roster.characters[identity];
        if (character.hp.base > 0x3fffU || character.mp.base > 0x3fffU)
            throw std::invalid_argument("Invalid native HP/MP base");
        auto stats = calculate_equipment(character, party_data, equipment);
        result.field_effects |= stats.field_effects;
        result.characters[identity] = std::move(stats);
    }
    for (const auto identity : identities) {
        roster.characters[identity].hp.boost = result.characters[identity]->hp_boost;
        roster.characters[identity].mp.boost = result.characters[identity]->mp_boost;
    }
    return result;
}
} // namespace ff6
