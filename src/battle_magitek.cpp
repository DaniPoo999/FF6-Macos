#include "ff6/battle_magitek.hpp"
#include "ff6/battle_action_preparation.hpp"
#include "rom-materials/magic_stone_bonus.macros.inc"
#include "rom-materials/magic_gil_level_cost.macros.inc"
#include "rom-materials/magic_needles_damage.macros.inc"
#include "rom-materials/magic_quick_packed_turns.macros.inc"
#include "rom-materials/magic_ball_damage_mask.macros.inc"
#include "rom-materials/magic_ball_count_mask.macros.inc"
#include "rom-materials/magic_launcher_additional.macros.inc"
#include "rom-materials/magic_poison_cap.macros.inc"
#include "rom-materials/magic_seize_power.macros.inc"
#include "rom-materials/physical_damage_variance.macros.inc"
#include "rom-materials/physical_protect_scale.macros.inc"
#include "rom-materials/physical_damage_cap.macros.inc"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
using S = BattleStatus;
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
bool has(const BattleState& s, unsigned slot, S status) {
    return (s.status.targets[slot].active & status_mask(status)) != 0;
}
std::uint16_t target_bit(unsigned slot) {
    return static_cast<std::uint16_t>(1U << (slot < 4 ? slot : slot + 4));
}
std::uint16_t scale(std::uint16_t damage, unsigned fraction) {
    // Mult24 divides by 256; MultDmg then increments the WORD, including wrap.
    return wrap16(((std::uint32_t{damage} * fraction) >> 8U) + 1U);
}
std::uint16_t random_set_bit(std::uint16_t mask, BattleState& s, const BattleMagitekData& data) {
    if (!mask)
        return 0;
    auto choice =
        (unsigned{s.random.next(data.random)} * static_cast<unsigned>(std::popcount(mask))) >> 8U;
    for (unsigned i = 0; i < 16; ++i)
        if (mask & (1U << i)) {
            if (choice == 0)
                return static_cast<std::uint16_t>(1U << i);
            --choice;
        }
    throw std::logic_error("Invalid random target selection");
}
std::uint32_t random_ailment(std::uint32_t candidates, BattleState& state,
                             const BattleMagitekData& data) {
    // C2/3BD0 weights the two status words by population, then draws a bit
    // within the chosen word. Immunity and existing status never reroll it.
    const auto lower = static_cast<std::uint16_t>(candidates);
    const auto upper = static_cast<std::uint16_t>(candidates >> 16U);
    const auto low_count = static_cast<unsigned>(std::popcount(lower));
    const auto count = low_count + static_cast<unsigned>(std::popcount(upper));
    const bool high = (unsigned{state.random.next(data.random)} * count >> 8U) >= low_count;
    return std::uint32_t{random_set_bit(high ? upper : lower, state, data)} << (high ? 16U : 0U);
}
bool check_hit(BattleState& state, unsigned slot, const BattleMagicEffect& effect,
               const BattleMagitekData& data, bool item) {
    // The original Vanish shortcut precedes instant-death protection (the
    // well-known original quirk), Reflect and hit-rate rolls.
    if (!item && !(effect.behavior & 64U) && has(state, slot, S::vanish))
        return true;
    if ((effect.flags & 2U) && state.stats[slot].instant_death_protection)
        return false;
    if ((effect.flags & 4U) &&
        (has(state, slot, S::dead) == ((state.stats[slot].monster_flags & 128U) != 0)))
        return false; // Resurrection hits a dead living target or a living undead target.
    if (effect.behavior & 32U)
        return true;
    if (effect.behavior & 64U)
        return effect.hit_rate ? state.stats[slot].level % effect.hit_rate == 0
                               : state.stats[slot].level == 0;
    const bool stamina_check = (effect.behavior & 16U) != 0;
    if (!stamina_check &&
        (has(state, slot, S::petrify) || has(state, slot, S::sleep) || has(state, slot, S::stop) ||
         has(state, slot, S::frozen) || effect.hit_rate == 255))
        return true;
    const auto threshold =
        (unsigned{effect.hit_rate} * state.stats[slot].magic_evade_threshold) >> 8U;
    if ((unsigned{state.random.next(data.random)} * 100U >> 8U) >= threshold)
        return false;
    return !stamina_check || (state.random.next(data.random) & 127U) > state.stats[slot].stamina;
}
std::uint16_t target_damage(BattleState& state, unsigned attacker, unsigned target,
                            const BattleMagicEffect& effect, std::uint16_t base,
                            const BattleMagitekData& data, bool& healing, bool item) {
    const auto& vital = (effect.options & 128U) ? state.stats[target].mp : state.stats[target].hp;
    const bool ratio = (effect.behavior & 128U) != 0;
    const bool unmodified = effect.special == 21 || effect.special == 25 || effect.special == 26 ||
        effect.special == 27 || effect.special == 30 || effect.special == 42 || effect.special == 61 || effect.special == 81;
    auto damage =
        ratio ? std::max<std::uint16_t>(
                    1,
                    wrap16(unsigned{(item || (effect.flags & 8U)) ? vital.maximum : vital.current} *
                           effect.power / 16U))
        : item || unmodified ? base
               : scale(base, state.random.next(data.random) | unsigned{FF6_ROM_VALUE_PHYSICAL_DAMAGE_VARIANCE_0});
    const auto& stats = state.stats[target];
    if (!item && !unmodified && !ratio && !(effect.flags & 32U)) {
        damage = scale(damage, 255U - stats.magic_defense);
        if (has(state, target, S::shell))
            damage = scale(damage, FF6_ROM_VALUE_PHYSICAL_PROTECT_SCALE_0);
        if (has(state, target, S::morph))
            damage /= 2;
    }
    if (!item && !unmodified && !ratio && !(effect.behavior & 1U) && attacker < 4 && target < 4)
        damage /= 2;
    if (effect.special == 34 && state.stats[attacker].level == stats.level) {
        // Stone adds its selected operand plus the equal-level carry in half-damage increments AFTER variance, defense
        // and friendly-fire scaling (C2/3922, C2/0D34). Keep odd-value rounding.
        damage = static_cast<std::uint16_t>(std::min(65535U,
            unsigned{damage} + unsigned{damage / 2U} * (FF6_ROM_VALUE_MAGIC_STONE_BONUS_0 + 1U)));
    }
    healing = (effect.behavior & 1U) != 0;
    if (has(state, target, S::petrify)) {
        healing = false;
        return 0;
    }
    if ((effect.flags & 8U) && (stats.monster_flags & 128U))
        healing =
            (effect.behavior & 2U) != 0; // Healing inversion for undead; Zombie alone differs.
    if ((effect.flags & 8U) && (effect.behavior & 2U) && has(state, target, S::zombie) &&
        !(stats.monster_flags & 128U))
        healing = !healing;
    // C2/0BD3: ForceField wins over absorption only when EVERY element of
    // the attack is protected. Mixed-element attacks retain the original mask.
    if (effect.elements && !(effect.elements & ~state.nullified_elements))
        damage = 0;
    else if (effect.elements & stats.absorbed)
        healing = !healing;
    else if (effect.elements & stats.nullified)
        damage = 0;
    else if (effect.elements & stats.halved)
        damage /= 2;
    else if ((effect.elements & stats.weak) && damage < 32768)
        damage = static_cast<std::uint16_t>(damage * 2U);
    return std::min<std::uint16_t>(damage, FF6_ROM_VALUE_PHYSICAL_DAMAGE_CAP_0);
}
} // namespace
bool battle_magic_effect_supported(const BattleMagicEffect& effect, bool item) {
    // Bit 7 excludes character targets in battle_attack_target_rules; it is
    // not another damage algorithm (including Interceptor's counter spells).
    // Dried Meat's special-effect table entries are RTS. Recruitment belongs
    // to the target's item counter AI, separately from ordinary item healing.
    return !(effect.flags & ~254U) &&
           !(effect.status_only & ~3U) && (effect.behavior & 12U) != 12U &&
           (effect.special == 17 || effect.special == 18 || effect.special == 19 ||
            effect.special == 36 || effect.special == 255 || effect.special == 35 || effect.special == 16 ||
            effect.special == 23 || effect.special == 24 || effect.special == 37 || effect.special == 38 || effect.special == 53 ||
            effect.special == 67 || effect.special == 39 || effect.special == 56 || effect.special == 21 ||
            effect.special == 22 || effect.special == 51 || effect.special == 57 || effect.special == 58 || effect.special == 59 || effect.special == 84 ||
            effect.special == 25 || effect.special == 26 || effect.special == 27 || effect.special == 28 ||
            effect.special == 29 || effect.special == 30 || effect.special == 31 || effect.special == 32 ||
            effect.special == 34 || effect.special == 40 || effect.special == 41 || effect.special == 42 ||
            effect.special == 60 || effect.special == 62 || effect.special == 63 || effect.special == 81 || effect.special == 83 ||
            effect.special == 43 || effect.special == 44 || effect.special == 45 ||
            effect.special == 52 || effect.special == 61 || effect.special == 46 || effect.special == 68 ||
            effect.special == 33 || effect.special == 47 || effect.special == 49 ||
            // Clear's attacker/target entries both return without a special
            // operation. Its ordinary encoded status flags still apply.
            effect.special == 55 || effect.special == 64 || effect.special == 69 ||
            (item && effect.special >= 73 && effect.special <= 78));
}
BattleMagitekData load_battle_magitek(const ContentPack& pack) {
    const auto records = pack.get("data_c46ac0_magic_prop");
    require(records.size() == 256 * 14, "Invalid magic effect table extent");
    BattleMagitekData data;
    for (unsigned i = 0; i < 8; ++i) {
        const auto p = records.subspan((131U + i) * 14U, 14);
        require(p[5] == 0, "Magitek effect unexpectedly requires MP");
        data.effects[i] = {p[0], p[1], p[2], p[3],          p[4], p[6],
                           p[7], p[8], p[9], read32(p, 10), p[5]};
    }
    const auto random = pack.get("encounter_rng_tbl_bin");
    require(random.size() == data.random.size(), "Invalid battle random table extent");
    std::copy(random.begin(), random.end(), data.random.begin());
    return data;
}
std::uint16_t battle_magic_base(std::uint8_t power, std::uint8_t magic, std::uint8_t level) {
    return wrap16(unsigned{power} * (level ? 4U : 1U) +
                  ((std::uint32_t{power} * magic * level) >> 5U));
}
namespace {
// ExecAttack performs direct effects first, then one non-recursive reflection
// pass. Damage is accumulated before changing vitals, including both ends of a
// drain; self-targets and several reflected rays can share one recipient.
BattleAttackResult resolve_magic(BattleState& state, std::uint8_t attacker,
                                 BattleQueuedCommand command, BattleMagicEffect effect,
                                 const BattleMagitekData& data, bool item = false,
                                 std::optional<std::uint16_t> fixed_base = {},
                                 unsigned multiplier = 0, const BattleMagicData* extended = nullptr,
                                 bool immediate_self = false, bool seize_drain = false) {
    validate_battle_state(state);
    require(attacker < 10 && state.status.targets[attacker].present,
            "Magic requires a present attacker");
    require(!(command.parameters & 0xc0f0U), "Invalid magical target mask");
    require(immediate_self || !state.links[attacker].seize_attacker,
            "Magic linked-action context is not implemented yet");
    if (!battle_magic_effect_supported(effect, item))
        throw std::invalid_argument("Unsupported magic effect " + std::to_string(effect.special) +
                                    " for attack " + std::to_string(command.attack));
    auto next = state;
    // AttackerEffect PEARL_LORE (C2/40A1) divides all three gil bytes by
    // ten and uses the remainder as the level divisor. The ordinary level
    // hit test below owns zero-divisor behavior and bypasses evasion rolls.
    if (effect.special == 29)
        effect.hit_rate = static_cast<std::uint8_t>(next.inventory.gil % 10U);
    auto targets = effect.targeting == 2 ? target_bit(attacker) : command.parameters;
    BattleAttackResult result{attacker, command.attack, {}, false};
    std::optional<unsigned> forced_departure;
    bool departure_miss = false;
    if (effect.special == 51) {
        // C2/4061 prefers the lowest present incapacitated character, even
        // when ordinary targeting selected a living ally. Jump/Seize exclude
        // that override. Otherwise at least two living allies must remain.
        constexpr auto incapacitated = status_mask(S::dead) | status_mask(S::petrify) |
                                       status_mask(S::zombie);
        for (unsigned slot = 0; slot < 4; ++slot)
            if (next.status.targets[slot].present && !next.departed(slot) &&
                (next.status.targets[slot].active & incapacitated) &&
                !next.away_from_battle[slot] && !next.links[slot].seize_attacker) {
                forced_departure = slot;
                targets = target_bit(slot);
                break;
            }
        departure_miss = !forced_departure && next.participation.ally_count < 2;
    }
    if (effect.special == 68) {
        // C2/424B replaces the selected target with the retained captive.
        // No link means no effect, never Discard against an arbitrary actor.
        const auto captive = next.links[attacker].seize_target;
        targets = captive ? target_bit(*captive) : 0;
        effect.behavior |= 32;
    }
    std::optional<std::uint16_t> mantra_power;
    if (effect.special == 21 || effect.special == 22) {
        // The target count includes the caster before the attacker effect.
        // Mantra divides current HP among the other selected allies and skips
        // damage modifiers; Spiraler sacrifices its caster before target work.
        const auto others = std::popcount(targets) - 1;
        if (effect.special == 21) {
            mantra_power = others > 0 ? static_cast<std::uint16_t>(next.stats[attacker].hp.current / others)
                                     : std::uint16_t{65535};
            effect.flags |= 96;
        } else {
            next.stats[attacker].hp.current = next.stats[attacker].mp.current = 0;
            next.status.targets[attacker].pending.apply |= status_mask(S::dead) | status_mask(S::hide);
            next.untargetable.set(attacker);
            result.departures.set(attacker);
        }
        targets &= static_cast<std::uint16_t>(~target_bit(attacker));
    }
    if (effect.special == 19) {
        constexpr auto blocked = status_mask(S::petrify) | status_mask(S::sleep) |
            status_mask(S::stop) | status_mask(S::hide) | status_mask(S::frozen);
        for (unsigned i = 0; i < 10; ++i)
            if (next.status.targets[i].active & blocked) targets &= ~target_bit(i);
    }
    if (effect.options & 8U) {
        std::uint16_t recipients = 0;
        for (unsigned slot = 0; slot < 10; ++slot) {
            if (!next.status.targets[slot].present || !(next.stats[slot].retaliation_flags & 6U))
                continue;
            // RunicEffect consumes character Runic even on an incapacitated
            // owner. The separate monster interception bit remains armed.
            next.stats[slot].retaliation_flags &= 251U;
            constexpr auto blocked = status_mask(S::dead) | status_mask(S::petrify) |
                                     status_mask(S::sleep) | status_mask(S::stop) |
                                     status_mask(S::frozen) | status_mask(S::hide);
            if (!(next.status.targets[slot].active & blocked))
                recipients |= target_bit(slot);
        }
        if (recipients) {
            result.runic = true;
            targets = recipients;
            effect.power =
                static_cast<std::uint8_t>(effect.original_mp_cost / std::popcount(recipients));
            effect.flags = 96;
            effect.options = 130;
            effect.behavior = 33;
            effect.status = effect.status_only = 0;
            effect.special = 255;
        }
    }
    for (unsigned slot = 0; slot < 10; ++slot) {
        if (!(targets & target_bit(slot)))
            continue;
        // ChooseTarget's SELF branch (C2/587E) bypasses the ordinary presence
        // and status filters. Death-counter scripts rely on this to execute
        // their departure effect after HP reaches zero.
        if (effect.targeting == 2 && !result.runic)
            continue;
        // ExecSelfAttack (C2/3167) sets both no-retarget and the bypass of
        // CheckTargetsPresent. Timer-owned Doom/auto-Raise/relic spells keep
        // their already selected recipient even if its status changed while
        // the immediate action waited. The immediate caller has checked the
        // presence/visibility masks; ordinary spell status filters do not apply.
        if (immediate_self)
            continue;
        if (forced_departure == slot)
            continue; // BabaBreath's explicit override admits incapacitated recipients.
        if ((effect.special == 68 || seize_drain) && next.links[attacker].seize_target == slot)
            continue; // The link, not ordinary target selection, owns this recipient.
        const bool joker = command.command == 15 && command.attack == 148;
        const bool eligible = next.status.targets[slot].present && (joker || next.participation.targetable[slot]) &&
                    (!has(next, slot, S::dead) || (effect.flags & 4U) ||
                     (item && (effect.status & 0xc2U))) &&
                    !has(next, slot, S::hide) &&
                    !(slot >= 4 && (has(next, slot, S::petrify) || has(next, slot, S::zombie)) &&
                      !(item && (effect.status & 0xc2U))) &&
                    (joker || immediate_self || (!next.away_from_battle[slot] && !next.links[slot].seize_attacker));
        if (!eligible)
            throw std::invalid_argument("Magic target changed; caller must retarget: attack " +
                std::to_string(command.attack) + ", target " + std::to_string(slot) +
                ", status " + std::to_string(next.status.targets[slot].active) +
                ", present " + std::to_string(next.status.targets[slot].present) +
                ", targetable " + std::to_string(next.participation.targetable[slot]));
    }
    // Runic divides by all eligible owners before this spell's ordinary
    // target policy. Fixed single-target spells still choose only one owner.
    if (!(effect.targeting & 44U) && effect.targeting != 2)
        targets = random_set_bit(targets, next, data);
    if (next.stats[attacker].mp.current < command.mp_cost) {
        // Runic admission already consumed character stances. CalcAttackEffect
        // reports insufficient MP without a spell animation or resource charge.
        result.runic = false;
        result.messages.push_back({40, {}});
        state = std::move(next);
        return result;
    }
    next.stats[attacker].mp.current =
        static_cast<std::uint16_t>(next.stats[attacker].mp.current - command.mp_cost);
    // CalcAttackEffect C2/32E0 requests UpdateEnabledMagic after a paid cast.
    // Besides affordability, that refresh publishes the used-Esper flag set
    // at submission. Leaving it pending forever permits repeated summons.
    if (command.mp_cost)
        next.status.targets[attacker].updates.set(static_cast<unsigned>(BattleStatusUpdate::spells));
    if (effect.special == 17) result.golem_hp = next.stats[attacker].hp.current;
    auto base = (item || result.runic)
                    ? std::uint16_t{effect.power}
                    : battle_magic_base(effect.power, next.stats[attacker].magic_power,
                                        next.stats[attacker].level);
    // Runic replaces the attack with MP recovery before CalcDmg. A copied or
    // fixed-power attack must not overwrite that replacement with its damage.
    if (fixed_base && !result.runic) base = *fixed_base;
    if (!item && !result.runic && attacker < 4 && !(effect.behavior & 1U)) {
        const auto& relics = next.stats[attacker].relic_effects;
        unsigned bonus = (relics[0] & 2U) ? base / 2U : (relics[1] & 2U) ? base / 4U : 0U;
        base = static_cast<std::uint16_t>(std::min(unsigned{base} + bonus, 65535U));
    }
    if (!(effect.flags & 64U) && std::popcount(targets) >= 2)
        base /= 2;
    if (!item && !result.runic && has(next, attacker, S::morph) && !(effect.flags & 32U))
        base = static_cast<std::uint16_t>(std::min(unsigned{base} + (base / 2U) * 2U, 65535U));
    base = static_cast<std::uint16_t>(std::min(unsigned{base} + (base / 2U) * multiplier, 65535U));
    if (mantra_power) base = *mantra_power;
    // These attacker effects bypass variance/defense/friendly-fire modifiers.
    // They still use the ordinary target, elemental, status and result owners.
    if (effect.special == 81) {
        // C2/3FB7 uses the party wallet for characters and the attacking
        // monster's remaining reward gil for enemies. Both use the extracted level rate
        // and divide twice that payment across targets, without damage modifiers.
        const unsigned available = attacker < 4 ? next.inventory.gil
            : next.monster_information[attacker - 4].remaining_gil;
        const auto paid = std::min(available, unsigned{next.stats[attacker].level} * unsigned{FF6_ROM_VALUE_MAGIC_GIL_LEVEL_COST_0});
        if (attacker < 4)
            next.inventory.gil -= paid;
        else
            next.monster_information[attacker - 4].remaining_gil =
                static_cast<std::uint16_t>(available - paid);
        if (!paid) {
            targets = 0;
            result.messages.push_back({8, {}});
        }
        base = targets ? static_cast<std::uint16_t>(paid * 2U / static_cast<unsigned>(std::popcount(targets))) : 0;
    }
    if (effect.special == 26) base = FF6_ROM_VALUE_MAGIC_NEEDLES_DAMAGE_0;
    if (effect.special == 25 || effect.special == 27) base = next.stats[attacker].hp.current;
    if (effect.special == 61) base = wrap16(unsigned{next.stats[attacker].hp.maximum} - next.stats[attacker].hp.current);
    if (effect.special == 30)
        base = static_cast<std::uint16_t>(std::min(65535U, effect.power ? next.field_steps / effect.power : 65535U));
    if (effect.special == 42 && targets) {
        const auto chosen = random_set_bit(targets, next, data);
        unsigned slot = 0;
        while (!(chosen & target_bit(slot))) ++slot;
        base = static_cast<std::uint16_t>(unsigned{next.stats[slot].level} * effect.power /
                                          static_cast<unsigned>(std::popcount(targets)));
    }
    // N. Cross removes hits, not participants: misses still animate against
    // the selected targets (C2/414D).
    std::uint16_t random_misses = 0;
    if (effect.special == 41)
        random_misses = static_cast<std::uint16_t>(unsigned{next.random.next(data.random)} |
                            (unsigned{next.random.next(data.random)} << 8U));
    std::array<std::uint16_t, 4> ailment_recipients{};
    if (effect.special == 40)
        // C2/413C draws four recipients with replacement before CheckHit.
        // A missed recipient loses its draws; another target does not inherit them.
        for (auto& recipient : ailment_recipients)
            recipient = random_set_bit(targets, next, data);

    bool force_miss = departure_miss, miss_notice = false;
    if (effect.special == 49) {
        // C2/40DA chooses from the complement; never reroll an existing element.
        const auto available = static_cast<std::uint8_t>(next.nullified_elements ^ 255U);
        if (!available) force_miss = true;
        else {
            const auto chosen = static_cast<std::uint8_t>(random_set_bit(available, next, data));
            next.nullified_elements |= chosen;
            result.messages.push_back({static_cast<std::uint8_t>(55 + std::countr_zero(chosen)), {}});
        }
    }
    if (effect.special == 73) {
        // RandGenju C2/37DC draws 0..24, skipping Odin and Raiden. Ownership
        // and the equipped Esper's once-per-battle flag are not consulted.
        const auto choice = unsigned{next.random.next(data.random)} * 25U >> 8U;
        result.follow_up_summon = static_cast<std::uint8_t>(54U + choice + (choice >= 11 ? 2U : 0U));
    }
    if (effect.special == 24 || effect.special == 77) {
        if (next.display.escape_tools_blocked) {
            force_miss = true;
            result.messages.push_back({10, {}});
        } else
            next.end_event = 2;
    } else if (effect.special == 75 && next.display.escape_tools_blocked) {
        force_miss = true;
        // C2/40C8 intentionally uses RUN_FAIL, unlike Warp's WARP_FAIL.
        result.messages.push_back({9, {}});
    } else if (effect.special == 67) {
        // Recasting during Quick leaves its counter alone. The original
        // still runs the hit/memory pass; it does not clear the hit mask.
        miss_notice = next.quick.has_value() || next.quick_recast_blocked;
        if (!miss_notice)
            next.quick = BattleQuickState{attacker, FF6_ROM_VALUE_MAGIC_QUICK_PACKED_TURNS_0 / 256U};
    }
    std::array<std::optional<std::uint16_t>, 10> damage{}, healing{}, drain_damage{},
        drain_healing{};
    const auto accumulate = [](auto& amounts, unsigned slot, std::uint16_t amount) {
        amounts[slot] = static_cast<std::uint16_t>(
            std::min(unsigned{FF6_ROM_VALUE_PHYSICAL_DAMAGE_CAP_0}, unsigned{amounts[slot].value_or(0)} + amount));
    };
    const auto apply_vitals = [&] {
        for (unsigned slot = 0; slot < 10; ++slot) {
            if (!damage[slot] && !healing[slot])
                continue;
            // C2/13A7 suppresses damage after drain amounts are established.
            // Healing, status changes, hit memory and random draws survive.
            if (next.invincible[slot] && damage[slot]) damage[slot] = 0;
            auto& stats = next.stats[slot];
            auto& vital = (effect.options & 128U) ? stats.mp : stats.hp;
            const int net = int{healing[slot].value_or(0)} - int{damage[slot].value_or(0)};
            vital.current = static_cast<std::uint16_t>(
                std::clamp(int{vital.current} + net, 0, int{vital.maximum}));
            if ((effect.options & 128U) && net)
                next.status.targets[slot].updates.set(
                    static_cast<unsigned>(BattleStatusUpdate::spells));
            if (net < 0 && !vital.current &&
                (!(effect.options & 128U) || (stats.monster_flags & 1U)) &&
                !has(next, slot, S::zombie)) {
                stats.hp.current = 0;
                next.status.targets[slot].pending.apply |= status_mask(S::dead);
            }
        }
    };
    const auto apply_target = [&](BattleAttackTargetResult& target, unsigned source,
                                  std::uint16_t power) {
        if (!target.hit)
            return;
        auto& memory = next.attack_memory[target.slot];
        if (!memory.retaliation_pending || (next.random.next(data.random) & 1U)) {
            memory.retaliation_attacker = static_cast<std::uint8_t>(source);
            memory.retaliation_pending = true;
        }
        memory.command =
            BattleObservedAttackValue{static_cast<std::uint8_t>(source), command.command};
        // C2/3601 retains item identity separately from the most recent spell.
        // Ordinary consumables must not overwrite that spell history.
        auto& observed = item ? memory.item : memory.attack;
        observed = BattleObservedAttackValue{static_cast<std::uint8_t>(source), command.attack};
        memory.elements =
            BattleObservedAttackValue{static_cast<std::uint8_t>(source), effect.elements};
        // C2/344E records the attack, then skips effects on the possessor.
        // SELF and reflected attacks can reach it despite ordinary targeting.
        if (next.zinger && next.zinger->attacker == target.slot) {
            target.hit = target.display_miss = false;
            return;
        }
        auto& status = next.status.targets[target.slot];
        auto& stats = next.stats[target.slot];
        // Tapir C2/3CA2 only cures a sleeping target. An awake target misses
        // before any status removal; sleeping targets recover both vitals.
        if (effect.special == 23) {
            if (!has(next, target.slot, S::sleep)) { target.hit = false; return; }
            stats.hp.current = stats.hp.maximum;
            stats.mp.current = stats.mp.maximum;
            status.updates.set(static_cast<unsigned>(BattleStatusUpdate::spells));
        }
        const auto mode = effect.behavior & 12U;
        auto apply = mode == 4 ? 0U : effect.status & ~status.active;
        auto remove = mode ? effect.status & status.active : 0U;
        if (!item)
            remove |= (status.active & status_mask(S::vanish)) & ~effect.status;
        const bool undead_revive =
            (stats.monster_flags & 128U) && (effect.flags & 8U) && (effect.behavior & 4U) &&
            (effect.status & (status_mask(S::dead) | status_mask(S::zombie)));
        if (undead_revive)
            apply |= status_mask(S::dead);
        if (effect.elements & 1U)
            remove |= (status.active & status_mask(S::frozen)) & ~effect.status;
        status.pending.apply |= apply;
        status.pending.remove |= remove;
        if ((effect.status_only & 1U) && !((apply | remove) & ~stats.blocked_status)) {
            target.hit = false;
            if (effect.special != 40 && effect.special != 59)
                return;
        }
        if (effect.special == 83) {
            require(extended && source < 4, "Control requires character ownership and monster commands");
            constexpr auto blocked = status_mask(S::dead) | status_mask(S::petrify) |
                status_mask(S::vanish) | status_mask(S::zombie) | status_mask(S::sleep) |
                status_mask(S::confuse) | status_mask(S::berserk) | status_mask(S::rage) |
                status_mask(S::hide) | status_mask(S::morph);
            if (target.slot < 4 || next.monster_information[target.slot - 4].cannot_control ||
                (status.active & blocked) || next.links[target.slot].controlled_by) {
                target.hit = false;
                result.messages.push_back({4, {}});
                return;
            }
            const auto chance = battle_sketch_threshold(next.stats[source].level, stats.level,
                (next.stats[source].relic_effects[2] & 8U) != 0);
            if (chance < 256 && next.random.next(data.random) >= chance) {
                target.hit = false; // The random miss has no Control-failure message.
                return;
            }
            next.links[source].controls = BattleControlLink{target.slot, false};
            next.links[target.slot].controlled_by = BattleControlLink{static_cast<std::uint8_t>(source), false};
            next.links[source].control_menu = true;
            next.status.targets[source].pending.apply |= status_mask(S::control);
            status.reversed_facing = true;
            status.wake_action_pending = true; // Shared AfterAction owner blocks monster turns.
            refresh_battle_control_commands(next, *extended);
        }
        if (effect.special == 40 || effect.special == 59) {
            // The random-ailment effect first chooses a status word weighted by
            // its bit count, then chooses a bit inside that word. Preserve both
            // draws, even with a single candidate. ResetStatusMod replaces all
            // ordinary modifications, including Vanish removal; immunity is
            // applied afterward, without rerolling a protected/already-set bit.
            status.pending.apply = status.pending.remove = 0;
            if (effect.special == 59)
                status.pending.apply = random_ailment(effect.status, next, data);
            else
                // C2/3BB0 resets once per target and accumulates each of its
                // assigned rolls. Repeated ailments collapse into the same bit.
                for (const auto recipient : ailment_recipients)
                    if (recipient & target_bit(target.slot))
                        status.pending.apply |= random_ailment(effect.status, next, data);
            if (!target.hit)
                return; // Target effects still run after the ordinary status-miss decision.
        }
        if (effect.special == 63 && next.random.next(data.random) >= effect.hit_rate) {
            // C2/3D85: damage still lands when Stunner's independent Stop roll
            // fails. ResetStatusMod also cancels the usual Vanish removal.
            status.pending.apply = status.pending.remove = 0;
        }
        // C2/3D7C stores Phantasm separately from ordinary Sap. The shared
        // periodic owner reads either condition, but curing Sap must not cure
        // Phantasm; death/petrification clear it in the status owner.
        if (effect.special == 62)
            status.phantasm = true;
        if (effect.special == 55)
            status.overcast = true; // C2/3D1E; death conversion belongs to UpdateStatus.
        if (effect.special == 64)
            stats.hp.current = 1; // C2/3DB0 writes HP directly, without a damage numeral.
        if (effect.special == 47)
            result.preferred_targets[source] = target.slot; // C2/3DA8; AI selector 4D consumes it.
        if (effect.special == 33) {
            // C2/3C04 exchanges differing bits of both status words. Assignment
            // deliberately replaces the usual Vanish-removal request. Immunity,
            // timers, Interceptor and other side effects remain with UpdateStatus.
            auto& caster = next.status.targets[source];
            const auto from_caster = caster.active & ~status.active;
            const auto from_target = status.active & ~caster.active;
            status.pending = {from_caster, from_target};
            caster.pending = {from_target, from_caster};
        }
        if (effect.special == 18) {
            require(extended != nullptr, "Metamorph needs its extracted item table");
            if (target.slot < 4) { target.hit = false; return; }
            // C2/3A3C chooses the item before its independent success roll.
            const auto group = stats.metamorph & 31U;
            unsigned choice = (next.random.next(data.random) & 1U) << 1U;
            choice |= next.random.next(data.random) & 1U;
            const auto obtained = extended->metamorph_items[group][choice];
            result.messages.push_back({29, {obtained, 0, 0, 0}});
            constexpr unsigned rates[] = {
#include "rom-materials/metamorph_rates.inc"
            };
            if (next.random.next(data.random) >= rates[stats.metamorph >> 5U]) {
                target.hit = false;
                return;
            }
            result.obtained_items.push_back(obtained);
            status.pending.apply |= status_mask(S::dead);
        }
        if (effect.special == 19) {
            // The authored Hide status is a temporary Jump, not a departure.
            status.pending.apply &= ~status_mask(S::hide);
            result.jumping.set(target.slot);
        }
        if (effect.special == 31)
            stats.level = static_cast<std::uint8_t>(wrap8(unsigned{stats.level} + 1U) >> 1U);
        if (effect.special == 38) {
            // C2/3989: choose one weakness, nullify its complement, then choose
            // one absorbed element from that complement. Absorption takes
            // precedence over nullification. Half damage and status immunity
            // are deliberately retained; this is not equipment reinitialization.
            stats.weak = static_cast<std::uint8_t>(random_set_bit(255, next, data));
            stats.nullified = static_cast<std::uint8_t>(stats.weak ^ 255U);
            stats.absorbed = static_cast<std::uint8_t>(random_set_bit(stats.nullified, next, data));
        }
        // C2/3980 changes combat row immediately; the authored animation moves
        // character pictures later. Monsters retain the same defensive row bit.
        if (effect.special == 43) {
            if (target.slot < 4) next.entry.back_rows.flip(target.slot);
            else next.entry.monster_back_rows.flip(target.slot - 4);
        }
        if (effect.special == 58) {
            // C2/3D27: Zinger retains both owners and removes only the
            // possessor from targeting. It is neither dead nor departed;
            // the victim keeps taking turns. Death/petrification/zombie
            // release this link through the shared AfterAction owner.
            next.zinger = BattleZingerLink{static_cast<std::uint8_t>(source), target.slot};
            next.untargetable.set(source);
        }
        if (effect.special == 45) {
            next.links[source].love_target = target.slot;
            next.links[target.slot].love_attacker = static_cast<std::uint8_t>(source);
        }
        if (effect.special == 46 && target.slot < 4 && !next.links[source].seize_target) {
            next.links[source].seize_target = target.slot;
            next.links[target.slot].seize_attacker = static_cast<std::uint8_t>(source);
            status.menu_allowed = false;
            status.updates.set(static_cast<unsigned>(BattleStatusUpdate::remove_advance_actions));
            result.seize_started.set(source);
        }
        if (effect.special == 68) {
            next.links[source].seize_target.reset();
            next.links[target.slot].seize_attacker.reset();
            result.seize_released.set(source);
        }
        if (effect.special == 52) {
            if (next.links[source].charm_target) {
                target.hit = false; // C2/3C91 branches to the ordinary miss handler.
                return;
            }
            next.links[source].charm_target = target.slot;
            next.links[target.slot].charm_attacker = static_cast<std::uint8_t>(source);
        }
        if (effect.special == 32) {
            next.stats[source].hp.current = next.stats[source].mp.current = 0;
            next.status.targets[source].pending.apply |= status_mask(S::dead) | status_mask(S::hide);
            next.untargetable.set(source);
            result.departures.set(source);
            stats.mp.current = stats.mp.maximum;
        }
        if (effect.special == 84) {
            if (!next.leap_allowed || next.participation.ally_count < 2) {
                target.hit = false;
                result.messages.push_back({5, {}});
                return;
            }
            next.status.targets[attacker].pending.apply |= status_mask(S::hide);
            next.end_event = 4;
        }
        if (effect.special == 37 && has(next, target.slot, S::floating)) {
            target.hit = false;
            return;
        }
        if (effect.special == 35)
            result.suppress_retaliation = true;
        if (effect.special == 39 || effect.special == 51 || effect.special == 56 || effect.special == 57 || effect.special == 75) {
            // C2/3966 falls through to the shared Escape/Sneeze removal handler.
            // Sneeze is allowed even when ordinary running away is forbidden.
            // Remove this target without killing it. Retain
            // vitals/status for rewards and battle return; the authored spell
            // owns its departure picture, not the ordinary monster death fade.
            next.untargetable.set(target.slot);
            if (target.slot < 4)
                next.departed_characters.set(target.slot);
            else
                next.departed_monsters.set(target.slot - 4);
            result.departures.set(target.slot);
            if (effect.special == 51 && target.slot < 4)
                next.removed_from_party.set(target.slot);
            // C2/395E also records Engulf separately from shared departure.
            // The battle-ending owner tests the complete original party mask.
            if (effect.special == 57 && target.slot < 4)
                next.engulfed_characters.set(target.slot);
        }
        if (effect.special == 16) {
            if (target.slot >= 4 && next.monster_information[target.slot - 4].cannot_scan)
                result.messages.push_back({44, {}});
            else {
                result.messages.push_back({52, {stats.level}});
                result.messages.push_back({48, {stats.hp.current, stats.hp.maximum}});
                if (stats.mp.maximum)
                    result.messages.push_back({49, {stats.mp.current, stats.mp.maximum}});
                const unsigned weak = unsigned{stats.weak} &
                                      ~(unsigned{stats.halved} | stats.absorbed | stats.nullified);
                for (unsigned bit = 0; bit < 8; ++bit)
                    if (weak & (1U << bit))
                        result.messages.push_back({static_cast<std::uint8_t>(21 + bit), {}});
            }
        }
        if (effect.special == 53 && (stats.monster_flags & 128U)) {
            if (target.slot < 4) {
                status.pending.apply &= ~status_mask(S::dead);
                stats.hp.current = stats.hp.maximum;
            } else {
                // The original queues MATERIALIZE after applying Death. Keep
                // that intent separate from the ordinary death presentation.
                status.pending.apply |= status_mask(S::hide);
                result.rematerialize.set(target.slot - 4);
            }
        }
        if (item && effect.special == 76) {
            // C2/3D17 requests this even when MP was already full.
            status.updates.set(static_cast<unsigned>(BattleStatusUpdate::spells));
            stats.mp.current = stats.mp.maximum;
        }
        if (effect.power && !undead_revive) {
            target.display_amount = true;
            target.amount = target_damage(next, source, target.slot, effect, power, data,
                                          target.healing, item || result.runic);
            if (effect.special == 74) {
                // C2/3DB8 overwrites damage after ordinary modifiers. Each
                // projectile draws independently, in units of 256 HP.
                target.amount = static_cast<std::uint16_t>(((next.random.next(data.random) & unsigned{FF6_ROM_VALUE_MAGIC_BALL_DAMAGE_MASK_0}) + 1U) * 256U);
                target.healing = false;
            }
            if (effect.behavior & 2U) {
                const auto victim = target.healing ? source : target.slot;
                const auto recipient = target.healing ? target.slot : source;
                const auto vital = [&](unsigned i) {
                    return (effect.options & 128U) ? next.stats[i].mp : next.stats[i].hp;
                };
                target.amount = std::min(target.amount, vital(victim).current);
                if (!seize_drain) {
                    // Ordinary casts cap both ends; Seize has a separate
                    // caller that clears the original drain-cap flag.
                    const auto v = vital(recipient);
                    target.amount =
                        std::min(target.amount, static_cast<std::uint16_t>(v.maximum - v.current));
                }
                accumulate(target.healing ? damage : healing, source, target.amount);
                accumulate(target.healing ? drain_damage : drain_healing, source, target.amount);
                auto& returned = next.attack_memory[source];
                if (!returned.retaliation_pending) {
                    returned.retaliation_attacker = (target.healing && target.amount)
                                                        ? static_cast<std::uint8_t>(source)
                                                        : target.slot;
                    // Restoration records provenance; an inverted drain also
                    // damages this actor and requests retaliation in ExecDmg.
                    returned.retaliation_pending = target.healing && target.amount != 0;
                }
            }
            accumulate(target.healing ? healing : damage, target.slot, target.amount);
        }
    };
    const bool projectile_volley = effect.special == 44 || effect.special == 74;
    const unsigned projectile_count = effect.special == 74
        ? (next.random.next(data.random) & unsigned{FF6_ROM_VALUE_MAGIC_BALL_COUNT_MASK_0}) + 1U : FF6_ROM_VALUE_MAGIC_LAUNCHER_ADDITIONAL_0 + 1U;
    std::vector<unsigned> reflectors;
    // All direct hit rolls precede every direct effect/damage roll.
    for (unsigned remaining = projectile_volley ? 0U : 10U; remaining; --remaining) {
        const auto slot = remaining - 1;
        if (!(targets & target_bit(slot)))
            continue;
        const auto hp = next.stats[slot].hp.current;
        const auto status = next.status.targets[slot].active;
        const bool reflected = !force_miss && !(effect.options & 2U) &&
                               has(next, slot, S::reflect) && (item || !has(next, slot, S::vanish));
        const bool hit = !force_miss && !reflected && !(random_misses & target_bit(slot)) &&
                         check_hit(next, slot, effect, data, item) &&
                         // C2/42B7 removes non-Reflect targets from the hit
                         // mask. Reflect??? retains its ordinary status data.
                         (effect.special != 28 || has(next, slot, S::reflect));
        result.targets.push_back(
            {static_cast<std::uint8_t>(slot), hit, false, 0, hp, hp, status, status, reflected});
        result.targets.back().display_miss = !(force_miss &&
            (effect.special == 24 || effect.special == 75 || effect.special == 77));
        if (reflected)
            reflectors.push_back(slot);
    }
    for (auto& target : result.targets)
        apply_target(target, attacker, base);
    apply_vitals();
    // LearnLore (C2/36D6) follows direct damage, before UpdateStatus and the
    // reflected pass. Strago need not be targeted or hit. Check the published
    // status, not this attack's pending ailments (including a pending death).
    // Runic has already replaced the effect and cleared its learnable flag.
    if ((effect.options & 4U) && command.attack >= 139 && command.attack <= 162) {
        constexpr auto blocked = status_mask(S::dead) | status_mask(S::petrify) |
            status_mask(S::blind) | status_mask(S::zombie) | status_mask(S::sleep) |
            status_mask(S::confuse) | status_mask(S::berserk) | status_mask(S::stop) |
            status_mask(S::frozen) | status_mask(S::rage) | status_mask(S::hide);
        for (unsigned slot = 0; slot < 4; ++slot)
            if (next.actors[slot] == 7 && next.status.targets[slot].present &&
                !(next.status.targets[slot].active & blocked))
                next.observed_lores.set(command.attack - 139U);
    }
    damage = {};
    healing = {};
    // C2/402C–4039 and C2/3483: Launcher and Super Ball share the deferred
    // damage pass with independently drawn targets. Keep HP unchanged until all
    // projectiles resolve: fractional damage uses each target's pre-volley HP.
    const auto deferred_count = projectile_volley ? std::size_t{projectile_count} : reflectors.size();
    for (std::size_t projectile = 0; projectile < deferred_count; ++projectile) {
        const auto source = projectile_volley ? unsigned{attacker} : reflectors[projectile];
        BattleAttackTargetRules rules;
        rules.targeting = 64;
        rules.policy.force_retarget = true;
        rules.policy.reflected = !projectile_volley;
        rules.policy.prefer_previous_attacker =
            attacker < 4 ? !(targets & 15U) : (targets & 15U) != 0;
        BattleTargetEnvironment environment;
        environment.previous_attacker = attacker;
        environment.selectable =
            next.participation.targetable & next.participation.active & ~next.away_from_battle;
        for (unsigned i = 0; i < 10; ++i)
            if (next.links[i].seize_attacker)
                environment.selectable.reset(i);
        const auto mask = choose_battle_targets(next, static_cast<std::uint8_t>(source), 0, rules,
                                                environment, data.random, next.random);
        if (!mask)
            continue;
        unsigned slot = 0;
        while (!(mask & target_bit(slot)))
            ++slot;
        BattleAttackTargetResult target{static_cast<std::uint8_t>(slot),
                                        check_hit(next, slot, effect, data, item),
                                        false,
                                        0,
                                        next.stats[slot].hp.current,
                                        next.stats[slot].hp.current,
                                        next.status.targets[slot].active,
                                        next.status.targets[slot].active};
        apply_target(target, source, static_cast<std::uint16_t>(base / 2U));
        if (projectile_volley) {
            // The original counter descends, while graphics visit ascending
            // logical owners. Preserve that order independently of recipients.
            const auto owner = projectile_count - 1U - projectile;
            result.projectile_targets[owner] = static_cast<std::uint8_t>(slot);
            result.projectile_hits[owner] = target.hit;
        } else
            result.reflections.push_back(
                {static_cast<std::uint8_t>(source), static_cast<std::uint8_t>(slot), target.hit});
        result.targets.push_back(target);
    }
    apply_vitals();
    if (effect.options & 64U) {
        next.status.targets[attacker].pending.apply |= status_mask(S::dead);
    }
    std::array<BattleStatusContext, 10> context{};
    for (unsigned i = 0; i < 10; ++i)
        context[i] = {next.stats[i].hp, next.stats[i].blocked_status};
    update_battle_status(next.status, context);
    for (auto& target : result.targets) {
        if (next.invincible[target.slot] && target.display_amount && !target.healing)
            target.amount = 0;
        target.miss_notice =
            miss_notice || (!target.hit && target.display_miss && !target.reflected);
        target.hp_after = next.stats[target.slot].hp.current;
        target.status_after = next.status.targets[target.slot].active;
    }
    for (unsigned i = 0; i < 10; ++i) {
        if (!drain_damage[i] && !drain_healing[i])
            continue;
        if (next.invincible[i] && drain_damage[i]) drain_damage[i] = 0;
        result.drain_returns.push_back(
            {static_cast<std::uint8_t>(i), true, drain_healing[i].has_value(),
             drain_healing[i].value_or(drain_damage[i].value_or(0)), state.stats[i].hp.current,
             next.stats[i].hp.current, state.status.targets[i].active,
             next.status.targets[i].active, false, true});
    }
    if ((effect.special == 21 && (command.parameters & target_bit(attacker))) ||
        (effect.special == 22 && result.targets.empty())) {
        // Mantra clears only the caster's hit bit (C2/4263); keep its authored
        // animation target without a heal or miss numeral. A solo Spiraler also
        // needs a presentation recipient so its disappearance can finish.
        result.targets.push_back({static_cast<std::uint8_t>(attacker), false, false, 0,
                                  state.stats[attacker].hp.current, next.stats[attacker].hp.current,
                                  state.status.targets[attacker].active, next.status.targets[attacker].active,
                                  false, false, false});
    }
    state = std::move(next);
    return result;
}
} // namespace
BattleAttackResult resolve_battle_magitek(BattleState& state, std::uint8_t attacker,
                                          BattleQueuedCommand command,
                                          const BattleMagitekData& data) {
    require(attacker < 4 && command.command == 29 && command.attack >= 131 &&
                command.attack <= 138 && command.mp_cost == 0,
            "Unsupported queued Magitek command");
    return resolve_magic(state, attacker, command, data.effects[command.attack - 131], data);
}
BattleMagicData load_battle_magic(const ContentPack& pack) {
    BattleMagicData data;
    const auto raw = pack.get("data_c46ac0_magic_prop");
    require(raw.size() == 256 * 14, "Invalid magic property resource");
    for (unsigned i = 0; i < 256; ++i) {
        const auto p = raw.subspan(i * 14, 14);
        data.effects[i] = {p[0], p[1], p[2], p[3],          p[4], p[6],
                           p[7], p[8], p[9], read32(p, 10), p[5]};
    }
    data.random = load_battle_magitek(pack).random;
    const auto metamorph = pack.get("data_c47f40_metamorph_prop");
    require(metamorph.size() == 128, "Invalid Metamorph item table");
    for (unsigned i = 0; i < 32; ++i)
        std::ranges::copy(metamorph.subspan(i * 4, 4), data.metamorph_items[i].begin());
    const auto control = pack.get("data_cf3d00_monster_control");
    require(control.size() == data.control_attacks.size() * 4, "Invalid Control attack table");
    for (unsigned i = 0; i < data.control_attacks.size(); ++i)
        std::ranges::copy(control.subspan(i * 4, 4), data.control_attacks[i].begin());
    data.inventory = load_battle_inventory(pack);
    return data;
}
void refresh_battle_control_commands(BattleState& state, const BattleMagicData& data) {
    for (unsigned slot = 0; slot < 4; ++slot) {
        state.control_commands[slot] = {};
        const auto link = state.links[slot].controls;
        if (!link || link->remove_pending) continue;
        require(link->target >= 4 && link->target < 10, "Invalid controlled monster");
        const auto id = state.monster_information[link->target - 4].identity;
        require(id < data.control_attacks.size(), "Invalid Control monster identity");
        // SetControlCmd compares against MP+1 as a word; no character relic discount.
        const auto available = wrap16(unsigned{state.stats[link->target].mp.current} + 1U);
        for (unsigned i = 0; i < 4; ++i) {
            const auto attack = data.control_attacks[id][i];
            if (attack == 255) continue;
            const auto& effect = data.effects[attack];
            state.control_commands[slot][i] = BattleSpell{attack, effect.targeting,
                effect.original_mp_cost, effect.original_mp_cost >= available};
        }
    }
}
BattleAttackResult resolve_battle_control(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && command.command == 14 && !command.mp_cost, "Invalid Control attempt");
    BattleMagicEffect effect;
    effect.options = 34; effect.behavior = 32; effect.special = 83;
    BattleMagitekData shared; shared.random = data.random;
    return resolve_magic(state, attacker, command, effect, shared, false, {}, 0, &data);
}
BattleAttackResult resolve_battle_gil_toss(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 10 && command.command == 24 && !command.mp_cost, "Invalid GP Rain command");
    BattleMagicEffect effect;
    effect.targeting = 8; effect.flags = 96; effect.options = 34;
    effect.behavior = 32; effect.power = 1; effect.special = 81;
    BattleMagitekData shared; shared.random = data.random;
    return resolve_magic(state, attacker, command, effect, shared);
}
BattleAttackResult resolve_battle_monster_magic(BattleState& state, std::uint8_t attacker,
                                                BattleQueuedCommand command,
                                                const BattleMagicData& data) {
    require(attacker >= 4 && attacker < 10 &&
                (command.command == 2 || command.command == 12 || command.command == 7),
            "Unsupported monster magic command");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[command.attack], shared);
}
BattleAttackResult resolve_battle_immediate_spell(BattleState& state, std::uint8_t target,
    std::uint8_t attack, const BattleMagicData& data) {
    require(target < 10 && attack < data.effects.size(), "Invalid immediate spell");
    // C2/4F5F-4F96: Life 3's Raise and relic spells share this self-attack with Doom.
    // Only Doom retires Condemned and checks whether enemies remain. All use
    // the original effect with free MP, guaranteed hit and no Reflect/Runic;
    // ordinary status immunity/death protection still belongs to that effect.
    if (attack == 13) {
        state.turns[target].condemned = 0;
        if (!state.participation.enemy_count)
            return {target, attack, {}, false};
    }
    if (!state.status.targets[target].present || !state.participation.targetable[target])
        return {target, attack, {}, false};
    auto effect = data.effects[attack];
    effect.options = 2;
    effect.behavior |= 32;
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, target, {2, attack, target_bit(target), 0}, effect, shared,
                         false, {}, 0, &data, true);
}
BattleAttackResult resolve_battle_doom(BattleState& state, std::uint8_t target,
                                      const BattleMagicData& data) {
    return resolve_battle_immediate_spell(state, target, 13, data);
}
BattleAttackResult resolve_battle_umaro_storm(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && state.actors[attacker] == 13 && command.command == 2 &&
                command.attack == 84 && !command.mp_cost, "Invalid Blizzard Orb attack");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[84], shared, false, {}, 0, &data);
}
BattleAttackResult resolve_battle_dance_magic(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && command.command == 19 && !command.mp_cost,
            "Invalid Dance attack");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[command.attack], shared);
}
BattleAttackResult resolve_battle_rage_magic(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && !command.mp_cost, "Invalid Rage spell");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[command.attack], shared);
}
BattleAttackResult resolve_battle_sketch_magic(BattleState& state, std::uint8_t attacker,
    std::uint8_t source, BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && source >= 4 && source < 10 && !command.mp_cost,
            "Invalid Sketch spell ownership");
    BattleMagitekData shared;
    shared.random = data.random;
    const auto& effect = data.effects[command.attack];
    // InitAttacker reads the monster's magic; InitAttackerLevel substitutes the
    // sketcher's level. ExecAttack restores the character before target effects.
    return resolve_magic(state, attacker, command, effect, shared, false,
        battle_magic_base(effect.power, state.stats[source].magic_power, state.stats[attacker].level),
        0, &data);
}
BattleAttackResult resolve_battle_morph(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagitekData& data) {
    require(attacker < 4 && (command.command == 3 || command.command == 4),
            "Invalid Morph/Revert command");
    // An expiry may already have reverted Terra while this command waited.
    // The original then resets her action without playing another effect.
    if (command.command == 4 && !has(state, attacker, S::morph))
        return {attacker, command.attack, {}, false};
    BattleMagicEffect effect;
    effect.options = 34; // Simple magic cannot Reflect or Runic.
    effect.behavior = command.command == 3 ? 32 : 36; // Unblockable; apply/remove.
    effect.status = status_mask(S::morph);
    command.parameters = static_cast<std::uint16_t>(1U << attacker);
    command.mp_cost = 0;
    return resolve_magic(state, attacker, command, effect, data);
}
BattleAttackResult resolve_battle_leap(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagitekData& data) {
    require(attacker < 4 && command.command == 17, "Invalid Leap command");
    BattleMagicEffect effect;
    effect.targeting = 0x40;
    effect.options = 34;
    effect.behavior = 32;
    effect.special = 84;
    return resolve_magic(state, attacker, command, effect, data);
}
BattleAttackResult resolve_battle_slot(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && command.command == 15 && !command.mp_cost,
            "Invalid Slot command");
    auto effect = data.effects[command.attack];
    effect.original_mp_cost = 0; // C2/1778: also determines any Runic recovery.
    if (command.attack == 148) {
        // ShowAttackName_07 C2/36A6 changes the effect before execution. Joker
        // ignores death protection and the level gate; ordinary Lore does not.
        effect.flags &= ~2U;
        effect.behavior |= 32;
    }
    BattleMagitekData shared;
    shared.random = data.random;
    auto next = state;
    auto result = resolve_magic(next, attacker, command, effect, shared, false, {}, 0, &data);
    for (const auto item : result.obtained_items) queue_battle_item(next.inventory, data.inventory, item);
    if (command.attack == 254) result.messages.push_back({7, {}}); // C2/173B: Slot failure.
    state = std::move(next);
    return result;
}
BattleAttackResult resolve_battle_summon(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && command.command == 25 && command.attack >= 54 && command.attack < 81,
            "Invalid summon command");
    BattleMagitekData shared;
    shared.random = data.random;
    auto next = state;
    auto result = resolve_magic(next, attacker, command, data.effects[command.attack], shared,
                                false, {}, 0, &data);
    for (const auto item : result.obtained_items) queue_battle_item(next.inventory, data.inventory, item);
    state = std::move(next);
    return result;
}
BattleAttackResult resolve_battle_item_summon(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data) {
    require(command.command == 25 && command.attack >= 54 && command.attack < 81 && !command.mp_cost,
            "Invalid item summon continuation");
    // C2/37EB reloads the chosen spell, zeroes its cost and suppresses Reflect
    // and the hit-rate gate. This bypasses player Summon submission, preserving
    // equipped-Esper availability. The ordinary effect owner still handles
    // immunity, Ragnarok rewards, Golem and Palidor's queued jumps.
    auto effect = data.effects[command.attack];
    effect.original_mp_cost = 0;
    effect.options |= 2;
    effect.behavior |= 32;
    BattleMagitekData shared;
    shared.random = data.random;
    auto next = state;
    auto result = resolve_magic(next, attacker, command, effect, shared, false, {}, 0, &data);
    for (const auto id : result.obtained_items) queue_battle_item(next.inventory, data.inventory, id);
    state = std::move(next);
    return result;
}
BattleAttackResult resolve_battle_weapon_magic(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data, bool same_target) {
    require(attacker < 4 && command.command == 2 && !command.mp_cost,
            "Invalid free weapon spell");
    auto effect = data.effects[command.attack];
    effect.original_mp_cost = 0;
    effect.options |= 2; // C2/3832: weapon spells cannot reflect.
    if (same_target) {
        effect.options &= ~12U; // No Runic or split damage on the retained weapon target.
        if (!(effect.behavior & 16U)) effect.behavior |= 32; // Keep stamina-based exceptions.
    } else effect.behavior |= 32; // Tempest uses the spell's ordinary area target policy.
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, effect, shared);
}
BattleAttackResult resolve_battle_character_magic(BattleState& state, std::uint8_t attacker,
                                                  BattleQueuedCommand command,
                                                  const BattleMagicData& data) {
    const bool magic = (command.command == 2 || command.command == 23) &&
                       (command.attack < 54 || (command.attack >= 240 && command.attack <= 253));
    const bool lore = command.command == 12 && command.attack >= 139 && command.attack <= 162;
    // C2/175F joins the ordinary spell effect owner for Lore. Character slots
    // may be controlled by enemy scripts too; those casts still pay their
    // prepared MP cost and retain the original command for retaliation.
    require(attacker < 4 && (magic || lore), "Invalid character spell command");
    BattleMagitekData shared;
    shared.random = data.random;
    auto next = state;
    auto effect = data.effects[command.attack];
    // C2/1741: the authored flag is consumed by Terra's first Magic command,
    // regardless of target. Its spell cannot be evaded; the reaction is queued
    // after the attack, not substituted for it or tied to a particular map.
    if (magic && !(command.attack >= 240 && command.attack <= 251) &&
        next.actors[attacker] == 0 && next.terra_magic_pending) {
        append_battle_immediate_action(next, BattleScriptedEventAction{6});
        next.terra_magic_pending = false;
        effect.behavior |= 32;
    }
    auto result = resolve_magic(next, attacker, command, effect, shared);
    state = std::move(next);
    return result;
}
BattleAttackResult resolve_battle_bushido_magic(BattleState& state, std::uint8_t attacker,
    BattleQueuedCommand command, const BattleMagicData& data, bool mp_pass, bool retort_stance) {
    require(attacker < 4 && command.command == 7 && command.attack >= 85 && command.attack <= 92,
            "Invalid magical SwdTech command");
    auto effect = data.effects[command.attack];
    if (retort_stance) {
        require(command.attack == 86 && !mp_pass, "Retort stance requires its own effect");
        // Arming still executes the zero-power attack: it records targeting
        // for enemy reactions, without a damage numeral or strike animation.
        effect.power = 0;
        effect.behavior |= 32U;
        effect.flags &= 254U;
    }
    if (effect.special == 54) {
        // C2/421B toggles HP/MP each pass and quarters power only once. The
        // second pass suppresses its body/name but still dispatches numerals.
        effect.power >>= 2U;
        effect.options = static_cast<std::uint8_t>((effect.options & 127U) | (mp_pass ? 128U : 0U));
        effect.special = 255;
    } else require(!mp_pass, "MP continuation requires a paired drain");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, effect, shared);
}
BattleAttackResult resolve_battle_blitz_magic(BattleState& state, std::uint8_t attacker,
                                              BattleQueuedCommand command,
                                              const BattleMagicData& data) {
    require(attacker < 4 && command.command == 10 && command.attack >= 93 && command.attack <= 100,
            "Invalid magical Blitz command");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[command.attack], shared);
}
BattleAttackResult resolve_battle_innate_magic(BattleState& state, std::uint8_t attacker,
                                               BattleQueuedCommand command, const BattleMagicData& data) {
    require(attacker < 4 && (command.command == 26 || command.command == 27) && command.mp_cost == 0,
            "Invalid innate magic command");
    // C2/171A..1725: both commands replace the unused queued attack byte
    // before joining the ordinary spell-effect path, preserving command identity.
    command.attack = command.command == 26 ? 46 : 130;
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[command.attack], shared);
}
BattleItemEffectData load_battle_item_effects(const ContentPack& pack) {
    const auto raw = pack.get("data_d85000_item_prop");
    const auto animation = pack.get("data_d10000_item_animation_properties");
    require(raw.size() == 256 * 30 && animation.size() == 321, "Invalid item effect data");
    BattleItemEffectData data;
    data.random = load_battle_magitek(pack).random;
    const auto targeting = load_battle_action_preparation(pack);
    for (unsigned i = 0; i < 8; ++i) {
        const auto p = raw.subspan((163 + i) * 30, 30);
        auto& effect = data.tools[i];
        effect.targeting = p[14];
        effect.elements = p[15];
        effect.power = p[20];
        effect.hit_rate = p[21];
        effect.flags = i == 5 ? 33 : i == 7 ? 65 : 1;
        effect.options = 34;
        effect.behavior = 0;
        effect.special = 0;
        if (i == 0)
            effect.status = status_mask(S::confuse);
        if (i == 4)
            effect.special = 86;
        if (i == 6)
            effect.special = 87;
        data.tool_spells[i] =
            battle_attack_target_rules(targeting, 9, static_cast<std::uint8_t>(163 + i)).spell;
    }
    for (unsigned i = 0; i < 256; ++i) {
        const auto p = raw.subspan(i * 30, 30);
        // C2/188D and CalcItemEffect: a physical throw uses the item's power
        // and element, ignoring weapon procs, defense and evasion. Its separate
        // two-half-step multiplier belongs to the physical damage owner.
        auto& thrown = data.thrown[i];
        thrown = {p[14], p[15], 33, 34, 32, p[20], 0, 0, 255, 0};
        // CalcItemEffect's two defensive scrolls retain the physical,
        // zero-power effect and apply their status through the shared pass.
        if (i == 174) thrown.status = status_mask(BattleStatus::vanish);
        if (i == 175) thrown.status = status_mask(BattleStatus::image);
        data.throw_spells[i] = battle_attack_target_rules(targeting, 8,
                                                         static_cast<std::uint8_t>(i)).spell;
        if (i < 230 && (p[0] & 32U)) {
            const auto rules = battle_attack_target_rules(targeting, 1, static_cast<std::uint8_t>(i));
            data.item_spells[i] = rules.spell;
            data.reusable_items[i] = !rules.break_item;
        }
        const auto picture = animation[64U + wrap8(i + 1U)];
        // C1/BA4E selects the Jump family from the same item's high nibble.
        const auto jump = (picture & 127U) >> 4U;
        data.jump_animations[i] = static_cast<std::uint16_t>(jump ? 303U + jump : 279U);
        data.throw_weapon_animation[i] = (picture & 128U) != 0;
        data.throw_animations[i] = data.throw_weapon_animation[i]
            ? static_cast<std::uint16_t>(wrap8(i + 1U))
            : static_cast<std::uint16_t>(312U + (picture & 15U));
        auto& e = data.effects[i];
        // CalcItemEffect constructs a magical effect with level zero, always
        // hit and no variance/defense modifiers. Keep the original bit meanings.
        e.targeting = p[14];
        e.elements = p[15];
        e.power = p[20];
        e.flags = 32 | ((p[19] & 2U) ? 8U : 0U);
        e.options = 34 | ((p[19] & 16U) ? 128U : 0U);
        e.behavior = 32 | (p[19] & 128U) | ((p[19] & 32U) ? 4U : 0U) | ((p[19] & 24U) ? 1U : 0U);
        e.status = read32(p, 21);
        if (e.status & status_mask(S::dead))
            e.flags |= 12;
        e.special = p[27] & 128U ? 255 : static_cast<std::uint8_t>(72 + p[27]);
        if (e.special == 76)
            e.options &= 127; // Elixir's target effect restores MP separately.
    }
    for (unsigned i = 0; i < 32; ++i) {
        const auto offset = read16(animation, i * 2);
        if (offset == 65535)
            continue;
        require(offset % 14 == 0, "Invalid item animation identity");
        data.animations[i] = offset / 14;
    }
    return data;
}
namespace {
BattleAttackResult resolve_spell_item(BattleState& state, std::uint8_t attacker,
                                             BattleQueuedCommand command,
                                             const BattleMagicData& data, std::uint8_t spell) {
    require(attacker < 10 && (command.command == 1 || command.command == 8 || command.command == 9) && !command.mp_cost,
            "Invalid spell-item command");
    const auto item = command.attack;
    // C2/18E3 changes Item to Magic and substitutes the spell identity before
    // execution. Throw/Tools retain their command; all retain item AI memory.
    if (command.command == 1) {
        command.command = 2;
        command.attack = spell;
    }
    auto effect = data.effects[spell];
    // Tools' spell branch ignores Reflect, cannot be dodged, and costs no MP.
    // Excluding character targets is owned by target selection, not hit effects.
    effect.flags &= 127U;
    effect.options |= 2;
    effect.behavior |= 32;
    effect.original_mp_cost = 0;
    BattleMagitekData shared;
    shared.random = data.random;
    auto result = resolve_magic(state, attacker, command, effect, shared);
    for (const auto& target : result.targets)
        if (target.hit) {
            state.attack_memory[target.slot].attack = BattleObservedAttackValue{attacker, spell};
            state.attack_memory[target.slot].item = BattleObservedAttackValue{attacker, item};
        }
    return result;
}
}
BattleAttackResult resolve_battle_tool_magic(BattleState& state, std::uint8_t attacker,
                                             BattleQueuedCommand command,
                                             const BattleMagicData& data, std::uint8_t spell) {
    require(attacker < 4 && command.command == 9, "Invalid magical Tool command");
    return resolve_spell_item(state, attacker, command, data, spell);
}
BattleAttackResult resolve_battle_throw_magic(BattleState& state, std::uint8_t attacker,
                                              BattleQueuedCommand command,
                                              const BattleMagicData& data, std::uint8_t spell) {
    require(command.command == 8, "Invalid magical Throw command");
    return resolve_spell_item(state, attacker, command, data, spell);
}
BattleAttackResult resolve_battle_item_magic(BattleState& state, std::uint8_t attacker,
                                             BattleQueuedCommand command,
                                             const BattleMagicData& data, std::uint8_t spell,
                                             bool reusable) {
    require(command.command == 1, "Invalid spellcasting Item command");
    auto next = state;
    auto result = resolve_spell_item(next, attacker, command, data, spell);
    // C2/273C and C2/18AE retain non-breaking equipment as an obtained item;
    // C2/62C7 returns it after the action through the same publication as Steal.
    // Mimic bypasses item reservation (C2/4DAF). Returning a copy without a
    // reservation would manufacture equipment each time the command is mimicked.
    if (reusable && attacker < 4 && next.selections.items[attacker] == command.attack)
        queue_battle_item(next.inventory, data.inventory, command.attack);
    if (attacker < 4) next.selections.items[attacker] = 255;
    state = std::move(next);
    return result;
}
BattleAttackResult resolve_battle_item(BattleState& state, std::uint8_t attacker,
                                       BattleQueuedCommand command,
                                       const BattleItemEffectData& data) {
    require(attacker < 10 && command.command == 1 && command.attack >= 232 && command.attack < 255 &&
                command.mp_cost == 0,
            "Item spellcasting and special item commands need their native owners");
    BattleMagitekData shared;
    shared.random = data.random;
    return resolve_magic(state, attacker, command, data.effects[command.attack], shared, true);
}
BattleAttackResult resolve_battle_periodic(BattleState& state, std::uint8_t target,
                                          BattleQueuedCommand command,
                                          const BattleMagicData& data) {
    validate_battle_state(state);
    require(target < 10 && command.command == 34 &&
                (command.attack == 2 || command.attack == 4 || command.attack == 64),
            "Invalid periodic status action");
    BattleAttackResult result{target, command.attack, {}, true};
    if (!state.participation.enemy_count) return result;
    auto next = state;
    auto& time = next.time.targets[target];
    time.periodic_pending = false;
    if (next.turns[target].status_actions_blocked) {
        state = std::move(next);
        return result;
    }
    // C2/500B constructs always-hit, defense-ignoring, zero-level magic.
    // Its capped ADC retains carry: the saturation branch produces 255.
    const auto scaled = (std::uint32_t{next.stats[target].hp.maximum} * next.stats[target].stamina) >> 10U;
    const auto power = static_cast<std::uint8_t>(scaled >= 254 ? 255 : scaled + 2);
    unsigned multiplier = 0;
    if (command.attack == 4) {
        multiplier = time.poison_multiplier;
        time.poison_multiplier = static_cast<std::uint8_t>(std::min(multiplier + 2U, unsigned{FF6_ROM_VALUE_MAGIC_POISON_CAP_0}));
    }
    BattleMagicEffect effect;
    effect.targeting = 2;
    effect.elements = command.attack == 4 ? 8 : 0;
    effect.flags = 0x68;
    effect.options = 2;
    effect.behavior = static_cast<std::uint8_t>(32U | (command.attack == 2 ? 1U : 0U));
    effect.power = power;
    BattleMagitekData shared;
    shared.random = data.random;
    result = resolve_magic(next, target, {18, command.attack, target_bit(target), 0},
                           effect, shared, false, power, multiplier, nullptr, true);
    result.suppress_retaliation = true;
    state = std::move(next);
    return result;
}
BattleAttackResult resolve_battle_seize_drain(BattleState& state, std::uint8_t attacker,
                                             BattleQueuedCommand command,
                                             const BattleMagicData& data) {
    validate_battle_state(state);
    require(attacker < 10 && command.command == 45, "Invalid Seize periodic action");
    auto next = state;
    next.time.targets[attacker].periodic_pending = false;
    BattleAttackResult result{attacker, command.attack, {}, true};
    const auto captive = next.links[attacker].seize_target;
    if (next.participation.enemy_count && captive && !next.turns[attacker].status_actions_blocked) {
        // C2/51B2 shares one selected level/power operand, zero magic and uncapped HP drain.
        // Preserve the shared variance, undead inversion and HP application.
        BattleMagicEffect effect;
        effect.targeting = 64;
        effect.flags = 0x28;
        effect.options = 2;
        effect.behavior = 34;
        effect.power = FF6_ROM_VALUE_MAGIC_SEIZE_POWER_0;
        BattleMagitekData shared;
        shared.random = data.random;
        result = resolve_magic(next, attacker, {18, command.attack, target_bit(*captive), 0},
                               effect, shared, false, FF6_ROM_VALUE_MAGIC_SEIZE_POWER_0 * 4U, 0, nullptr, false, true);
        result.suppress_retaliation = true;
    }
    state = std::move(next);
    return result;
}
} // namespace ff6
