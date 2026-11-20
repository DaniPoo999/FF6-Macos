#include "ff6/battle_action_preparation.hpp"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
std::uint16_t mask(unsigned slot) {
    require(slot < 10, "Target participant outside battle");
    return static_cast<std::uint16_t>(1U << (slot < 4 ? slot : slot + 4));
}
std::uint16_t encoded(std::bitset<10> slots) {
    const auto value = static_cast<unsigned>(slots.to_ulong());
    return static_cast<std::uint16_t>((value & 15U) | ((value & 1008U) << 4U));
}
bool has(const BattleState& battle, unsigned slot, BattleStatus status) {
    return (battle.status.targets[slot].active & status_mask(status)) != 0;
}
std::uint16_t single(std::uint16_t targets, const std::array<std::uint8_t, 256>& table,
                      BattleRandom& random) {
    if (!targets)
        return 0;
    unsigned choice = (unsigned{random.next(table)} * static_cast<unsigned>(std::popcount(targets))) >> 8U;
    for (unsigned i = 0; i < 10; ++i)
        if (targets & mask(i)) {
            if (!choice)
                return mask(i);
            --choice;
        }
    throw std::logic_error("Unresolved target draw");
}
std::uint16_t choose(const BattleState& battle, std::uint8_t attacker, std::uint16_t targets,
                      const BattleAttackTargetRules& rules, BattleTargetEnvironment& environment,
                      const std::array<std::uint8_t, 256>& table, BattleRandom& random) {
    require(attacker < 10 && (targets & 0xc0f0U) == 0 &&
                (environment.backup_targets & 0xc0f0U) == 0,
            "Invalid battle target identity");
    if (environment.previous_attacker)
        require(*environment.previous_attacker < 10, "Invalid previous attacker");
    if (rules.targeting == 2)
        return mask(attacker); // Explicit SELF bypasses eligibility and consumes no RNG.
    const auto& policy = rules.policy;
    const auto filter = [&] {
        if (environment.skip_next_filter) {
            environment.skip_next_filter = false;
            return;
        }
        targets &= encoded(environment.selectable);
        if (policy.exclude_characters)
            targets &= 0xff00U;
        if (policy.exclude_attacker)
            targets &= static_cast<std::uint16_t>(~mask(attacker));
    };
    const auto present = [&] {
        if (policy.allow_dead)
            return;
        for (unsigned i = 0; i < 10; ++i)
            if (!battle.status.targets[i].present || has(battle, i, BattleStatus::dead) ||
                has(battle, i, BattleStatus::hide) ||
                (i >= 4 && (has(battle, i, BattleStatus::petrify) || has(battle, i, BattleStatus::zombie))))
                targets &= static_cast<std::uint16_t>(~mask(i));
    };
    filter();
    if (!policy.force_retarget) {
        present();
        if (targets)
            return (rules.targeting & 0x2cU) ? targets : single(targets, table, random);
    }
    const auto initial = rules.targeting & 12U;
    if (policy.force_retarget || !policy.no_retarget) {
        bool enemies = attacker >= 4, both = false;
        if (policy.reflected)
            enemies = !enemies;
        else {
            if (battle.links[attacker].charm_attacker)
                enemies = !enemies;
            if (attacker < 4 && battle.participation.enemy_characters[attacker])
                enemies = !enemies;
            both = initial == 4 || has(battle, attacker, BattleStatus::zombie);
            if (rules.targeting & 64U)
                enemies = !enemies;
            if (has(battle, attacker, BattleStatus::confuse))
                enemies = !enemies;
        }
        const auto enemy_characters = static_cast<std::uint16_t>(battle.participation.enemy_characters.to_ulong());
        targets = both || enemies ? static_cast<std::uint16_t>(0x3f00U | enemy_characters) : 0;
        if (both || !enemies)
            targets |= 15U ^ enemy_characters;
        filter();
        present();
    }
    const auto odd = [&] { return (random.next(table) & 1U) != 0; };
    if (policy.reflected) {
        if (policy.prefer_previous_attacker && !has(battle, attacker, BattleStatus::confuse) && !odd()) {
            require(environment.previous_attacker.has_value(), "Reflected preference needs previous attacker");
            const auto previous = mask(*environment.previous_attacker);
            if (targets & previous)
                targets = previous;
        }
    } else {
        if (!(initial & 4U)) {
            if (battle.entry.approach == BattleApproach::pincer) {
                const auto left = static_cast<std::uint16_t>(battle.monsters_on_left.to_ulong() << 8U);
                const auto right = static_cast<std::uint16_t>(battle.monsters_on_right.to_ulong() << 8U);
                if (attacker >= 4)
                    targets &= static_cast<std::uint16_t>(~((left & mask(attacker)) ? right : left));
                else if ((targets & left) && (targets & right))
                    targets &= static_cast<std::uint16_t>(~(odd() ? left : right));
            }
            if (battle.entry.approach == BattleApproach::side) {
                if (attacker < 4)
                    targets &= static_cast<std::uint16_t>(~((mask(attacker) & 3U) ? 12U : 3U));
                else if ((targets & 3U) && (targets & 12U))
                    targets &= static_cast<std::uint16_t>(~(odd() ? 3U : 12U));
            }
        }
        if (initial != 4 && (targets & 15U) && (targets & 0x3f00U))
            targets &= odd() ? 0x000fU : 0x3f00U;
    }
    if (!targets && policy.use_backup)
        targets = environment.backup_targets; // Intentional: the backup is not refiltered.
    if (initial || ((rules.targeting & 32U) && odd()))
        return targets;
    return single(targets, table, random);
}
} // namespace
BattleActionPreparationData load_battle_action_preparation(const ContentPack& pack) {
    BattleActionPreparationData data;
    const auto copy = [&](const char* name, auto& out) {
        const auto bytes = pack.get(name);
        require(bytes.size() == out.size(), "Invalid action preparation table extent");
        std::ranges::copy(bytes, out.begin());
    };
    copy("encounter_rng_tbl_bin", data.random);
    copy("battle_command_delays", data.delays);
    copy("battle_command_target_flags", data.command_flags);
    const auto tools = pack.get("battle_tool_target_mapping");
    const auto commands = pack.get("data_cffe00_battle_cmd_prop");
    const auto spells = pack.get("data_c46ac0_magic_prop");
    const auto items = pack.get("data_d85000_item_prop");
    require(tools.size() == 10 && commands.size() == 64 && spells.size() == 256 * 14 &&
                items.size() == 256 * 30, "Invalid targeting property extent");
    std::ranges::copy(tools.first(5), data.tool_items.begin());
    std::ranges::copy(tools.subspan(5), data.tool_offsets.begin());
    for (unsigned i = 0; i < 32; ++i)
        data.targeting[i] = commands[i * 2 + 1];
    for (unsigned i = 0; i < 256; ++i) {
        data.spells[i] = {spells[i * 14], spells[i * 14 + 2]};
        data.items[i] = {items[i * 30 + 14], items[i * 30 + 18], items[i * 30 + 21]};
    }
    return data;
}
std::uint8_t battle_action_delay(const BattleActionPreparationData& data, std::uint8_t command,
                                 std::uint8_t duration) {
    if (command >= 30)
        return duration;
    return static_cast<std::uint8_t>(std::min(unsigned{duration} + data.delays[command], 254U));
}
BattleAttackTargetRules battle_attack_target_rules(const BattleActionPreparationData& data,
                                                   std::uint8_t command, std::uint8_t attack) {
    BattleAttackTargetRules rules;
    rules.targeting = command < 30 ? data.targeting[command] : 64;
    const auto flags = command < 30 ? data.command_flags[command] : 0U;
    auto& policy = rules.policy;
    // The declaration's legacy flag names do not all describe their consumers.
    // Preserve the actual mapping and give the native policy behavioral names.
    policy.exclude_attacker = (flags & 1U) != 0;
    policy.no_retarget = (flags & 8U) != 0;
    policy.allow_dead = (flags & 16U) != 0;
    policy.use_backup = (flags & 32U) != 0;
    policy.force_retarget = (flags & 64U) != 0;
    policy.exclude_characters = (flags & 128U) != 0;
    auto type = flags & 6U;
    if (type == 6) {
        for (unsigned i = 5; i != 0; --i)
            if (attack == data.tool_items[i - 1]) {
                attack = static_cast<std::uint8_t>(unsigned{attack} - data.tool_offsets[i - 1]);
                type = 4;
                break;
            }
    }
    if (type == 2 || type == 6) {
        rules.item = attack;
        const auto item = data.items[attack];
        rules.targeting = item.targeting;
        if (!(item.statuses & 0xc2U))
            policy.allow_dead = false;
        if (type == 6 || attack >= 230)
            return rules;
        // C2/273C leaves the return-item bit clear when bit 7 is set.
        // C2/18AE then clears the reserved item: rods/shields are consumed.
        rules.break_item = (item.spell & 128U) != 0;
        attack = item.spell & 63U;
        type = 4;
    }
    if (type == 4)
        rules.spell = attack;
    const auto spell = data.spells[type == 4 ? attack : 238];
    if (rules.targeting == 255)
        rules.targeting = spell.targeting;
    policy.allow_dead = policy.allow_dead || (spell.flags & 4U);
    policy.force_retarget = policy.force_retarget || (spell.flags & 16U);
    policy.exclude_characters = policy.exclude_characters || (spell.flags & 128U);
    return rules;
}
std::uint16_t choose_battle_targets(const BattleState& battle, std::uint8_t attacker,
                                    std::uint16_t targets, const BattleAttackTargetRules& rules,
                                    BattleTargetEnvironment& environment,
                                    const std::array<std::uint8_t, 256>& table, BattleRandom& random) {
    auto next_environment = environment;
    auto next_random = random;
    const auto result = choose(battle, attacker, targets, rules, next_environment, table, next_random);
    environment = next_environment;
    random = next_random;
    return result;
}
BattlePreparedAction prepare_battle_action(const BattleActionPreparationData& data,
                                            const BattleState& battle, std::uint8_t attacker,
                                            BattleQueuedCommand command, std::uint8_t duration,
                                            BattleTargetEnvironment& environment, BattleRandom& random) {
    require(attacker < 10, "Action participant outside battle");
    BattlePreparedAction out{command, duration, {}};
    if (command.command >= 30)
        return out;
    require((command.parameters & 0xc0f0U) == 0, "Invalid action target identity");
    out.duration = battle_action_delay(data, command.command, duration);
    out.targeting = battle_attack_target_rules(data, command.command, command.attack);
    out.targeting->policy.no_retarget = false;
    if (!command.parameters) {
        auto next_environment = environment;
        next_environment.backup_targets = 0;
        out.command.parameters = choose_battle_targets(battle, attacker, 0, *out.targeting,
                                                        next_environment, data.random, random);
        environment = next_environment;
    }
    return out;
}
} // namespace ff6
