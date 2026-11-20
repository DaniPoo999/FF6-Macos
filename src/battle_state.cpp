#include "ff6/battle_state.hpp"

namespace ff6 {
namespace {
using S = BattleStatus;
constexpr auto mask(S status) {
    return status_mask(status);
}
void resume_actions(BattleStatusTarget& target) {
    target.wake_action_pending = true;
    target.updates.set(static_cast<unsigned>(BattleStatusUpdate::remove_advance_actions));
    target.menu_allowed = false;
}
void release_control(BattleState& state, std::size_t target, std::size_t controller) {
    state.links[controller].control_menu = false;
    state.status.targets[controller].active &= ~mask(S::control);
    state.links[target].controlled_by.reset();
    state.links[controller].controls.reset();
    if (target >= 4)
        state.status.targets[target].reversed_facing = false;
    resume_actions(state.status.targets[target]);
    resume_actions(state.status.targets[controller]);
}
void clear_death(BattleStatusTarget& target) {
    target.active &= ~mask(S::dead);
    target.updates.reset(static_cast<unsigned>(BattleStatusUpdate::remove_advance_actions));
}
void release_death_links(BattleState& state, std::size_t slot) {
    if (state.zinger && state.zinger->target == slot) {
        std::bitset<6> monsters;
        if (state.zinger->attacker >= 4)
            monsters.set(state.zinger->attacker - 4U);
        append_battle_immediate_action(
            state,
            BattleMonsterVisibilityAction{monsters, BattleMonsterVisibility::hide_keep_battle,
                                          BattleMonsterAnimation::instant});
        append_battle_immediate_action(
            state,
            BattleMonsterVisibilityAction{monsters, BattleMonsterVisibility::revive_current_hp,
                                          BattleMonsterAnimation::fade_down});
        state.zinger.reset();
    }
    auto& links = state.links[slot];
    if (const auto other = links.love_target) {
        links.love_target.reset();
        state.links[*other].love_attacker.reset();
    }
    if (const auto other = links.love_attacker) {
        state.links[*other].love_target.reset();
        links.love_attacker.reset();
    }
    if (const auto other = links.charm_target) {
        links.charm_target.reset();
        state.links[*other].charm_attacker.reset();
    }
    if (const auto other = links.charm_attacker) {
        state.links[*other].charm_target.reset();
        links.charm_attacker.reset();
    }
    if (state.quick && state.quick->target == slot)
        state.quick.reset();
}
} // namespace
void release_battle_target_links(BattleState& state, std::uint8_t participant) {
    if (participant >= 10)
        throw std::invalid_argument("Invalid departing participant");
    release_death_links(state, participant);
}
void validate_battle_state(const BattleState& state) {
    if (static_cast<unsigned>(state.entry.approach) > 3)
        throw std::invalid_argument("Invalid battle approach");
    for (const auto& monster : state.monster_information)
        if (monster.name_identity && *monster.name_identity >= 384)
            throw std::invalid_argument("Invalid monster name identity");
    auto participant = [](std::uint8_t slot) {
        if (slot >= 10)
            throw std::invalid_argument("Invalid battle participant reference");
    };
    for (const auto& memory : state.attack_memory) {
        if (memory.retaliation_attacker)
            participant(*memory.retaliation_attacker);
        for (const auto& observed : {memory.command, memory.attack, memory.elements})
            if (observed)
                participant(observed->attacker);
    }
    for (const auto& links : state.links) {
        for (const auto& link : {links.controls, links.controlled_by})
            if (link)
                participant(link->target);
        for (const auto& link : {links.love_target, links.love_attacker, links.charm_target,
                                 links.charm_attacker, links.seize_target, links.seize_attacker})
            if (link)
                participant(*link);
    }
    if (state.zinger) {
        participant(state.zinger->attacker);
        participant(state.zinger->target);
    }
    if (state.quick)
        participant(state.quick->target);
    if (state.morph.owner)
        participant(*state.morph.owner);
    if (state.time.roulette_target)
        participant(*state.time.roulette_target);
    for (const auto actor : state.actors)
        if (actor && *actor >= 64)
            throw std::invalid_argument("Invalid battle actor identity");
    for (const auto* schedule :
         {&state.advance_schedule, &state.ready_schedule, &state.retaliation_schedule})
        for (const auto entry : schedule->entries)
            if (entry)
                participant(*entry);
    std::size_t count = state.immediate_actions.size();
    for (const auto& turn : state.turns) {
        count += turn.normal.size() + turn.retaliation.size();
        for (const auto* chain : {&turn.normal, &turn.retaliation})
            for (const auto& action : *chain)
                if (action.command >= 52)
                    throw std::invalid_argument("Invalid pending battle command");
    }
    if (count > 128)
        throw std::invalid_argument("Invalid pending action count");
    for (const auto& action : state.immediate_actions)
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, BattleMonsterVisibilityAction>) {
                    if ((value.effect != BattleMonsterVisibility::hide_keep_battle &&
                         value.effect != BattleMonsterVisibility::revive_current_hp) ||
                        (value.animation != BattleMonsterAnimation::instant &&
                         value.animation != BattleMonsterAnimation::fade_down))
                        throw std::invalid_argument("Invalid immediate visibility action");
                } else if constexpr (std::is_same_v<T, BattleAnnouncementAction>) {
                    if (static_cast<unsigned>(value.message) > 3)
                        throw std::invalid_argument("Invalid battle announcement");
                } else if constexpr (std::is_same_v<T, BattleScriptedEventAction>) {
                    if (value.identity >= 33)
                        throw std::invalid_argument("Invalid immediate battle event");
                } else if constexpr (!std::is_same_v<T, BattleEscapeAction>) {
                    participant(value.target);
                    if constexpr (std::is_same_v<T, BattleRelicSpellAction>)
                        if (static_cast<unsigned>(value.spell) > 2)
                            throw std::invalid_argument("Invalid relic spell action");
                    if constexpr (std::is_same_v<T, BattleRestorePoseAction>)
                        if (value.target >= 4)
                            throw std::invalid_argument("Invalid character pose target");
                }
            },
            action);
    for (const auto& request : state.menu_requests)
        if (request.character >= 4)
            throw std::invalid_argument("Invalid menu character");
}
void append_battle_immediate_action(BattleState& state, BattleImmediateAction action) {
    std::size_t count = state.immediate_actions.size();
    for (const auto& turn : state.turns)
        count += turn.normal.size() + turn.retaliation.size();
    if (count >= 128)
        throw std::runtime_error("Immediate battle action capacity exhausted");
    state.immediate_actions.push_back(std::move(action));
    state.waive_next_action_mp = false;
}
void schedule_battle_turn(BattleState& state, std::uint8_t participant, bool ready) {
    if (participant >= 10)
        throw std::invalid_argument("Invalid scheduled participant");
    auto& queue = ready ? state.ready_schedule : state.advance_schedule;
    queue.entries[queue.write] = participant;
    queue.write = wrap8(unsigned{queue.write} + 1U);
    if (!ready)
        state.turns[participant].bypass_advance_wait = false;
}
std::uint8_t battle_character_command_mp_cost(const BattleState& state, unsigned slot, BattleQueuedCommand command) {
    if (slot >= 4)
        throw std::invalid_argument("Invalid character command cost participant");
    if (state.waive_next_action_mp || (command.command != 2 && command.command != 12 &&
                                       command.command != 23 && command.command != 25))
        return 0;
    const auto& spells = state.availability.spells[slot];
    if (command.attack >= 54 && command.attack < 81)
        return spells.esper ? spells.esper->mp_cost : std::uint8_t{255};
    if (command.attack < 54) {
        for (const auto& spell : spells.magic)
            if (spell && spell->identity == command.attack)
                return spell->mp_cost;
    } else if (command.attack >= 139 && command.attack < 163) {
        const auto& lore = spells.lores[command.attack - 139U];
        if (lore)
            return lore->mp_cost;
    }
    return 0;
}
void append_battle_normal_action(BattleState& state, std::uint8_t participant,
                                 BattleQueuedCommand action) {
    if (participant >= 10 || action.command >= 52)
        throw std::invalid_argument("Invalid normal action");
    std::size_t count = state.immediate_actions.size();
    for (const auto& turn : state.turns)
        count += turn.normal.size() + turn.retaliation.size();
    if (count >= 128)
        throw std::runtime_error("Normal action capacity exhausted");
    state.turns[participant].normal.push_back(action);
    state.waive_next_action_mp = false;
}
void append_battle_retaliation_action(BattleState& state, std::uint8_t participant,
                                      BattleQueuedCommand action) {
    if (participant >= 10 || action.command >= 52)
        throw std::invalid_argument("Invalid retaliation action");
    std::size_t count = state.immediate_actions.size();
    for (const auto& turn : state.turns)
        count += turn.normal.size() + turn.retaliation.size();
    if (count >= 128)
        throw std::runtime_error("Retaliation action capacity exhausted");
    auto& chain = state.turns[participant].retaliation;
    const bool first = chain.empty();
    chain.push_back(action);
    if (first) {
        auto& queue = state.retaliation_schedule;
        queue.entries[queue.write] = participant;
        queue.write = wrap8(unsigned{queue.write} + 1U);
    }
    state.waive_next_action_mp = false;
}
BattleState initialize_battle_state(const BattlePreparation& preparation, const BattleParty& party,
                                    const BattleStatusState& status) {
    BattleState result;
    result.status = status;
    result.available_characters = preparation.available_characters;
    result.monster_targets = party.monster_targets;
    result.availability = initialize_battle_availability(preparation.characters);
    result.random = preparation.random;
    result.speed_constant = preparation.configuration.speed_constant;
    result.skills = preparation.skills;
    result.potential_engulf_targets = preparation.characters.potential_engulf_targets;
    result.inventory = preparation.inventory;
    result.time.wait_mode = preparation.configuration.wait_mode;
    for (unsigned i = 0; i < 10; ++i)
        result.time.targets[i].periodic_phase = preparation.status_timers[i];
    result.colosseum = party.colosseum;
    result.entry.gau_recruitment = party.gau_may_return;
    for (std::size_t i = 0; i < 4; ++i) {
        result.actors[i] = party.slots[i].actor;
        result.participation.enemy_characters[i] = party.slots[i].enemy;
        result.entry.back_rows[i] = party.slots[i].back_row;
        result.ai[i] = party.slots[i].ai;
        if (const auto& character = preparation.characters.slots[i]) {
            result.stats[i] = character->combat.stats;
            result.equipment_characters[i] = character->combat.persistent;
            result.genji_gloves[i] = character->combat.genji_graphics;
            result.block_graphics[i] = character->combat.block_graphics;
            result.relics[i] = character->combat.relics;
            result.time.targets[i].escape_rate = character->combat.run_factor;
        }
    }
    for (std::size_t i = 0; i < 6; ++i)
        if (const auto& monster = preparation.monsters.slots[i]) {
            result.stats[i + 4] = monster->stats;
            result.steal_stock[i + 4].items = monster->steal_items;
            result.ai[i + 4] = monster->ai;
            result.monster_information[i] = {monster->name_identity, (monster->behavior & 1U) != 0,
                                             (monster->behavior & 8U) != 0,
                                             (monster->behavior & 16U) != 0,
                                             (monster->behavior & 4U) != 0,
                                             (monster->behavior & 32U) != 0, monster->identity,
                                             (monster->behavior & 128U) != 0, monster->gil};
        }
    for (const auto monster : preparation.monsters.first_strike_queue) {
        if (monster >= 6)
            throw std::invalid_argument("Invalid first-strike monster");
        // C2/2D58..2D5E retains CheckFirstStrike's carry in the global
        // menu gate. Scripted opening chains must run before player input.
        result.menus_disabled = true;
        const auto slot = static_cast<std::uint8_t>(monster + 4U);
        auto& turn = result.turns[slot];
        turn.atb_stopped = true;
        turn.atb &= 255U;
        turn.advance_wait = 0xff00;
        auto& queue = result.advance_schedule;
        queue.entries[queue.write] = slot;
        queue.write = wrap8(unsigned{queue.write} + 1U);
    }
    // Requests from the record loaders were already transferred to status.
    // Keep one live owner, even for hidden targets that have not consumed them.
    for (auto& stats : result.stats)
        stats.pending_status = 0;
    return result;
}
void resolve_battle_status_consequences(BattleState& state) {
    validate_battle_state(state);
    auto next = state;
    constexpr auto disrupt_control =
        mask(S::dead) | mask(S::petrify) | mask(S::zombie) | mask(S::berserk) | mask(S::confuse) |
        mask(S::sleep) | mask(S::dance) | mask(S::stop) | mask(S::rage) | mask(S::frozen);
    for (std::size_t remaining = 10; remaining != 0; --remaining) {
        const auto slot = remaining - 1;
        auto& status = next.status.targets[slot];
        if (!status.present || next.arrival_override[slot])
            continue;
        auto& links = next.links[slot];
        if ((status.active & disrupt_control) != 0) {
            if (links.controls)
                links.controls->remove_pending = true;
            if (links.controlled_by)
                links.controlled_by->remove_pending = true;
        }
        if ((status.active & (mask(S::dead) | mask(S::zombie))) != 0)
            next.stats[slot].hp.current = 0;
        if ((status.active & (mask(S::dead) | mask(S::petrify) | mask(S::zombie))) != 0) {
            if (slot >= 4)
                status.monster_dead_or_escaped = true;
            release_death_links(next, slot);
        }
        if ((status.active & mask(S::dead)) == 0)
            continue;
        if (slot < 4 && next.actors[slot] == 14)
            next.end_event = 6; // Banon death event.
        if (next.away_from_battle[slot]) {
            clear_death(status);
            next.deferred_death.set(slot);
        }
        if ((status.active & mask(S::zombie)) != 0)
            clear_death(status);
        if ((status.active & (mask(S::dead) | mask(S::reraise))) ==
            (mask(S::dead) | mask(S::reraise))) {
            status.active &= ~mask(S::reraise);
            // The original clears this presentation bit before the queued Raise.
            // It does not restore HP or remove death at this point.
            if (slot >= 4)
                status.monster_alive = false;
            append_battle_immediate_action(next,
                                           BattleRaiseAction{static_cast<std::uint8_t>(slot)});
        }
    }
    // Rage removal requests an equipment refresh in the original. Restore the
    // properties it replaced, preserving current vitals, timers and ailments.
    for (unsigned slot = 0; slot < 4; ++slot)
        if (next.rage_baseline[slot] && !(next.status.targets[slot].active & mask(S::rage))) {
            const auto& base = *next.rage_baseline[slot];
            auto& stats = next.stats[slot];
            stats.absorbed = base.absorbed; stats.nullified = base.nullified;
            stats.weak = base.weak; stats.blocked_status = base.blocked_status;
            stats.equipment_status = base.equipment_status;
            stats.instant_death_protection = base.instant_death_protection;
            stats.relic_effects[2] = static_cast<std::uint8_t>(
                (stats.relic_effects[2] & ~64U) | (base.relic_effects[2] & 64U));
            stats.metamorph = base.metamorph; stats.monster_special = base.monster_special;
            for (unsigned hand = 0; hand < 2; ++hand) stats.hands[hand].item = base.hands[hand].item;
            next.rage_baseline[slot].reset();
        }
    // Pending control cleanup is not gated by participant presence/arrival.
    for (std::size_t remaining = 10; remaining != 0; --remaining) {
        const auto slot = remaining - 1;
        auto& links = next.links[slot];
        if (const auto link = links.controlled_by; link && link->remove_pending)
            release_control(next, slot, link->target);
        if (const auto link = links.controls; link && link->remove_pending)
            release_control(next, link->target, slot);
    }
    for (std::size_t slot = 0; slot < 4; ++slot)
        next.character_display[slot] = {next.stats[slot].hp, next.stats[slot].mp,
                                        next.status.targets[slot].active};
    state = std::move(next);
}
std::bitset<4> refresh_battle_equipment(BattleState& state, const PartyData& party,
                                       const EquipmentData& equipment, const BattleCommandData& commands) {
    bool pending = false;
    for (unsigned slot = 0; slot < 4; ++slot)
        pending |= state.equipment_characters[slot].has_value() &&
                   (state.equipment_changed[slot] || state.status.targets[slot].equipment_refresh_count != 0);
    if (!pending) return {};
    auto next = state;
    std::bitset<4> changed;
    for (unsigned slot = 0; slot < 4; ++slot) {
        if ((!next.equipment_changed[slot] && !next.status.targets[slot].equipment_refresh_count) ||
            !next.equipment_characters[slot] || !next.inventory.hands[slot]) continue;
        auto character = *next.equipment_characters[slot];
        for (unsigned hand = 0; hand < 2; ++hand) {
            const auto& held = (*next.inventory.hands[slot])[hand];
            character.equipment[hand] = held.item ? held.item->identity : 255;
        }
        character.status = wrap8(next.status.targets[slot].active);
        const auto fresh = initialize_character_combat_stats(character, party, equipment,
            next.inventory.actor_masks[slot], next.actors[slot].value_or(0), false);
        const auto previous = next.stats[slot];
        auto stats = fresh.stats;
        // C2/2095 -> 286D refreshes equipment properties, not InitCharProp's
        // HP/MP, level or active statuses. In-battle damage and level changes
        // survive. Upper-word immunity retains its independent status owner.
        stats.hp = previous.hp; stats.mp = previous.mp; stats.level = previous.level;
        stats.pending_status = previous.pending_status;
        stats.blocked_status = (previous.blocked_status & 0xffff0000U) | (stats.blocked_status & 65535U);
        stats.retaliation_flags = previous.retaliation_flags;
        stats.metamorph = previous.metamorph; stats.monster_special = previous.monster_special;
        normalize_battle_immunity(stats, next.morph.permanent);
        next.stats[slot] = stats;
        next.equipment_characters[slot] = fresh.persistent;
        next.genji_gloves[slot] = fresh.genji_graphics;
        next.block_graphics[slot] = fresh.block_graphics;
        next.time.targets[slot].escape_rate = fresh.run_factor;
        refresh_battle_commands(next.availability, slot, commands, stats,
            next.status.targets[slot].active, next.morph.saved_gauge);
        next.turns[slot].rates = battle_speed_rates(stats.speed, next.status.targets[slot].active,
                                                   false, next.speed_constant);
        next.equipment_changed.reset(slot);
        next.status.targets[slot].equipment_refresh_count = 0;
        changed.set(slot);
    }
    state = std::move(next);
    return changed;
}
} // namespace ff6
