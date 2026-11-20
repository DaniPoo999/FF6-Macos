#include "ff6/battle_target_selection.hpp"
#include "rom-materials/roulette_slow_mask.macros.inc"
#include "rom-materials/roulette_fast_mask.macros.inc"
#include "rom-materials/roulette_hold.macros.inc"
#include "rom-materials/roulette_slow_wait_mask.macros.inc"
#include "rom-materials/roulette_slow_wait_bias.macros.inc"
#include "rom-materials/roulette_hold_clock_mask.macros.inc"
#include "rom-materials/roulette_initial_wait.macros.inc"
#include <algorithm>
#include <cstdlib>

namespace ff6 {
namespace {
constexpr unsigned manual = 1, one_side = 2, initial_all = 4, initial_group = 8, initial_half = 12,
                   automatic = 16, multiple = 32, enemy = 64, roulette = 128;
constexpr Buttons directions =
    button(Button::up) | button(Button::down) | button(Button::left) | button(Button::right);
constexpr Buttons primary = directions | button(Button::b) | button(Button::y) |
                            button(Button::start) | button(Button::select);
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
const std::vector<std::uint8_t>& group(const BattleMonsterPositions& p, unsigned g) {
    switch (g) {
    case 0:
        return p.groups.monsters_left;
    case 1:
        return p.groups.characters_left;
    case 2:
        return p.groups.monsters_right;
    case 3:
        return p.groups.characters_right;
    default:
        throw std::invalid_argument("Invalid battle target group");
    }
}
unsigned mask(const BattleMonsterPositions& p, unsigned g) {
    switch (g) {
    case 0:
        return static_cast<unsigned>(p.groups.monsters_on_left.to_ulong());
    case 1:
        return static_cast<unsigned>(p.groups.characters_on_left.to_ulong());
    case 2:
        return static_cast<unsigned>(p.groups.monsters_on_right.to_ulong());
    case 3:
        return static_cast<unsigned>(p.groups.characters_on_right.to_ulong());
    default:
        throw std::invalid_argument("Invalid battle target group");
    }
}
unsigned eligible_mask(BattleTargetEligibility e, unsigned g) {
    return static_cast<unsigned>((g & 1U) ? e.characters.to_ulong() : e.monsters.to_ulong());
}
void validate(const BattleMonsterPositions& p) {
    for (unsigned g = 0; g < 4; ++g) {
        const auto& members = group(p, g);
        unsigned bits = 0;
        for (auto id : members) {
            require(id < ((g & 1U) ? 4 : 6) && !(bits & (1U << id)),
                    "Invalid battle target membership");
            bits |= 1U << id;
        }
        require(std::ranges::is_sorted(members) && bits == mask(p, g),
                "Inconsistent battle target group");
    }
    for (const auto& order : p.monster_cursor_order)
        for (auto id : order)
            require(id < 6, "Invalid monster cursor order");
    for (const auto& order : p.character_cursor_order)
        for (auto id : order)
            require(id < 4, "Invalid character cursor order");
}
void select(BattleTargetCursor& c, const BattleMonsterPositions& p, unsigned g, unsigned index) {
    const auto& members = group(p, g);
    require(index < members.size(), "Invalid battle target index");
    c.group = static_cast<std::uint8_t>(g);
    c.index = static_cast<std::uint8_t>(index);
    c.characters.reset();
    c.monsters.reset();
    if (g & 1U)
        c.characters.set(members[index]);
    else
        c.monsters.set(members[index]);
    c.multiple = false;
}
bool enter_group(BattleTargetCursor& c, const BattleMonsterPositions& p, BattleTargetEligibility e,
                 unsigned g, bool from_right = false) {
    const auto& members = group(p, g);
    const auto candidates = eligible_mask(e, g);
    const auto visit = [&](auto order) {
        for (auto id : order) {
            const auto it = std::ranges::find(members, id);
            if (it != members.end() && (candidates & (1U << id))) {
                select(c, p, g, static_cast<unsigned>(it - members.begin()));
                return true;
            }
        }
        return false;
    };
    if (g & 1U)
        return visit(p.character_cursor_order[2]);
    return visit(p.monster_cursor_order[(g == 0 || from_right) ? 1 : 0]);
}
bool nearest_monster(BattleTargetCursor& c, BattleTargetDirection direction,
                     const BattleMonsterPositions& p, BattleTargetEligibility e) {
    const auto& members = group(p, c.group);
    require(c.index < members.size(), "Monster cursor has no origin");
    const auto origin = p.positions[members[c.index]];
    unsigned best_distance = 65535;
    std::optional<unsigned> best;
    for (unsigned i = 0; i < members.size(); ++i) {
        const auto id = members[i];
        if (!e.monsters[id])
            continue;
        const auto target = p.positions[id];
        const int dx = int{target.x >> 3} - int{origin.x >> 3};
        const int dy = int{target.y >> 3} - int{origin.y >> 3};
        const int x = std::abs(dx), y = std::abs(dy);
        bool inside = false;
        switch (direction) {
        case BattleTargetDirection::up:
            inside = dy < 0 && y >= x;
            break;
        case BattleTargetDirection::down:
            inside = dy > 0 && y >= x;
            break;
        case BattleTargetDirection::left:
            inside = dx < 0 && x >= y;
            break;
        case BattleTargetDirection::right:
            inside = dx > 0 && x >= y;
            break;
        }
        if (!inside)
            continue;
        // Original multiplication reads only the low byte of each absolute
        // displacement, then adds squares with a sixteen-bit result.
        const unsigned low_x = static_cast<unsigned>(x) & 255U;
        const unsigned low_y = static_cast<unsigned>(y) & 255U;
        const auto distance = (low_x * low_x + low_y * low_y) & 65535U;
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    if (!best)
        return false;
    select(c, p, c.group, *best);
    return true;
}
void move(BattleTargetCursor& c, BattleTargetDirection direction, BattleApproach approach,
          const BattleMonsterPositions& p, BattleTargetEligibility e) {
    require(c.group < 4, "Invalid battle target group");
    const bool character = (c.group & 1U) != 0;
    if (direction == BattleTargetDirection::up || direction == BattleTargetDirection::down) {
        if (!character) {
            nearest_monster(c, direction, p, e);
            return;
        }
        const bool down = direction == BattleTargetDirection::down;
        // Two character groups, four retained positions each. Bound a search
        // that loops forever in the original when supplied inconsistent state.
        auto next = c;
        for (unsigned tries = 0; tries < 8; ++tries) {
            next.index = static_cast<std::uint8_t>((unsigned{next.index} + (down ? 1U : 3U)) & 3U);
            if (next.index == (down ? 0 : 3))
                next.group = static_cast<std::uint8_t>((next.group + 2U) & 3U);
            const auto& members = group(p, next.group);
            if (next.index < members.size() && e.characters[members[next.index]]) {
                select(c, p, next.group, next.index);
                return;
            }
        }
        throw std::invalid_argument("No eligible character in target groups");
    }
    require(direction == BattleTargetDirection::left || direction == BattleTargetDirection::right,
            "Invalid target direction");
    const bool right = direction == BattleTargetDirection::right;
    const bool cross_kind = (c.policy & one_side) == 0;
    if (!character && nearest_monster(c, direction, p, e))
        return;
    switch (approach) {
    case BattleApproach::normal:
        if (character && !right) {
            if (!cross_kind || !enter_group(c, p, e, 0))
                c.group = 1;
        } else if (!character && right && cross_kind)
            enter_group(c, p, e, 1);
        break;
    case BattleApproach::back:
        if (character && right) {
            if (!cross_kind || !enter_group(c, p, e, 2))
                c.group = 1;
        } else if (!character && !right) {
            if (!cross_kind || !enter_group(c, p, e, 1))
                c.group = 2;
        }
        break;
    case BattleApproach::pincer:
        if (character) {
            if (!cross_kind || !enter_group(c, p, e, right ? 2 : 0))
                c.group = 1;
        } else if ((right && c.group == 0) || (!right && c.group == 2)) {
            if (!cross_kind || !enter_group(c, p, e, 1))
                enter_group(c, p, e, right ? 2 : 0);
        }
        break;
    case BattleApproach::side:
        if (character) {
            if ((right && c.group == 1) || (!right && c.group == 3)) {
                if (!cross_kind || !enter_group(c, p, e, 2, !right))
                    enter_group(c, p, e, right ? 3 : 1);
            }
        } else if (cross_kind)
            enter_group(c, p, e, right ? 3 : 1);
        break;
    default:
        throw std::invalid_argument("Invalid battle approach");
    }
}
} // namespace
bool initialize_battle_target_cursor(BattleTargetCursor& cursor, std::uint8_t policy,
                                     std::uint8_t character, const BattleMonsterPositions& p,
                                     BattleTargetEligibility e) {
    require(character < 4, "Invalid target-selection character");
    validate(p);
    BattleTargetCursor c;
    c.policy = policy;
    c.roulette_index = cursor.roulette_index;
    const auto initial = policy & 12U;
    bool available = true;
    if (policy == one_side || (policy & roulette))
        c.characters.set(character);
    else if (initial == initial_half || initial == initial_all) {
        if (!(policy & enemy) || initial == initial_all)
            c.characters = e.characters;
        if ((policy & enemy) || initial == initial_all)
            c.monsters = e.monsters;
        c.multiple = true;
    } else if (policy & enemy) {
        if (initial == initial_group) {
            c.group = (mask(p, 0) & eligible_mask(e, 0)) ? 0 : 2;
            c.monsters = mask(p, c.group) & eligible_mask(e, c.group);
            c.multiple = true;
        } else
            available = enter_group(c, p, e, 0) || enter_group(c, p, e, 2);
    } else {
        c.group =
            std::ranges::find(p.groups.characters_left, character) != p.groups.characters_left.end()
                ? 1
                : 3;
        if (initial == initial_group) {
            c.characters = mask(p, c.group) & eligible_mask(e, c.group);
            c.multiple = true;
        } else {
            c.characters.set(character);
            // The original retains the combat slot, not its position in the
            // side-group vector. Vertical navigation must retain that quirk.
            c.index = character;
        }
    }
    if (policy & automatic)
        c.multiple = false;
    cursor = c;
    return available;
}
void move_battle_target_cursor(BattleTargetCursor& c, BattleTargetDirection direction,
                               BattleApproach approach, const BattleMonsterPositions& p,
                               BattleTargetEligibility e) {
    validate(p);
    auto next = c;
    move(next, direction, approach, p, e);
    c = next;
}
BattleTargetResult navigate_battle_targets(BattleTargetCursor& c, BattleTargetInput input,
                                           BattleApproach approach, const BattleMonsterPositions& p,
                                           BattleTargetEligibility e,
                                           const std::array<std::uint8_t, 256>& random_table,
                                           std::uint8_t& random_index) {
    validate(p);
    require(c.group < 4, "Invalid battle target group");
    BattleTargetResult result;
    const auto cancel = [&] {
        c.characters.reset();
        c.monsters.reset();
        c.multiple = false;
        result.decision = BattleTargetDecision::cancel;
    };
    const auto confirm = [&] { result.decision = BattleTargetDecision::confirm; };
    if (input.closing) {
        cancel();
        return result;
    }
    if (c.policy & automatic) {
        confirm();
        return result;
    }
    if ((c.characters & e.characters).none() && (c.monsters & e.monsters).none()) {
        cancel();
        return result;
    }
    if (c.policy & roulette) {
        if (c.roulette_stopped) {
            c.roulette_countdown = wrap8(unsigned{c.roulette_countdown} - 1U);
            if (!c.roulette_countdown) {
                confirm();
                return result;
            }
        } else if ((input.clock & (c.roulette_slowing ? FF6_ROM_VALUE_ROULETTE_SLOW_MASK_0 : FF6_ROM_VALUE_ROULETTE_FAST_MASK_0)) == 0) {
            if (c.roulette_slowing) {
                c.roulette_countdown = wrap8(unsigned{c.roulette_countdown} - 1U);
                if (!c.roulette_countdown) {
                    c.roulette_stopped = true;
                    c.roulette_countdown = FF6_ROM_VALUE_ROULETTE_HOLD_0;
                    c.multiple = true;
                    return result;
                }
            }
            for (unsigned attempt = 0; attempt < 16; ++attempt) {
                ++result.sounds.move;
                c.roulette_index = wrap8(unsigned{c.roulette_index} + 1U);
                const unsigned id = c.roulette_index & 7U;
                const bool character = (c.roulette_index & 8U) != 0;
                if ((character && id < 4 && e.characters[id]) ||
                    (!character && id < 6 && e.monsters[id])) {
                    c.characters = character ? (1U << id) : 0;
                    c.monsters = character ? 0 : (1U << id);
                    break;
                }
            }
        }
    } else {
        bool select_group = false;
        if ((c.policy & 13U) == initial_group &&
            (input.repeating & (button(Button::left) | button(Button::right)))) {
            ++result.sounds.move;
            const bool left = held(input.repeating, Button::left);
            if (left == ((c.group & 2U) != 0) && eligible_mask(e, c.group)) {
                // Original validation checks the target kind, not this side's
                // mask. It can select an empty group; the next visit cancels.
                c.group = static_cast<std::uint8_t>((c.group + 2U) & 3U);
                select_group = true;
            }
        }
        if (!select_group && (c.policy & manual)) {
            const auto shoulders = input.repeating & (button(Button::l) | button(Button::r));
            if (shoulders == button(Button::l) || shoulders == button(Button::r)) {
                ++result.sounds.move;
                select_group = (c.policy & multiple) != 0;
            }
            if (!select_group) {
                if (input.repeating & directions)
                    ++result.sounds.move;
                const auto key = input.repeating & primary;
                for (const auto [button_id, direction] :
                     {std::pair{Button::up, BattleTargetDirection::up},
                      {Button::down, BattleTargetDirection::down},
                      {Button::left, BattleTargetDirection::left},
                      {Button::right, BattleTargetDirection::right}})
                    if (key == button(button_id))
                        move(c, direction, approach, p, e);
            }
        }
        if (select_group) {
            const auto selected = mask(p, c.group) & eligible_mask(e, c.group);
            if (c.group & 1U)
                c.characters = selected;
            else
                c.monsters = selected;
            c.multiple = true;
        }
    }
    if (held(input.repeating, Button::a)) {
        ++result.sounds.select;
        if (!(c.policy & roulette)) {
            confirm();
            return result;
        }
        if (!c.roulette_slowing) {
            c.roulette_slowing = true;
            c.roulette_countdown =
                static_cast<std::uint8_t>((random_table[random_index] & FF6_ROM_VALUE_ROULETTE_SLOW_WAIT_MASK_0) + FF6_ROM_VALUE_ROULETTE_SLOW_WAIT_BIAS_0);
            random_index = wrap8(unsigned{random_index} + 1U);
        }
    }
    if (held(input.pressed, Button::b) && !c.roulette_slowing) {
        ++result.sounds.select;
        cancel();
    }
    return result;
}
BattleAutomaticRoulette begin_battle_automatic_roulette(
    const std::array<std::uint8_t, 256>& random, std::uint8_t& index) {
    BattleAutomaticRoulette out;
    out.index = out.fast_wait = static_cast<std::uint8_t>((random[index] & FF6_ROM_VALUE_ROULETTE_INITIAL_WAIT_0) + FF6_ROM_VALUE_ROULETTE_INITIAL_WAIT_1);
    index = wrap8(unsigned{index} + 1U);
    return out;
}
std::optional<std::uint8_t> advance_battle_automatic_roulette(BattleAutomaticRoulette& out,
    BattleTargetEligibility eligible, std::uint8_t clock,
    const std::array<std::uint8_t, 256>& random, std::uint8_t& index) {
    if (out.complete) return {};
    // The source searches eligible bits until one is found. An empty set can
    // arise as combat ends; retire the cursor instead of entering that loop.
    if (eligible.characters.none() && eligible.monsters.none()) {
        out.complete = true;
        return {};
    }
    if (out.stopped) {
        if ((clock & FF6_ROM_VALUE_ROULETTE_HOLD_CLOCK_MASK_0) && --out.wait == 0) {
            out.complete = true;
            return out.target;
        }
        return {};
    }
    if (out.slowing) {
        if (clock & FF6_ROM_VALUE_ROULETTE_SLOW_MASK_0) return {};
        if (--out.wait == 0) {
            out.stopped = true;
            out.wait = FF6_ROM_VALUE_ROULETTE_HOLD_0;
            return {};
        }
    } else {
        if (!out.fast_wait) {
            out.wait = static_cast<std::uint8_t>((random[index] & FF6_ROM_VALUE_ROULETTE_SLOW_WAIT_MASK_0) + FF6_ROM_VALUE_ROULETTE_SLOW_WAIT_BIAS_0);
            index = wrap8(unsigned{index} + 1U);
            out.slowing = true;
        }
        out.fast_wait = wrap8(unsigned{out.fast_wait} - 1U);
        if (clock & FF6_ROM_VALUE_ROULETTE_FAST_MASK_0) return {};
    }
    // Bit 3 changes sides; sparse slots are skipped in the original byte order.
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        out.index = wrap8(unsigned{out.index} + 1U);
        const unsigned slot = out.index & 7U;
        if ((out.index & 8U) ? (slot < 4 && eligible.characters[slot])
                             : (slot < 6 && eligible.monsters[slot])) {
            out.target = static_cast<std::uint8_t>(slot + ((out.index & 8U) ? 0 : 4));
            return {};
        }
    }
    return {};
}
} // namespace ff6
