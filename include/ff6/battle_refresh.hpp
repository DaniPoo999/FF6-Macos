#pragma once
#include "ff6/battle_status.hpp"

namespace ff6 {
struct BattleAvailability {
    std::array<BattleCommandList, 4> commands;
    std::array<BattleSpellList, 4> spells;
    std::bitset<4> changed_commands, used_espers;
    bool operator==(const BattleAvailability&) const = default;
};
BattleAvailability initialize_battle_availability(const BattleCharacters& characters);
// Complete character helpers; the caller owns presence and pending-work gating.
// Invalid character slots or malformed entries leave the owner unchanged.
void refresh_battle_commands(BattleAvailability& state, std::size_t slot,
                             const BattleCommandData& data, const CombatStats& stats,
                             std::uint32_t conditions, std::uint8_t morph_gauge);
void refresh_battle_spells(BattleAvailability& state, std::size_t slot, std::uint16_t current_mp,
                           std::uint32_t conditions);
struct BattleSpeedRates {
    std::uint8_t status = 0;
    std::uint16_t atb = 0;
    bool operator==(const BattleSpeedRates&) const = default;
};
BattleSpeedRates battle_speed_rates(std::uint8_t speed, std::uint32_t conditions, bool monster,
                                    std::uint8_t battle_speed_constant);
std::uint8_t start_battle_condemned(std::uint8_t level,
                                    const std::array<std::uint8_t, 256>& random_table,
                                    BattleRandom& random);
} // namespace ff6
