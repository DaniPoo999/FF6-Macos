#include "ff6/battle_ai.hpp"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
std::uint16_t mask(unsigned slot) {
    require(slot < 10, "AI participant outside battle");
    return static_cast<std::uint16_t>(1U << (slot < 4 ? slot : slot + 4));
}
std::uint16_t encoded(std::bitset<10> slots) {
    std::uint16_t out = 0;
    for (unsigned i = 0; i < 10; ++i)
        if (slots[i])
            out |= mask(i);
    return out;
}
std::optional<unsigned> last_target(std::uint16_t targets) {
    for (unsigned i = 10; i != 0; --i)
        if (targets & mask(i - 1))
            return i - 1;
    return {};
}
std::uint16_t random_target(std::uint16_t targets, const BattleAiData& data,
                             BattleRandom& random) {
    if (!targets)
        return 0; // Unlike RandA(0), RandBit's empty set consumes no draw.
    unsigned choice = (unsigned{random.next(data.random)} * static_cast<unsigned>(std::popcount(targets))) >> 8U;
    for (unsigned bit = 0; bit < 16; ++bit)
        if (targets & (1U << bit)) {
            if (choice == 0)
                return static_cast<std::uint16_t>(1U << bit);
            --choice;
        }
    throw std::logic_error("Unresolved AI target draw");
}
bool has(const BattleState& battle, unsigned i, BattleStatus status) {
    return (battle.status.targets[i].active & status_mask(status)) != 0;
}
std::uint16_t present_targets(std::uint16_t targets, const BattleState& battle) {
    for (unsigned i = 0; i < 10; ++i) {
        if (!battle.status.targets[i].present || has(battle, i, BattleStatus::dead) ||
            has(battle, i, BattleStatus::hide) ||
            (i >= 4 && (has(battle, i, BattleStatus::petrify) ||
                        has(battle, i, BattleStatus::zombie))))
            targets &= static_cast<std::uint16_t>(~mask(i));
    }
    return targets;
}
std::uint16_t select(std::uint8_t selector, bool condition, unsigned self,
                     const BattleAiData& data, const BattleState& battle,
                     BattleAiEnvironment& environment, BattleRandom& random) {
    require(selector <= 0x4d, "Unsupported AI target selector");
    auto targets = encoded(environment.allocated);
    if (!condition)
        targets = present_targets(targets, battle) & encoded(environment.selectable);
    // Named actors and explicit monster slots intentionally replace the initial
    // filtered set. These selectors can name absent/dead participants.
    if (selector < 0x30) {
        for (unsigned i = 4; i != 0; --i)
            if (battle.actors[i - 1] == selector)
                return mask(i - 1);
        return 0;
    }
    if (selector < 0x36)
        return environment.allocated[selector - 0x30 + 4] ? mask(selector - 0x30 + 4) : 0;
    if (selector == 0x36)
        return mask(self);
    if (selector == 0x45) {
        const auto& memory = battle.attack_memory[self];
        return memory.retaliation_attacker && !memory.retaliation_pending
                   ? mask(*memory.retaliation_attacker) : 0;
    }
    if (selector == 0x47)
        return 0;
    if (selector >= 0x48 && selector <= 0x4b)
        return battle.status.targets[selector - 0x48].present ? mask(selector - 0x48) : 0;
    if (selector == 0x4d) {
        const auto target = environment.preferred_targets[self];
        environment.preferred_targets[self].reset();
        return target ? present_targets(mask(*target), battle) : 0;
    }
    if (selector >= 0x3b && selector <= 0x42) {
        targets = 0;
        const auto status = selector <= 0x3e ? BattleStatus::dead : BattleStatus::reflect;
        for (unsigned i = 0; i < 10; ++i)
            if (has(battle, i, status))
                targets |= mask(i);
    }
    if (selector == 0x37 || selector == 0x39 || selector == 0x4c)
        targets &= static_cast<std::uint16_t>(~mask(self));
    if ((selector >= 0x37 && selector <= 0x3a) || selector == 0x3d ||
        selector == 0x3e || selector == 0x41 || selector == 0x42)
        targets &= 0xff00U;
    if (selector == 0x3b || selector == 0x3c || selector == 0x3f || selector == 0x40 ||
        selector == 0x43 || selector == 0x44)
        targets &= 0x00ffU;
    const bool choose = selector == 0x39 || selector == 0x3a || selector == 0x3c ||
                        selector == 0x3e || selector == 0x40 || selector == 0x42 ||
                        selector == 0x44 ||
                        (selector == 0x4c && (random.next(data.random) & 1U) == 0);
    return choose ? random_target(targets, data, random) : targets;
}
std::optional<unsigned> condition_target(std::uint8_t selector, BattleAiExecution& e,
                                         const BattleAiData& data, const BattleState& battle,
                                         BattleAiEnvironment& environment, BattleRandom& random) {
    const auto targets = select(selector, true, e.participant, data, battle, environment, random);
    if (targets)
        e.targets = targets;
    // The original clears the low target byte twice for SELF, so a monster's
    // self target remains. Keep this observable asymmetry, not a presumed fix.
    if (selector == 0x36)
        e.targets &= 0xff00U;
    return last_target(targets);
}
bool condition(std::uint8_t code, std::uint8_t a, std::uint8_t b, BattleAiExecution& e,
               const BattleAiData& data, const BattleState& battle,
               BattleAiEnvironment& environment, BattleRandom& random) {
    const auto self = e.participant;
    const auto remember = [&](const std::optional<BattleObservedAttackValue>& value, bool element) {
        if (!value || (element ? !(value->value & a) : value->value != a && value->value != b))
            return false;
        e.targets = mask(value->attacker);
        return true;
    };
    const auto variable = [&] {
        return a < environment.variables.size() ? environment.variables[a]
                                                : environment.personal_variables[self];
    };
    switch (code) {
    case 0: case 10: return false;
    case 1: return remember(battle.attack_memory[self].command, false);
    case 2: return remember(battle.attack_memory[self].attack, false);
    case 3: return remember(battle.attack_memory[self].item, false);
    case 4: return remember(battle.attack_memory[self].elements, true);
    case 5: {
        if (!environment.last_hit_targets[self])
            return false;
        e.targets = mask(*environment.last_hit_targets[self]);
        return true;
    }
    case 6: case 7: case 8: case 9: case 14: case 15: case 23: case 26: {
        const auto target = condition_target(a, e, data, battle, environment, random);
        bool result = false;
        if (target) {
            const auto& stats = battle.stats[*target];
            if (code == 6)
                result = stats.hp.current <= unsigned{b} * 128U;
            else if (code == 7)
                result = stats.mp.current <= b;
            else if (code == 14 || code == 15)
                result = stats.level >= b;
            else if (code == 26)
                result = (stats.weak & b) != 0;
            else if (code == 23)
                result = true;
            else {
                require(b < 32, "AI status condition outside status set");
                if (b >= 16) {
                    const auto living = static_cast<std::uint16_t>(
                        battle.participation.living_allies.to_ulong() |
                        (battle.participation.living_monsters.to_ulong() << 8U));
                    e.targets &= living;
                }
                for (unsigned i = 0; i < 10; ++i)
                    if (!(battle.status.targets[i].active & (std::uint32_t{1} << b)))
                        e.targets &= static_cast<std::uint16_t>(~mask(i));
                if (e.targets) {
                    e.targets = random_target(e.targets, data, random);
                    result = true;
                }
            }
        }
        // The inverse predicates also invert a failed target lookup.
        return code == 9 || code == 14 ? !result : result;
    }
    case 11: return battle.time.targets[self].elapsed / 2U >= (unsigned{a} | (unsigned{b} << 8U));
    case 12: return variable() < b;
    case 13: return variable() >= b;
    case 16: return battle.display.distinct_names <= 1;
    case 17: case 18: {
        if (code == 18)
            e.unresponsive = false;
        const auto requested = a ? a : static_cast<std::uint8_t>(mask(self) >> 8U);
        auto available = static_cast<std::uint8_t>(battle.participation.living_monsters.to_ulong());
        if (code == 18)
            available ^= static_cast<std::uint8_t>(encoded(environment.allocated) >> 8U);
        return (available & requested) == requested;
    }
    case 19: return a ? b >= battle.participation.enemy_count : battle.participation.ally_count >= b;
    case 20: case 21: {
        require(b < 8, "AI switch bit outside byte variable");
        const bool set = (variable() & (1U << b)) != 0;
        return code == 20 ? set : !set;
    }
    case 22: return battle.time.elapsed / 2U >= (unsigned{a} | (unsigned{b} << 8U));
    case 24: return !environment.gau_recruited;
    case 25: return ((mask(self) >> 8U) & a) != 0;
    case 27: return environment.battle_identity == (unsigned{a} | (unsigned{b} << 8U));
    case 28: e.unresponsive = false; return true;
    default: throw std::invalid_argument("Unsupported AI condition");
    }
}
ByteView instruction(const BattleAiData& data, std::size_t cursor) {
    require(cursor < data.scripts.size(), "AI cursor outside script data");
    const auto size = battle_ai_command_size(data.scripts[cursor]);
    require(size <= data.scripts.size() - cursor, "Truncated AI instruction");
    return ByteView(data.scripts).subspan(cursor, size);
}
void skip_block(BattleAiExecution& e, const BattleAiData& data) {
    e.targets = e.initial_targets;
    for (std::size_t cursor = e.cursor; cursor < data.scripts.size();) {
        const auto command = instruction(data, cursor);
        cursor += command.size();
        if (command[0] < 0xfe)
            continue;
        if (command[0] == 0xff) {
            e.complete = true;
            e.progress.block = 255;
        } else {
            require(cursor <= 65535, "AI continuation outside offset range");
            e.cursor = static_cast<std::uint16_t>(cursor);
            e.block = wrap8(unsigned{e.block} + 1U);
            e.unresponsive = e.initially_unresponsive;
        }
        return;
    }
    throw std::invalid_argument("Unterminated AI conditional block");
}
std::optional<BattleAiAction> step(BattleAiExecution& e, const BattleAiData& data,
                                  const BattleState& battle, BattleAiEnvironment& environment,
                                  BattleRandom& random) {
    if (e.complete)
        return {};
    auto bytes = instruction(data, e.cursor);
    if (bytes[0] < 0xfc && e.block == e.resume_block) {
        e.resume_block = 255;
        e.cursor = e.progress.resume;
        bytes = instruction(data, e.cursor);
    }
    const auto next = unsigned{e.cursor} + bytes.size();
    require(next <= 65535, "AI continuation outside offset range");
    e.cursor = static_cast<std::uint16_t>(next);
    const auto op = bytes[0];
    if (e.unresponsive && op < 0xfc) {
        skip_block(e, data);
        return {};
    }
    if (op == 0xfc) {
        if (!condition(bytes[1], bytes[2], bytes[3], e, data, battle, environment, random))
            skip_block(e, data);
        return {};
    }
    if (op >= 0xfd) {
        e.complete = true;
        e.progress.block = op == 0xfd ? e.block : 255;
        if (op == 0xfd)
            e.progress.resume = e.cursor;
        return {};
    }
    if (op == 0xf1) {
        e.targets = select(bytes[1], false, e.participant, data, battle, environment, random);
        return {};
    }
    if (op < 0xf0 || op == 0xf0 || op == 0xf4 || op == 0xf6) {
        std::uint8_t value = op, command = 0;
        if (op >= 0xf0) {
            const unsigned choice = (unsigned{random.next(data.random)} * 3U) >> 8U;
            value = op == 0xf6 ? bytes[choice == 2 ? 3 : 2] : bytes[choice + 1];
            if (value == 0xfe)
                return {};
        }
        command = op == 0xf4 ? value : op == 0xf6 ? (bytes[1] ? 8 : 1)
                                                            : battle_ai_attack_command(data, value);
        if (command == 12 && value == 140)
            command = 30; // Roulette uses its specialized target-preparation command.
        return BattleAiAction{{command, static_cast<std::uint8_t>(op == 0xf4 ? 0 : value),
                               has(battle, e.participant, BattleStatus::confuse) ? std::uint16_t{0}
                                                                              : e.targets, 0}, true};
    }
    std::uint8_t command = 0;
    switch (op) {
    case 0xf2: command = 32; break;
    case 0xf3: command = 33; break;
    case 0xf5: command = 36; break;
    case 0xf7: command = 35; break;
    case 0xf8: command = 46; break;
    case 0xf9: command = 47; break;
    case 0xfa: command = 43; break;
    case 0xfb: command = 48; break;
    default: throw std::invalid_argument("Unsupported AI instruction");
    }
    unsigned parameters = bytes.size() >= 3 ? bytes[2] : 0;
    if (bytes.size() == 4)
        parameters |= unsigned{bytes[3]} << 8U;
    if (op == 0xf5 && bytes[3] == 0)
        parameters |= mask(e.participant) & 0xff00U;
    return BattleAiAction{{command, bytes[1], static_cast<std::uint16_t>(parameters), 0}, false};
}
} // namespace
std::uint16_t battle_ai_control_targets(std::uint8_t selector, std::uint8_t participant,
                                        const BattleAiData& data, const BattleState& battle,
                                        BattleAiEnvironment& environment, BattleRandom& random) {
    require(participant < 10, "Invalid AI control participant");
    return select(selector, true, participant, data, battle, environment, random);
}
std::uint8_t battle_ai_attack_command(const BattleAiData& data, std::uint8_t attack) {
    for (unsigned i = 11; i != 0; --i)
        if (attack >= data.first_attacks[i - 1])
            return data.commands[i - 1];
    return 2; // Ordinary magic before the first alternate command range.
}
BattleAiData load_battle_ai(const ContentPack& pack) {
    BattleAiData result;
    auto source = pack.get("data_cf8700_ai_scripts");
    require(!source.empty() && source.size() <= 65536, "Invalid AI script data extent");
    result.scripts.assign(source.begin(), source.end());
    const auto copy = [&](const char* name, auto& out) {
        const auto bytes = pack.get(name);
        require(bytes.size() == out.size(), "Invalid AI lookup table extent");
        std::ranges::copy(bytes, out.begin());
    };
    copy("encounter_rng_tbl_bin", result.random);
    copy("battle_ai_attack_ranges", result.first_attacks);
    copy("battle_ai_commands", result.commands);
    require(std::ranges::is_sorted(result.first_attacks), "Unordered AI attack ranges");
    require(std::ranges::all_of(result.commands, [](auto c) { return c < 30; }),
            "Invalid AI attack command");
    return result;
}
BattleAiExecution begin_battle_ai(std::uint8_t participant, std::uint16_t entry,
                                 BattleAiProgress progress, std::uint16_t targets,
                                 bool unresponsive) {
    require(participant < 10, "AI participant outside battle");
    BattleAiExecution out;
    out.participant = participant;
    out.cursor = entry;
    out.progress = progress;
    out.resume_block = progress.block;
    out.initial_targets = out.targets = targets;
    out.initially_unresponsive = out.unresponsive = unresponsive;
    return out;
}
std::optional<BattleAiAction> step_battle_ai(BattleAiExecution& execution, const BattleAiData& data,
                                           const BattleState& battle,
                                           BattleAiEnvironment& environment, BattleRandom& random) {
    require(data.scripts.size() <= 65536 && execution.participant < 10,
            "Invalid AI execution input");
    auto next = execution;
    auto next_environment = environment;
    auto next_random = random;
    const auto result = step(next, data, battle, next_environment, next_random);
    execution = next;
    environment = std::move(next_environment);
    random = next_random;
    return result;
}
} // namespace ff6
