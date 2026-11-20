#include "ff6/battle_entry.hpp"
#include "rom-materials/battle_side_minimum_allies.macros.inc"
#include "rom-materials/battle_preemptive_base.macros.inc"
#include "rom-materials/battle_absent_gauge_increment.macros.inc"
#include "rom-materials/battle_preempted_enemy_gauge.macros.inc"
#include <algorithm>
#include <numeric>

namespace ff6 {
BattleApproachWeights decode_battle_approach_weights(ByteView bytes) {
    if (bytes.size() != 4)
        throw std::invalid_argument("Invalid battle approach table extent");
    BattleApproachWeights result;
    for (unsigned i = 0; i < 4; ++i)
        result.values[3 - i] = static_cast<std::uint16_t>(unsigned{bytes[i]} + 1U);
    return result;
}
BattleApproachWeights load_battle_approach_weights(const ContentPack& pack) {
    return decode_battle_approach_weights(pack.get("battle_approach_weights"));
}
void update_battle_participation(BattleState& state) {
    auto& participation = state.participation;
    state.arrival_override &= ~state.untargetable;
    participation.targetable = state.arrival_override;
    participation.living_allies.reset();
    participation.living_enemy_characters.reset();
    participation.living_monsters.reset();
    constexpr auto disallowed = status_mask(BattleStatus::dead) |
                                status_mask(BattleStatus::petrify) |
                                status_mask(BattleStatus::zombie);
    for (std::size_t slot = 0; slot < 10; ++slot) {
        const auto& target = state.status.targets[slot];
        if (state.untargetable[slot])
            continue;
        if (!state.arrival_override[slot]) {
            if (!target.present)
                continue;
            participation.targetable.set(slot);
            if ((target.active & disallowed) != 0 || (slot >= 4 && target.monster_dead_or_escaped))
                continue;
        }
        if (!participation.active[slot])
            continue;
        if (slot >= 4)
            participation.living_monsters.set(slot - 4);
        else if (participation.enemy_characters[slot])
            participation.living_enemy_characters.set(slot);
        else
            participation.living_allies.set(slot);
    }
    participation.ally_count = static_cast<std::uint8_t>(participation.living_allies.count());
    participation.enemy_count = static_cast<std::uint8_t>(
        participation.living_monsters.count() + participation.living_enemy_characters.count());
}
namespace {
BattleApproach choose_approach(std::array<bool, 4> allowed, bool back_guard, std::uint8_t allies,
                               const BattleApproachWeights& weights,
                               const std::array<std::uint8_t, 256>& table, BattleRandom& random) {
    if (back_guard) {
        if (allowed[0] || allowed[1] || allowed[3])
            allowed[2] = false;
        if (allowed[0] || allowed[2] || allowed[3])
            allowed[1] = false;
    }
    if (allies < FF6_ROM_VALUE_BATTLE_SIDE_MINIMUM_ALLIES_0 && (allowed[0] || allowed[1] || allowed[2]))
        allowed[3] = false;
    unsigned total = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (weights.values[i] == 0 || weights.values[i] > 255)
            throw std::invalid_argument("Invalid battle approach weight");
        if (allowed[i])
            total += weights.values[i];
    }
    if (total == 0 || total > 255)
        throw std::invalid_argument("Invalid battle approach selection");
    auto choice = (unsigned{random.next(table)} * total) >> 8U;
    for (unsigned remaining = 4; remaining != 0; --remaining) {
        const auto i = remaining - 1;
        if (!allowed[i])
            continue;
        if (choice < weights.values[i])
            return static_cast<BattleApproach>(i);
        choice -= weights.values[i];
    }
    throw std::logic_error("Unresolved battle approach");
}
void apply_approach(BattleState& state, bool preemptive_allowed, bool gale_hairpin,
                    const std::array<std::uint8_t, 256>& table) {
    auto& entry = state.entry;
    if (entry.approach != BattleApproach::normal)
        entry.gau_recruitment = false;
    switch (entry.approach) {
    case BattleApproach::normal:
    case BattleApproach::side:
        if (!state.menus_disabled && preemptive_allowed) {
            // C2/2E74 doubles the enum for its word dispatch; C2/2EA7
            // then shifts twice and ORs in the selected probability bits.
            constexpr auto side_bits = static_cast<unsigned>(BattleApproach::side) << 3U;
            static_assert((FF6_ROM_VALUE_BATTLE_PREEMPTIVE_BASE_0 & side_bits) == 0);
            unsigned threshold = entry.approach == BattleApproach::side
                ? (FF6_ROM_VALUE_BATTLE_PREEMPTIVE_BASE_0 | side_bits)
                : FF6_ROM_VALUE_BATTLE_PREEMPTIVE_BASE_0;
            if (gale_hairpin)
                threshold *= 2U;
            entry.preemptive |= state.random.next(table) < threshold;
        }
        break;
    case BattleApproach::back:
        entry.back_rows.flip();
        entry.back_or_pincer = true;
        break;
    case BattleApproach::pincer:
        entry.back_rows.reset();
        entry.back_or_pincer = true;
        break;
    }
    entry.displayed_back_rows = entry.back_rows;
}
void initialize_gauges(BattleState& state, const std::array<std::uint8_t, 256>& table) {
    unsigned absent = 0;
    for (const auto& target : state.status.targets)
        absent += !target.present;
    std::vector<unsigned> ranks(10);
    std::iota(ranks.begin(), ranks.end(), 0U);
    for (std::size_t remaining = 10; remaining != 0; --remaining) {
        const auto slot = remaining - 1;
        const auto pick = (unsigned{state.random.next(table)} * ranks.size()) >> 8U;
        const auto rank = ranks[pick];
        ranks.erase(ranks.begin() + static_cast<std::ptrdiff_t>(pick));
        auto& gauge = state.turns[slot].atb;
        if ((gauge >> 8U) != 255 || state.entry.final_scrolling)
            continue;
        unsigned initial = rank * 8U;
        const bool enemy = slot >= 4 || state.participation.enemy_characters[slot];
        if (!enemy && (state.entry.preemptive || state.entry.approach == BattleApproach::side))
            continue;
        if (enemy && (state.entry.preemptive || state.entry.approach == BattleApproach::side))
            initial = FF6_ROM_VALUE_BATTLE_PREEMPTED_ENEMY_GAUGE_0;
        else if (enemy || state.entry.approach == BattleApproach::normal) {
            const unsigned speed = state.stats[slot].speed;
            initial += speed + ((unsigned{state.random.next(table)} * speed) >> 8U) + absent * FF6_ROM_VALUE_BATTLE_ABSENT_GAUGE_INCREMENT_0;
        }
        initial = std::min(initial + 1U, 255U);
        gauge = static_cast<std::uint16_t>((gauge & 255U) | (initial << 8U));
    }
}
} // namespace
void change_battle_approach(BattleState& state, const BattleFormation& formation,
                             std::uint8_t relic_effects, const BattleApproachWeights& weights,
                             const std::array<std::uint8_t, 256>& table) {
    state.entry.approach = choose_approach(formation.allowed_approaches, (relic_effects & 2U) != 0,
                                          state.participation.ally_count, weights, table, state.random);
    state.leap_allowed = formation.leap;
}
void initialize_battle_entry(BattleState& state, const BattleFormation& formation,
                             std::uint8_t party_relic_effects, const BattleApproachWeights& weights,
                             const std::array<std::uint8_t, 256>& random_table) {
    validate_battle_state(state);
    auto next = state;
    next.leap_allowed = formation.leap;
    update_battle_participation(next);
    next.entry.approach =
        choose_approach(formation.allowed_approaches, (party_relic_effects & 2U) != 0,
                        next.participation.ally_count, weights, random_table, next.random);
    for (auto& stats : next.stats)
        normalize_battle_immunity(stats, next.morph.permanent);
    apply_approach(next, formation.preemptive, (party_relic_effects & 1U) != 0, random_table);
    initialize_gauges(next, random_table);
    state = std::move(next);
}
} // namespace ff6
