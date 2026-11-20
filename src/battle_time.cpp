#include "ff6/battle_time.hpp"

namespace ff6 {
BattleEscapeResult consume_battle_escape(BattleState& destination, bool held) {
    validate_battle_state(destination);
    auto state = destination;
    BattleEscapeResult result;
    // C2/20DE consumes the request, but C2/007D owns its admission reset.
    // Keep the latch through escape/failure presentation and queued counters.
    // Clearing it here lets held L+R starve ordinary turns with failure messages.
    if (!held) {
        destination = std::move(state);
        return result;
    }
    if (state.display.cannot_run) {
        result.forbidden = true;
        destination = std::move(state);
        return result;
    }
    const auto candidates = state.time.escaped;
    state.time.escaped.reset();
    for (unsigned slot = 4; slot-- > 0;) {
        auto& turn = state.turns[slot];
        const auto& status = state.status.targets[slot];
        if (!candidates[slot] || (turn.atb >> 8U) || turn.status_actions_blocked ||
            turn.actions_blocked || !status.present ||
            (status.active & (status_mask(BattleStatus::zombie) | status_mask(BattleStatus::hide))))
            continue;
        result.departing[slot] = true;
        state.departed_characters.set(slot);
        state.time.cannot_escape[slot] = true;
        state.untargetable[slot] = true;
        turn.normal.clear();
        turn.player_input = turn.menu_requests_allowed = false;
        for (auto& entry : state.advance_schedule.entries)
            if (entry == slot) entry.reset();
        for (auto& entry : state.ready_schedule.entries)
            if (entry == slot) entry.reset();
        release_battle_target_links(state, static_cast<std::uint8_t>(slot));
        state.menu_requests.push_back({static_cast<std::uint8_t>(slot), false, false});
    }
    destination = std::move(state);
    return result;
}
namespace {
using S = BattleStatus;
// Aggregate reviewed work follows semantic decisions, never an instruction
// stream. Child allocations/menu requests retain their order for the separate
// scheduler to resolve against its actual resource state.
struct TimeWork {
    BattleTimeWorkTrace trace;
    bool enabled;
    void add(std::uint32_t cycles, std::uint32_t slow) {
        if (!enabled)
            return;
        if (trace.empty() || trace.back().kind != BattleTimeWorkKind::execution)
            trace.push_back({});
        auto& out = trace.back();
        out.cycles += cycles;
        out.fast_rom_clocks += cycles * 6U + slow * 2U;
    }
    void action(const BattleState& state, std::optional<unsigned> slot = {}) {
        if (!enabled)
            return;
        BattleTimeWorkItem item;
        item.kind = slot ? BattleTimeWorkKind::retaliation_action : BattleTimeWorkKind::immediate_action;
        item.character = static_cast<std::uint8_t>(slot.value_or(0));
        item.chain_length = slot ? state.turns[*slot].retaliation.size() : state.immediate_actions.size();
        item.waived = state.waive_next_action_mp;
        trace.push_back(item);
    }
    void menu(unsigned slot, bool controlled) {
        if (!enabled)
            return;
        BattleTimeWorkItem item;
        item.kind = BattleTimeWorkKind::menu_request;
        item.character = static_cast<std::uint8_t>(slot);
        item.controlled = controlled;
        trace.push_back(item);
    }
};
bool has(const BattleState& state, unsigned slot, S condition) {
    return (state.status.targets[slot].active & status_mask(condition)) != 0;
}
void request_escape(BattleState& state, bool held, TimeWork& work) {
    if (!held) {
        work.add(13, 3);
        return;
    }
    if (state.time.escaped.none()) {
        work.add(19, 4);
        return;
    }
    if (state.colosseum) {
        work.add(25, 5);
        return;
    }
    if (state.time.escape_action_pending) {
        work.add(34, 7);
        return;
    }
    work.add(32, 5);
    work.action(state);
    state.time.escape_action_pending = true;
    append_battle_immediate_action(state, BattleEscapeAction{});
}
void check_escape(BattleState& state, unsigned slot, bool held,
                  const std::array<std::uint8_t, 256>& random, TimeWork& work) {
    if (slot >= 4) {
        work.add(11, 2);
        return;
    }
    if (!held) {
        work.add(17, 3);
        return;
    }
    if (state.time.cannot_escape[slot] || state.participation.enemy_characters[slot]) {
        work.add(31, 6); return;
    }
    work.add(24, 4);
    if (state.display.escape_difficulty == 0) {
        work.add(31, 8); // Mark escape, then tail-call the common request owner.
        state.time.escaped[slot] = true;
        request_escape(state, held, work);
        return;
    }
    work.add(125, 24); // Difficulty branch, RandA/MultAB, wrapped byte accumulation.
    auto& time = state.time.targets[slot];
    // RandA consumes a draw even for a zero rate. The byte counter wraps before
    // comparing with difficulty; overflow is not a guaranteed successful escape.
    const auto increment = (unsigned{state.random.next(random)} * time.escape_rate) >> 8U;
    time.escape_counter = wrap8(unsigned{time.escape_counter} + increment + 1U);
    if (time.escape_counter >= state.display.escape_difficulty) {
        work.add(18, 5);
        state.time.escaped[slot] = true;
    } else {
        work.add(9, 2);
    }
}
void expire_counters(BattleState& state, unsigned slot, bool stop_expired, TimeWork& work) {
    work.add(3, 1);
    auto& s = state.status.targets[slot];
    std::bitset<4> expired;
    expired[0] = stop_expired;
    work.add(slot >= 4 ? 1 : 0, 0); // Sleep counter read crosses its source table page.
    unsigned bit = 1;
    for (auto* counter : {&s.reflect_counter, &s.freeze_counter, &s.sleep_counter}) {
        if (*counter != 0) {
            --*counter;
            expired[bit] = *counter == 0;
            work.add(*counter == 0 ? 22 : 16, *counter == 0 ? 5 : 3);
        } else {
            work.add(7, 1);
        }
        ++bit;
    }
    if (expired.any()) {
        work.add(10, 1);
        work.action(state);
        append_battle_immediate_action(
            state, BattleTimerExpiredAction{static_cast<std::uint8_t>(slot), expired});
    } else {
        work.add(12, 3);
    }
}
void periodic_action(BattleState& state, unsigned slot, TimeWork& work) {
    auto& time = state.time.targets[slot];
    if (time.periodic_pending) {
        work.add(15, 3);
        return;
    }
    work.add(8, 1);
    std::optional<std::uint8_t> effect;
    if (time.poison_trigger) {
        time.poison_trigger = false;
        if (!has(state, slot, S::poison)) {
            work.add(30, 5);
            return; // Poison wins this visit even when the condition is absent.
        }
        work.add(26, 3);
        effect = 4;
    } else if (time.regen_trigger) {
        time.regen_trigger = false;
        if (has(state, slot, S::sap) || state.status.targets[slot].phantasm) {
            work.add(33, 4);
            effect = 64;
        } else if (has(state, slot, S::regen)) {
            work.add(40U + (slot >= 4), 5);
            effect = 2;
        } else {
            work.add(47U + (slot >= 4), 7);
            return;
        }
    } else {
        work.add(14, 1);
    }
    if (effect) {
        work.add(16, 4);
        work.action(state, slot);
        append_battle_retaliation_action(state, static_cast<std::uint8_t>(slot),
                                         {34, *effect, 0, 0});
        work.add(20, 4);
    } else if (const auto target = state.links[slot].seize_target) {
        work.add(35, 9);
        work.action(state, slot);
        const auto mask = static_cast<std::uint16_t>(1U << (*target < 4 ? *target : *target + 4U));
        append_battle_retaliation_action(state, static_cast<std::uint8_t>(slot), {45, 0, mask, 0});
        work.add(17, 4);
    } else {
        work.add(13, 3);
        return;
    }
    time.periodic_pending = true;
}
void status_counters(BattleState& state, BattleTimeInput input,
                     const std::array<std::uint8_t, 256>& random, TimeWork& work) {
    auto& time = state.time;
    const auto phase = time.status_phase & 15U;
    time.status_phase = wrap8(unsigned{time.status_phase} + 1U);
    if (phase < 10) {
        auto& status = state.status.targets[phase];
        if (!status.present || state.departed(phase)) {
            work.add(35, 6);
            return;
        }
        work.add(28, 4);
        auto& target = time.targets[phase];
        auto& turn = state.turns[phase];
        const auto sum = unsigned{target.status_fraction} + turn.rates.status;
        target.status_fraction = wrap8(sum);
        if (sum < 256) {
            work.add(24, 5);
            return;
        }
        work.add(17, 3);
        work.add(phase >= 8 ? 1 : 0, 0); // Stop counter table page boundary.
        if (status.stop_counter) {
            if (--status.stop_counter == 0) {
                work.add(20, 3);
                expire_counters(state, phase, true, work);
            } else {
                work.add(22, 5);
            }
            return; // Expiring Stop does not tick condemned, escape or periodic work.
        }
        if (turn.status_actions_blocked) {
            work.add(22, 4);
            return;
        }
        work.add(15, 2);
        if (turn.condemned >= 2) {
            if (--turn.condemned == 1) {
                work.add(35, 5);
                work.action(state);
                append_battle_immediate_action(state,
                                               BattleDoomAction{static_cast<std::uint8_t>(phase)});
            } else {
                work.add(20, 2);
            }
        } else {
            work.add(9, 1);
        }
        work.add(6, 2);
        check_escape(state, phase, input.escape_held, random, work);
        work.add(6, 2);
        periodic_action(state, phase, work);
        work.add(8, 2);
        expire_counters(state, phase, false, work);
        target.periodic_phase = wrap8(unsigned{target.periodic_phase} + 1U);
        work.add(25U + (phase >= 8), 3);
        if ((target.periodic_phase & 7U) == 2) {
            target.poison_trigger = true;
            work.add(19, 4);
        } else if ((target.periodic_phase & 3U) == 0) {
            target.regen_trigger = true;
            work.add(19, 4);
        } else {
            work.add(8, 2);
        }
        return;
    }
    work.add(29, 3);
    if (phase == 10) {
        if (time.roulette_target) {
            work.add(82U + 14U * (9U - *time.roulette_target), 16U + 2U * (9U - *time.roulette_target));
            work.action(state);
            append_battle_immediate_action(state, BattleDoomAction{*time.roulette_target});
            time.roulette_target.reset();
        } else {
            work.add(13, 3);
        }
    } else if (phase == 11) {
        work.add(25, 8);
        time.elapsed = wrap16(unsigned{time.elapsed} + 1U);
        for (unsigned i = 0; i < 10; ++i) {
            if (!state.status.targets[i].present) work.add(17, 2);
            else if (has(state, i, S::petrify) || has(state, i, S::dead)) work.add(27, 4);
            else { work.add(35, 8); time.targets[i].elapsed = wrap16(unsigned{time.targets[i].elapsed} + 1U); }
        }
    } else if (phase == 15 && input.escape_held) {
        work.add(13, 2);
        if (state.display.cannot_run) {
            if (!time.escape_action_pending) {
                work.add(15, 2);
                work.action(state);
                time.escape_action_pending = true;
                append_battle_immediate_action(state, BattleEscapeAction{});
            } else {
                work.add(17, 4);
            }
        } else if ((time.status_phase & 112U) == 0) {
            work.add(8, 1); request_escape(state, true, work);
        } else {
            work.add(15, 3);
        }
    } else {
        work.add(phase == 15 ? 13 : 6, phase == 15 ? 3 : 2);
    }
}
void morph_counter(BattleState& state, std::uint8_t slot, TimeWork& work) {
    auto& morph = state.morph;
    const bool expired = morph.counter < morph.rate;
    morph.counter = wrap16(unsigned{morph.counter} + 65536U - morph.rate);
    work.add(expired ? 25 : 26, 6);
    if (expired) {
        work.add(100, 19);
        work.action(state, slot);
        morph.counter &= 255U; // Preserve the fractional low byte after borrow.
        morph.saved_gauge = 0;
        morph.owner.reset();
        // Revert ignores attack/target operands. Keep no unowned scratch values.
        append_battle_retaliation_action(state, slot, {4, 0, 0, 0});
    }
    work.add(15, 4);
    state.turns[slot].morph_display = static_cast<std::uint8_t>(morph.counter >> 8U);
}
void open_menu_or_schedule(BattleState& state, std::uint8_t slot, TimeWork& work) {
    if (slot >= 4) {
        work.add(52, 10);
        schedule_battle_turn(state, slot, false);
        return;
    }
    if (!state.turns[slot].menu_requests_allowed) {
        work.add(17, 3);
        return;
    }
    if (state.menus_disabled) {
        work.add(22, 4);
        return;
    }
    work.add(56, 10);
    work.menu(slot, state.links[slot].control_menu);
    state.turns[slot].weapon_locked = true;
    state.menu_requests.push_back({slot, true, state.links[slot].control_menu});
}
void target_time(BattleState& state, std::uint8_t slot, bool escape_held, TimeWork& work) {
    // An escaped actor no longer owns a playable turn. Keep its HP/status for
    // UpdateSRAM, without creating empty automatic actions behind the scene.
    if (state.departed(slot))
        return;
    // Morph advances before presence and condition filtering.
    if (state.morph.owner == slot) {
        work.add(12, 3);
        morph_counter(state, slot, work);
    } else work.add(7, 1);
    auto& turn = state.turns[slot];
    if (!state.status.targets[slot].present) {
        work.add(9, 1);
        return;
    }
    if (state.zinger && state.zinger->attacker == slot) {
        work.add(15, 2);
        return;
    }
    if (turn.weapon_locked || turn.status_actions_blocked || turn.action_ready || turn.actions_blocked) {
        work.add(19, 2); return;
    }
    work.add(turn.atb_stopped ? 22 : 23, 2);
    if (turn.atb_stopped) {
        if (!escape_held) work.add(7, 1);
        else if (has(state, slot, S::zombie)) work.add(15, 2);
        else if (slot >= 4) work.add(21, 3);
        else if (state.away_from_battle[slot]) work.add(27, 4);
        else work.add(state.participation.enemy_characters[slot] ? 32 : 33, 5);
    }
    if (turn.atb_stopped &&
        (!escape_held || has(state, slot, S::zombie) || slot >= 4 || state.away_from_battle[slot] ||
         state.participation.enemy_characters[slot])) {
        const auto sum = unsigned{turn.advance_wait} + (turn.rates.atb >> 1U);
        turn.advance_wait = wrap16(sum);
        const bool ready = sum > 65535 || (turn.advance_wait >> 8U) >= turn.advance_duration;
        work.add(sum > 65535 ? 35 : ready ? 43 : 44, sum > 65535 ? 8 : 9);
        if (ready) {
            work.add(60, 15);
            turn.advance_duration = 255;
            schedule_battle_turn(state, slot, true);
            turn.action_ready = true;
        } else {
            work.add(6, 2);
        }
    }
    if ((turn.atb >> 8U) == 0) {
        work.add(7, 1);
        return;
    }
    work.add(12, 3);
    const auto sum = unsigned{turn.atb} + turn.rates.atb;
    turn.atb = wrap16(sum);
    if (sum <= 65535) {
        work.add(31, 8);
        return;
    }
    work.add(slot < 4 ? 34 : 29, slot < 4 ? 8 : 6);
    if (slot < 4) request_escape(state, escape_held, work);
    turn.atb &= 255U;
    turn.advance_wait &= 255U;
    turn.advance_duration = 255;
    work.add(turn.atb_stopped ? 26 : 25, 4);
    if (!turn.atb_stopped) {
        work.add(turn.player_input ? 25 : 26, 6);
        turn.atb_stopped = true;
        if (!turn.player_input) {
            work.add(47, 10);
            schedule_battle_turn(state, slot, false);
            return;
        }
    }
    work.add(23, 6);
    state.status.targets[slot].menu_allowed = true;
    open_menu_or_schedule(state, slot, work);
}
} // namespace
bool advance_battle_time(BattleState& state, BattleTimeInput input,
                         const std::array<std::uint8_t, 256>& random_table, BattleTimeWorkTrace* output) {
    validate_battle_state(state);
    TimeWork work{{}, output != nullptr};
    if (state.time.wait_mode && input.menu_wait) {
        work.add(30, 8);
        state.time.previous_clock = input.clock;
        if (output) *output = std::move(work.trace);
        return false;
    }
    // Compare against the previous byte and its successor, including wrap. A
    // large elapsed gap still advances once; this is not a catch-up accumulator.
    if (input.clock == state.time.previous_clock ||
        input.clock == wrap8(unsigned{state.time.previous_clock} + 1U)) {
        work.add(input.clock == state.time.previous_clock ? 34 : 46,
                 input.clock == state.time.previous_clock ? 8 : 9);
        if (output) *output = std::move(work.trace);
        return false;
    }
    work.add(39, 5);
    work.add((state.time.turns & 255U) == 255 ? 14 : 9, (state.time.turns & 255U) == 255 ? 4 : 2);
    auto next = state;
    next.time.turns = wrap16(unsigned{next.time.turns} + 1U);
    work.add(6, 2);
    status_counters(next, input, random_table, work);
    work.add(2, 0);
    for (unsigned remaining = 10; remaining; --remaining) {
        target_time(next, static_cast<std::uint8_t>(remaining - 1U), input.escape_held, work);
        work.add(remaining == 1 ? 6 : 7, 0);
    }
    work.add(189, 34); // Four counter readouts, saved clock and status/long return.
    for (unsigned i = 0; i < 4; ++i)
        next.display.counters[i] = {wrap8(static_cast<unsigned>(next.turns[i].atb >> 8U) + 255U),
                                    next.turns[i].morph_display, next.turns[i].condemned};
    next.time.previous_clock = input.clock;
    state = std::move(next);
    if (output) *output = std::move(work.trace);
    return true;
}
} // namespace ff6
