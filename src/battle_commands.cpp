#include "ff6/battle_commands.hpp"
#include <algorithm>

namespace ff6 {
BattleCommandData decode_battle_commands(ByteView spells, ByteView commands, ByteView relic_changes,
                                         ByteView magic_order) {
    if (spells.size() != 256 * 14 || commands.size() != 32 * 2 || relic_changes.size() != 10 ||
        magic_order.size() != 18)
        throw std::invalid_argument("Invalid battle command table extent");
    BattleCommandData result;
    for (std::size_t i = 0; i < result.spells.size(); ++i)
        result.spells[i] = {spells[i * 14], spells[i * 14 + 5]};
    for (std::size_t i = 0; i < result.targeting.size(); ++i) {
        result.flags[i] = commands[i * 2];
        result.targeting[i] = commands[i * 2 + 1];
    }
    for (std::size_t i = 0; i < result.relic_changes.size(); ++i) {
        if (relic_changes[i] >= 32 || relic_changes[i + 5] >= 32)
            throw std::invalid_argument("Invalid relic command identity");
        result.relic_changes[i] = {relic_changes[i], relic_changes[i + 5]};
    }
    for (std::size_t order = 0; order < result.magic_order.size(); ++order) {
        std::bitset<54> seen;
        for (unsigned spell = 0; spell < 54; ++spell) {
            const unsigned family = spell < 24 ? 0 : spell < 45 ? 1 : 2;
            const auto position = wrap8(spell + magic_order[family * 6 + order]);
            if (position >= 54 || seen[position])
                throw std::invalid_argument("Invalid magic order permutation");
            seen.set(position);
            result.magic_order[order][spell] = position;
        }
    }
    return result;
}
BattleCommandData load_battle_commands(const ContentPack& pack) {
    return decode_battle_commands(
        pack.get("data_c46ac0_magic_prop"), pack.get("data_cffe00_battle_cmd_prop"),
        pack.get("battle_relic_command_changes"), pack.get("battle_magic_order_offsets"));
}
BattleSpellList prepare_battle_spell_list(const BattleCommandData& data,
                                          const std::array<std::optional<std::uint8_t>, 4>& actors,
                                          const LearnedAbilities& abilities,
                                          std::uint8_t magic_order, GameTime time) {
    if (magic_order >= data.magic_order.size())
        throw std::invalid_argument("Invalid magic menu order");
    BattleSpellList result;
    std::array<std::optional<BattleSpell>, 54> sorted;
    for (const auto actor : actors) {
        if (!actor || *actor >= abilities.magic.size())
            continue; // Gogo copies the party union but contributes no learned list.
        for (std::uint8_t spell = 0; spell < 54; ++spell) {
            if (abilities.magic[*actor][spell] != 255)
                continue;
            const auto position = data.magic_order[magic_order][spell];
            if (position >= sorted.size())
                throw std::invalid_argument("Invalid magic menu position");
            const auto& properties = data.spells[spell];
            sorted[position] = BattleSpell{spell, properties.targeting, properties.mp_cost};
        }
    }
    std::size_t destination = 0;
    for (std::size_t row = 0; row < sorted.size(); row += 3) {
        if (!sorted[row] && !sorted[row + 1] && !sorted[row + 2])
            continue;
        for (unsigned column = 0; column < 3; ++column)
            result.magic[destination++] = sorted[row + column];
    }
    for (std::uint8_t lore = 0; lore < 24; ++lore) {
        if ((abilities.lores[lore / 8] & (1U << (lore % 8))) == 0)
            continue;
        const auto& properties = data.spells[139U + lore];
        // Step Mine rotates the hour byte with the half-hour comparison carry.
        const auto cost = lore == 14 ? wrap8(unsigned{time.hours} * 2U + (time.minutes >= 30))
                                     : properties.mp_cost;
        result.lores[lore] = BattleSpell{lore, properties.targeting, cost};
    }
    return result;
}
std::uint8_t battle_mp_cost(std::uint8_t original, std::uint8_t relic_effects) {
    // The increment is eight-bit BEFORE division: a cost of 255 halves to zero.
    if ((relic_effects & 32U) != 0)
        original = static_cast<std::uint8_t>(wrap8(unsigned{original} + 1U) / 2U);
    return (relic_effects & 64U) != 0 ? 1 : original;
}
BattleSpellList validate_battle_spell_list(const BattleCommandData& data,
                                           const BattleSpellList& master,
                                           const LearnedAbilities& abilities, std::uint8_t actor,
                                           std::optional<std::uint8_t> esper,
                                           std::uint8_t relic_effects) {
    if (actor >= 64 || (esper && *esper >= 27))
        throw std::invalid_argument("Invalid spell-list actor or esper");
    auto result = master;
    result.known_magic = 0;
    result.esper.reset();
    if (esper) {
        const auto& properties = data.spells[54U + *esper];
        result.esper = BattleSpell{*esper, properties.targeting,
                                   battle_mp_cost(properties.mp_cost, relic_effects)};
    }
    for (auto& spell : result.magic) {
        if (spell && spell->identity >= 54)
            throw std::invalid_argument("Invalid battle magic identity");
        if (spell && (actor > 12 || (actor < 12 && abilities.magic[actor][spell->identity] != 255)))
            spell.reset();
        if (spell) {
            ++result.known_magic;
            spell->mp_cost = battle_mp_cost(spell->mp_cost, relic_effects);
        }
    }
    for (auto& lore : result.lores) {
        if (lore && lore->identity >= 24)
            throw std::invalid_argument("Invalid battle lore identity");
        if (actor > 12)
            lore.reset();
        if (lore)
            lore->mp_cost = battle_mp_cost(lore->mp_cost, relic_effects);
    }
    return result;
}
BattleCommandList prepare_battle_command_list(const BattleCommandData& data,
                                              const Character& character, std::uint8_t actor,
                                              const BattleSpellList& spells,
                                              std::uint8_t relic_effects, std::uint8_t dances,
                                              BattleCommandRules rules) {
    constexpr std::uint8_t fight = 0, item = 1, magic = 2, morph = 3, lore = 12, rage = 16,
                           leap = 17, mimic = 18, dance = 19, x_magic = 23, magitek = 29,
                           none = 255;
    if (actor >= 64)
        throw std::invalid_argument("Invalid battle command actor");
    BattleCommandList result;
    for (std::size_t slot = 0; slot < character.commands.size(); ++slot) {
        auto command = character.commands[slot];
        if (command != none && command >= data.targeting.size())
            throw std::invalid_argument("Invalid battle command identity");
        if (((character.status & 8U) != 0 || rules.magic_only) && command != item &&
            command != mimic) {
            if (actor == 11 && command == rage)
                command = fight;
            command = command == fight ? magitek : command == magic ? magic : none;
            if (rules.magic_only)
                command = command == magitek ? magic : none;
        }
        // Descending table order is observable if replacements can cascade.
        for (unsigned remaining = 5; remaining != 0; --remaining) {
            const auto& change = data.relic_changes[remaining - 1];
            if (command == change.before && (relic_effects & (4U << (5U - remaining))) != 0)
                command = change.after;
        }
        switch (command) {
        case morph:
            if (!rules.morph_available || rules.permanent_morph) {
                result.apply_permanent_morph |= rules.morph_available && rules.permanent_morph;
                command = none;
            }
            break;
        case magic:
        case x_magic:
            if (spells.known_magic == 0 && !spells.esper)
                command = none;
            else
                result.uses_mp = true;
            break;
        case lore:
            result.uses_mp = true;
            break;
        case leap:
            if (!rules.on_veldt)
                command = none;
            break;
        case dance:
            if (dances == 0)
                command = none;
            break;
        default:
            break;
        }
        if (command != none) {
            if (command >= data.targeting.size())
                throw std::invalid_argument("Invalid replacement command identity");
            result.slots[slot] = PreparedBattleCommand{command, data.targeting[command]};
        }
    }
    return result;
}
BattleCharacters initialize_battle_characters(const BattleCommandData& commands,
                                              const PartyData& party_data,
                                              const EquipmentData& equipment,
                                              const BattleParty& party, Roster& roster,
                                              std::uint8_t magic_order, GameTime time,
                                              BattleCommandRules rules) {
    auto next = roster;
    std::array<std::optional<std::uint8_t>, 4> actors;
    for (std::size_t i = 0; i < actors.size(); ++i)
        actors[i] = party.slots[i].actor;
    const auto master =
        prepare_battle_spell_list(commands, actors, next.abilities, magic_order, time);
    BattleCharacters result;
    result.shared_spells = master;
    for (unsigned remaining = 4; remaining != 0; --remaining) {
        const auto index = remaining - 1;
        const auto& slot = party.slots[index];
        if (!slot.actor)
            continue;
        if (!slot.roster_index || *slot.roster_index >= next.characters.size() || !slot.graphics)
            throw std::invalid_argument("Battle actor has no matching persistent character");
        auto& persistent = next.characters[*slot.roster_index];
        BattleCharacter character;
        character.combat =
            initialize_character_combat_stats(persistent, party_data, equipment, slot.actor_mask,
                                              *slot.graphics, party.script_targets[index]);
        persistent = character.combat.persistent;
        auto& stats = character.combat.stats;
        character.spells = validate_battle_spell_list(commands, master, next.abilities, *slot.actor,
                                                      persistent.esper, stats.relic_effects[1]);
        character.commands = prepare_battle_command_list(
            commands, persistent, *slot.actor, character.spells,
            character.combat.party_relic_effects, next.abilities.dances, rules);
        if (!character.commands.uses_mp)
            stats.mp = {}; // Persistent MP survives; only combat vitals are hidden/cleared.
        if (character.commands.apply_permanent_morph) {
            stats.pending_status |= 0x08000000U;
            next.morph_gauge = 255;
        }
        if (*slot.actor < result.actor_slots.size())
            result.actor_slots[*slot.actor] = static_cast<std::uint8_t>(index);
        result.potential_engulf_targets.set(index);
        result.party_relic_effects |= character.combat.party_relic_effects;
        result.magitek_cue |= character.combat.magitek_cue;
        result.slots[index] = std::move(character);
    }
    roster = std::move(next);
    return result;
}
} // namespace ff6
