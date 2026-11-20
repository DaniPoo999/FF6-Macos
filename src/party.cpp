#include "ff6/party.hpp"
#include "rom-materials/party_empty_roster_level.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
template <class Array> void copy_bytes(Array& output, ByteView input) {
    if (input.size() != output.size())
        throw std::invalid_argument("Incorrect party table length");
    std::ranges::copy(input, output.begin());
}
void require_character(const Character& character) {
    if (!character.properties || *character.properties >= 64)
        throw std::invalid_argument("Character has no valid assigned properties");
}
VitalStat initial_vital(std::uint8_t start, std::span<const std::uint8_t> growth,
                        std::uint8_t level) {
    std::uint32_t total = start;
    for (unsigned i = 0; i + 1 < level; ++i)
        total += growth[i];
    // Original initialization stores a raw 16-bit total. Split the encoded boost
    // explicitly even for unusual data that puts bits into the boost field.
    const auto encoded = wrap16(total);
    return {encoded, static_cast<std::uint16_t>(encoded & 0x3fffU),
            static_cast<VitalBoost>(encoded >> 14U)};
}
void learn_natural(LearnedAbilities& learned, const PartyData& data, const Character& character) {
    const auto actor = *character.properties;
    if (actor == 0 || actor == 6) {
        const auto& list = data.natural_magic[actor == 0 ? 0 : 1];
        for (const auto spell : list) {
            if (spell.spell >= 54 || spell.level > 99)
                throw std::invalid_argument("Invalid natural spell entry");
            if (spell.level > character.level)
                break;
            // The original uses the actor's fixed spell list, even if another
            // character slot has been assigned that actor's properties.
            learned.magic[actor][spell.spell] = 255;
        }
    } else if (actor == 2 || actor == 5) {
        const auto& levels = actor == 2 ? data.bushido_levels : data.blitz_levels;
        unsigned count = 0;
        while (count < levels.size() && levels[count] <= character.level)
            ++count;
        auto& flags = actor == 2 ? learned.bushido : learned.blitz;
        flags = static_cast<std::uint8_t>(flags | ((1U << count) - 1U));
    }
}
} // namespace

void learn_character_level_abilities(LearnedAbilities& learned, const PartyData& data,
                                      const Character& character) {
    require_character(character);
    learn_natural(learned, data, character);
}

PartyData load_party_data(const ContentPack& pack) {
    PartyData data{};
    const auto records = pack.get("data_ed7ca0_character_properties");
    const auto names = pack.get("text_char_name_bin");
    if (records.size() != 64 * 22 || names.size() != 64 * 6)
        throw std::invalid_argument("Invalid character properties/name table length");
    for (std::size_t i = 0; i < 64; ++i) {
        auto& value = data.characters[i];
        const auto record = slice(records, i * 22, 22);
        value.hp = record[0];
        value.mp = record[1];
        copy_bytes(value.commands, record.subspan(2, 4));
        copy_bytes(value.attributes, record.subspan(6, 4));
        copy_bytes(value.combat_attributes, record.subspan(10, 5));
        copy_bytes(value.equipment, record.subspan(15, 6));
        value.run_factor = record[21] & 3U;
        value.level_modifier = (record[21] >> 2U) & 3U;
        value.fixed_equipment = (record[21] & 16U) != 0;
        copy_bytes(data.names[i], slice(names, i * 6, 6));
    }
    copy_bytes(data.hp_growth, pack.get("data_e6f4a0_level_up_hp"));
    copy_bytes(data.mp_growth, pack.get("data_e6f502_level_up_mp"));
    const auto experience = pack.get("data_ed8220_level_up_exp");
    if (experience.size() != 196)
        throw std::invalid_argument("Invalid experience growth table length");
    for (std::size_t i = 0; i < 98; ++i)
        data.experience_growth[i] = read16(experience, i * 2);
    const auto modifiers = pack.get("character_level_modifiers");
    if (modifiers.size() != 4)
        throw std::invalid_argument("Invalid character level modifiers length");
    for (std::size_t i = 0; i < 4; ++i)
        data.level_modifiers[i] = static_cast<std::int8_t>(signed8(modifiers[i]));
    const auto spells = pack.get("data_ece3c0_natural_magic");
    if (spells.size() != 64)
        throw std::invalid_argument("Invalid natural magic table length");
    for (std::size_t actor = 0; actor < 2; ++actor)
        for (std::size_t i = 0; i < 16; ++i) {
            const auto offset = actor * 32 + i * 2;
            if (spells[offset] >= 54 || spells[offset + 1] > 99)
                throw std::invalid_argument("Invalid natural spell entry");
            data.natural_magic[actor][i] = {spells[offset], spells[offset + 1]};
        }
    const auto levels = pack.get("data_e6f490_bushido_blitz_level");
    if (levels.size() != 16)
        throw std::invalid_argument("Invalid natural ability levels length");
    copy_bytes(data.bushido_levels, levels.first(8));
    copy_bytes(data.blitz_levels, levels.last(8));
    copy_bytes(data.initial_rages, pack.get("data_c47aa0_initial_rages"));
    copy_bytes(data.initial_lores, pack.get("data_e6f564_initial_lores"));
    copy_bytes(data.skill_commands, pack.get("menu_skill_enabled_commands"));
    return data;
}
Roster new_roster(const PartyData& data) {
    Roster roster{};
    roster.abilities.rages = data.initial_rages;
    roster.abilities.lores = data.initial_lores;
    return roster;
}
std::uint8_t recruited_average_level(const Roster& roster) {
    unsigned sum = 0, count = 0;
    // Guest slots 14 and 15 never participate, regardless of flag bits.
    for (unsigned i = 0; i < 14; ++i)
        if ((roster.recruited & (1U << i)) != 0) {
            sum += roster.characters[i].level;
            ++count;
        }
    return static_cast<std::uint8_t>(count == 0 ? unsigned{FF6_ROM_VALUE_PARTY_EMPTY_ROSTER_LEVEL_0} : std::min(sum / count, 99U));
}
void normalize_character_level(Roster& roster, const PartyData& data, std::uint8_t identity) {
    if (identity >= roster.characters.size())
        throw std::invalid_argument("Character identity out of range");
    auto character = roster.characters[identity];
    require_character(character);
    if (character.level == 0 || character.level > 99)
        throw std::invalid_argument("Invalid character level");
    auto learned = roster.abilities;
    const auto average = recruited_average_level(roster);
    // EventCmd_77 admits equality too: UpdateMaxHP/MP strip encoded boosts,
    // cap base vitals and refill them. Above-average characters keep vitals.
    if (average >= character.level) {
        const auto grow = [&](VitalStat& vital, const auto& growth, unsigned cap) {
            unsigned total = vital.base;
            for (unsigned i = character.level - 1U; i + 1U < average; ++i)
                total += growth[i];
            const auto value = static_cast<std::uint16_t>(std::min(total, cap));
            vital = {value, value, VitalBoost::none};
        };
        grow(character.hp, data.hp_growth, 9999);
        grow(character.mp, data.mp_growth, 999);
        character.level = average;
        learn_natural(learned, data, character);
    }
    std::uint32_t experience = 0;
    for (unsigned i = 0; i + 1U < character.level; ++i)
        experience += data.experience_growth[i];
    character.experience = (experience * 8U) & 0xffffffU;
    roster.characters[identity] = character;
    roster.abilities = learned;
}
void assign_character_properties(Roster& roster, const PartyData& data, std::uint8_t identity,
                                 std::uint8_t properties) {
    if (identity >= 16 || properties >= 64)
        throw std::invalid_argument("Character/property identity out of range");
    const auto& source = data.characters[properties];
    if (source.level_modifier >= 4)
        throw std::invalid_argument("Invalid character level modifier");
    auto character = roster.characters[identity];
    auto learned = roster.abilities;
    character.properties = properties;
    character.commands = source.commands;
    character.attributes = source.attributes;
    character.equipment = source.equipment;
    character.esper.reset();
    // The original performs an eight-bit add, treats zero/negative as level 1,
    // then caps at 99. Do not replace that ordering with a host signed clamp.
    const auto adjusted =
        wrap8(static_cast<std::uint32_t>(int{recruited_average_level(roster)} +
                                         int{data.level_modifiers[source.level_modifier]} + 256));
    character.level = adjusted == 0 || adjusted >= 128 ? 1 : std::min<std::uint8_t>(adjusted, 99);
    character.hp = initial_vital(source.hp, data.hp_growth, character.level);
    character.mp = initial_vital(source.mp, data.mp_growth, character.level);
    std::uint32_t experience = 0;
    for (unsigned i = 0; i + 1 < character.level; ++i)
        experience += data.experience_growth[i];
    character.experience = (experience * 8U) & 0xffffffU;
    learn_natural(learned, data, character);
    roster.characters[identity] = character;
    roster.abilities = learned;
}
void assign_character_name(Roster& roster, const PartyData& data, std::uint8_t identity,
                           std::uint8_t name) {
    if (identity >= 16 || name >= 64)
        throw std::invalid_argument("Character/name identity out of range");
    roster.characters[identity].name = data.names[name];
}
MenuParty current_menu_party(const Roster& roster) {
    if (roster.active_party > 7)
        throw std::invalid_argument("Invalid active party");
    MenuParty party{};
    for (std::uint8_t i = 0; i < 16; ++i) {
        const auto member = roster.membership[i];
        if (member.party > 7 || member.position >= 4)
            throw std::invalid_argument("Invalid party membership");
        if (member.enabled && member.party == roster.active_party) {
            require_character(roster.characters[i]);
            // Original ascending scan: later identities overwrite a duplicate
            // position. Retain this observable rule; don't compact empty slots.
            party[member.position] = i;
        }
    }
    return party;
}
std::uint16_t maximum_vital(const VitalStat& stat, std::uint16_t cap) {
    if (stat.base > 0x3fffU || static_cast<unsigned>(stat.boost) > 3)
        throw std::invalid_argument("Invalid native HP/MP base or boost");
    unsigned bonus = 0;
    switch (stat.boost) {
    case VitalBoost::none:
        break;
    case VitalBoost::quarter:
        bonus = stat.base >> 2U;
        break;
    case VitalBoost::half:
        bonus = stat.base >> 1U;
        break;
    case VitalBoost::eighth:
        bonus = stat.base >> 3U;
        break;
    }
    return static_cast<std::uint16_t>(std::min(unsigned{stat.base} + bonus, unsigned{cap}));
}
bool menu_mp_visible(const Roster& roster, const Character& character) {
    require_character(character);
    const auto actor = *character.properties;
    if (roster.espers_owned != 0 || actor == 12)
        return true;
    return actor < 12 && std::ranges::find(roster.abilities.magic[actor], 255) !=
                             roster.abilities.magic[actor].end();
}
std::array<bool, 7> available_menu_skills(const PartyData& data, const Character& character) {
    require_character(character);
    std::array<bool, 7> enabled{};
    for (std::size_t i = 0; i < enabled.size(); ++i)
        enabled[i] = std::ranges::find(character.commands, data.skill_commands[i]) !=
                     character.commands.end();
    if (*character.properties == 12)
        enabled[0] = false;
    return enabled;
}
bool character_menu_allowed(const PartyData& data, const Character& character,
                            CharacterMenuKind kind) {
    require_character(character);
    if (kind == CharacterMenuKind::status)
        return true;
    if (kind == CharacterMenuKind::equipment) {
        if (*character.properties >= 13)
            return false;
    } else if (kind == CharacterMenuKind::relic) {
        if (*character.properties >= 14)
            return false;
    } else if (kind == CharacterMenuKind::skills) {
        const auto enabled = available_menu_skills(data, character);
        if (std::ranges::none_of(enabled, [](bool value) { return value; }))
            return false;
    } else {
        throw std::invalid_argument("Invalid character menu kind");
    }
    return (character.status & (0x80U | 0x40U | 0x02U)) == 0;
}
} // namespace ff6
