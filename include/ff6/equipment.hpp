#pragma once
#include "ff6/party.hpp"

namespace ff6 {
// Only the item fields consumed by equipment calculation. Inventory/use/equip
// eligibility belong to separate consumers of the original item resource.
struct EquipmentItem {
    bool weapon = false, imp_equipment = false;
    std::uint8_t field_effects = 0;
    std::uint16_t status_immunity = 0;
    std::uint8_t status2 = 0, status3 = 0;
    std::array<std::uint8_t, 5> relic_effects{};
    std::array<std::int8_t, 4> attribute_bonuses{}; // vigor, speed, stamina, magic
    std::int16_t evade_bonus = 0, magic_evade_bonus = 0;
    std::uint8_t power = 0, hit_rate_or_magic_defense = 0;
    std::uint8_t attack_elements_or_halved = 0, absorbed = 0, nullified = 0, weak = 0;
    std::uint8_t spell = 0, weapon_effects = 0, block_animation = 0;
};
struct EquipmentData {
    std::array<EquipmentItem, 256> items;
};
EquipmentData load_equipment_data(const ContentPack& pack);

enum class EquippedHandKind { other, weapon, unarmed };
struct EquippedHand {
    EquippedHandKind kind = EquippedHandKind::other;
    std::uint8_t attack = 0, hit_rate = 0, elements = 0, absorbed = 0;
    std::uint8_t spell = 0, effects = 0;
    bool operator==(const EquippedHand&) const = default;
};
struct EquipmentStats {
    std::array<std::uint8_t, 4> attributes{};
    std::uint8_t evade = 0, magic_evade = 0, defense = 0, magic_defense = 0;
    std::uint8_t run_factor = 0;
    std::array<EquippedHand, 2> hands{}; // right, left
    bool paired_weapons = false;
    std::uint16_t status_immunity = 0;
    std::uint8_t status2 = 0, status3 = 0;
    std::array<std::uint8_t, 5> relic_effects{};
    std::uint8_t absorbed = 0, nullified = 0, weak = 0, halved = 0, field_effects = 0;
    std::array<std::uint8_t, 6> block_animations{};
    std::uint8_t physical_block_graphics = 0, magic_block_graphics = 0;
    VitalBoost hp_boost = VitalBoost::none, mp_boost = VitalBoost::none;
    bool operator==(const EquipmentStats&) const = default;
};
EquipmentStats calculate_equipment(const Character& character, const PartyData& party_data,
                                   const EquipmentData& equipment);
struct EquipmentRefresh {
    std::array<std::optional<EquipmentStats>, 16> characters;
    std::uint8_t field_effects = 0;
};
// Caller supplies its selected identities (field objects and menu membership are
// not always identical). Failure preserves the roster. Only HP/MP boost flags
// are committed; current vitals, base values and inactive characters survive.
EquipmentRefresh refresh_equipment(Roster& roster, const PartyData& party_data,
                                   const EquipmentData& equipment,
                                   std::span<const std::uint8_t> identities);
} // namespace ff6
