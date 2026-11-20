#include "ff6/battle_display.hpp"

namespace ff6 {
namespace {
BattleDisplayState make_display(const BattleState& state) {
    BattleDisplayState result;
    std::bitset<6> visible_alive;
    for (unsigned i = 0; i < 6; ++i)
        visible_alive[i] = state.status.targets[i + 4].monster_alive;
    // Pincer escape checks the presentation groups/alive bits, independently
    // of the subsequent presence/status filter used for monster names.
    result.cannot_run = state.entry.approach == BattleApproach::pincer &&
                        (state.monsters_on_left & visible_alive).any() &&
                        (state.monsters_on_right & visible_alive).any();
    constexpr auto excluded = status_mask(BattleStatus::dead) | status_mask(BattleStatus::petrify) |
                              status_mask(BattleStatus::zombie);
    for (unsigned i = 0; i < 6; ++i) {
        const auto slot = i + 4;
        const auto& target = state.status.targets[slot];
        if (!target.present || target.monster_dead_or_escaped ||
            !state.participation.active[slot] || (target.active & excluded) != 0)
            continue;
        const auto& monster = state.monster_information[i];
        if (monster.cannot_escape) {
            result.cannot_run = true;
            result.escape_tools_blocked = true;
        }
        // C2/5CD0 constructs this from the harder-to-run carry with
        // clear/rotate/set-carry/rotate/shift. There is no stored difficulty
        // table or selected numeric operand to extract here.
        result.escape_difficulty = static_cast<std::uint8_t>(unsigned{result.escape_difficulty} +
                                                             (monster.harder_to_escape ? 6U : 2U));
        // Hidden names still contribute difficulty and escape restrictions.
        if ((state.stats[slot].monster_flags & 4U) != 0)
            continue;
        if (!monster.name_identity)
            throw std::invalid_argument("Visible monster has no name identity");
        for (auto& row : result.names) {
            if (!row.identity) {
                row.identity = monster.name_identity;
                ++result.distinct_names;
            }
            if (row.identity == monster.name_identity) {
                ++row.count;
                break;
            }
        }
    }
    if (state.entry.approach == BattleApproach::side || state.entry.preemptive)
        result.escape_difficulty = 0;
    if (state.participation.living_enemy_characters.any())
        result.cannot_run = true;
    for (unsigned i = 0; i < 4; ++i)
        result.counters[i] = {wrap8(static_cast<unsigned>(state.turns[i].atb >> 8) + 255U),
                              state.turns[i].morph_display, state.turns[i].condemned};
    return result;
}
std::optional<BattleAnnouncement> announcement(const BattleState& state,
                                               const BattleFormation& formation) {
    // The formation flag suppresses every approach announcement, not just
    // preemptive. Back/pincer return before the preemptive override.
    if (!formation.preemptive)
        return {};
    if (state.entry.approach == BattleApproach::back)
        return BattleAnnouncement::back;
    if (state.entry.approach == BattleApproach::pincer)
        return BattleAnnouncement::pincer;
    if (state.entry.preemptive)
        return BattleAnnouncement::preemptive;
    if (state.entry.approach == BattleApproach::side)
        return BattleAnnouncement::side;
    return {};
}
} // namespace
void update_battle_display(BattleState& state) {
    validate_battle_state(state);
    state.display = make_display(state);
}
void update_battle_action_readouts(BattleState& state) {
    validate_battle_state(state);
    auto next = make_display(state);
    next.cannot_run = state.display.cannot_run;
    next.escape_tools_blocked = state.display.escape_tools_blocked;
    next.escape_difficulty = state.display.escape_difficulty;
    state.display = std::move(next);
}
void prepare_battle_presentation(BattleState& state, const BattleFormation& formation) {
    validate_battle_state(state);
    auto next = state;
    if (const auto message = announcement(next, formation))
        append_battle_immediate_action(next, BattleAnnouncementAction{*message});
    next.display = make_display(next);
    // Publication deliberately precedes these changes; a returning jumper's
    // readout does not acquire its cleared counter until the next publication.
    for (unsigned remaining = 4; remaining != 0; --remaining) {
        const auto slot = static_cast<std::uint8_t>(remaining - 1U);
        if (!next.away_from_battle[slot])
            continue;
        auto& turn = next.turns[slot];
        turn.atb_stopped = true;
        turn.action_ready = true;
        turn.atb &= 255U;
        auto& schedule = next.ready_schedule;
        schedule.entries[schedule.write] = slot;
        schedule.write = wrap8(unsigned{schedule.write} + 1U);
        // Jump uses its command's ordinary targeting, not an attack identity.
        // The original leaves that unused operand stale; native requests use 0.
        turn.normal.push_back({22, 0, 0, 0});
        next.waive_next_action_mp = false;
    }
    validate_battle_state(next); // Shared 128-action capacity, including Jump.
    state = std::move(next);
}
} // namespace ff6
