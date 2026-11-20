#pragma once
#include "ff6/content.hpp"
#include <array>
#include <optional>

namespace ff6 {
struct CharacterTemplate {
    std::uint8_t hp = 0, mp = 0;
    // Attributes: vigor, speed, stamina, magic. Combat attributes: attack,
    // defense, magic defense, evade, magic evade; these are template inputs,
    // not the character's equipment-adjusted battle statistics.
    std::array<std::uint8_t, 4> commands{}, attributes{};
    std::array<std::uint8_t, 5> combat_attributes{};
    // Weapon, shield, helmet, armor, relic 1, relic 2; preserve encoded sentinels.
    std::array<std::uint8_t, 6> equipment{};
    std::uint8_t run_factor = 0, level_modifier = 0;
    bool fixed_equipment = false;
};
struct NaturalSpell {
    std::uint8_t spell = 0, level = 0;
};
struct PartyData {
    std::array<CharacterTemplate, 64> characters;
    std::array<std::array<std::uint8_t, 6>, 64> names;
    std::array<std::uint8_t, 98> hp_growth, mp_growth;
    std::array<std::uint16_t, 98> experience_growth;
    std::array<std::int8_t, 4> level_modifiers;
    std::array<std::array<NaturalSpell, 16>, 2> natural_magic;
    std::array<std::uint8_t, 8> bushido_levels, blitz_levels;
    std::array<std::uint8_t, 32> initial_rages;
    std::array<std::uint8_t, 3> initial_lores;
    std::array<std::uint8_t, 7> skill_commands;
};
PartyData load_party_data(const ContentPack& pack);

enum class VitalBoost : std::uint8_t { none, quarter, half, eighth };
struct VitalStat {
    std::uint16_t current = 0, base = 0;
    VitalBoost boost = VitalBoost::none;
    bool operator==(const VitalStat&) const = default;
};
struct Character {
    std::optional<std::uint8_t> properties;
    std::uint8_t graphics = 0, level = 0;
    std::array<std::uint8_t, 6> name{};
    VitalStat hp, mp;
    std::uint32_t experience = 0;
    std::uint8_t status = 0, persistent_effects = 0;
    // Same command, attribute and equipment order as CharacterTemplate.
    std::array<std::uint8_t, 4> commands{}, attributes{};
    std::array<std::uint8_t, 6> equipment{};
    std::optional<std::uint8_t> esper;
    bool operator==(const Character&) const = default;
};
struct PartyMembership {
    bool enabled = false, back_row = false;
    std::uint8_t party = 0, position = 0;
    bool visible = false;
    bool operator==(const PartyMembership&) const = default;
};
struct LearnedAbilities {
    std::array<std::array<std::uint8_t, 54>, 12> magic{};
    std::uint8_t bushido = 0, blitz = 0, dances = 0;
    std::array<std::uint8_t, 32> rages{};
    std::array<std::uint8_t, 3> lores{};
    bool operator==(const LearnedAbilities&) const = default;
};
struct Roster {
    std::array<Character, 16> characters;
    std::array<PartyMembership, 16> membership;
    LearnedAbilities abilities;
    // Available-character flags (event bits 2F0..2FF / $1EDE). Scenario
    // scripts may clear these without clearing the initialized story flags.
    std::uint16_t recruited = 0;
    std::uint32_t espers_owned = 0;
    std::uint8_t active_party = 0, morph_gauge = 0;
    bool operator==(const Roster&) const = default;
};
using MenuParty = std::array<std::optional<std::uint8_t>, 4>;
void learn_character_level_abilities(LearnedAbilities& learned, const PartyData& data,
                                      const Character& character);
Roster new_roster(const PartyData& data);
std::uint8_t recruited_average_level(const Roster& roster);
// Event recruitment raises to the recruited average, never lowers a level.
// Experience is reset to that level's threshold even when no growth is needed.
void normalize_character_level(Roster& roster, const PartyData& data, std::uint8_t character);
// Only the character/ability portion of property assignment. Names, graphics,
// statuses and membership survive. Field-object setup belongs to the event owner.
void assign_character_properties(Roster& roster, const PartyData& data, std::uint8_t character,
                                 std::uint8_t properties);
void assign_character_name(Roster& roster, const PartyData& data, std::uint8_t character,
                           std::uint8_t name);
MenuParty current_menu_party(const Roster& roster);
std::uint16_t maximum_vital(const VitalStat& stat, std::uint16_t cap);
bool menu_mp_visible(const Roster& roster, const Character& character);
std::array<bool, 7> available_menu_skills(const PartyData& data, const Character& character);
enum class CharacterMenuKind { skills, equipment, relic, status };
bool character_menu_allowed(const PartyData& data, const Character& character,
                            CharacterMenuKind kind);
} // namespace ff6
