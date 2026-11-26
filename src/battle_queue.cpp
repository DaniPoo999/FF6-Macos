#include "ff6/battle_queue.hpp"
#include "rom-materials/gau_return_threshold.macros.inc"
#include "rom-materials/shadow_departure_threshold.macros.inc"
#include "rom-materials/battle_special_minimum_allies.macros.inc"
#include "rom-materials/battle_retort_attack.macros.inc"
#include "rom-materials/battle_interceptor_attack_base.macros.inc"
#include "rom-materials/battle_random_magitek_count.macros.inc"
#include "rom-materials/battle_morph_enable_threshold.macros.inc"
#include "ff6/battle_entry.hpp"
#include "ff6/battle_schedule.hpp"
#include "ff6/battle_submission.hpp"
#include "ff6/battle_display.hpp"
#include <algorithm>

namespace ff6 {
namespace {
bool pending_battle_priority_work(const BattleState& battle, const BattleQueueContext& context) {
    return !battle.immediate_actions.empty() || context.continuing_retaliation ||
        std::ranges::any_of(battle.turns, [](const auto& turn) { return !turn.retaliation.empty(); });
}
} // namespace
void publish_battle_loop_display(BattleState& battle, const BattleQueueContext& context) {
    if (!pending_battle_priority_work(battle, context))
        update_battle_display(battle);
}
bool queue_battle_ai_retaliation(BattleState& battle, std::uint8_t target) {
    if (target >= 10) throw std::invalid_argument("Invalid counter participant");
    const auto& status = battle.status.targets[target];
    const auto& memory = battle.attack_memory[target];
    const bool attacked = memory.retaliation_pending && memory.retaliation_attacker &&
                          *memory.retaliation_attacker != target;
    // C2/4C5B..4CB0: death responses may follow counters or self damage;
    // ordinary responses need an external hit and cannot counter a counter.
    if (!status.present || !battle.ai[target].retaliation ||
        !battle.turns[target].retaliation.empty() ||
        (!status.died && (battle.retaliation_action || !attacked)))
        return false;
    std::uint16_t targets = 0;
    if (attacked) {
        const auto source = *memory.retaliation_attacker;
        targets = static_cast<std::uint16_t>(1U << (source < 4 ? source : source + 4U));
    }
    append_battle_retaliation_action(battle, target, {31, 0, targets, 0});
    return true;
}
bool queue_battle_retort(BattleState& battle, std::uint8_t target, bool physical) {
    if (target >= 4) return false;
    const auto& memory = battle.attack_memory[target];
    if (!physical || !battle.status.targets[target].present || battle.departed(target) ||
        battle.retaliation_action || battle.ai[target].retaliation ||
        !(battle.stats[target].retaliation_flags & 1U) || !memory.retaliation_pending ||
        !memory.retaliation_attacker || *memory.retaliation_attacker == target)
        return false;
    const auto source = *memory.retaliation_attacker;
    const auto mask = static_cast<std::uint16_t>(1U << (source < 4 ? source : source + 4U));
    // C2/4CC3 precedes Interceptor and accepts friendly physical attackers too.
    append_battle_retaliation_action(battle, target, {7, FF6_ROM_VALUE_BATTLE_RETORT_ATTACK_0, mask, 0});
    return true;
}
bool queue_battle_interceptor(BattleState& battle, const BattleQueueData& data,
                              std::uint8_t target) {
    if (target >= 4)
        throw std::invalid_argument("Interceptor defender must be a character");
    const auto& memory = battle.attack_memory[target];
    // C2/4C5B..4CF4: scripted counters and Retort precede the dog branch;
    // another retaliation cannot recursively provoke this counter.
    if (!battle.status.targets[target].present || battle.departed(target) ||
        battle.retaliation_action || battle.ai[target].retaliation ||
        (battle.stats[target].retaliation_flags & 1U) || !memory.retaliation_pending ||
        !memory.retaliation_attacker || *memory.retaliation_attacker < 4)
        return false;
    const auto draw = battle.random.next(data.ai.random);
    if (!(draw & 1U))
        return false;
    const auto mask = static_cast<std::uint16_t>(1U << (*memory.retaliation_attacker + 4U));
    BattleQueuedCommand command{2, static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_INTERCEPTOR_ATTACK_BASE_0 + ((draw >> 1U) & 1U)), mask, 0};
    command.mp_cost = battle_queued_mp_cost(battle, target, command, data.spell_mp);
    append_battle_retaliation_action(battle, target, command);
    return true;
}
bool queue_veldt_gau_appearance(BattleState& battle, BattleQueueContext& context,
                                const std::array<std::uint8_t, 256>& random) {
    const auto gau = std::ranges::find(battle.actors, std::optional<std::uint8_t>{11});
    if (gau == battle.actors.end() || !battle.entry.gau_recruitment) return false;
    battle.entry.gau_recruitment = false;
    // C2/4840 branches at the selected threshold; its source comment reverses
    // that probability. Preserve the canonical operand and unsigned predicate.
    if (battle.random.next(random) >= FF6_ROM_VALUE_GAU_RETURN_THRESHOLD_0 ||
        (!(context.ai.variables[13] & 2U) && battle.participation.ally_count < FF6_ROM_VALUE_BATTLE_SPECIAL_MINIMUM_ALLIES_0))
        return false;
    const auto slot = static_cast<unsigned>(gau - battle.actors.begin());
    battle.participation.enemy_characters.set(slot);
    append_battle_immediate_action(battle, BattleScriptedEventAction{27});
    cancel_battle_actions_for_gau(battle);
    return true;
}
bool queue_shadow_departure(BattleState& battle, const BattleQueueContext& context,
                            const std::array<std::uint8_t, 256>& random) {
    const auto shadow = std::ranges::find(battle.actors, std::optional<std::uint8_t>{3});
    if (shadow == battle.actors.end()) return false;
    const auto slot = static_cast<std::uint8_t>(shadow - battle.actors.begin());
    constexpr auto incapacitated = status_mask(BattleStatus::dead) |
        status_mask(BattleStatus::petrify) | status_mask(BattleStatus::zombie);
    // C2/4861: draw before the approach, survivor, ailment and story gates,
    // including the next victory check after his departure event has ended.
    if (battle.random.next(random) >= FF6_ROM_VALUE_SHADOW_DEPARTURE_THRESHOLD_0 || battle.entry.approach != BattleApproach::normal ||
        battle.participation.ally_count < FF6_ROM_VALUE_BATTLE_SPECIAL_MINIMUM_ALLIES_0 ||
        (battle.status.targets[slot].active & incapacitated) ||
        (context.ai.variables[13] & 8U) || !(battle.available_characters & 8U))
        return false;
    // C2/48A6 removes eligibility now, while the graphics event still owns his
    // walk, dialogue and exit. Reward division must count only the survivors.
    battle.available_characters &= ~std::uint16_t{8};
    battle.shadow_departure = slot;
    battle.removed_from_party.set(slot);
    battle.status.targets[slot].present = false;
    update_battle_participation(battle);
    append_battle_immediate_action(battle, BattleScriptedEventAction{11});
    return true;
}
void record_battle_conclusion(BattleQueueContext& context, BattleConclusion conclusion) {
    // C2/47FB, 4897 and 5FCA set independent bits in global AI variable 12.
    // The field receives this as variable 8 and tests bits 45/44/40 before
    // awarding soldier/banquet victory points. Returning alone is not a win.
    std::uint8_t flags = 0;
    switch (conclusion) {
    case BattleConclusion::interrupted: flags = 0x20; break;
    case BattleConclusion::escaped: flags = 0x10; break;
    case BattleConclusion::defeated: flags = 0x01; break;
    case BattleConclusion::engulfed: flags = 0x80; break;
    case BattleConclusion::ongoing:
    case BattleConclusion::scripted_return:
    case BattleConclusion::victorious: break;
    }
    context.ai.variables[12] |= flags;
}
BattleConclusion battle_conclusion_at_rest(const BattleState& battle,
                                           const BattleQueueContext& context) {
    // BattleLoop C2/0033..0085 reaches CheckBattleEnd only after immediate
    // work and all counters drain. This applies to every ending, not just
    // victory: a remaining response can revive someone or set story state.
    if (pending_battle_priority_work(battle, context))
        return BattleConclusion::ongoing;
    if (battle.end_event == 4 || battle.end_event == 8 || battle.end_event == 10)
        return BattleConclusion::scripted_return;
    // C2/47FB: an event timer ends combat before ordinary victory/defeat tests.
    // A selected special battle event retains precedence over the timer.
    if (battle.event_timer_expired && !battle.end_event)
        return BattleConclusion::interrupted;
    // C2/4807: a hidden replacement phase suspends ordinary endings.
    if (battle.ending_inhibited && !battle.end_event)
        return BattleConclusion::ongoing;
    // C2/480E: swallowing the complete entry roster takes precedence over
    // ordinary escape/defeat. One swallowed survivor plus a fallen ally is not
    // enough, even though both cases leave no active characters on screen.
    if (!battle.end_event && !battle.participation.ally_count &&
        battle.engulfed_characters.any() &&
        battle.engulfed_characters == battle.potential_engulf_targets)
        return BattleConclusion::engulfed;
    if (battle.end_event == 2 ||
        (!battle.end_event && !battle.participation.ally_count &&
         battle.departed_characters.any()))
        return BattleConclusion::escaped;
    if (battle.end_event == 6 ||
        !battle.participation.ally_count)
        return BattleConclusion::defeated;
    if (!battle.participation.enemy_count)
        return BattleConclusion::victorious;
    return BattleConclusion::ongoing;
}
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
unsigned random_choice(BattleState& battle, const BattleQueueData& data, unsigned count) {
    return unsigned{battle.random.next(data.ai.random)} * count >> 8U;
}
std::uint8_t random_known_bit(BattleState& battle, const BattleQueueData& data,
                             std::bitset<8> known) {
    // RandBit/GetBitNum: an empty mask returns bit zero without consuming RNG.
    if (known.none()) return 0;
    auto choice = random_choice(battle, data, static_cast<unsigned>(known.count()));
    for (std::uint8_t i = 0; i < 8; ++i)
        if (known[i] && choice-- == 0) return i;
    throw std::logic_error("Invalid known-ability selection");
}
template<std::size_t N>
std::optional<std::uint8_t> random_spell(BattleState& battle, unsigned participant,
    const BattleQueueData& data, const std::array<std::optional<BattleSpell>, N>& spells) {
    if (battle.status.targets[participant].active & status_mask(BattleStatus::silence)) return {};
    const auto count = static_cast<unsigned>(std::ranges::count_if(spells, [](const auto& v) {
        return v.has_value();
    }));
    if (!count) return {};
    auto choice = random_choice(battle, data, count);
    // C2/0534 scans the prepared spell list backwards. MP affordability is
    // checked at execution; a dimmed menu entry still belongs to this draw.
    for (auto i = spells.size(); i; --i)
        if (spells[i - 1] && choice-- == 0) return spells[i - 1]->identity;
    throw std::logic_error("Invalid automatic spell selection");
}
std::optional<std::uint8_t> automatic_attack(BattleState& battle, unsigned participant,
                                            const BattleQueueData& data, std::uint8_t command) {
    switch (command) {
    case 2: case 23:
        return random_spell(battle, participant, data, battle.availability.spells[participant].magic);
    case 12: {
        const auto lore = random_spell(battle, participant, data, battle.availability.spells[participant].lores);
        return lore ? std::optional{static_cast<std::uint8_t>(139U + *lore)} : std::nullopt;
    }
    case 3:
        return battle.morph.saved_gauge > FF6_ROM_VALUE_BATTLE_MORPH_ENABLE_THRESHOLD_0 ? std::optional<std::uint8_t>{0} : std::nullopt;
    case 7:
        if (!((battle.stats[participant].hands[0].flags | battle.stats[participant].hands[1].flags) & 2U))
            return {};
        return static_cast<std::uint8_t>(85U + random_choice(battle, data, battle.skills.bushido_count));
    case 10:
        return static_cast<std::uint8_t>(93U + random_known_bit(battle, data, battle.skills.learned_blitzes));
    case 9:
        return static_cast<std::uint8_t>(163U + random_known_bit(battle, data, battle.inventory.tools_owned));
    case 16:
        return choose_battle_rage_attack(battle, participant, data.rage_attacks, data.ai.random);
    case 19:
        return choose_battle_dance_attack(battle, participant, data.dance_attacks,
                                          data.dance_thresholds, data.ai.random);
    case 29:
        return static_cast<std::uint8_t>(131U + random_choice(battle, data, FF6_ROM_VALUE_BATTLE_RANDOM_MAGITEK_COUNT_0));
    default:
        return 0;
    }
}
BattleQueuedCommand automatic_character_action(BattleState& battle, std::uint8_t participant,
                                               const BattleQueueData& data) {
    // C2/0420–04CF: Berserk takes precedence over Confuse/Charm/Colosseum.
    // Zombie alone uses the narrow Berserk command set. The extra Fight entry
    // remains even when Fight also occupies a menu slot.
    const auto status = battle.status.targets[participant].active;
    const bool broad = !(status & status_mask(BattleStatus::berserk)) &&
        ((status & status_mask(BattleStatus::confuse)) || battle.links[participant].charm_attacker || battle.colosseum);
    std::array<BattleQueuedCommand, 5> candidates{};
    unsigned count = 1;
    const auto& slots = battle.availability.commands[participant].slots;
    for (unsigned reverse = 4; reverse; --reverse) {
        if (!slots[reverse - 1]) continue;
        const auto command = slots[reverse - 1]->identity;
        constexpr std::array<std::uint8_t, 21> confused{
#include "rom-materials/confused_command_ids.inc"
        };
        constexpr std::array<std::uint8_t, 5> berserk{
#include "rom-materials/berserk_command_ids.inc"
        };
        const bool eligible = broad ? std::ranges::find(confused, command) != confused.end()
                                    : std::ranges::find(berserk, command) != berserk.end();
        if (!eligible) continue;
        // Prepare every eligible candidate before choosing a command: even an
        // unselected Rage/Dance remembers its choice and consumes its draws.
        if (const auto attack = automatic_attack(battle, participant, data, command))
            candidates[count++] = {command, *attack, 0, 0};
    }
    return candidates[random_choice(battle, data, count)];
}
void append(BattleState& battle, const BattleQueueData& data, std::uint8_t participant,
             BattleQueuedCommand command, BattleCommandChain chain) {
    command.mp_cost = battle_queued_mp_cost(battle, participant, command, data.spell_mp);
    if (chain == BattleCommandChain::retaliation)
        append_battle_retaliation_action(battle, participant, command);
    else
        append_battle_normal_action(battle, participant, command);
}
std::size_t run_ai(BattleState& battle, BattleQueueContext& context, const BattleQueueData& data,
                    std::uint8_t participant, BattleCommandChain chain,
                    std::uint16_t initial_targets, bool unresponsive) {
    require(participant < 10, "Invalid AI queue participant");
    require(chain == BattleCommandChain::normal || chain == BattleCommandChain::retaliation,
            "Invalid AI command chain");
    const bool retaliation = chain == BattleCommandChain::retaliation;
    const auto entry = retaliation ? battle.ai[participant].retaliation : battle.ai[participant].entry;
    require(entry.has_value(), "AI queue request has no script entry");
    auto& progress = (retaliation ? context.retaliation_progress : context.normal_progress)[participant];
    auto execution = begin_battle_ai(participant, *entry, progress, initial_targets, unresponsive);
    std::size_t count = 0;
    // The language has no general backward jump. A finite ceiling also bounds
    // corrupt continuation/condition scans without introducing a host-time limit.
    for (unsigned steps = 0; !execution.complete && steps < 65536; ++steps) {
        const auto request = step_battle_ai(execution, data.ai, battle, context.ai, battle.random);
        if (!request)
            continue;
        auto command = request->command;
        if (request->prepare_attack) {
            auto& duration = battle.turns[participant].advance_duration;
            const auto prepared = prepare_battle_action(data.preparation, battle, participant,
                                                         command, duration, context.targets, battle.random);
            duration = prepared.duration;
            command = prepared.command;
        }
        append(battle, data, participant, command, chain);
        ++count;
    }
    require(execution.complete, "AI decision turn exceeded its instruction budget");
    progress = execution.progress;
    return count;
}
std::optional<std::uint8_t> take(BattleParticipantSchedule& schedule) {
    while (schedule.read != schedule.write) {
        const auto participant = schedule.entries[schedule.read];
        schedule.read = wrap8(unsigned{schedule.read} + 1U);
        if (participant)
            return participant;
    }
    return {};
}
} // namespace
BattleQueueData load_battle_queue(const ContentPack& pack) {
    BattleQueueData data;
    data.ai = load_battle_ai(pack);
    data.preparation = load_battle_action_preparation(pack);
    const auto spells = pack.get("data_c46ac0_magic_prop");
    require(spells.size() == 256 * 14, "Invalid queue spell cost table");
    for (unsigned i = 0; i < 256; ++i)
        data.spell_mp[i] = spells[i * 14 + 5];
    const auto submission = load_battle_submission_data(pack);
    data.rage_attacks = submission.rage_attacks;
    data.dance_attacks = submission.dance_attacks;
    data.dance_thresholds = submission.dance_thresholds;
    return data;
}
std::bitset<10> battle_target_admission(const BattleState& battle,
                                       std::bitset<6> monster_targets, bool allow_away) {
    monster_targets &= battle.monster_targets;
    auto admitted = battle.participation.targetable & battle.participation.active;
    for (unsigned i = 0; i < 10; ++i) {
        if (i < 4 ? battle.links[i].seize_attacker.has_value() : !monster_targets[i - 4])
            admitted.reset(i);
    }
    if (!allow_away)
        admitted &= ~battle.away_from_battle;
    return admitted;
}
std::uint8_t battle_queued_mp_cost(const BattleState& battle, std::uint8_t participant,
                                  BattleQueuedCommand command,
                                  const std::array<std::uint8_t, 256>& spell_mp) {
    require(participant < 10, "Invalid command cost participant");
    if (battle.waive_next_action_mp || (command.command != 2 && command.command != 12 &&
                                       command.command != 23 && command.command != 25))
        return 0;
    if (participant >= 4)
        return spell_mp[command.attack];
    return battle_character_command_mp_cost(battle, participant, command);
}
std::size_t queue_battle_ai_turn(BattleState& battle, BattleQueueContext& context,
                                const BattleQueueData& data, std::uint8_t participant,
                                BattleCommandChain chain, std::uint16_t initial_targets,
                                bool unresponsive) {
    validate_battle_state(battle);
    auto next_battle = battle;
    auto next_context = context;
    const auto count = run_ai(next_battle, next_context, data, participant, chain, initial_targets, unresponsive);
    battle = std::move(next_battle);
    context = std::move(next_context);
    return count;
}
bool prepare_battle_advance(BattleState& battle, BattleQueueContext& context,
                            const BattleQueueData& data, std::uint8_t participant) {
    validate_battle_state(battle);
    require(participant < 10, "Invalid advance participant");
    if (battle.departed(participant))
        return false;
    auto& original = battle.turns[participant];
    if (original.status_actions_blocked || original.actions_blocked) {
        original.reschedule_pending = true;
        return false;
    }
    auto next = battle;
    auto next_context = context;
    auto& turn = next.turns[participant];
    turn.reschedule_pending = false;
    turn.bypass_advance_wait = true;
    if (turn.advance_duration == 255)
        turn.advance_duration = 0;
    turn.defending = false;
    next.stats[participant].retaliation_flags &= 0xfaU; // Clear Retort and Runic, retain other effects.
    if (participant < 4)
        next_context.retarget_pending.reset(participant);
    if (participant < 4 && next.ai[participant].entry)
        run_ai(next, next_context, data, participant, BattleCommandChain::normal, 0, false);
    else if (participant >= 4 && turn.normal.empty())
        append(next, data, participant, {31, 0, 0, 0}, BattleCommandChain::normal);
    else if (!turn.normal.empty()) {
        for (auto& command : turn.normal) {
            const auto prepared = prepare_battle_action(data.preparation, next, participant, command,
                                                         turn.advance_duration, next_context.targets, next.random);
            command = prepared.command;
            turn.advance_duration = prepared.duration;
        }
    } else {
        const auto status = next.status.targets[participant].active;
        BattleQueuedCommand command;
        if (status & status_mask(BattleStatus::dance))
            command = {19, *automatic_attack(next, participant, data, 19), 0, 0};
        else if (status & status_mask(BattleStatus::rage))
            command = {16, *automatic_attack(next, participant, data, 16), 0, 0};
        else if (status & status_mask(BattleStatus::magitek))
            command = {29, *automatic_attack(next, participant, data, 29), 0, 0};
        else {
            // C2/037B falls through to RandCharAction even if the condition
            // which admitted this turn was removed while it waited. That
            // routine uses the narrow command set when Confuse/Charm is gone.
            command = automatic_character_action(next, participant, data);
        }
        const auto prepare = [&](BattleQueuedCommand selected) {
            if (selected.command == 12 && selected.attack == 140) selected.command = 30;
            const auto prepared = prepare_battle_action(data.preparation, next, participant, selected,
                                                         turn.advance_duration, next_context.targets, next.random);
            turn.advance_duration = prepared.duration;
            append(next, data, participant, prepared.command, BattleCommandChain::normal);
        };
        // C2/0391: X-Magic draws/prepares its additional first cast before
        // preparing the spell already chosen with the command candidate.
        if (command.command == 23) {
            const auto first = automatic_attack(next, participant, data, 2);
            require(first.has_value(), "Automatic X-Magic lost its learned spell list");
            prepare({2, *first, 0, 0});
        }
        prepare(command);
    }
    battle = std::move(next);
    context = std::move(next_context);
    return true;
}
BattleQueueWork take_battle_queue_work(BattleState& battle, BattleQueueContext& context) {
    validate_battle_state(battle);
    for (const auto participant : {context.continuing_action, context.continuing_retaliation})
        if (participant)
            require(*participant < 10, "Invalid continuing battle participant");
    if (!battle.immediate_actions.empty())
        return {BattleQueueWorkKind::immediate, {}};
    if (context.continuing_retaliation)
        return {BattleQueueWorkKind::retaliation, context.continuing_retaliation};
    if (auto participant = take(battle.retaliation_schedule))
        return {BattleQueueWorkKind::retaliation, participant};
    // C2/007D: another escape request is admitted only after immediate work
    // and retaliation drain, on the visit that also permits an ordinary turn.
    battle.time.escape_action_pending = false;
    // C2/0088 resets response opportunities only after every counter drains.
    context.retaliation_evaluated.reset();
    if (auto participant = take(battle.advance_schedule))
        return {BattleQueueWorkKind::advance, participant};
    if (context.continuing_action)
        return {BattleQueueWorkKind::action, context.continuing_action};
    if (auto participant = take(battle.ready_schedule))
        return {BattleQueueWorkKind::action, participant};
    // C2/00B1 releases the hold once every action chain has drained.
    battle.ending_inhibited = false;
    return {};
}
} // namespace ff6
