#include "ff6/battle_refresh.hpp"
#include "rom-materials/battle_morph_enable_threshold.macros.inc"
#include "rom-materials/battle_offering_targeting.macros.inc"
#include "rom-materials/battle_imp_allowed_spell.macros.inc"
#include "rom-materials/battle_slow_rate.macros.inc"
#include "rom-materials/battle_haste_rate.macros.inc"
#include "rom-materials/battle_normal_rate.macros.inc"
#include "rom-materials/battle_speed_bias.macros.inc"
#include "rom-materials/battle_condemned_level_bound.macros.inc"
#include "rom-materials/battle_condemned_floor.macros.inc"
#include "rom-materials/battle_condemned_sum.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
bool has(std::uint32_t conditions, BattleStatus status) {
    return (conditions & status_mask(status)) != 0;
}
void validate_character(std::size_t slot) {
    if (slot >= 4)
        throw std::invalid_argument("Invalid battle-menu character slot");
}
} // namespace
BattleAvailability initialize_battle_availability(const BattleCharacters& characters) {
    BattleAvailability result;
    result.spells.fill(characters.shared_spells);
    for (std::size_t slot = 0; slot < 4; ++slot)
        if (const auto& character = characters.slots[slot]) {
            result.commands[slot] = character->commands;
            result.spells[slot] = character->spells;
        }
    return result;
}
void refresh_battle_commands(BattleAvailability& state, std::size_t slot,
                             const BattleCommandData& data, const CombatStats& stats,
                             std::uint32_t conditions, std::uint8_t morph_gauge) {
    validate_character(slot);
    auto commands = state.commands[slot];
    const auto weapons = static_cast<std::uint8_t>(stats.hands[0].flags | stats.hands[1].flags);
    for (auto& command : commands.slots) {
        if (!command)
            continue;
        if (command->identity >= data.flags.size())
            throw std::invalid_argument("Invalid battle command identity");
        command->disabled =
            has(conditions, BattleStatus::imp) && (data.flags[command->identity] & 4U) == 0;
        if (command->disabled)
            continue;
        switch (command->identity) {
        case 3: // Morph depends on the retained gauge, not the currently visible counter.
            command->disabled = morph_gauge <= FF6_ROM_VALUE_BATTLE_MORPH_ENABLE_THRESHOLD_0;
            break;
        case 7: // Bushido and Runic require either hand's weapon capability.
            command->disabled = (weapons & 2U) == 0;
            break;
        case 11:
            command->disabled = (weapons & 128U) == 0;
            break;
        case 2:
        case 12:
        case 23:
            command->disabled = has(conditions, BattleStatus::silence);
            break;
        case 0:
        case 6: // Offering's targeting change persists when the relic is later absent.
            if ((stats.relic_effects[2] & 1U) != 0)
                command->targeting = FF6_ROM_VALUE_BATTLE_OFFERING_TARGETING_0;
            break;
        default:
            break;
        }
    }
    state.commands[slot] = std::move(commands);
    state.changed_commands.set(slot);
}
void refresh_battle_spells(BattleAvailability& state, std::size_t slot, std::uint16_t current_mp,
                           std::uint32_t conditions) {
    validate_character(slot);
    auto spells = state.spells[slot];
    const auto limit = std::min(unsigned{current_mp} + 1U, 255U);
    const bool imp = has(conditions, BattleStatus::imp);
    auto refresh = [&](std::optional<BattleSpell>& spell, unsigned count, bool used) {
        if (!spell)
            return;
        if (spell->identity >= count)
            throw std::invalid_argument("Invalid battle spell identity");
        spell->disabled = used || (imp && spell->identity != FF6_ROM_VALUE_BATTLE_IMP_ALLOWED_SPELL_0) || spell->mp_cost >= limit;
    };
    refresh(spells.esper, 27, state.used_espers[slot]);
    for (auto& spell : spells.magic)
        refresh(spell, 54, false);
    for (auto& spell : spells.lores)
        refresh(spell, 24, false);
    state.spells[slot] = std::move(spells);
}
BattleSpeedRates battle_speed_rates(std::uint8_t speed, std::uint32_t conditions, bool monster,
                                    std::uint8_t battle_speed_constant) {
    // C2/09D2 supplies the rates. Bind the compiler's result-width bound to
    // the extracted maximum instead of retaining a folded selected width.
    static_assert(FF6_ROM_VALUE_BATTLE_SLOW_RATE_0 <= FF6_ROM_VALUE_BATTLE_HASTE_RATE_0 &&
                  FF6_ROM_VALUE_BATTLE_NORMAL_RATE_0 <= FF6_ROM_VALUE_BATTLE_HASTE_RATE_0);
    const std::uint8_t rate = has(conditions, BattleStatus::slow)
        ? FF6_ROM_VALUE_BATTLE_SLOW_RATE_0 : has(conditions, BattleStatus::haste)
        ? FF6_ROM_VALUE_BATTLE_HASTE_RATE_0 : FF6_ROM_VALUE_BATTLE_NORMAL_RATE_0;
    unsigned scaled_speed = wrap8(unsigned{speed} + FF6_ROM_VALUE_BATTLE_SPEED_BIAS_0);
    if (monster)
        scaled_speed = (scaled_speed * battle_speed_constant) >> 8U;
    return {rate, static_cast<std::uint16_t>((scaled_speed * (rate + rate / 2U)) >> 4U)};
}
std::uint8_t start_battle_condemned(std::uint8_t level,
                                    const std::array<std::uint8_t, 256>& random_table,
                                    BattleRandom& random) {
    const auto draw = random.next(random_table); // Consumed even when level is zero.
    const auto sum = wrap8(unsigned{level} + ((unsigned{level} * draw) >> 8U));
    // C2/09B4's nonnegative subtraction keeps carry set through the final
    // base addition. Authenticate the compiler-folded sum against both inputs.
    static_assert(FF6_ROM_VALUE_BATTLE_CONDEMNED_SUM_0 ==
                  FF6_ROM_VALUE_BATTLE_CONDEMNED_LEVEL_BOUND_0 + FF6_ROM_VALUE_BATTLE_CONDEMNED_FLOOR_0);
    return sum <= FF6_ROM_VALUE_BATTLE_CONDEMNED_LEVEL_BOUND_0
        ? static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_CONDEMNED_SUM_0 + 1U - sum)
        : FF6_ROM_VALUE_BATTLE_CONDEMNED_FLOOR_0;
}
} // namespace ff6
