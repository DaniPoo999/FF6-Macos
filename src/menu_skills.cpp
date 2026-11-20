#include "ff6/menu_skills.hpp"
#include <algorithm>
#include <bitset>
namespace ff6 {
MenuSkillData load_menu_skills(const ContentPack& pack) {
    MenuSkillData result;
    result.commands = load_battle_commands(pack);
    const auto spells = pack.get("data_c46ac0_magic_prop"),
               espers = pack.get("data_d86e00_genju_prop"),
               order = pack.get("data_d1f9b5_genju_order");
    if (spells.size() != 256 * 14 || espers.size() != 27 * 11 || order.size() != 27)
        throw std::invalid_argument("Invalid menu ability tables");
    for (unsigned i = 0; i < 256; ++i) {
        const auto s = spells.subspan(i * 14, 14);
        result.spells[i] = {s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[10], s[13]};
    }
    std::bitset<27> seen;
    for (unsigned i = 0; i < 27; ++i) {
        if (!order[i] || order[i] > 27 || seen[order[i] - 1])
            throw std::invalid_argument("Invalid esper order");
        seen[order[i] - 1] = true;
        result.esper_order[order[i] - 1] = static_cast<std::uint8_t>(i);

    }
    result.espers = load_espers(pack);
    return result;
}
std::uint8_t menu_magic_progress(const Roster& roster, std::uint8_t id, std::uint8_t spell) {
    const auto properties = roster.characters.at(id).properties;
    if (!properties || spell >= 54)
        return 0;
    if (*properties < 12)
        return roster.abilities.magic[*properties][spell];
    if (*properties == 12) {
        for (auto member : current_menu_party(roster))
            if (member && *member != id) {
                const auto p = roster.characters[*member].properties;
                if (p && *p < 12 && roster.abilities.magic[*p][spell] == 255)
                    return 255;
            }
    }
    return 0;
}
std::uint8_t menu_spell_cost(const MenuSkillData& data, std::uint8_t spell,
                             const EquipmentStats& stats, GameTime time) {
    const auto original = spell == 153 ? wrap8(unsigned{time.hours} * 2 + (time.minutes >= 30))
                                       : data.spells[spell].mp_cost;
    return battle_mp_cost(original, stats.relic_effects[2]);
}
bool menu_spell_available(const MenuSkillData& data, const Roster& roster, std::uint8_t caster,
                          const EquipmentStats& stats, std::uint8_t spell, FieldItemPermissions p) {
    const auto& c = roster.characters.at(caster);
    if (spell >= 54 || menu_magic_progress(roster, caster, spell) != 255 ||
        !(data.spells[spell].menu_flags & 1U))
        return false;
    if (spell == 42 && !p.warp)
        return false;
    if (spell == 18)
        return false; // X-Zone is not enabled in an ordinary field menu.
    if ((c.status & 32U) && spell != 35)
        return false;
    return c.mp.current >= menu_spell_cost(data, spell, stats, {});
}
bool menu_spell_accepts(std::uint8_t spell, const Character& c) {
    if (c.status & 128U)
        return spell == 48 || spell == 49;
    switch (spell) {
    case 45:
    case 46:
    case 47:
        return !(c.status & (128U | 64U | 2U)) && c.hp.current < maximum_vital(c.hp, 9999);
    case 50:
        return (c.status & 4U) != 0;
    case 51:
        return (c.status & (64U | 4U | 1U)) != 0;
    case 34:
        return !(c.persistent_effects & 128U);
    case 35:
        return true;
    case 44:
        return (((c.status & 127U) | c.persistent_effects) & 0x90U) != 0;
    default:
        return false;
    }
}
FieldItemResult use_menu_spell(const MenuSkillData& data, Roster& roster, std::uint8_t caster,
                               const EquipmentStats& stats, std::uint8_t spell,
                               std::span<const std::uint8_t> targets, bool multiple,
                               FieldItemPermissions permissions) {
    if (!menu_spell_available(data, roster, caster, stats, spell, permissions))
        return FieldItemResult::invalid;
    const auto& magic = data.spells[spell];
    if (spell == 42) {
        roster.characters[caster].mp.current -= menu_spell_cost(data, spell, stats, {});
        return FieldItemResult::warp;
    }
    auto next = roster;
    bool applied = false;
    std::bitset<16> seen;
    for (const auto target : targets) {
        if (target >= 16 || seen[target])
            throw std::invalid_argument("Invalid menu spell targets");
        seen[target] = true;
        auto& c = next.characters[target];
        if (!menu_spell_accepts(spell, c))
            continue;
        // MenuMagicEffect uses base healing without battle variance, defense,
        // elemental multipliers or a combat special-effect dispatch.
        unsigned heal =
            magic.effects & 128U
                ? unsigned{maximum_vital(c.hp, 9999)} * magic.power / 16U
                : (roster.characters[caster].level ? unsigned{magic.power} * 4U : magic.power) +
                      unsigned{magic.power} * stats.attributes[3] *
                          roster.characters[caster].level / 16U;
        heal = wrap16(heal);
        if (multiple)
            heal /= 2;
        c.hp.current = std::min(wrap16(unsigned{c.hp.current} + heal), maximum_vital(c.hp, 9999));
        const auto modify = [mode = (magic.effects >> 2U) & 3U](std::uint8_t value,
                                                                std::uint8_t mask) {
            if (mode == 0)
                return static_cast<std::uint8_t>(value | mask);
            if (mode == 1)
                return static_cast<std::uint8_t>(value & ~mask);
            if (mode == 2)
                return static_cast<std::uint8_t>(value ^ mask);
            throw std::invalid_argument("Invalid menu spell status operation");
        };
        c.status = modify(c.status, magic.status);
        c.persistent_effects = modify(c.persistent_effects, magic.persistent);
        applied = true;
    }
    if (!applied)
        return FieldItemResult::invalid;
    next.characters[caster].mp.current -= menu_spell_cost(data, spell, stats, {});
    roster = std::move(next);
    return FieldItemResult::applied;
}
bool equip_menu_esper(Roster& roster, std::uint8_t id, std::optional<std::uint8_t> esper) {
    auto& c = roster.characters.at(id);
    if (!c.properties || *c.properties >= 12)
        return false;
    if (esper) {
        if (*esper >= 27 || !(roster.espers_owned & (1U << *esper)))
            return false;
        for (unsigned i = 0; i < 16; ++i)
            if (i != id && roster.characters[i].esper == esper)
                return false;
    }
    c.esper = esper;
    return true;
}
std::vector<std::uint8_t> menu_gogo_commands(const Roster& roster, std::uint16_t initialized,
                                           const MenuSkillData& data) {
    std::vector<std::uint8_t> result{255};
    // DrawStatusGogoWindow (C3/5DC1) includes commands from initialized actors,
    // including those a scenario has temporarily removed from availability.
    for (unsigned i = 0; i < 16; ++i)
        if (initialized & (1U << i)) {
            for (auto cmd : roster.characters[i].commands)
                if (cmd < 32 && cmd != 18 && (data.commands.flags[cmd] & 1U) &&
                    std::ranges::find(result, cmd) == result.end())
                    result.push_back(cmd);
        }
    return result;
}
unsigned menu_attack_power(const Character& c, const MenuItemData& items,
                           const EquipmentStats& stats) {
    const bool single = (c.equipment[0] == 255) != (c.equipment[1] == 255);
    const auto weapon = c.equipment[c.equipment[0] == 255 ? 1 : 0];
    if ((stats.relic_effects[3] & 8U) && single && items.items[weapon].two_handed)
        return unsigned{stats.hands[c.equipment[0] == 255 ? 1 : 0].attack} * 2;
    return stats.hands[0].attack +
           ((stats.relic_effects[3] & 16U) || !stats.hands[0].attack ? stats.hands[1].attack : 0U);
}
} // namespace ff6
