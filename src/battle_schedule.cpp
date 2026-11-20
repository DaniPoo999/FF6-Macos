#include "ff6/battle_schedule.hpp"
#include "rom-materials/summon_advance_wait.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
using S = BattleStatus;
using U = BattleStatusUpdate;
bool has(const BattleState& state, std::size_t slot, S status) {
    return (state.status.targets[slot].active & status_mask(status)) != 0;
}
void remove_scheduled(BattleParticipantSchedule& queue, std::size_t slot) {
    auto index = queue.read;
    // Preserve the source's post-increment unsigned comparison, including
    // its single-slot search for most wrapped/empty queues. See the contract.
    do {
        if (queue.entries[index] == slot)
            queue.entries[index].reset();
        index = wrap8(unsigned{index} + 1U);
    } while (index < queue.write);
}
void cancel_input(BattleState& state, std::size_t slot) {
    auto& turn = state.turns[slot];
    auto& status = state.status.targets[slot];
    // The source reads only the low target-mask byte here. Monster bits do
    // not participate in its untargetability test.
    if (!has(state, slot, S::hide) && !(slot < 4 && state.untargetable[slot]) &&
        status.menu_allowed) {
        schedule_battle_turn(state, static_cast<std::uint8_t>(slot), false);
        status.menu_allowed = false;
    }
    turn.player_input = turn.weapon_locked = false;
    if (slot < 4)
        state.menu_requests.push_back({static_cast<std::uint8_t>(slot), false, false});
}
void disable_atb(BattleState& state, std::size_t slot) {
    auto& turn = state.turns[slot];
    turn.weapon_locked = turn.atb_stopped = turn.action_ready = false;
    state.status.targets[slot].wake_action_pending = false;
    turn.atb = static_cast<std::uint16_t>((turn.atb & 255U) | 256U);
    cancel_input(state, slot);
}
void remove_actions(BattleState& state, std::size_t slot) {
    auto& turn = state.turns[slot];
    turn.normal.clear(); // Retaliation belongs to a different chain.
    remove_scheduled(state.advance_schedule, slot);
    if ((turn.atb >> 8U) == 0) {
        turn.atb |= 0xff00U;
        turn.weapon_locked = turn.atb_stopped = turn.action_ready = false;
    }
    if (slot < 4)
        append_battle_immediate_action(state,
                                       BattleRestorePoseAction{static_cast<std::uint8_t>(slot)});
}
void quetzalli(BattleState& state, std::size_t slot) {
    auto& turn = state.turns[slot];
    state.away_from_battle.set(slot);
    turn.weapon_locked = turn.action_ready = turn.actions_blocked = false;
    turn.atb_stopped = true;
    remove_scheduled(state.ready_schedule, slot);
    turn.menu_requests_allowed = false;
    turn.advance_wait &= 255U;
    turn.advance_duration = FF6_ROM_VALUE_SUMMON_ADVANCE_WAIT_0;
}
void swap_morph_commands(BattleState& state, std::size_t slot, bool morphed) {
    const std::uint8_t wanted = morphed ? 3 : 4;
    auto swap = [wanted](std::uint8_t& value) {
        if (value == wanted)
            value ^= 7U;
    };
    if (slot < 4) {
        for (auto& command : state.availability.commands[slot].slots)
            if (command)
                swap(command->identity);
    } else if (slot < 8) {
        // Original monster indices traverse the adjacent control-command lists.
        for (auto& command : state.control_commands[slot - 4])
            if (command)
                swap(command->identity);
    } else {
        auto& spells = state.availability.spells[0];
        auto& identity_and_cost = slot == 8 ? spells.esper : spells.magic[2];
        if (identity_and_cost) {
            swap(identity_and_cost->identity);
            swap(identity_and_cost->mp_cost);
        }
        auto& targeting = spells.magic[slot == 8 ? 0 : 3];
        if (targeting)
            swap(targeting->targeting);
        // The fourth overlap is a disabled-history byte. Swapping 3/4 changes
        // neither its meaningful high bit nor any later eligibility decision.
    }
}
void update_morph(BattleState& state, std::size_t slot, const BattleCommandData& commands) {
    auto& turn = state.turns[slot];
    if (!state.retaliation_action && (turn.atb >> 8U) == 0) {
        turn.atb_stopped = true;
        state.status.targets[slot].menu_allowed = true;
    }
    const bool morphed = has(state, slot, S::morph);
    swap_morph_commands(state, slot, morphed);
    auto& morph = state.morph;
    if (!morphed) {
        if (morph.owner)
            morph.saved_gauge = static_cast<std::uint8_t>(
                (unsigned{morph.saved_gauge} * (morph.counter >> 8U)) >> 8U);
        morph.owner.reset();
        turn.morph_display = 0;
        if (slot < 4)
            refresh_battle_commands(state.availability, slot, commands, state.stats[slot],
                                    state.status.targets[slot].active, morph.saved_gauge);
        morph.counter = 0;
        return;
    }
    if (morph.owner || morph.permanent)
        return;
    morph.owner = static_cast<std::uint8_t>(slot);
    morph.counter = 65535;
    // SNES division by zero yields a quotient of 65535. Express it explicitly
    // rather than asking C++ to divide by zero.
    unsigned rate = morph.saved_gauge == 0 ? 65535U : 65535U / morph.saved_gauge;
    // UpdateMorph (C2/0B01) halves depletion when persistent switch 39 is set.
    // Div preserves the carry containing that bit (C2/4792).
    if (morph.double_duration)
        rate >>= 1U;
    morph.rate = static_cast<std::uint16_t>(std::min(rate >> 3U, 2047U) + 1U);
}
void player_menu(BattleState& state, std::size_t slot) {
    if (slot >= 4 || state.actors[slot] == 13 || state.ai[slot].entry || state.colosseum)
        return;
    auto& turn = state.turns[slot];
    auto& status = state.status.targets[slot];
    turn.player_input = true;
    if (state.quick && state.quick->target == slot) {
        turn.atb_stopped = true;
        status.menu_allowed = true;
    }
    constexpr auto disallowed = status_mask(S::dead) | status_mask(S::petrify) |
                                status_mask(S::zombie) | status_mask(S::sleep) |
                                status_mask(S::confuse) | status_mask(S::berserk) |
                                status_mask(S::dance) | status_mask(S::hide) | status_mask(S::rage);
    if (state.untargetable[slot] || state.links[slot].seize_attacker ||
        state.links[slot].charm_attacker || (status.active & disallowed) != 0) {
        cancel_input(state, slot);
        return;
    }
    if (!status.menu_allowed || !turn.normal.empty())
        return;
    turn.atb_stopped = true;
    if (!turn.menu_requests_allowed || state.menus_disabled)
        return;
    turn.weapon_locked = true;
    state.menu_requests.push_back(
        {static_cast<std::uint8_t>(slot), true, state.links[slot].control_menu});
}
void resume_pending(BattleState& state, std::size_t slot) {
    auto& turn = state.turns[slot];
    turn.status_actions_blocked = turn.actions_blocked = true;
    if ((state.quick && state.quick->target != slot) || has(state, slot, S::dead) ||
        has(state, slot, S::petrify) || has(state, slot, S::stop))
        return;
    turn.status_actions_blocked = false;
    const auto controller = state.links[slot].controlled_by;
    if ((controller && !controller->remove_pending) || has(state, slot, S::sleep) ||
        has(state, slot, S::frozen) || state.links[slot].seize_attacker)
        return;
    turn.actions_blocked = false;
    if (!turn.reschedule_pending)
        return;
    turn.reschedule_pending = false;
    if (!turn.normal.empty())
        schedule_battle_turn(state, static_cast<std::uint8_t>(slot), turn.bypass_advance_wait);
}
void near_fatal(BattleState& state, std::size_t slot) {
    auto& turn = state.turns[slot];
    if (turn.status_actions_blocked || !has(state, slot, S::near_fatal) ||
        !turn.near_fatal_available)
        return;
    // The one-shot is consumed even if none of these relic effects is equipped.
    turn.near_fatal_available = false;
    for (unsigned spell = 0; spell < 3; ++spell)
        if ((state.stats[slot].relic_effects[3] & (1U << spell)) != 0)
            append_battle_immediate_action(
                state, BattleRelicSpellAction{static_cast<std::uint8_t>(slot),
                                              static_cast<BattleRelicSpell>(spell)});
}
} // namespace
void cancel_battle_actions_for_gau(BattleState& state) {
    for (unsigned i = 4; i-- > 0;)
        if (state.participation.living_allies[i] && !state.away_from_battle[i])
            remove_actions(state, i);
}
void refresh_battle_actions(BattleState& state, const BattleCommandData& commands,
                            const std::array<std::uint8_t, 256>& random_table,
                            std::uint8_t attacker_level) {
    validate_battle_state(state);
    auto next = state;
    for (std::size_t remaining = 10; remaining != 0; --remaining) {
        const auto slot = remaining - 1;
        auto& status = next.status.targets[slot];
        if (!status.present)
            continue;
        if (next.departed(static_cast<unsigned>(slot))) {
            disable_atb(next, slot);
            next.turns[slot].normal.clear();
            next.turns[slot].reschedule_pending = false;
            status.updates.reset();
            continue;
        }
        next.attack_memory[slot].retaliation_pending = false;
        auto& turn = next.turns[slot];
        status.capture_retaliation = false;
        if (has(next, slot, S::dead) || status.wake_action_pending)
            disable_atb(next, slot);
        // None of these handlers adds new bits to the dispatcher's live pending
        // byte. Consume its semantic flags in the verified source order.
        const auto work = status.updates;
        if (work[static_cast<unsigned>(U::quetzal)])
            quetzalli(next, slot);
        if (work[static_cast<unsigned>(U::spells)] && slot < 4)
            refresh_battle_spells(next.availability, slot, next.stats[slot].mp.current,
                                  status.active);
        if (work[static_cast<unsigned>(U::remove_advance_actions)]) {
            remove_actions(next, slot);
            turn.reschedule_pending = true;
        }
        if (work[static_cast<unsigned>(U::start_condemned)])
            turn.condemned = start_battle_condemned(attacker_level, random_table, next.random);
        if (work[static_cast<unsigned>(U::stop_condemned)])
            turn.condemned = 0;
        if (work[static_cast<unsigned>(U::commands)] && slot < 4)
            refresh_battle_commands(next.availability, slot, commands, next.stats[slot],
                                    status.active, next.morph.saved_gauge);
        if (work[static_cast<unsigned>(U::speed)])
            turn.rates = battle_speed_rates(next.stats[slot].speed, status.active, slot >= 4,
                                            next.speed_constant);
        if (work[static_cast<unsigned>(U::morph)])
            update_morph(next, slot, commands);
        status.updates.reset();
        player_menu(next, slot);
        resume_pending(next, slot);
        if (turn.status_actions_blocked || turn.actions_blocked)
            turn.defending = false;
        near_fatal(next, slot);
    }
    state = std::move(next);
}
} // namespace ff6
