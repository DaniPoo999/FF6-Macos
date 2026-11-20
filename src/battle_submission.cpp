#include "ff6/battle_submission.hpp"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
void require(bool ok, const char* reason) {
    if (!ok)
        throw std::invalid_argument(reason);
}
void validate(const BattlePlayerCommand& command) {
    require(command.character < 4 && command.command < 32, "Invalid player battle command");
    require(command.second.has_value() == (command.command == 23), "Invalid second-cast selection");
}
std::uint8_t random_range(BattleState& state, const BattleSubmissionData& data, unsigned count) {
    return static_cast<std::uint8_t>((unsigned{state.random.next(data.random)} * count) >> 8U);
}
BattleQueuedCommand convert(BattleState& state, const BattleSubmissionData& data,
                            const BattlePlayerCommand& selected) {
    const auto slot = selected.character;
    auto command = selected.command;
    auto attack = selected.first.attack;
    auto targets = selected.first.targets;
    if (command == 23)
        command = 2; // Only the first X-Magic cast takes the conversion path.
    if (command == 25) {
        if (attack == 255) {
            const auto& esper = state.availability.spells[slot].esper;
            attack = esper ? esper->identity : std::uint8_t{255};
        }
        state.availability.used_espers[slot] = true;
    }
    if (command == 1 || command == 8)
        state.selections.items[slot] = attack;
    if (command == 15) {
        require(attack < data.slot_attacks.size(), "Invalid Slot selection");
        const auto selection = attack;
        attack = data.slot_attacks[selection];
        if (selection < 2) {
            if (selection == 0)
                targets = static_cast<std::uint16_t>((targets & 0xff00U) | data.joker_targets[0]);
            else
                targets = static_cast<std::uint16_t>((targets & 255U) |
                                                     (unsigned{data.joker_targets[1]} << 8U));
            targets ^= static_cast<std::uint16_t>(state.participation.enemy_characters.to_ulong());
        }
        if (attack == 255) {
            auto esper = random_range(state, data, 25);
            if (esper >= 11)
                esper = static_cast<std::uint8_t>(esper + 2U); // Exclude Odin and Raiden.
            attack = static_cast<std::uint8_t>(esper + 54U);
        }
    }
    if (command == 19) {
        state.selections.last_dance = state.selections.dances[slot] = attack;
        attack = choose_battle_dance_attack(state, slot, data.dance_attacks,
                                             data.dance_thresholds, data.random);
    }
    if (command == 16) {
        state.selections.rages[slot] =
            static_cast<std::uint16_t>((state.selections.rages[slot] & 0xff00U) | attack);
        attack = choose_battle_rage_attack(state, slot, data.rage_attacks, data.random);
    }
    if (command == 10 && attack < 128) {
        // The original learned-bit helper rotates through carry: its cycle is
        // nine positions, with the ninth producing no bit. Preserve that quirk
        // for explicit out-of-range positive selections without unsafe shifts.
        const auto bit = attack % 9U;
        if (bit == 8 || !state.skills.learned_blitzes[bit])
            attack = 255;
    }
    for (unsigned remaining = 5; remaining; --remaining) {
        const auto i = remaining - 1;
        if (command == data.attack_commands[i]) {
            const auto sum = unsigned{attack} + data.attack_offsets[i];
            attack = sum > 255 ? std::uint8_t{238} : static_cast<std::uint8_t>(sum);
        }
    }
    if (data.retarget_commands[command])
        targets = 0;
    return {command, attack, targets, 0};
}

void append(BattleState& state, unsigned slot, BattleQueuedCommand action) {
    const auto conditions = state.status.targets[slot].active;
    if (conditions & (status_mask(BattleStatus::confuse) | status_mask(BattleStatus::zombie)))
        action.parameters = 0;
    action.mp_cost = battle_character_command_mp_cost(state, slot, action);
    append_battle_normal_action(state, static_cast<std::uint8_t>(slot), action);
}
} // namespace
std::uint8_t choose_battle_dance_attack(BattleState& state, unsigned slot,
    const std::array<std::array<std::uint8_t, 4>, 8>& attacks,
    const std::array<std::uint8_t, 3>& thresholds,
    const std::array<std::uint8_t, 256>& random) {
    require(slot < 4, "Dance requires a character slot");
    auto& dance = state.selections.dances[slot];
    if (dance == 255) {
        const auto count = state.skills.dances.count();
        dance = 0; // An empty learned mask selects dance zero without a draw.
        if (count) {
            auto choice = unsigned{state.random.next(random)} * static_cast<unsigned>(count) >> 8U;
            for (unsigned i = 0; i < 8; ++i)
                if (state.skills.dances[i]) {
                    if (choice == 0) {
                        dance = static_cast<std::uint8_t>(i);
                        break;
                    }
                    --choice;
                }
        }
    }
    require(dance < 8, "Invalid dance selection");
    const auto draw = state.random.next(random);
    unsigned attack = 0;
    for (auto threshold : thresholds)
        attack += draw < threshold;
    return attacks[dance][attack];
}
std::uint8_t choose_battle_rage_attack(BattleState& state, unsigned slot,
    const std::array<std::array<std::uint8_t, 2>, 256>& attacks,
    const std::array<std::uint8_t, 256>& random) {
    require(slot < 4, "Rage requires a character slot");
    auto& rage = state.selections.rages[slot];
    rage &= 255U;
    auto attack_rage = static_cast<std::uint8_t>(rage);
    if (rage == 255) {
        rage = 0;
        const auto count = state.skills.rages.size();
        require(count <= 255 &&
                    std::ranges::none_of(state.skills.rages, [](auto v) { return v == 255; }),
                "Invalid learned rage list");
        const auto choice = (unsigned{state.random.next(random)} * static_cast<unsigned>(count)) >> 8U;
        if (count) {
            rage = state.skills.rages[choice];
            attack_rage = static_cast<std::uint8_t>(rage);
        }
        // With an empty list the source retains the FF terminator for attack
        // selection, while the stored rage identity has already become zero.
    }
    const auto choice = state.random.next(random) & 1U;
    return attacks[attack_rage][choice];
}
BattleSubmissionData load_battle_submission_data(const ContentPack& pack) {
    BattleSubmissionData out;
    const auto tables = pack.get("battle_player_command_tables"),
               dance = pack.get("data_cffe80_dance_prop"),
               rage = pack.get("data_cf4600_monster_rage"),
               weights = pack.get("battle_dance_thresholds"),
               random = pack.get("encounter_rng_tbl_bin");
    require(tables.size() == 24 && weights.size() == 3 && dance.size() == 32 &&
                rage.size() == 512 && random.size() == 256,
            "Invalid player command resource extent");
    std::copy_n(tables.begin(), 5, out.attack_commands.begin());
    std::copy_n(tables.begin() + 5, 5, out.attack_offsets.begin());
    for (unsigned i = 0; i < 32; ++i)
        out.retarget_commands[i] = (tables[10 + i / 8] & (1U << (i % 8))) != 0;
    std::copy_n(tables.begin() + 14, 8, out.slot_attacks.begin());
    std::copy_n(tables.begin() + 22, 2, out.joker_targets.begin());
    std::copy(weights.begin(), weights.end(), out.dance_thresholds.begin());
    std::copy(random.begin(), random.end(), out.random.begin());
    for (unsigned i = 0; i < 8; ++i)
        std::copy_n(dance.begin() + i * 4, 4, out.dance_attacks[i].begin());
    for (unsigned i = 0; i < 256; ++i)
        std::copy_n(rage.begin() + i * 2, 2, out.rage_attacks[i].begin());
    require(std::ranges::all_of(out.attack_commands, [](auto v) { return v < 32; }),
            "Invalid attack-offset command");
    return out;
}
void submit_battle_player_command(BattleState& state, BattlePlayerCommand command) {
    validate(command);
    auto& queue = state.player_commands;
    require(queue.read < 4 && queue.write < 4, "Invalid player queue position");
    if (queue.pending[queue.write])
        throw std::runtime_error("Player command queue is full");
    queue.pending[queue.write] = std::move(command);
    queue.write = static_cast<std::uint8_t>((queue.write + 1U) & 3U);
}
std::size_t consume_battle_player_commands(BattleState& state, const BattleSubmissionData& data) {
    validate_battle_state(state);
    require(state.player_commands.read < 4 && state.player_commands.write < 4,
            "Invalid player queue position");
    auto next = state;
    std::size_t consumed = 0;
    auto& queue = next.player_commands;
    for (unsigned i = 0; i < 4 && queue.pending[queue.read]; ++i) {
        const auto selected = *queue.pending[queue.read];
        validate(selected);
        const auto slot = selected.character;
        schedule_battle_turn(next, slot, false);
        next.turns[slot].weapon_locked = false;
        next.status.targets[slot].menu_allowed = false;
        queue.pending[queue.read].reset();
        queue.read = static_cast<std::uint8_t>((queue.read + 1U) & 3U);
        append(next, slot, convert(next, data, selected));
        if (selected.second)
            append(next, slot, {23, selected.second->attack, selected.second->targets, 0});
        ++consumed;
    }
    state = std::move(next);
    return consumed;
}
BattleTimeResult service_battle_time(BattleState& state, BattleTimeInput input,
                                     const BattleSubmissionData& data) {
    auto next = state;
    const auto count = consume_battle_player_commands(next, data);
    const auto stepped = advance_battle_time(next, input, data.random);
    state = std::move(next);
    return {count, stepped};
}
} // namespace ff6
