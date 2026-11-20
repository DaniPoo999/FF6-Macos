#include "ff6/battle_party.hpp"

namespace ff6 {
std::size_t battle_ai_command_size(std::uint8_t command) {
    // AI data-language record widths, including the command byte. FE/FF are
    // boundaries; operands can contain either byte without ending the script.
    switch (command) {
    case 0xf0:
    case 0xf2:
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf9:
    case 0xfa:
    case 0xfc:
        return 4;
    case 0xf3:
    case 0xf8:
    case 0xfb:
        return 3;
    case 0xf1:
    case 0xf7:
        return 2;
    default:
        return 1;
    }
}
namespace {
std::optional<std::uint8_t> optional_byte(std::uint8_t value) {
    return value == 255 ? std::nullopt : std::optional{value};
}
void validate(const std::optional<BattleActorSelection>& actor) {
    if (actor && actor->properties >= 64)
        throw std::invalid_argument("Battle actor properties outside table");
}
} // namespace
BattleAiProgram locate_battle_ai(ByteView script, std::uint16_t entry) {
    if (script.size() > 65536)
        throw std::invalid_argument("Battle AI resource exceeds its offset format");
    std::size_t cursor = entry;
    while (cursor < script.size()) {
        const auto command = script[cursor];
        if (command == 255) {
            ++cursor;
            if (cursor >= script.size())
                throw std::invalid_argument("Battle AI has no retaliation boundary");
            return {entry, script[cursor] == 255
                               ? std::nullopt
                               : std::optional{static_cast<std::uint16_t>(cursor)}};
        }
        const auto size = battle_ai_command_size(command);
        if (size > script.size() - cursor)
            throw std::invalid_argument("Truncated battle AI command");
        cursor += size;
    }
    throw std::invalid_argument("Unterminated battle AI script");
}
void initialize_battle_ai(BattleAiState& state, const BattleAiProgram& program) {
    state.entry = program.entry;
    if (program.retaliation)
        state.retaliation = program.retaliation;
}
BattlePartyData decode_battle_party_data(ByteView characters, ByteView pointers, ByteView script) {
    if (characters.size() != 24 * 24 || pointers.size() != 384 * 2)
        throw std::invalid_argument("Invalid battle party table extent");
    BattlePartyData result;
    for (std::size_t i = 0; i < result.ai.size(); ++i)
        result.ai[i] = locate_battle_ai(script, read16(pointers, i * 2));
    for (std::size_t i = 0; i < result.scripts.size(); ++i) {
        auto& row = result.scripts[i];
        const auto bytes = characters.subspan(i * 24, 24);
        row.hide_party = (bytes[0] & 128U) != 0;
        row.hide_names_and_gauges = (bytes[0] & 1U) != 0;
        row.reserved_flags = bytes[0] & 126U;
        row.background = optional_byte(bytes[1]);
        row.monster_targets = bytes[2];
        row.song = optional_byte(bytes[3]);
        for (unsigned slot = 0; slot < 4; ++slot) {
            auto& actor = row.actors[slot];
            const auto at = 4 + slot * 5;
            if (bytes[at] != 255)
                actor.actor = BattleActorSelection{static_cast<std::uint8_t>(bytes[at] & 63U),
                                                   (bytes[at] & 128U) == 0, (bytes[at] & 64U) != 0};
            actor.graphics = optional_byte(bytes[at + 1]);
            if (bytes[at + 2] != 255) {
                const unsigned identity = 256U + bytes[at + 2];
                if (actor.actor && identity >= result.ai.size())
                    throw std::invalid_argument("Character AI script identity outside table");
                actor.ai = static_cast<std::uint16_t>(identity);
            }
            actor.x = bytes[at + 3];
            actor.y = bytes[at + 4];
        }
    }
    return result;
}
BattlePartyData load_battle_party_data(const ContentPack& pack) {
    return decode_battle_party_data(pack.get("data_d0fd00_character_ai_properties"),
                                    pack.get("ptr_table_cf8400"),
                                    pack.get("data_cf8700_ai_scripts"));
}
BattleScriptedPresentation battle_scripted_presentation(const BattlePartyData& data,
                                                       const BattleParty& party) {
    BattleScriptedPresentation out;
    if (!party.character_script) return out;
    if (*party.character_script >= data.scripts.size())
        throw std::invalid_argument("Invalid scripted scene record");
    const auto& script = data.scripts[*party.character_script];
    out.background = script.background;
    out.song = script.song;
    out.hide_names = script.hide_names_and_gauges;
    out.monster_targets = script.monster_targets;
    // set_play_xy uses sequential slots for a replacement party, otherwise
    // matches character properties. The first empty entry terminates the latter.
    for (unsigned row = 0; row < 4; ++row) {
        const auto& actor = script.actors[row];
        if (!script.hide_party && !actor.actor) break;
        std::optional<unsigned> slot;
        if (script.hide_party) slot = row;
        else for (unsigned i = 0; i < 4; ++i)
            if (party.slots[i].actor == actor.actor->properties) { slot = i; break; }
        // set_play_xy retains the last matching enemy actor, even without
        // a position override. This graphics alias is not the combat enemy mask.
        if (slot && actor.actor && actor.actor->enemy)
            out.enemy_character_proxy = 1U << *slot;
        if (slot && actor.x != 255)
            out.positions[*slot] = {static_cast<std::uint16_t>(unsigned{actor.x} * 2),
                                    static_cast<std::uint16_t>(unsigned{actor.y} * 2)};
    }
    return out;
}
BattleParty initialize_battle_party(const BattlePartyData& data, const Roster& roster,
                                    const BattleFormationEntry& formation,
                                    const BattlePartyContext& context) {
    if (roster.active_party > 7)
        throw std::invalid_argument("Invalid active battle party");
    for (unsigned i = 0; i < 16; ++i) {
        if ((roster.characters[i].properties && *roster.characters[i].properties >= 64) ||
            roster.membership[i].party > 7 || roster.membership[i].position > 3)
            throw std::invalid_argument("Invalid battle roster member");
    }
    auto selected = context.retained;
    for (const auto& slot : selected)
        validate(slot.actor);
    BattleParty result;
    for (const auto& character : roster.characters)
        if (character.properties && !result.dialogue_names[*character.properties])
            result.dialogue_names[*character.properties] = character.name;
    result.final_order = context.final_order;
    result.gau_may_return = context.gau_may_return;
    if (formation.formation.character_ai_enabled)
        result.character_script = formation.formation.character_ai;
    if (context.final_lineup) {
        for (const auto index : result.final_order)
            if (index && *index >= 16)
                throw std::invalid_argument("Final battle lineup index outside roster");
        for (auto& slot : selected) {
            slot.graphics_override.reset();
            if (slot.actor)
                continue;
            for (auto& index : result.final_order) {
                if (!index)
                    continue;
                const auto properties = roster.characters[*index].properties;
                slot.actor =
                    properties ? std::optional{BattleActorSelection{*properties}} : std::nullopt;
                index.reset();
                break;
            }
        }
    } else if (formation.selection.battle_identity >= 574) {
        validate(context.colosseum_actor);
        selected[0].actor = context.colosseum_actor;
        result.script_targets.set(0);
        result.colosseum = true;
    } else {
        for (unsigned remaining = 16; remaining != 0; --remaining) {
            const auto index = remaining - 1;
            const auto& member = roster.membership[index];
            if (member.party != roster.active_party)
                continue;
            ++result.matching_members;
            const auto properties = roster.characters[index].properties;
            selected[member.position].actor =
                properties ? std::optional{BattleActorSelection{*properties}} : std::nullopt;
        }
        // The source checks only the last monster's low byte, independent of
        // presence. Both an empty slot and valid monster 255 pass this test.
        if ((roster.recruited & (1U << 11U)) != 0 || result.matching_members >= 4 ||
            (formation.formation.monsters[5].monster.value_or(511) & 255U) != 255)
            result.gau_may_return = false;
        if (result.gau_may_return)
            result.character_script = 10;
    }
    if (result.character_script) {
        if (*result.character_script >= data.scripts.size())
            throw std::invalid_argument("Invalid effective character AI record");
        const auto& script = data.scripts[*result.character_script];
        result.monster_targets = script.monster_targets;
        if (script.hide_party)
            for (auto& slot : selected)
                slot.actor.reset();
        for (const auto& command : script.actors) {
            if (!command.actor)
                continue;
            validate(command.actor);
            std::optional<unsigned> chosen;
            // Compare the complete stored identity: an earlier flagged actor
            // cannot match the next command's unflagged six-bit lookup key.
            for (unsigned remaining = 4; remaining != 0; --remaining) {
                const auto index = remaining - 1;
                if (selected[index].actor == BattleActorSelection{command.actor->properties}) {
                    chosen = index;
                    break;
                }
            }
            if (!chosen) {
                for (unsigned index = 0; index < 4; ++index) {
                    if (!selected[index].actor) {
                        chosen = index;
                        break;
                    }
                }
            }
            // The branch skips an overflowing entry. The source comment
            // suggesting replacement of slot four contradicts its instructions.
            if (!chosen)
                continue;
            selected[*chosen] = {command.actor, command.graphics};
            result.script_targets.set(*chosen);
            if (command.ai) {
                if (*command.ai >= data.ai.size())
                    throw std::invalid_argument("Character AI script outside table");
                initialize_battle_ai(result.slots[*chosen].ai, data.ai[*command.ai]);
            }
        }
    }
    for (unsigned slot_index = 0; slot_index < 4; ++slot_index) {
        const auto& selection = selected[slot_index];
        auto& slot = result.slots[slot_index];
        slot.graphics_override = selection.graphics_override;
        if (!selection.actor)
            continue;
        slot.actor = selection.actor->properties;
        slot.present = selection.actor->in_party;
        slot.enemy = selection.actor->enemy;
        auto graphics = selection.graphics_override;
        for (unsigned remaining = 16; remaining != 0; --remaining) {
            const auto index = remaining - 1;
            const auto& character = roster.characters[index];
            if (character.properties != slot.actor)
                continue;
            slot.roster_index = static_cast<std::uint8_t>(index);
            slot.graphics = graphics.value_or(character.graphics);
            slot.back_row = roster.membership[index].back_row;
            slot.name = character.name;
            // All matching roster entries are visited. A prior match leaves
            // its storage index in the selector used as the next graphics ID.
            graphics = static_cast<std::uint8_t>(index);
            // For guest properties >=14 the original keeps the final name
            // word in this mask. Do not silently normalize that value to zero.
            slot.actor_mask = *slot.actor < 14 ? static_cast<std::uint16_t>(1U << *slot.actor)
                                               : read16(character.name, 4);
        }
    }
    return result;
}
std::array<BattlePartySeedSlot, 4> retain_battle_party(const BattleParty& party) {
    std::array<BattlePartySeedSlot, 4> result;
    for (unsigned i = 0; i < 4; ++i) {
        result[i].actor = party.slots[i].actor
                              ? std::optional{BattleActorSelection{*party.slots[i].actor}}
                              : std::nullopt;
        result[i].graphics_override = party.slots[i].roster_index
                                          ? party.slots[i].roster_index
                                          : party.slots[i].graphics_override;
    }
    return result;
}
} // namespace ff6
