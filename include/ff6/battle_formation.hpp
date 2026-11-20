#pragma once
#include "ff6/content.hpp"
#include <array>
#include <bitset>
#include <optional>

namespace ff6 {
struct BattleMonsterPlacement {
    // Inactive slots retain identities/coordinates; monster 255 is valid.
    std::optional<std::uint16_t> monster;
    std::uint8_t x_tiles = 0, y_tiles = 0;
    bool present = false, background_one = false;
    bool operator==(const BattleMonsterPlacement&) const = default;
};
struct BattleFormation {
    std::uint8_t entrance = 0, graphics_layout = 0;
    // Normal, back, pincer, side: eligibility, not the selected approach.
    std::array<bool, 4> allowed_approaches{};
    bool victory_music = true, joker_doom = true, leap = true;
    bool character_ai_enabled = false;
    std::uint8_t character_ai = 0;
    bool escape = true, veldt = true, preemptive = true, continue_music = false;
    std::uint8_t song_slot = 0;
    // Unknown stored bits remain available without invented gameplay meaning.
    std::uint8_t reserved_behavior = 0, reserved_music = 0, reserved_monsters = 0;
    std::array<BattleMonsterPlacement, 6> monsters{};
    bool operator==(const BattleFormation&) const = default;
};
struct ConditionalBattleFormation {
    std::uint16_t from = 0, to = 0;
};
struct BattleFormationData {
    std::array<std::uint8_t, 256> random{};
    // All sixteen encoded records are retained, but the original loader only
    // visits the first eight, using condition bits 8..15. See BATTLE_SETUP.md.
    std::array<ConditionalBattleFormation, 16> conditional{};
    std::array<BattleFormation, 576> formations{};
};
BattleFormationData decode_battle_formation_data(ByteView random, ByteView conditional,
                                                 ByteView properties, ByteView monsters);
BattleFormationData load_battle_formation_data(const ContentPack& pack);
struct BattleRandom {
    std::uint8_t index = 0;
    static BattleRandom from_game_clock(std::uint8_t frames);
    std::uint8_t next(const std::array<std::uint8_t, 256>& table);
    bool operator==(const BattleRandom&) const = default;
};
struct BattleFormationSelection {
    std::uint16_t formation = 0, battle_identity = 0;
    bool operator==(const BattleFormationSelection&) const = default;
};
// An absent retained identity represents a fresh battle. Reinforcements may
// retain the original battle identity while selecting another formation.
// Invalid data leaves the caller's RNG untouched.
BattleFormationSelection select_battle_formation(const BattleFormationData& data,
                                                 std::uint16_t request, std::bitset<16> conditions,
                                                 std::optional<std::uint16_t> retained_identity,
                                                 BattleRandom& random);
struct BattleFormationEntry {
    BattleFormationSelection selection;
    BattleRandom random;
    BattleFormation formation;
};
// UpdateSRAM records encounters on return, including escape. Eligibility uses
// the retained battle identity and original monster IDs, not survivors.
void remember_veldt_formation(std::bitset<512>& history, const BattleFormationEntry& entry);
} // namespace ff6
