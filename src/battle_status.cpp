#include "ff6/battle_status.hpp"
#include "rom-materials/battle_status_durations.macros.inc"

namespace ff6 {
namespace {
using Status = BattleStatus;
using Update = BattleStatusUpdate;
constexpr auto mask(Status status) {
    return status_mask(status);
}
void request(BattleStatusTarget& target, Update update) {
    target.updates.set(static_cast<unsigned>(update));
}
struct StatusApplication {
    BattleStatusTarget& target;
    bool monster;
    std::uint8_t* equipment_neighbor;
    std::uint32_t apply, remove;

    void clear_existing(std::uint32_t bits) {
        remove |= (target.active | apply) & bits;
    }
    void refresh_equipment() {
        if (!monster) {
            // The source increments a word spanning this byte and the next
            // counter (or the window-graphics index after character three).
            if (target.equipment_refresh_count == 255)
                *equipment_neighbor = wrap8(unsigned{*equipment_neighbor} + 1U);
            target.equipment_refresh_count = wrap8(unsigned{target.equipment_refresh_count} + 1U);
        }
    }
    void cancel_advance() {
        target.menu_allowed = false;
        request(target, Update::remove_advance_actions);
    }
    void revive() {
        if (monster) {
            target.monster_alive = true;
            target.monster_dead_or_escaped = false;
        }
    }
    void refresh_appearance() {
        // The source helper loads the target mask through an eight-bit index;
        // only character bits reach the changed-appearance mask. Death and
        // facing use separate full-width updates and also affect monsters.
        if (!monster)
            target.appearance_changed = true;
    }
    void petrify() {
        target.died = true;
        clear_existing(mask(Status::blind) | mask(Status::poison) | mask(Status::vanish) |
                       mask(Status::near_fatal) | mask(Status::image) | mask(Status::silence) |
                       mask(Status::berserk) | mask(Status::confuse) | mask(Status::sap) |
                       mask(Status::sleep));
        constexpr auto survive =
            mask(Status::reraise) | mask(Status::hide) | mask(Status::interceptor);
        remove |= (target.active | apply) & 0xffff0000U & ~survive;
        target.phantasm = false;
        cancel_advance();
    }
    void set(Status status) {
        switch (status) {
        case Status::zombie:
            clear_existing(mask(Status::dead));
            target.died = true;
            refresh_appearance();
            break;
        case Status::confuse:
            target.reversed_facing = true;
            refresh_appearance();
            break;
        case Status::vanish:
            if (monster)
                target.invisible = true;
            break;
        case Status::imp:
            request(target, Update::spells);
            request(target, Update::commands);
            refresh_equipment();
            break;
        case Status::dead:
            clear_existing(mask(Status::petrify) | mask(Status::condemned));
            remove &= ~mask(Status::dead);
            petrify();
            break;
        case Status::petrify:
            petrify();
            break;
        case Status::sleep:
            target.sleep_counter = FF6_ROM_VALUE_BATTLE_STATUS_DURATIONS_0;
            cancel_advance();
            break;
        case Status::condemned:
            request(target, Update::start_condemned);
            break;
        case Status::silence:
            request(target, Update::commands);
            break;
        case Status::sap:
            remove |= mask(Status::regen);
            break;
        case Status::regen:
            remove |= mask(Status::sap);
            break;
        case Status::slow:
            remove |= mask(Status::haste);
            request(target, Update::speed);
            break;
        case Status::haste:
            remove |= mask(Status::slow);
            request(target, Update::speed);
            break;
        case Status::morph:
            request(target, Update::morph);
            break;
        case Status::stop:
            target.stop_counter = FF6_ROM_VALUE_BATTLE_STATUS_DURATIONS_1;
            break;
        case Status::reflect:
            target.reflect_counter = FF6_ROM_VALUE_BATTLE_STATUS_DURATIONS_2;
            break;
        case Status::frozen:
            target.freeze_counter = FF6_ROM_VALUE_BATTLE_STATUS_DURATIONS_3;
            break;
        default:
            break; // The remaining conditions have no application hook.
        }
    }
    void clear(Status status) {
        switch (status) {
        case Status::zombie:
            revive();
            refresh_appearance();
            break;
        case Status::confuse:
            target.reversed_facing = false;
            refresh_appearance();
            break;
        case Status::vanish:
            if (monster)
                target.invisible = false;
            break;
        case Status::imp:
            request(target, Update::spells);
            request(target, Update::commands);
            refresh_equipment();
            break;
        case Status::rage:
            refresh_equipment();
            break;
        case Status::petrify:
        case Status::dead:
            revive();
            target.wake_action_pending = true;
            request(target, Update::remove_advance_actions);
            break;
        case Status::condemned:
            request(target, Update::stop_condemned);
            break;
        case Status::silence:
            request(target, Update::commands);
            break;
        case Status::sleep:
            target.wake_action_pending = true;
            break;
        case Status::slow:
        case Status::haste:
            request(target, Update::speed);
            break;
        case Status::morph:
            request(target, Update::morph);
            break;
        default:
            break; // Clearing other conditions has no immediate hook.
        }
    }
    template <class F> void visit(std::uint32_t bits, F work) {
        // The source traverses status 1/2 high to low, then 3/4 high to low.
        // Snapshot the bits: hooks can change requests, not this traversal.
        for (const unsigned end : {16U, 32U})
            for (unsigned remaining = end; remaining > end - 16U; --remaining) {
                const auto status = static_cast<Status>(remaining - 1U);
                if ((bits & mask(status)) != 0)
                    work(status);
            }
    }
};
void update_target(BattleStatusTarget& target, const BattleStatusContext& stats, bool monster,
                   std::uint8_t* equipment_neighbor) {
    if (!target.present)
        return;
    StatusApplication work{target, monster, equipment_neighbor,
                           target.pending.apply & ~stats.blocked_status, target.pending.remove};
    // Existing petrification is reapplied even if its immunity is now set.
    work.apply |= target.active & mask(Status::petrify);
    const bool near_fatal = stats.hp.current <= (stats.hp.maximum >> 3U);
    if (near_fatal && (target.active & mask(Status::near_fatal)) == 0)
        work.apply |= mask(Status::near_fatal);
    if (!near_fatal && (target.active & mask(Status::near_fatal)) != 0)
        work.remove |= mask(Status::near_fatal);
    if ((work.apply & mask(Status::dead)) != 0 && target.overcast) {
        work.apply = (work.apply | mask(Status::zombie)) & ~mask(Status::dead);
        work.remove |= mask(Status::condemned);
    }
    // Retaliation sees the provisional result before per-condition hooks and
    // before immunity filters removals. Zombie cleanup happens even later.
    if (target.capture_retaliation)
        target.retaliation = (target.active | work.apply) & ~work.remove;
    work.visit(work.apply, [&](Status status) { work.set(status); });
    work.remove &= ~stats.blocked_status;
    work.visit(work.remove, [&](Status status) { work.clear(status); });
    target.active = (target.active | work.apply) & ~work.remove;
    if ((target.active & mask(Status::zombie)) != 0)
        target.active &= ~(mask(Status::blind) | mask(Status::poison) | mask(Status::near_fatal) |
                           mask(Status::sleep) | mask(Status::confuse) | mask(Status::berserk));
    target.pending = {};
}
} // namespace
void update_battle_status(BattleStatusState& state,
                          const std::array<BattleStatusContext, 10>& context) {
    for (std::size_t remaining = state.targets.size(); remaining != 0; --remaining) {
        const auto slot = remaining - 1;
        auto* neighbor = slot < 3 ? &state.targets[slot + 1].equipment_refresh_count
                                  : &state.menu_window_graphics_index;
        update_target(state.targets[slot], context[slot], slot >= 4, neighbor);
    }
}
BattleStatusState initialize_battle_status(const BattlePreparation& preparation,
                                           const BattleParty& party) {
    BattleStatusState result;
    result.menu_window_graphics_index = 255;
    std::array<BattleStatusContext, 10> context;
    for (std::size_t remaining = result.targets.size(); remaining != 0; --remaining) {
        const auto i = remaining - 1;
        auto& target = result.targets[i];
        const bool monster = i >= 4;
        const CombatStats* stats = nullptr;
        target.capture_retaliation = true;
        target.updates = 0xbc;
        if (monster) {
            target.present = preparation.monsters.present[i - 4];
            target.monster_alive = true;
            if (const auto& record = preparation.monsters.slots[i - 4])
                stats = &record->stats;
        } else {
            target.present = party.slots[i].present;
            target.equipment_refresh_count = 255;
            if (const auto& record = preparation.characters.slots[i]) {
                stats = &record->combat.stats;
                target.updates.set(static_cast<unsigned>(Update::stop_condemned),
                                   record->combat.condemned_counter_update);
            }
        }
        const CombatStats empty;
        const auto& source = stats ? *stats : empty;
        target.pending.apply = source.pending_status;
        context[i] = {source.hp, source.blocked_status};
    }
    update_battle_status(result, context);
    return result;
}
} // namespace ff6
