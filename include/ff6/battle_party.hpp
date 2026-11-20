#pragma once
#include "ff6/battle_formation.hpp"
#include "ff6/party.hpp"

namespace ff6 {
struct BattleAiProgram {
    // Offsets within the extracted AI data resource, never CPU addresses.
    std::uint16_t entry = 0;
    std::optional<std::uint16_t> retaliation;
    bool operator==(const BattleAiProgram&) const = default;
};
struct BattleAiState {
    std::optional<std::uint16_t> entry, retaliation;
    bool operator==(const BattleAiState&) const = default;
};
// Width of one instruction in the game's AI data language, including opcode.
std::size_t battle_ai_command_size(std::uint8_t command);
BattleAiProgram locate_battle_ai(ByteView script, std::uint16_t entry);
// A script without retaliation leaves an existing retaliation entry unchanged.
void initialize_battle_ai(BattleAiState& state, const BattleAiProgram& program);
struct BattleActorSelection {
    std::uint8_t properties = 0;
    bool in_party = true, enemy = false;
    bool operator==(const BattleActorSelection&) const = default;
};
struct ScriptedBattleActor {
    std::optional<BattleActorSelection> actor;
    std::optional<std::uint8_t> graphics;
    std::optional<std::uint16_t> ai;
    std::uint8_t x = 0, y = 0;
};
struct BattleCharacterScript {
    bool hide_party = false, hide_names_and_gauges = false;
    std::uint8_t reserved_flags = 0, monster_targets = 0;
    std::optional<std::uint8_t> background, song;
    std::array<ScriptedBattleActor, 4> actors;
};
struct BattlePartyData {
    std::array<BattleCharacterScript, 24> scripts;
    std::array<BattleAiProgram, 384> ai;
};
BattlePartyData decode_battle_party_data(ByteView characters, ByteView pointers, ByteView script);
BattlePartyData load_battle_party_data(const ContentPack& pack);
struct BattlePartySeedSlot {
    std::optional<BattleActorSelection> actor;
    std::optional<std::uint8_t> graphics_override;
};
struct BattlePartyContext {
    bool final_lineup = false, gau_may_return = false;
    std::array<BattlePartySeedSlot, 4> retained;
    std::array<std::optional<std::uint8_t>, 12> final_order;
    std::optional<BattleActorSelection> colosseum_actor;
};
struct BattlePartySlot {
    std::optional<std::uint8_t> actor, roster_index, graphics;
    // Retained for an actor without a matching roster entry. Once resolved,
    // the original reuses its roster index as the next graphics selector.
    std::optional<std::uint8_t> graphics_override;
    std::array<std::uint8_t, 6> name{};
    bool present = false, enemy = false, back_row = false;
    std::uint16_t actor_mask = 0;
    BattleAiState ai;
};
struct BattleParty {
    // Dialogue searches the whole persistent roster by properties, including
    // absent party members. The first matching roster entry supplies its name.
    std::array<std::optional<std::array<std::uint8_t, 6>>, 64> dialogue_names;
    std::array<BattlePartySlot, 4> slots;
    std::array<std::optional<std::uint8_t>, 12> final_order;
    std::bitset<4> script_targets;
    std::bitset<6> monster_targets{63};
    std::optional<std::uint8_t> character_script;
    std::uint8_t matching_members = 0;
    bool gau_may_return = false, colosseum = false;
};
struct BattleScriptedPresentation {
    std::bitset<4> enemy_character_proxy;
    std::array<std::optional<std::array<std::uint16_t, 2>>, 4> positions;
    std::optional<std::uint8_t> background, song;
    bool hide_names = false;
    std::uint8_t monster_targets = 63;
};
BattleScriptedPresentation battle_scripted_presentation(const BattlePartyData& data,
                                                       const BattleParty& party);
BattleParty initialize_battle_party(const BattlePartyData& data, const Roster& roster,
                                    const BattleFormationEntry& formation,
                                    const BattlePartyContext& context);
std::array<BattlePartySeedSlot, 4> retain_battle_party(const BattleParty& party);
} // namespace ff6
