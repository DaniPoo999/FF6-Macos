#pragma once
#include "ff6/battle_stats.hpp"
#include "ff6/game_time.hpp"

namespace ff6 {
struct BattleSpellProperties {
    std::uint8_t targeting = 0, mp_cost = 0;
};
struct BattleCommandChange {
    std::uint8_t before = 0, after = 0;
};
struct BattleCommandData {
    std::array<BattleSpellProperties, 256> spells{};
    std::array<std::uint8_t, 32> targeting{};
    std::array<std::uint8_t, 32> flags{};
    std::array<BattleCommandChange, 5> relic_changes{};
    // Each permutation maps spell identity to its uncompressed menu position.
    std::array<std::array<std::uint8_t, 54>, 6> magic_order{};
};
BattleCommandData decode_battle_commands(ByteView spells, ByteView commands, ByteView relic_changes,
                                         ByteView magic_order);
BattleCommandData load_battle_commands(const ContentPack& pack);
struct BattleSpell {
    // Identity is local to its list: magic, esper or lore, not an attack address.
    std::uint8_t identity = 0, targeting = 0, mp_cost = 0;
    bool disabled = true; // Availability is refreshed after status application.
    bool operator==(const BattleSpell&) const = default;
};
struct BattleSpellList {
    std::optional<BattleSpell> esper;
    std::array<std::optional<BattleSpell>, 54> magic;
    std::array<std::optional<BattleSpell>, 24> lores;
    std::uint8_t known_magic = 0;
    bool operator==(const BattleSpellList&) const = default;
};
// Whole empty three-spell rows disappear; holes within a retained row survive.
BattleSpellList prepare_battle_spell_list(const BattleCommandData& data,
                                          const std::array<std::optional<std::uint8_t>, 4>& actors,
                                          const LearnedAbilities& abilities,
                                          std::uint8_t magic_order, GameTime time);
std::uint8_t battle_mp_cost(std::uint8_t original, std::uint8_t relic_effects);
BattleSpellList validate_battle_spell_list(const BattleCommandData& data,
                                           const BattleSpellList& master,
                                           const LearnedAbilities& abilities, std::uint8_t actor,
                                           std::optional<std::uint8_t> esper,
                                           std::uint8_t relic_effects);
struct BattleCommandRules {
    bool magic_only = false, morph_available = false, permanent_morph = false, on_veldt = false;
    // Persistent battle switch 39, set by Terra's Mobliz recruitment.
    bool double_morph_duration = false;
    bool operator==(const BattleCommandRules&) const = default;
};
struct PreparedBattleCommand {
    std::uint8_t identity = 0, targeting = 0;
    bool disabled = true;
    bool operator==(const PreparedBattleCommand&) const = default;
};
struct BattleCommandList {
    std::array<std::optional<PreparedBattleCommand>, 4> slots;
    bool uses_mp = false, apply_permanent_morph = false;
    bool operator==(const BattleCommandList&) const = default;
};
// This prepares identities/targeting. Status-dependent availability and command
// execution belong to subsequent battle owners, not to this initial menu list.
BattleCommandList prepare_battle_command_list(const BattleCommandData& data,
                                              const Character& character, std::uint8_t actor,
                                              const BattleSpellList& spells,
                                              std::uint8_t relic_effects, std::uint8_t dances,
                                              BattleCommandRules rules);
struct BattleCharacter {
    CharacterCombatStats combat;
    BattleSpellList spells;
    BattleCommandList commands;
};
struct BattleCharacters {
    // The shared party spell union is installed before occupied slots filter it.
    // Unassigned slots retain this list, including named Morph overlaps.
    BattleSpellList shared_spells;
    std::array<std::optional<BattleCharacter>, 4> slots;
    std::array<std::optional<std::uint8_t>, 16> actor_slots;
    std::bitset<4> potential_engulf_targets;
    std::uint8_t party_relic_effects = 0;
    bool magitek_cue = false;
};
// Full initial character preparation, before pending statuses are applied.
// Occupied slots participate even if hidden. Persistent edits commit together;
// a malformed later slot cannot leave earlier equipment/status edits behind.
BattleCharacters initialize_battle_characters(const BattleCommandData& commands,
                                              const PartyData& party_data,
                                              const EquipmentData& equipment,
                                              const BattleParty& party, Roster& roster,
                                              std::uint8_t magic_order, GameTime time,
                                              BattleCommandRules rules);
} // namespace ff6
