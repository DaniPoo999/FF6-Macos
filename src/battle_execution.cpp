#include "ff6/battle_execution.hpp"
#include "rom-materials/summon_advance_wait.macros.inc"
#include "ff6/battle_entry.hpp"
#include "ff6/battle_display.hpp"
#include <algorithm>
#include <bit>

namespace ff6 {
void begin_battle_summon_jumps(BattleState& battle, BattleQueueContext& queue, std::bitset<10> targets) {
    for (unsigned i = 0; i < 10; ++i) {
        if (!targets[i]) continue;
        const auto slot = static_cast<std::uint8_t>(i);
        auto& turn = battle.turns[i];
        // C2/3934 and C2/0B4A: remove pending orders, stop ATB and use Jump's
        // advance wait. It lands through the normal execution owner.
        turn.normal = {{22, 0, 0, 0}};
        turn.atb_stopped = true;
        turn.menu_requests_allowed = false;
        turn.weapon_locked = turn.action_ready = turn.actions_blocked = false;
        turn.advance_wait &= 255U;
        turn.advance_duration = FF6_ROM_VALUE_SUMMON_ADVANCE_WAIT_0;
        battle.away_from_battle.set(i);
        for (auto* schedule : {&battle.ready_schedule, &battle.advance_schedule})
            for (auto& entry : schedule->entries) if (entry == slot) entry.reset();
        if (queue.continuing_action == slot) queue.continuing_action.reset();
        battle.menu_requests.push_back({slot, false, {}});
    }
    update_battle_participation(battle);
    update_battle_action_readouts(battle);
}

namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
std::uint16_t random_target(std::uint16_t targets, BattleRandom& random,
                             const std::array<std::uint8_t, 256>& table) {
    if (!targets) return 0;
    auto choice = (unsigned{random.next(table)} * static_cast<unsigned>(std::popcount(targets))) >> 8U;
    for (unsigned bit = 0; bit < 16; ++bit)
        if (targets & (1U << bit)) {
            if (!choice) return static_cast<std::uint16_t>(1U << bit);
            --choice;
        }
    throw std::logic_error("Unresolved execution target draw");
}
void load_mimic(BattleState& battle, const BattleExecutionContext& execution,
                 std::uint8_t participant) {
    auto& front = battle.turns[participant].normal.front();
    const auto mp = front.mp_cost; // Mimic replaces the command/targets, not its queued cost.
    front = execution.mimic.jump.value_or(execution.mimic.first);
    front.mp_cost = mp;
    if (!execution.mimic.jump && execution.mimic.second) {
        auto second = *execution.mimic.second;
        second.mp_cost = 0;
        battle.waive_next_action_mp = true;
        append_battle_normal_action(battle, participant, second);
    }
}
void random_monster_action(BattleState& battle, BattleQueueContext& queue,
                            const BattleExecutionContext& execution,
                            const BattleExecutionData& data, std::uint8_t participant,
                            std::uint16_t targets) {
    require(participant >= 4, "Deferred monster choice requires a monster");
    const auto identity = execution.monster_identities[participant - 4];
    require(identity && *identity < data.monster_choices.size(), "Missing monster choice identity");
    const auto& choices = data.monster_choices[*identity];
    auto cursor = unsigned{battle.random.next(data.queue.ai.random)} & 3U;
    std::uint8_t attack = 255;
    for (unsigned count = 0; count < 4; ++count) {
        attack = choices[cursor];
        if (attack != 255) break;
        cursor = (cursor + 3U) & 3U;
    }
    // An entirely empty authored table eventually returns NONE in the original.
    // Its repeated scans consume no additional random values.
    if (battle.status.targets[participant].active & status_mask(BattleStatus::berserk))
        attack = 238;
    const auto command = battle_ai_attack_command(data.queue.ai, attack);
    // This caller uses the raw command mapping. The script runner's separate
    // Roulette normalization is deliberately not part of random monster choice.
    auto& duration = battle.turns[participant].advance_duration;
    auto prepared = prepare_battle_action(data.queue.preparation, battle, participant,
                                          {command, attack, targets, 0}, duration,
                                          queue.targets, battle.random);
    duration = prepared.duration;
    prepared.command.mp_cost = battle_queued_mp_cost(battle, participant, prepared.command,
                                                     data.queue.spell_mp);
    append_battle_normal_action(battle, participant, prepared.command);
}
BattleExecutionRequest begin(BattleState& battle, BattleQueueContext& queue,
                              BattleExecutionContext& execution, const BattleExecutionData& data,
                              std::uint8_t participant) {
    queue.continuing_action.reset();
    battle.quick_recast_blocked = false;
    BattleQueuedCommand command{18, 0, 0, 0};
    // Each deferred request must produce a finite authored decision turn. The
    // guard also rejects corrupt programs that continually emit RANDOM.
    for (unsigned decisions = 0;; ++decisions) {
        require(decisions < 128, "Deferred action chain did not reach an executable command");
        auto& turn = battle.turns[participant];
        if (turn.normal.empty()) {
            // An AI branch may intentionally emit no action. Do not expose
            // the consumed RANDOM request as a concrete battle command.
            command = {18, 0, 0, 0};
            break;
        }
        if (turn.normal.front().command == 18) load_mimic(battle, execution, participant);
        const auto initialized = initialize_battle_command(battle, queue, data, participant,
                                                            turn.normal.front(), true);
        command = initialized.command;
        if (initialized.queued_identity == 31) {
            turn.normal.erase(turn.normal.begin());
            if (battle.colosseum || battle.links[participant].charm_attacker ||
                (battle.status.targets[participant].active &
                 (status_mask(BattleStatus::berserk) | status_mask(BattleStatus::confuse))))
                random_monster_action(battle, queue, execution, data, participant, command.parameters);
            else
                queue_battle_ai_turn(battle, queue, data.queue, participant,
                                     BattleCommandChain::normal, command.parameters);
            continue;
        }
        if (initialized.queued_identity == 22) {
            const bool returning = battle.away_from_battle[participant];
            battle.away_from_battle[participant] = !returning;
            if (!returning) {
                turn.atb_stopped = true;
                turn.action_ready = false;
                turn.advance_wait &= 255U;
                schedule_battle_turn(battle, participant, false);
                return {BattleExecutionDisposition::jump_wait, command};
            }
        }
        if (turn.normal.size() == 1) {
            battle.menus_disabled = false;
            if (battle.quick && battle.quick->target == participant) {
                battle.quick->remaining_actions = wrap8(unsigned{battle.quick->remaining_actions} - 1U);
                if (!battle.quick->remaining_actions) {
                    battle.quick.reset();
                    battle.quick_recast_blocked = true;
                }
            }
        }
        if (turn.status_actions_blocked || turn.actions_blocked) {
            turn.reschedule_pending = true;
            battle.quick_recast_blocked = false;
            return {BattleExecutionDisposition::blocked, command};
        }
        turn.normal.erase(turn.normal.begin());
        break;
    }
    auto& turn = battle.turns[participant];
    turn.atb_stopped = turn.action_ready = false;
    turn.actions_blocked = true;
    execution.executing = participant;
    if (!battle.status.targets[participant].present ||
        battle.departed(participant))
        return {BattleExecutionDisposition::absent, command};
    battle.status.targets[participant].updates.set(static_cast<unsigned>(BattleStatusUpdate::speed));
    turn.menu_requests_allowed = true;
    return {BattleExecutionDisposition::execute, command};
}
} // namespace
BattleQueuedCommand begin_battle_rage(BattleState& battle, std::uint8_t slot,
    BattleQueuedCommand command, const MonsterStatsData& monsters, const BattleAiData& ai) {
    require(slot < 4 && command.command == 16, "Invalid Rage command");
    auto& identity = battle.selections.rages[slot];
    if ((identity & 255U) == 255) {
        const auto previous = battle.last_rage_user.value_or(0);
        identity = battle.selections.rages[previous] & 255U;
    }
    require(identity < 256, "Invalid Rage monster identity");
    battle.last_rage_user = slot;
    auto& stats = battle.stats[slot];
    if (!battle.rage_baseline[slot]) battle.rage_baseline[slot] = stats;
    const auto& monster = monsters.monsters[identity];
    const auto flags = stats.monster_flags;
    apply_monster_rage_properties(stats, monster);
    stats.monster_flags = flags; // LoadRageProp does not replace character identity flags.
    for (auto& hand : stats.hands) hand.item = monster.attack_item;
    battle.rage_special_animations[slot] = monster.special_animation;
    battle.status.targets[slot].active |= status_mask(BattleStatus::rage);
    battle.status.targets[slot].pending.apply |= stats.pending_status;
    stats.pending_status = 0;
    fix_battle_immunity(stats);
    command.command = battle_ai_attack_command(ai, command.attack);
    return command;
}
BattleExecutionData load_battle_execution(const ContentPack& pack) {
    BattleExecutionData data;
    data.queue = load_battle_queue(pack);
    const auto properties = pack.get("data_cffe00_battle_cmd_prop");
    const auto choices = pack.get("data_cf3d00_monster_control");
    require(properties.size() == 64 && choices.size() == 384 * 4, "Invalid execution tables");
    for (unsigned i = 0; i < 32; ++i) data.command_properties[i] = properties[i * 2];
    for (unsigned i = 0; i < 384; ++i)
        std::ranges::copy(choices.subspan(i * 4, 4), data.monster_choices[i].begin());
    return data;
}
BattleInitializedCommand initialize_battle_command(BattleState& battle, BattleQueueContext& queue,
                                                    const BattleExecutionData& data,
                                                    std::uint8_t participant,
                                                    BattleQueuedCommand command,
                                                    bool retarget_changed_status) {
    require(participant < 10 && command.command < 52, "Invalid queued execution command");
    auto random = battle.random;
    auto changed = queue.retarget_pending;
    const auto original = command.command;
    // Original status invalidation touches the character byte only; Magitek and
    // internal commands retain their parameters even when status changed.
    if (retarget_changed_status && original < 29 && participant < 4 && changed[participant]) {
        changed.reset(participant);
        command.parameters = 0;
    }
    if (original < 30 && (battle.status.targets[participant].active & status_mask(BattleStatus::imp)) &&
        !(data.command_properties[original] & 4U)) {
        command.command = command.attack = command.mp_cost = 0;
        if (participant < 4)
            command.parameters &= static_cast<std::uint16_t>(0xff00U | battle.participation.enemy_characters.to_ulong());
        // Monsters are all allies to each other for this conversion.
        else command.parameters &= 0x00ffU;
        command.parameters = random_target(command.parameters, random, data.queue.ai.random);
    }
    battle.random = random;
    queue.retarget_pending = changed;
    return {command, original};
}
BattleExecutionRequest begin_battle_normal_action(BattleState& battle, BattleQueueContext& queue,
                                                  BattleExecutionContext& execution,
                                                  const BattleExecutionData& data,
                                                  std::uint8_t participant) {
    validate_battle_state(battle);
    require(participant < 10 && !execution.executing, "Battle action already executing or invalid participant");
    auto next = battle; auto next_queue = queue; auto next_execution = execution;
    const auto result = begin(next, next_queue, next_execution, data, participant);
    battle = std::move(next); queue = std::move(next_queue); execution = std::move(next_execution);
    return result;
}
bool begin_battle_jump_advance(BattleState& battle, BattleExecutionContext& execution,
    unsigned participant) {
    if (participant >= 4 || !battle.turns[participant].menu_requests_allowed ||
        battle.turns[participant].normal.empty() ||
        battle.turns[participant].normal.front().command != 22) return false;
    // C2/21BC: a character's ordinary advance owns takeoff and the separate
    // Jump history. Palidor already hides its passengers and disables this gate.
    execution.mimic.jump = BattleQueuedCommand{22, 0,
        battle.turns[participant].normal.front().parameters, 0};
    battle.away_from_battle.set(participant);
    update_battle_participation(battle);
    return true;
}
void remember_battle_mimic_action(BattleExecutionContext& execution,
    const BattleExecutionData& data, unsigned participant,
    std::optional<BattleQueuedCommand> performed) {
    // SaveMimicAction (C2/021E) is shared by ordinary actions and counters.
    // Keep the queued command identity even if its consumer delegates to
    // another effect (Rage, Sketch, spellcasting equipment, weapon procs).
    if (!performed || participant >= 4 || performed->command >= 30 ||
        !(data.command_properties[performed->command] & 2U)) return;
    performed->mp_cost = 0;
    execution.mimic.jump.reset();
    if (performed->command == 23) execution.mimic.second = performed;
    else { execution.mimic.first = *performed; execution.mimic.second.reset(); }
}
bool finish_battle_normal_action(BattleState& battle, BattleQueueContext& queue,
                                 BattleExecutionContext& execution, const BattleExecutionData& data,
                                 std::optional<BattleQueuedCommand> performed) {
    validate_battle_state(battle);
    require(execution.executing && *execution.executing < 10, "No battle action to finish");
    require(!performed || performed->command < 52, "Invalid performed battle command");
    const auto participant = *execution.executing;
    remember_battle_mimic_action(execution, data, participant, performed);
    execution.refresh_monsters = true;
    execution.normal_in_progress = true;
    const bool return_actor = execution.sonic_dive || battle.turns[participant].normal.empty();
    execution.sonic_dive = false;
    if (return_actor) {
        auto& turn = battle.turns[participant];
        if (!turn.atb_stopped)
            turn.atb = static_cast<std::uint16_t>((turn.atb & 255U) |
                        (std::min((unsigned{turn.atb} >> 8U) + 1U, 255U) << 8U));
        turn.advance_duration = 255;
        turn.advance_wait &= 255U;
        execution.normal_in_progress = false;
    } else queue.continuing_action = participant;
    execution.executing.reset();
    battle.quick_recast_blocked = false;
    return return_actor;
}
BattleExecutionRequest begin_battle_retaliation(BattleState& battle, BattleQueueContext& queue,
    BattleExecutionContext& execution, const BattleExecutionData& data, std::uint8_t participant) {
    validate_battle_state(battle);
    require(participant < 10 && !execution.executing, "Counter action already executing or invalid participant");
    auto next = battle;
    auto next_queue = queue;
    next_queue.continuing_retaliation.reset();
    next.retaliation_action = false;
    for (;;) {
        auto& commands = next.turns[participant].retaliation;
        if (commands.empty()) {
            // C2/4B86: both an exhausted scheduled visit and a deferred AI
            // branch with no commands return without attack presentation.
            battle = std::move(next);
            queue = std::move(next_queue);
            return {BattleExecutionDisposition::no_action, {}};
        }
        const auto request = initialize_battle_command(next, next_queue, data, participant,
                                                        commands.front(), false);
        commands.erase(commands.begin());
        if (request.queued_identity == 31) {
            // C2/4BF4: evaluate at execution, once per response opportunity.
            // In particular a preceding death response can revive this actor;
            // its old zero-enemy condition must not spawn a second wave.
            if (next_queue.retaliation_evaluated[participant]) continue;
            next_queue.retaliation_evaluated.set(participant);
            auto& status = next.status.targets[participant];
            const bool self_death = status.died && !request.command.parameters;
            status.died = false;
            const bool unresponsive = self_death ||
                (next.quick && next.quick->target != participant) ||
                next.links[participant].charm_attacker ||
                (status.retaliation &
                 (status_mask(BattleStatus::sleep) | status_mask(BattleStatus::confuse) |
                  status_mask(BattleStatus::berserk) | status_mask(BattleStatus::frozen) |
                  status_mask(BattleStatus::stop))) != 0;
            queue_battle_ai_turn(next, next_queue, data.queue, participant,
                                 BattleCommandChain::retaliation,
                                 request.command.parameters, unresponsive);
            continue;
        }
        next.retaliation_action = true;
        const bool departed = participant < 4 && next.departed_characters[participant] &&
                              request.command.command < 30;
        battle = std::move(next);
        queue = std::move(next_queue);
        execution.executing = participant;
        return {departed ? BattleExecutionDisposition::absent : BattleExecutionDisposition::execute,
                request.command};
    }
}
void finish_battle_retaliation(BattleState& battle, BattleQueueContext& queue,
    BattleExecutionContext& execution) {
    require(execution.executing && battle.retaliation_action, "No counter action to finish");
    const auto slot = *execution.executing;
    if (!battle.turns[slot].retaliation.empty()) queue.continuing_retaliation = slot;
    execution.executing.reset();
    battle.retaliation_action = false;
}
bool execute_battle_ai_control(BattleState& battle, BattleAiEnvironment& ai,
                               const BattleAiData& data, std::uint8_t participant,
                               BattleQueuedCommand command) {
    require(participant < 10, "Invalid AI control participant");
    if (command.command == 46) {
        auto& value = command.attack < 36 ? ai.variables[command.attack] : ai.personal_variables[participant];
        const unsigned operand = command.parameters & 255U;
        if (!(operand & 128U)) value = static_cast<std::uint8_t>(operand & 63U);
        else if (operand & 64U) value = wrap8(unsigned{value} - (operand & 63U));
        else value = wrap8(unsigned{value} + (operand & 63U));
        return true;
    }
    if (command.command == 47) {
        const unsigned index = command.parameters & 255U, bit = command.parameters >> 8U;
        require(bit < 8 && command.attack <= 2, "Invalid AI switch operation");
        auto& value = index < 36 ? ai.variables[index] : ai.personal_variables[participant];
        const auto mask = static_cast<std::uint8_t>(1U << bit);
        if (command.attack == 0) value ^= mask;
        else if (command.attack == 1) value |= mask;
        else value &= static_cast<std::uint8_t>(~mask);
        return true;
    }
    if (command.command == 48 && (command.attack == 1 || command.attack == 5)) {
        // C2/1E79..1E94: explicit, unfiltered recipients, including characters.
        // Protection changes neither presence nor permission to select a target.
        auto next_ai = ai;
        auto random = battle.random;
        const auto targets =
            battle_ai_control_targets(static_cast<std::uint8_t>(command.parameters & 255U),
                                      participant, data, battle, next_ai, random);
        for (unsigned slot = 0; slot < 10; ++slot)
            if (targets & (1U << (slot < 4 ? slot : slot + 4)))
                battle.invincible.set(slot, command.attack == 1);
        ai = std::move(next_ai);
        battle.random = random;
        return true;
    }
    if (command.command == 48 && (command.attack == 6 || command.attack == 7)) {
        // C2/1E95..1EA8: unfiltered selection, then change only monster permission.
        auto next_ai = ai;
        auto random = battle.random;
        const auto targets =
            battle_ai_control_targets(static_cast<std::uint8_t>(command.parameters & 255U),
                                      participant, data, battle, next_ai, random);
        const std::bitset<6> monsters{unsigned{targets} >> 8U};
        if (command.attack == 6)
            battle.monster_targets |= monsters;
        else
            battle.monster_targets &= ~monsters;
        ai = std::move(next_ai);
        battle.random = random;
        return true;
    }
    if (command.command == 48 && command.attack == 3) {
        require(participant < 4 && battle.actors[participant].has_value(),
                "Recruitment requires an allocated character");
        // C2/1EA9 changes persistent membership, not this enemy-character AI
        // or its targets. The authored script owns the subsequent departure.
        battle.recruited_characters.set(participant);
        ai.gau_recruited = true;
        return true;
    }
    if (command.command == 48 && (command.attack == 2 || command.attack == 9)) {
        battle.end_event = command.attack == 9 ? 10 : 8; // Original immediate-return event; no victory awards.
        battle.menus_disabled = true;
        return true;
    }
    if (command.command == 48 && command.attack == 0) {
        battle.time.targets[participant].elapsed = 0;
        return true;
    }
    if (command.command == 48 && command.attack == 4) {
        // C2/1EC7 resets the shared battle counter, not the acting monster's
        // personal timer or ATB. Authored AI uses this to repeat timed phases.
        battle.time.elapsed = 0;
        return true;
    }
    if (command.command == 48 && (command.attack == 11 || command.attack == 12)) {
        require(command.parameters < 32, "Invalid AI status index");
        // C2/1EDA..1EFF builds a guaranteed-hit, zero-power self effect, then
        // joins ExecSelfAttack. Use the ordinary status owner so immunity,
        // mutually exclusive conditions and ATB/menu refreshes still apply.
        const auto bit = std::uint32_t{1} << command.parameters;
        auto& status = battle.status.targets[participant];
        if (command.attack == 11)
            status.pending.apply |= bit & ~status.active;
        else
            status.pending.remove |= bit & status.active;
        // ExecSelfAttack does not ignore Vanish. As for ordinary magic, a
        // different requested condition removes it, even on a self effect.
        status.pending.remove |= status.active & status_mask(BattleStatus::vanish) & ~bit;
        auto& memory = battle.attack_memory[participant];
        if (!memory.retaliation_pending || (battle.random.next(data.random) & 1U)) {
            memory.retaliation_attacker = participant;
            memory.retaliation_pending = true;
        }
        memory.command = BattleObservedAttackValue{participant, command.command};
        memory.elements = BattleObservedAttackValue{participant, 0};
        // No spell or item was loaded: retain those independent histories.
        std::array<BattleStatusContext, 10> contexts{};
        for (unsigned i = 0; i < 10; ++i)
            contexts[i] = {battle.stats[i].hp, battle.stats[i].blocked_status};
        update_battle_status(battle.status, contexts);
        return true;
    }
    if (command.command == 48 && command.attack == 13) {
        // C2/1F00 changes the acting participant's Hide status directly. The
        // following authored visibility command owns any restored wave, HP
        // reset and entrance animation; this operation must not create them.
        battle.status.targets[participant].active |= status_mask(BattleStatus::hide);
        return true;
    }
    return false;
}
} // namespace ff6
