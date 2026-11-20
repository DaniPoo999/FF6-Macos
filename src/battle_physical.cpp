#include "ff6/battle_physical.hpp"
#include "rom-materials/physical_desperation_wait.macros.inc"
#include "rom-materials/physical_desperation_lottery.macros.inc"
#include "rom-materials/physical_tempest_chance.macros.inc"
#include "rom-materials/physical_mp_critical_cost.macros.inc"
#include "rom-materials/physical_mp_critical_unused_cost.macros.inc"
#include "rom-materials/physical_mp_critical_variance.macros.inc"
#include "rom-materials/physical_possess_chance.macros.inc"
#include "rom-materials/physical_image_removal_chance.macros.inc"
#include "rom-materials/physical_critical_chance.macros.inc"
#include "rom-materials/physical_steal_level_bonus.macros.inc"
#include "rom-materials/physical_steal_rare_chance.macros.inc"
#include "rom-materials/physical_sketch_rare_chance.macros.inc"
#include "rom-materials/physical_weapon_death_chance.macros.inc"
#include "rom-materials/physical_damage_variance.macros.inc"
#include "rom-materials/physical_blocked_defense.macros.inc"
#include "rom-materials/physical_protect_scale.macros.inc"
#include "rom-materials/physical_damage_cap.macros.inc"
#include "rom-materials/physical_atma_medium_threshold.macros.inc"
#include "rom-materials/physical_atma_large_threshold.macros.inc"
#include "rom-materials/physical_atma_medium_animation.macros.inc"
#include "rom-materials/physical_zombie_poison_chance.macros.inc"
#include "rom-materials/physical_zombie_status_chance.macros.inc"
#include "rom-materials/physical_weapon_spell_chance.macros.inc"
#include "rom-materials/physical_chainsaw_lottery.macros.inc"
#include "rom-materials/physical_break_divisor.macros.inc"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
using S = BattleStatus;
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
bool has(const BattleState& b, unsigned slot, S status) {
    return (b.status.targets[slot].active & status_mask(status)) != 0;
}
std::uint16_t mask(unsigned slot) {
    return static_cast<std::uint16_t>(1U << (slot < 4 ? slot : slot + 4));
}
std::uint16_t scale(std::uint16_t damage, unsigned fraction) {
    return wrap16(((std::uint32_t{damage} * fraction) >> 8U) + 1U);
}
std::uint16_t multiply(std::uint16_t damage, unsigned half_steps) {
    return static_cast<std::uint16_t>(std::min(unsigned{damage} + (damage / 2U) * half_steps, 65535U));
}
std::uint16_t cover(const BattleState& b, unsigned attacker, std::uint16_t targets,
                     BattlePhysicalResult& result) {
    if (!targets) return targets;
    unsigned original = 9;
    while (!(targets & mask(original))) --original;
    const auto eligible = [&](unsigned i) {
        const auto forbidden = status_mask(S::zombie) | status_mask(S::dead) | status_mask(S::petrify) |
            status_mask(S::vanish) | status_mask(S::sleep) | status_mask(S::confuse) |
            status_mask(S::stop) | status_mask(S::control) | status_mask(S::hide) | status_mask(S::frozen);
        return b.status.targets[i].present && !b.links[i].controls && !b.links[i].seize_target &&
            !(b.status.targets[i].active & forbidden);
    };
    if (const auto love = b.links[original].love_target; love && eligible(*love)) {
        result.covered_target = static_cast<std::uint8_t>(original);
        return mask(*love); // Love Token has priority over the relic search.
    }
    if (!has(b, original, S::near_fatal) || has(b, original, S::vanish) || b.links[original].seize_target)
        return targets;
    unsigned highest_hp = 0;
    for (unsigned n = 10; n; --n) {
        const auto i = n - 1;
        if ((i < 4) != (original < 4) || i == original || i == attacker ||
            !(b.stats[i].relic_effects[2] & 64U) || !eligible(i)) continue;
        if (b.stats[i].hp.current >= highest_hp) {
            highest_hp = b.stats[i].hp.current;
            if (highest_hp) {
                targets = mask(i);
                result.covered_target = static_cast<std::uint8_t>(original);
            }
        }
    }
    return targets;
}
void record_attacker(BattleState& b, unsigned attacker, unsigned target, bool replace) {
    auto& memory = b.attack_memory[target];
    if (!memory.retaliation_pending || replace) {
        memory.retaliation_attacker = static_cast<std::uint8_t>(attacker);
        memory.retaliation_pending = true;
    }
}
} // namespace
std::bitset<10> battle_rear_attack_targets(BattleApproach approach, std::uint8_t attacker,
                                          std::bitset<10> facing_right) {
    require(attacker < 10, "Rear-attack participant outside battle");
    std::bitset<10> result;
    const bool pincer = approach == BattleApproach::pincer && attacker >= 4;
    const bool side = approach == BattleApproach::side && attacker < 4;
    if (pincer || side)
        for (unsigned target = pincer ? 0U : 4U; target < (pincer ? 4U : 10U); ++target)
            result[target] = facing_right[attacker] == facing_right[target];
    return result;
}
BattlePhysicalData load_battle_physical(const ContentPack& pack) {
    BattlePhysicalData data{load_battle_action_preparation(pack), load_battle_inventory(pack)};
    const auto weights = pack.get("battle_umaro_attack_weights");
    require(weights.size() == 16, "Invalid Umaro attack weights");
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 4; ++col)
            data.umaro_weights[row][col] = static_cast<std::uint16_t>(unsigned{weights[row * 4 + col]} + 1U);
    const auto sketch = pack.get("data_cf4300_monster_sketch");
    require(sketch.size() == data.sketch_attacks.size() * 2, "Invalid Sketch attack table");
    for (unsigned i = 0; i < data.sketch_attacks.size(); ++i)
        std::copy_n(sketch.begin() + i * 2, 2, data.sketch_attacks[i].begin());
    const auto p = pack.get("data_c46ac0_magic_prop").subspan(238 * 14, 14);
    // The ordinary Fight record supplies physical/no-reflect behavior. Reject
    // changed content until its additional effect has an explicit native owner.
    require(p[2] == 1 && p[3] == 34 && p[4] == 0 && p[5] == 0 && p[7] == 0 &&
                p[9] == 255 && read32(p, 10) == 0, "Unsupported physical effect record");
    return data;
}
std::optional<std::uint8_t> choose_battle_desperation(BattleState& battle,
    const BattlePhysicalData& data, std::uint8_t attacker, BattleQueuedCommand command) {
    if (attacker >= 4 || command.command != 0 || battle.time.turns < (FF6_ROM_VALUE_PHYSICAL_DESPERATION_WAIT_0 << 8U) ||
        !(command.parameters & 0xff00U)) return {};
    const auto status = battle.status.targets[attacker].active;
    if (!(status & status_mask(S::near_fatal)) ||
        (status & (status_mask(S::confuse) | status_mask(S::image) |
                   status_mask(S::vanish) | status_mask(S::zombie)))) return {};
    // The source draws before testing its once-per-battle bit, then marks
    // the bit before excluding Gau, Umaro and temporary characters.
    if ((battle.random.next(data.targeting.random) & unsigned{FF6_ROM_VALUE_PHYSICAL_DESPERATION_LOTTERY_0}) ||
        battle.desperation_used[attacker]) return {};
    battle.desperation_used.set(attacker);
    const auto actor = battle.actors[attacker].value_or(255);
    if (actor == 12) return 251;
    if (actor < 11) return static_cast<std::uint8_t>(240U + actor);
    return {};
}
BattleUmaroAttack choose_battle_umaro_attack(BattleState& battle, const BattlePhysicalData& data,
                                             std::uint8_t user) {
    require(user < 4 && battle.actors[user] == 13 && battle.status.targets[user].present,
            "Umaro selection requires Umaro");
    auto random = battle.random;
    auto allies = battle.participation.living_allies;
    allies.reset(user);
    const auto equipped = [&](unsigned item) {
        return std::ranges::find(battle.relics[user], item) != battle.relics[user].end();
    };
    const bool ring = equipped(198) && allies.any(), orb = equipped(197);
    const auto& weights = data.umaro_weights[(ring ? 1U : 0U) + (orb ? 2U : 0U)];
    const std::array<bool, 4> allowed{true, true, orb, ring};
    unsigned total = 0;
    for (unsigned i = 0; i < 4; ++i) if (allowed[i]) total += weights[i];
    require(total && total <= 255, "Invalid Umaro attack lottery");
    auto roll = unsigned{random.next(data.targeting.random)} * total >> 8U;
    unsigned selected = 0;
    for (; selected < 4; ++selected) if (allowed[selected]) {
        if (roll < weights[selected]) break;
        roll -= weights[selected];
    }
    BattleUmaroAttack result{static_cast<BattleUmaroAttackKind>(selected), user, {}};
    if (result.kind == BattleUmaroAttackKind::ally_throw) {
        // C2/169E scans backwards and replaces the pool at the first sleepy or
        // confused ally. Lower slots cannot displace that selected single bit.
        for (unsigned n = 4; n; --n) {
            const auto i = n - 1;
            if (!allies[i]) continue;
            if (has(battle, i, S::hide)) allies.reset(i);
            else if (has(battle, i, S::sleep) || has(battle, i, S::confuse)) {
                allies.reset(); allies.set(i);
            }
        }
        if (allies.none()) result.kind = BattleUmaroAttackKind::tackle;
        else {
            auto choice = unsigned{random.next(data.targeting.random)} * allies.count() >> 8U;
            for (unsigned i = 0; i < 4; ++i) if (allies[i]) {
                if (!choice) { result.ally = static_cast<std::uint8_t>(i); break; }
                --choice;
            }
        }
    }
    battle.random = random;
    return result;
}
std::uint16_t battle_monster_physical_base(std::uint8_t power, std::uint8_t strength,
                                          std::uint8_t level) {
    // The first product is explicitly a word before the second multiplication.
    // This matters for unusually high stats, even though ordinary enemies fit.
    const auto first = wrap16((unsigned{power} * 4U + strength) * level);
    return wrap16((std::uint32_t{first} * level) >> 8U);
}
std::uint16_t battle_character_physical_base(std::uint8_t power, std::uint8_t strength,
                                            std::uint8_t level, bool two_handed) {
    // CalcDmg keeps word-sized intermediates and adds the weapon term after
    // the two level products. Strength is already the equipment owner's doubled
    // vigor. Gauntlet increases the first weapon term, not the final addend.
    const unsigned weapon = two_handed ? power + (power + power / 2U) / 2U : power;
    const auto first = wrap16((weapon + strength) * level);
    const auto level_term = wrap16((std::uint32_t{first} * level) >> 8U);
    return wrap16(unsigned{level_term} + (wrap16(unsigned{power} * 2U + level_term) >> 1U));
}
namespace {
BattlePhysicalResult resolve_physical(BattleState& battle, BattlePhysicalEnvironment& environment,
                                      const BattlePhysicalData& data, std::uint8_t attacker,
                                      BattleQueuedCommand command,
                                      const BattleMagicEffect* technique = nullptr,
                                      const BattleUmaroAttack* umaro = nullptr) {
    validate_battle_state(battle);
    require(attacker < 10 && battle.status.targets[attacker].present,
            "Physical attack requires a present participant");
    const bool character = attacker < 4;
    const bool jumping = command.command == 22;
    const bool capture = character && command.command == 6;
    auto b = battle;
    auto env = environment;
    const auto nullified_elements = static_cast<std::uint8_t>(env.nullified_elements | b.nullified_elements);
    const auto draw = [&] { return b.random.next(data.targeting.random); };
    require(
        (technique || jumping || ((command.command == 0 || capture) && (command.attack == 238 || command.attack == 239))) &&
            (technique || command.mp_cost == 0) && !(command.parameters & 0xc0f0U),
        "Unsupported physical command");
    require(!env.weapon_hand || *env.weapon_hand < 2, "Invalid selected weapon hand");
    require(!env.property_source || (*env.property_source >= 4 && *env.property_source < 10),
            "Sketch property source must be a monster");
    const auto property_source = umaro ? umaro->user : env.property_source.value_or(attacker);
    const auto& offense = battle.stats[property_source];
    unsigned hand_index = env.weapon_hand.value_or(0);
    auto hand = offense.hands[hand_index];
    if (jumping) {
        // C2/17F6 selects one hand, then uses ordinary weapon physical rules.
        // Jump bypasses evasion and the attacker row, but not Vanish, defense,
        // Safe, target row, elements or the ordinary critical roll.
        const auto& left = battle.stats[attacker].hands[1];
        if (!env.weapon_hand && left.attack && (!hand.attack || (draw() & 1U))) {
            hand = left;
            hand_index = 1;
        }
        hand.flags |= 32U;
        b.status.targets[attacker].active &= ~status_mask(S::hide);
    }
    if (technique) {
        require(
            (technique->flags & 1U) && !(technique->flags & ~0x63U) && technique->options == 34 &&
                !(technique->behavior & ~176U) && !(technique->status_only & ~1U) &&
                (technique->special == 0 || technique->special == 255 || technique->special == 82 || technique->special == 80 ||
                 technique->special == 48 || technique->special == 85 || technique->special == 86 || technique->special == 87),
            "Physical technique needs an additional native effect owner");
        hand = {};
        hand.attack = technique->power;
        hand.hit_rate = technique->hit_rate;
        hand.elements = technique->elements;
        hand.flags = 32; // InitAttacker does not apply Fight's attacker-row penalty.
    }
    const unsigned weapon_effect = character && !technique && !capture && (command.attack != 239 || has(b, attacker, S::imp))
        ? hand.special >> 4U : 0;
    const bool offering = !technique && (b.stats[attacker].relic_effects[2] & 1U);
    const bool no_critical = offering || weapon_effect == 2;
    const bool mp_critical = !technique &&
        (weapon_effect == 7 || weapon_effect == 14 || weapon_effect == 15);
    require(character || technique || !(hand.special & 240U),
            "Monster weapon metadata cannot select character weapon effects");
    for (auto flags : environment.block_graphics)
        require(!(flags & 128U), "Invalid physical block graphics");
    BattlePhysicalResult result;
    result.attacker = attacker;
    result.weapon_hand = static_cast<std::uint8_t>(hand_index);
    result.weapon_animation = wrap8(unsigned{hand.item} + 1U);
    result.special = !technique && !jumping && command.attack == 239 && !has(b, attacker, S::imp);
    if (technique) {
        if (b.stats[attacker].mp.current < command.mp_cost) {
            result.messages.push_back({40, {}});
            return result;
        }
        b.stats[attacker].mp.current -= command.mp_cost;
    }
    if (!hand.attack && !technique) {
        battle = std::move(b); // Jump still clears Hide even when its chosen weapon broke.
        return result; // No target selection, hit checks or status effect.
    }
    auto rules = battle_attack_target_rules(data.targeting, command.command, command.attack);
    // Ordinary Fight permits retargeting when its selected target is lost.
    if (!technique)
        rules.policy.no_retarget = false;
    if (umaro && umaro->ally) rules.policy.exclude_attacker = true; // C2/1700.
    rules.policy.force_retarget = rules.policy.force_retarget || env.random_retarget;
    if (!env.random_retarget) env.targets.backup_targets = command.parameters;
    auto targets = choose_battle_targets(b, attacker, command.parameters, rules, env.targets,
                                         data.targeting.random, b.random);
    result.selected_targets = targets;
    // C2/3865 unions the selected targets across an attack sequence. The
    // command's authored backup flag permits its remaining strikes even if
    // every opponent has already fallen; their graphics depart after the turn.
    if (env.random_retarget) env.targets.backup_targets |= targets;
    if (technique && technique->special == 48) {
        // Suplex chooses one eligible monster from the admitted side. If all
        // are immune, it still chooses a target and the target effect misses
        // (C2/428B, C2/3C6E); it must not silently redirect to the other side.
        auto eligible = targets;
        for (unsigned i = 0; i < 6; ++i)
            if (b.monster_information[i].cannot_suplex)
                eligible &= static_cast<std::uint16_t>(~mask(i + 4));
        if (eligible) targets = eligible;
        if (targets) {
            auto choice = unsigned{draw()} * static_cast<unsigned>(std::popcount(targets)) >> 8U;
            for (unsigned i = 0; i < 10; ++i)
                if (targets & mask(i)) {
                    if (!choice) { targets = mask(i); break; }
                    --choice;
                }
        }
    }
    const auto target_count = std::popcount(targets);
    if (!technique)
        targets = cover(b, attacker, targets, result);
    auto power = hand.attack;
    if (!character && has(b, attacker, S::imp) && !(b.stats[attacker].monster_flags & 64U)) power = 1;
    auto base = character
        ? battle_character_physical_base(power, offense.strength,
                                          umaro ? offense.level : b.stats[attacker].level, (hand.flags & 64U) != 0)
        : battle_monster_physical_base(power, b.stats[attacker].strength, b.stats[attacker].level);
    if (character && (offering || (umaro && (b.stats[attacker].relic_effects[2] & 1U)))) base /= 2;
    if (character && (b.stats[attacker].relic_effects[2] & 16U))
        base = static_cast<std::uint16_t>(base - base / 4U);
    if (b.stats[attacker].relic_effects[0] & 1U)
        base = static_cast<std::uint16_t>(std::min(unsigned{base} + base / 4U, 65535U));
    std::uint32_t statuses = technique ? technique->status : 0;
    const auto spear = [](const auto& h) { return h.item >= 29 && h.item < 37; };
    unsigned half_steps = command.command == 8 ? 2U : jumping ?
        (spear(b.stats[attacker].hands[0]) || spear(b.stats[attacker].hands[1]) ? 2U : 1U) : 0U; // Before variance.
    bool drain = weapon_effect == 5 || weapon_effect == 6, remove_status = false;
    result.affects_mp = weapon_effect == 6;
    const bool healing_weapon = weapon_effect == 12;
    const bool dice_weapon = weapon_effect == 9;
    const bool ignore_defense = weapon_effect == 2 || weapon_effect == 10 || healing_weapon;
    // C2/318D replaces the weapon special with Steal for Capture.
    // Thief Knife instead selects the same command graphics on its coin flip.
    const bool stealing_weapon = capture || (weapon_effect == 1 && !(draw() & 1U));
    result.capture = stealing_weapon;
    bool status_only = technique && (technique->status_only & 1U);
    bool never_miss = jumping || offering || dice_weapon || (technique ? (technique->behavior & 32U) != 0
                                : (b.stats[attacker].relic_effects[1] & 16U) != 0);
    if (result.special) {
        const auto property = offense.monster_special;
        const auto effect = property & 63U;
        status_only = (property & 64U) != 0;
        never_miss = never_miss || (property & 128U);
        if (effect < 32) statuses = std::uint32_t{1} << effect;
        else if (effect < 48) half_steps = effect - 31U;
        else if (effect < 50) {
            drain = true;
            result.affects_mp = effect == 49;
        } else {
            remove_status = true;
            statuses = status_mask(S::reflect);
        }
    }
    if (has(b, attacker, S::morph)) half_steps += 2;
    if (has(b, attacker, S::berserk)) ++half_steps;
    if (target_count >= 2 && (!technique || !(technique->flags & 64U)))
        base /= 2;
    if (character && b.entry.back_rows[attacker] && !(hand.flags & 32U)) base /= 2;
    // Attacker effects run after the ordinary base and row/relic adjustments.
    // C2/3F89 uses a wrapping word addition for Valiant Knife's missing HP.
    if (weapon_effect == 10)
        base = wrap16(unsigned{base} + b.stats[attacker].hp.maximum - b.stats[attacker].hp.current);
    if (weapon_effect == 11 && draw() < FF6_ROM_VALUE_PHYSICAL_TEMPEST_CHANCE_0) {
        hand.attack = 0;
        result.followup_spell = 101; // C2/3F9F: Wind Slash replaces the weapon damage.
    }
    if (dice_weapon) {
        const auto roll = draw();
        result.dice = {static_cast<std::uint8_t>((roll & 15U) % 6U + 1U),
                       static_cast<std::uint8_t>((roll % 96U) / 16U + 1U), 0};
        unsigned product = result.dice[0] * result.dice[1];
        if (hand.hit_rate >= 3) {
            result.dice[2] = env.clock % 6U + 1U;
            product *= result.dice[2];
        }
        product *= wrap8(unsigned{b.stats[attacker].level} * 2U);
        if (result.dice[2] && result.dice[0] == result.dice[1] && result.dice[1] == result.dice[2])
            product *= result.dice[2];
        base = static_cast<std::uint16_t>(std::min(product, 65535U));
    }
    if (weapon_effect == 14 && !env.counterattack && !env.prevent_weapon_break &&
        (unsigned{draw()} * (b.stats[attacker].hp.current % unsigned{FF6_ROM_VALUE_PHYSICAL_BREAK_DIVISOR_0} + 1U) >> 8U) == 0) {
        // C2/3ECA intentionally chooses the broken hand by remaining-hit parity,
        // even for Jump. Keep that original quirk instead of removing the held hand.
        const unsigned broken = (env.remaining_hits & 1U) ? 0 : 1;
        b.stats[attacker].hands[broken].attack = 0;
        if (b.inventory.hands[attacker]) {
            auto& stack = (*b.inventory.hands[attacker])[broken];
            if (stack.item && stack.item->identity == 23) {
                stack = {};
                result.messages.push_back({68, {}});
            }
        }
    }
    // C2/3F22 runs before hit checks, including misses and friendly targets.
    // The charge adds the source-selected base cost and random variance,
    // not a queued spell cost and not conditional on a successful hit.
    bool paid_critical = false;
    if (mp_critical && !no_critical && target_count) {
        const unsigned cost = (weapon_effect == 15 ? unsigned{FF6_ROM_VALUE_PHYSICAL_MP_CRITICAL_UNUSED_COST_0} :
            unsigned{FF6_ROM_VALUE_PHYSICAL_MP_CRITICAL_COST_0}) +
            (draw() & unsigned{FF6_ROM_VALUE_PHYSICAL_MP_CRITICAL_VARIANCE_0});
        if (b.stats[attacker].mp.current >= cost) {
            b.stats[attacker].mp.current -= cost;
            paid_critical = true;
        }
    }
    // Possess's attacker effect runs once, before individual hit admission.
    const bool possess_miss = technique && technique->special == 80 && draw() >= FF6_ROM_VALUE_PHYSICAL_POSSESS_CHANCE_0;
    bool missed = false;
    for (unsigned n = 10; n; --n) {
        const auto slot = n - 1;
        if (!(targets & mask(slot))) continue;
        result.targets[slot] = true;
        BattlePhysicalImpact impact{static_cast<std::uint8_t>(slot), true, {}, {}};
        unsigned guard = 0;
        bool image_miss = false;
        if (possess_miss || has(b, slot, S::vanish)) impact.hit = false;
        else if (technique && (technique->flags & 2U) && b.stats[slot].instant_death_protection)
            impact.hit = false;
        else if (!technique && slot < 4 && target_count == 1 && has(b, slot, S::interceptor) &&
                 (draw() & 1U)) {
            impact.hit = false; guard = 64;
        } else if (!technique && slot < 4 && target_count == 1 && env.golem_hp) {
            impact.hit = false; guard = 32;
        } else if (technique && (technique->behavior & 16U) && !never_miss) {
            impact.hit = (unsigned{draw()} * 100U >> 8U) <
                         (unsigned{hand.hit_rate} * b.stats[slot].magic_evade_threshold >> 8U);
            if (impact.hit)
                impact.hit = (draw() & 127U) > b.stats[slot].stamina;
        } else if (!never_miss && !has(b, slot, S::petrify) && !has(b, slot, S::sleep) &&
                   !has(b, slot, S::stop) && !has(b, slot, S::frozen) &&
                   !env.hit_from_behind[slot] && hand.hit_rate != 255 &&
                   !(b.stats[slot].retaliation_flags & 1U)) {
            if (has(b, slot, S::image)) {
                if (draw() < FF6_ROM_VALUE_PHYSICAL_IMAGE_REMOVAL_CHANCE_0) b.status.targets[slot].pending.remove |= status_mask(S::image);
                impact.hit = false;
                image_miss = true; // Image bypasses the equipment block-animation draw.
            } else {
                // USA Rev 1's preserved Evade quirk: ordinary physical checks
                // use magic evasion and bypass Blind/Beads hit-rate modifiers.
                impact.hit = (unsigned{draw()} * 100U >> 8U) <
                    (unsigned{hand.hit_rate} * b.stats[slot].magic_evade_threshold >> 8U);
            }
        }
        if (!impact.hit && !image_miss && slot < 4 && !has(b, slot, S::vanish) &&
            !has(b, slot, S::zombie) && !has(b, slot, S::magitek)) {
            auto blocks = unsigned{env.block_graphics[slot]} | guard;
            if (blocks) {
                auto choice = unsigned{draw()} * static_cast<unsigned>(std::popcount(blocks)) >> 8U;
                unsigned bit = 0;
                for (; bit < 7; ++bit) if (blocks & (1U << bit)) {
                    if (!choice) break;
                    --choice;
                }
                result.block_animations[slot] = static_cast<std::uint8_t>(bit + 1);
                if (bit >= 5) {
                    impact.hit = true;
                    statuses = 0;
                    if (bit == 5) result.golem_target = static_cast<std::uint8_t>(slot);
                    else result.interceptor_target = static_cast<std::uint8_t>(slot);
                }
            }
        }
        missed = missed || !impact.hit;
        result.impacts.push_back(impact);
    }
    result.critical = (umaro && umaro->ally && b.actors[attacker] == 10) || paid_critical || (!character && (b.stats[attacker].monster_flags & 64U) &&
                                        has(b, attacker, S::imp));
    if (!no_critical && (!technique || umaro) && !result.critical && !result.special && !missed &&
        (targets & (character ? 0xff00U : 15U)))
        result.critical = draw() < FF6_ROM_VALUE_PHYSICAL_CRITICAL_CHANCE_0 && target_count != 0;
    if (result.critical) half_steps += 2;
    base = multiply(base, half_steps); // C2/3414 consumes attacker multipliers before variance.

    const bool blocked = result.golem_target.has_value() || result.interceptor_target.has_value();
    std::array<std::optional<std::uint16_t>, 10> damage{}, healing{};
    const auto accumulate = [](auto& values, unsigned slot, unsigned amount) {
        values[slot] = static_cast<std::uint16_t>(std::min(unsigned{values[slot].value_or(0)} + amount, unsigned{FF6_ROM_VALUE_PHYSICAL_DAMAGE_CAP_0}));
    };
    for (auto& impact : result.impacts) {
        if (!impact.hit) continue;
        const auto slot = impact.slot;
        const bool replace = b.attack_memory[slot].retaliation_pending && (draw() & 1U);
        record_attacker(b, attacker, slot, replace);
        b.attack_memory[slot].command = BattleObservedAttackValue{attacker, static_cast<std::uint8_t>(stealing_weapon && command.command == 0 ? 6 : command.command)};
        b.attack_memory[slot].elements = BattleObservedAttackValue{attacker, hand.elements};
        if (command.command == 9 || command.command == 8)
            b.attack_memory[slot].item = BattleObservedAttackValue{attacker, command.attack};
        // Physical spells participate in if_attack AI conditions; Fight/Special
        // leave the most recent spell untouched (C2/35AD).
        if (technique && (command.command == 10 || command.command == 2 || command.command == 12 ||
                          command.command == 7))
            b.attack_memory[slot].attack = BattleObservedAttackValue{attacker, command.attack};
        // C2/344E also protects the possessor from physical effects after
        // recording the attack. Other participants continue fighting normally.
        if (b.zinger && b.zinger->attacker == slot) {
            impact.hit = false;
            continue;
        }
        auto& status = b.status.targets[slot];
        auto& stats = b.stats[slot];
        if (technique && technique->special == 80) {
            // C2/3B98 removes the user from the field party and applies the
            // shared Hide/Dead transition to both participants. This is not
            // Zinger's reversible possession link, nor ordinary escape.
            b.untargetable.set(attacker);
            b.removed_from_party.set(attacker);
            b.status.targets[attacker].pending.apply |= status_mask(S::hide) | status_mask(S::dead);
            status.pending.apply |= status_mask(S::hide) | status_mask(S::dead);
            continue;
        }
        if (technique && technique->special == 48 && slot >= 4 &&
            b.monster_information[slot - 4].cannot_suplex) {
            impact.hit = false;
            continue;
        }
        // Target effects run after hit admission and attacker memory, even for
        // zero-power commands. C2/399E preserves the two-slot steal lottery:
        // selecting an empty rare/common slot fails without falling back.
        if ((technique && technique->special == 82) || stealing_weapon) {
            auto& stock = b.steal_stock[slot].items;
            unsigned message = 1;
            if (stock[0] != 255 || stock[1] != 255) {
                message = 2;
                const int chance = int{b.stats[attacker].level} + FF6_ROM_VALUE_PHYSICAL_STEAL_LEVEL_BONUS_0 - b.stats[slot].level;
                // ADC starts with carry set; SBC then consumes its borrow.
                const bool automatic =
                    unsigned{b.stats[attacker].level} + FF6_ROM_VALUE_PHYSICAL_STEAL_LEVEL_BONUS_0 + 1U >= 256 || chance >= 128;
                const unsigned threshold =
                    chance < 0
                        ? 0U
                        : unsigned(chance) * ((b.stats[attacker].relic_effects[1] & 1U) ? 2U : 1U);
                if (automatic || (chance >= 0 && (unsigned{draw()} * 100U >> 8U) < threshold)) {
                    const auto item = stock[draw() < FF6_ROM_VALUE_PHYSICAL_STEAL_RARE_CHANCE_0 ? 0 : 1];
                    if (item != 255) {
                        queue_battle_item(b.inventory, data.inventory, item);
                        stock.fill(255);
                        result.messages.push_back({3, {item}});
                        message = 3;
                    }
                }
            }
            if (message != 3) result.messages.push_back({static_cast<std::uint8_t>(message), {}});
            if (!stealing_weapon) continue;
        }
        if (technique && technique->special == 85) {
            // C2/3B29: character targets miss; immune monsters additionally
            // announce failure. Attack choice is independent of the hit roll.
            if (slot < 4 || b.monster_information[slot - 4].cannot_sketch) {
                impact.hit = false;
                if (slot >= 4) result.messages.push_back({31, {}});
                continue;
            }
            const auto threshold = battle_sketch_threshold(b.stats[attacker].level,
                stats.level, (b.stats[attacker].relic_effects[1] & 4U) != 0);
            if (threshold < 256 && draw() >= threshold) {
                impact.hit = false;
                continue;
            }
            const auto identity = b.monster_information[slot - 4].identity;
            require(identity < data.sketch_attacks.size(), "Sketch monster identity out of range");
            result.sketch_source = static_cast<std::uint8_t>(slot);
            result.sketch_attack = data.sketch_attacks[identity][draw() < FF6_ROM_VALUE_PHYSICAL_SKETCH_RARE_CHANCE_0 ? 0 : 1];
            continue;
        }
        if (technique && technique->special == 86) {
            const auto available =
                static_cast<std::uint8_t>(~(stats.weak | nullified_elements));
            if (!available) {
                impact.hit = false;
                continue;
            }
            auto choice = unsigned{draw()} * static_cast<unsigned>(std::popcount(available)) >> 8U;
            unsigned element = 0;
            for (; element < 8; ++element)
                if (available & (1U << element)) {
                    if (!choice)
                        break;
                    --choice;
                }
            const auto bit = static_cast<std::uint8_t>(1U << element);
            stats.weak |= bit;
            stats.halved &= static_cast<std::uint8_t>(~bit);
            stats.absorbed &= static_cast<std::uint8_t>(~bit);
            stats.nullified &= static_cast<std::uint8_t>(~bit);
            result.messages.push_back({static_cast<std::uint8_t>(11 + element), {}});
        }
        if (technique && technique->special == 87) {
            if (stats.instant_death_protection) {
                impact.hit = false;
                continue;
            }
            b.deferred_death.set(slot);
            result.suppress_retaliation = true;
            result.messages.push_back({19, {}});
        }
        unsigned target_half_steps = 0;
        if (weapon_effect == 4 && (stats.monster_flags & 16U)) target_half_steps += 2;
        if (weapon_effect == 8 && (draw() & 1U)) {
            ++target_half_steps;
            if (has(b, slot, S::floating) && command.command == 0) {
                target_half_steps += 3;
                result.thrown_weapon = true;
            }
        }
        if (weapon_effect == 3 || weapon_effect == 13) {
            const bool undead = (stats.monster_flags & 128U) != 0;
            const bool admitted = !stats.instant_death_protection &&
                ((weapon_effect == 3 && undead) || (draw() < FF6_ROM_VALUE_PHYSICAL_WEAPON_DEATH_CHANCE_0 && (draw() & 127U) > stats.stamina));
            const bool killed = admitted && !(jumping && env.remaining_hits);
            if (killed) {
                status.pending.apply |= status_mask(S::dead);
                result.death_animation = weapon_effect == 3 ? 126 : 238;
            }
            // C2/3D43 falls through to Doom even when instant death was
            // rejected. Undead restoration is independent of that lottery.
            if (weapon_effect == 3 && undead) {
                if (slot < 4) {
                    status.pending.apply &= ~status_mask(S::dead);
                    stats.hp.current = stats.hp.maximum;
                } else {
                    status.pending.apply |= status_mask(S::hide);
                    result.rematerialize.set(slot - 4);
                }
            }
            if (killed) continue;
        }
        const auto apply = remove_status ? 0U : statuses & ~status.active;
        auto remove = remove_status ? statuses & status.active : 0U;
        if (hand.elements & 1U) remove |= status.active & status_mask(S::frozen) & ~statuses;
        status.pending.apply |= apply;
        status.pending.remove |= remove;
        if (status_only) {
            if (!((apply | remove) & ~stats.blocked_status)) impact.hit = false;
            continue;
        }
        // A status/tool effect with zero power must not acquire the +1 damage
        // floor or consume damage-variance randomness (C2/3327).
        if (!hand.attack)
            continue;
        const bool fractional = technique && (technique->behavior & 128U);
        auto amount = fractional
            ? std::max<std::uint16_t>(1, wrap16(unsigned{stats.hp.current} * technique->power / 16U))
            : dice_weapon ? base : scale(base, draw() | unsigned{FF6_ROM_VALUE_PHYSICAL_DAMAGE_VARIANCE_0});
        if (!dice_weapon && !ignore_defense && !fractional && (!technique || !(technique->flags & 32U))) {
            auto defense = result.affects_mp ? stats.magic_defense : stats.defense;
            if (blocked && defense != 255)
                defense = FF6_ROM_VALUE_PHYSICAL_BLOCKED_DEFENSE_0 - 1;
            amount = scale(amount, 255U - defense);
            if (has(b, slot, result.affects_mp ? S::shell : S::safe))
                amount = scale(amount, FF6_ROM_VALUE_PHYSICAL_PROTECT_SCALE_0);
            if (!result.affects_mp) {
                if (b.turns[slot].defending)
                    amount /= 2;
                if (slot < 4 ? b.entry.back_rows[slot] : b.entry.monster_back_rows[slot - 4])
                    amount /= 2;
            } else if (has(b, slot, S::morph))
                amount /= 2;
        }
        if (!fractional && !dice_weapon) {
            if (character && slot < 4 && !healing_weapon) amount /= 2;
            // C2/0D34 applies target-specific multipliers after defense. The
            // attacker/Jump/critical multipliers were already consumed at C2/3414.
            amount = multiply(amount, target_half_steps + (env.hit_from_behind[slot] ? 1U : 0U));
        }
        bool restores = healing_weapon || (drain && (weapon_effect == 5 || weapon_effect == 6) &&
                                           ((stats.monster_flags & 128U) || has(b, slot, S::zombie)));
        if (has(b, slot, S::petrify)) amount = 0;
        else if (hand.elements) {
            if (!(hand.elements & ~nullified_elements)) amount = 0;
            else if (hand.elements & stats.absorbed) restores = !restores;
            else if (hand.elements & stats.nullified) amount = 0;
            else if (hand.elements & stats.halved) amount /= 2;
            else if ((hand.elements & stats.weak) && amount < 32768) amount = static_cast<std::uint16_t>(amount * 2U);
        }
        if (weapon_effect == 2) {
            const auto& owner = b.stats[attacker];
            const unsigned ratio = ((owner.hp.current >> 8U) + 1U) * owner.level /
                                   ((owner.hp.maximum >> 8U) + 1U);
            amount = wrap16((std::uint32_t{amount} * ratio >> 6U) + 1U);
            if (amount >= FF6_ROM_VALUE_PHYSICAL_ATMA_MEDIUM_THRESHOLD_0)
                result.weapon_animation = amount >= FF6_ROM_VALUE_PHYSICAL_ATMA_LARGE_THRESHOLD_0
                    ? FF6_ROM_VALUE_PHYSICAL_ATMA_MEDIUM_ANIMATION_0 + 1 : FF6_ROM_VALUE_PHYSICAL_ATMA_MEDIUM_ANIMATION_0;
        }
        if (!blocked) {
            if (has(b, attacker, S::zombie)) {
                const auto random = draw();
                if (random < FF6_ROM_VALUE_PHYSICAL_ZOMBIE_STATUS_CHANCE_0)
                    status.pending.apply |= status_mask(random < FF6_ROM_VALUE_PHYSICAL_ZOMBIE_POISON_CHANCE_0 ? S::poison : S::blind);
            }
            status.pending.remove |= status.active & (status_mask(S::sleep) | status_mask(S::confuse)) & ~statuses;
            if (b.links[slot].controlled_by) b.links[slot].controlled_by->remove_pending = true;
        }
        if (drain) {
            // Weapon drains invert for undead; monster specials do not.
            const auto victim = restores ? attacker : slot;
            const auto recipient = restores ? slot : attacker;
            const auto vitals = [&](unsigned i) { return result.affects_mp ? b.stats[i].mp : b.stats[i].hp; };
            amount = std::min(amount, vitals(victim).current);
            if (b.menus_disabled) {
                const auto v = vitals(recipient);
                amount = std::min<std::uint16_t>(amount, static_cast<std::uint16_t>(v.maximum - v.current));
            }
            accumulate(restores ? damage : healing, attacker, amount);
            record_attacker(b, slot, attacker, false);
        }
        accumulate(restores ? healing : damage, slot, amount);
    }
    bool lethal_damage = false;
    for (unsigned n = 10; n; --n) {
        const auto slot = n - 1;
        if (!damage[slot] && !healing[slot]) continue;
        if (b.invincible[slot] && damage[slot]) damage[slot] = 0;
        auto& v = result.affects_mp ? b.stats[slot].mp : b.stats[slot].hp;
        const int taken = blocked ? 0 : damage[slot].value_or(0);
        const int net = int{healing[slot].value_or(0)} - taken;
        v.current = static_cast<std::uint16_t>(std::clamp(int{v.current} + net, 0, int{v.maximum}));
        if (result.affects_mp && net != 0)
            b.status.targets[slot].updates.set(static_cast<unsigned>(BattleStatusUpdate::spells));
        if (net < 0 && !v.current && (!result.affects_mp || (b.stats[slot].monster_flags & 1U))) {
            // C2/1390 clears the pending weapon spell before applying death,
            // including zero-HP zombies and monsters which die at zero MP.
            lethal_damage = true;
            b.stats[slot].hp.current = 0;
            if (!has(b, slot, S::zombie)) b.status.targets[slot].pending.apply |= status_mask(S::dead);
        }
        if (result.golem_target == slot)
            env.golem_hp = static_cast<std::uint16_t>(env.golem_hp - std::min(env.golem_hp, damage[slot].value_or(0)));
        if (std::ranges::none_of(result.impacts, [slot](const auto& i) { return i.slot == slot; }))
            result.impacts.push_back({static_cast<std::uint8_t>(slot), true, {}, {}});
    }
    std::array<BattleStatusContext, 10> contexts;
    for (unsigned i = 0; i < 10; ++i) contexts[i] = {b.stats[i].hp, b.stats[i].blocked_status};
    update_battle_status(b.status, contexts);
    // A skipped possessor gets neither damage nor a MISS numeral.
    std::erase_if(result.impacts, [&](const auto& impact) {
        return b.zinger && b.zinger->attacker == impact.slot;
    });
    for (auto& impact : result.impacts) {
        impact.damage = damage[impact.slot];
        impact.healing = healing[impact.slot];
        if (result.targets[impact.slot]) result.hits[impact.slot] = impact.hit;
    }
    // C2/363E checks random spellcasting after status updates. Dragon Horn
    // permits it only on the final landing; Offering, missed hits (C2/22D1)
    // and lethal damage clear it before even drawing the spellcast lottery.
    if (character && !technique && !offering && !missed && !lethal_damage && (hand.spell & 64U) &&
        (!jumping || !env.remaining_hits) && draw() < FF6_ROM_VALUE_PHYSICAL_WEAPON_SPELL_CHANCE_0) {
        result.followup_spell = hand.spell & 63U;
        result.followup_same_target = true;
    }
    battle = std::move(b);
    environment = env;
    return result;
}
} // namespace
BattlePhysicalResult resolve_battle_umaro_physical(BattleState& battle,
    BattlePhysicalEnvironment& environment, const BattlePhysicalData& data,
    BattleUmaroAttack attack, BattleQueuedCommand command) {
    require(attack.user < 4 && battle.actors[attack.user] == 13 && command.command == 0 &&
                !command.mp_cost &&
                (attack.kind == BattleUmaroAttackKind::tackle || attack.kind == BattleUmaroAttackKind::ally_throw) &&
                (attack.ally.has_value() == (attack.kind == BattleUmaroAttackKind::ally_throw)),
            "Invalid Umaro physical attack");
    auto next = battle;
    const auto summed_power = [&](unsigned slot) {
        const auto& hands = next.stats[slot].hands;
        return unsigned{hands[0].attack} + hands[1].attack;
    };
    auto power = std::min(255U, summed_power(attack.user));
    auto owner = attack.user;
    if (attack.ally) {
        owner = *attack.ally;
        require(owner < 4 && owner != attack.user && next.participation.living_allies[owner] &&
                    !has(next, owner, S::hide), "Invalid thrown ally");
        // C2/16DA retains the CMP Mog carry in the weapon addition. In the
        // overflowing case FE + carry saturates identically to this widened sum.
        power = std::min(255U, power + summed_power(owner) + (next.actors[owner].value_or(0) >= 10 ? 1U : 0U));
        next.status.targets[owner].pending.remove |=
            next.status.targets[owner].active & (status_mask(S::sleep) | status_mask(S::confuse));
    }
    BattleMagicEffect technique;
    technique.flags = 33; // Physical, ignore defense (also Safe, Defend and row).
    technique.options = 34;
    technique.behavior = 32; // Never miss, but Vanish still blocks physical hits.
    technique.power = static_cast<std::uint8_t>(power);
    technique.hit_rate = next.stats[attack.user].strength;
    technique.special = 255;
    command.attack = 238;
    auto result = resolve_physical(next, environment, data, owner, command, &technique, &attack);
    battle = std::move(next);
    return result;
}
BattlePhysicalResult resolve_battle_monster_physical(BattleState& battle,
    BattlePhysicalEnvironment& environment, const BattlePhysicalData& data,
    std::uint8_t attacker, BattleQueuedCommand command) {
    require(attacker >= 4, "Monster physical attack requires a monster");
    // InitCommand can replace an Imp's queued action with Fight and a zero
    // payload. FightAttack/C2:161B selects weapon properties regardless of
    // that payload; only SPECIAL selects the monster's additional effect.
    // Match the character Fight path instead of treating zero as Fire.
    if (command.command == 0 && command.attack != 239)
        command.attack = 238;
    return resolve_physical(battle, environment, data, attacker, command);
}
BattlePhysicalResult resolve_battle_character_physical(BattleState& battle,
    BattlePhysicalEnvironment& environment, const BattlePhysicalData& data,
    std::uint8_t attacker, BattleQueuedCommand command) {
    require(attacker < 4 && (command.command == 0 || command.command == 6 || command.command == 22),
            "Character physical attack requires Fight or Jump");
    // FightAttack selects weapon properties itself. Both the menu's FF
    // sentinel and RandCharAction's zero are payloads, not spell identities.
    command.attack = 238;
    return resolve_physical(battle, environment, data, attacker, command);
}
BattlePhysicalResult
resolve_battle_physical_technique(BattleState& battle, BattlePhysicalEnvironment& environment,
                                  const BattlePhysicalData& data, std::uint8_t attacker,
                                  BattleQueuedCommand command, const BattleMagicEffect& technique) {
    return resolve_physical(battle, environment, data, attacker, command, &technique);
}
BattlePhysicalResult resolve_battle_rage_physical(BattleState& battle,
    BattlePhysicalEnvironment& environment, const BattlePhysicalData& data,
    std::uint8_t attacker, BattleQueuedCommand command) {
    require(attacker < 4 && command.command == 0 &&
            (command.attack == 238 || command.attack == 239), "Invalid Rage physical attack");
    return resolve_physical(battle, environment, data, attacker, command);
}
BattlePhysicalResult resolve_battle_possess(BattleState& battle,
    BattlePhysicalEnvironment& environment, const BattlePhysicalData& data,
    std::uint8_t attacker, BattleQueuedCommand command) {
    require(attacker < 4 && command.command == 28, "Character Possess requires its command");
    BattleMagicEffect effect;
    // InitCmdTarget loads the zero-power physical Battle template (C2/2752).
    // Possess bypasses evasion, but physical Vanish still prevents a hit.
    effect.flags = 1;
    effect.options = 34;
    effect.behavior = 32;
    effect.special = 80;
    return resolve_physical(battle, environment, data, attacker, command, &effect);
}
BattlePhysicalResult resolve_battle_steal(BattleState& battle,
                                          BattlePhysicalEnvironment& environment,
                                          const BattlePhysicalData& data, std::uint8_t attacker,
                                          BattleQueuedCommand command) {
    require(attacker < 4 && command.command == 5, "Character Steal requires its command");
    BattleMagicEffect effect;
    effect.flags = 1;
    effect.options = 34;
    effect.behavior = 32;
    effect.special = 82;
    return resolve_physical(battle, environment, data, attacker, command, &effect);
}
BattlePhysicalResult
resolve_battle_tool_physical(BattleState& battle, BattlePhysicalEnvironment& environment,
                             const BattlePhysicalData& data, std::uint8_t attacker,
                             BattleQueuedCommand command, const BattleMagicEffect& supplied) {
    require(attacker < 4 && command.command == 9 && command.attack >= 163 && command.attack <= 170,
            "Invalid physical Tool command");
    auto next = battle;
    auto effect = supplied;
    const bool death = command.attack == 166 && !(next.random.next(data.targeting.random) & unsigned{FF6_ROM_VALUE_PHYSICAL_CHAINSAW_LOTTERY_0});
    if (death) {
        effect.power = 0;
        effect.flags |= 34;    // Instant-death protection, ignore defense.
        effect.behavior |= 16; // Stamina hit test, even at hit rate 255.
        effect.status = status_mask(S::dead);
    } else if (command.attack == 166)
        effect.flags |= 32;
    auto result = resolve_physical(next, environment, data, attacker, command, &effect);
    result.chainsaw_death = death;
    battle = std::move(next);
    return result;
}
} // namespace ff6
