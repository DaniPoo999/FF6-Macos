#include "ff6/battle_task_clock.hpp"
#include <stdexcept>

namespace ff6 {
namespace {
constexpr BattleExecutionWork work(std::uint32_t cycles, std::uint32_t slow_bytes) {
    return {cycles, cycles * 6U + slow_bytes * 2U};
}
void valid_slot(unsigned slot) {
    if (slot >= 85) throw std::invalid_argument("Animation task slot outside its owner");
}
}
BattleTaskGateWork battle_task_gate_work(BattleTaskVisit visit) {
    valid_slot(visit.slot);
    BattleTaskGateWork result{{}, false, false, visit.counter};
    if (!visit.active) { result.work = work(14, 2); return result; }
    if (visit.sprites_paused && visit.slot < 80) {
        result.work = work(24, 3);
        return result;
    }
    result.counted = true;
    result.counter = static_cast<std::uint8_t>(unsigned(visit.counter) - 1U);
    result.ready = result.counter == 0;
    // Pausing ordinary sprites still visits the five background/esper tasks,
    // with an additional task-kind check. A zero delay wraps to 255.
    const auto selection = visit.sprites_paused ? 4U : 0U;
    if (result.ready) {
        result.counter = visit.rate;
        result.work = work(59U + selection, 15);
    } else result.work = work(35U + selection, 7);
    return result;
}
BattleExecutionWork battle_task_dispatch_work(BattleTaskDispatch dispatch) {
    if (static_cast<unsigned>(dispatch.layer) > static_cast<unsigned>(BattleTaskLayer::overlay) ||
        static_cast<unsigned>(dispatch.token) > static_cast<unsigned>(BattleTaskToken::command) ||
        static_cast<unsigned>(dispatch.entry) > static_cast<unsigned>(BattleTaskDispatchEntry::overlay_command) ||
        (dispatch.layer_paused && (dispatch.layer == BattleTaskLayer::sprite ||
                                  dispatch.entry != BattleTaskDispatchEntry::select_layer)))
        throw std::invalid_argument("Unsupported animation task dispatch");
    const bool sprite = dispatch.entry != BattleTaskDispatchEntry::overlay_command &&
                        dispatch.layer == BattleTaskLayer::sprite;
    if (dispatch.supplied_sprite_frame && (!sprite ||
        (dispatch.token != BattleTaskToken::frame && dispatch.token != BattleTaskToken::blank_frame)))
        throw std::invalid_argument("Supplied animation frame requires sprite publication");
    if (dispatch.entry != BattleTaskDispatchEntry::select_layer || dispatch.supplied_sprite_frame) {
        // Resume at the owning layer, without replaying its outer selection or
        // pause check. Foreground rechecks conversion to a sprite; overlay does
        // not. Compute from the same reviewed publication/command paths below.
        auto selected = dispatch;
        selected.entry = BattleTaskDispatchEntry::select_layer;
        selected.supplied_sprite_frame = false;
        if (dispatch.entry == BattleTaskDispatchEntry::overlay_command)
            selected.layer = BattleTaskLayer::overlay;
        else if (dispatch.entry == BattleTaskDispatchEntry::foreground_command && !sprite)
            selected.layer = BattleTaskLayer::foreground;
        auto result = battle_task_dispatch_work(selected);
        if (dispatch.entry == BattleTaskDispatchEntry::foreground_command && sprite) {
            const auto conversion = work(9, 2);
            result.cycles += conversion.cycles;
            result.fast_rom_clocks += conversion.fast_rom_clocks;
        } else if (dispatch.entry != BattleTaskDispatchEntry::select_layer) {
            const auto selection = work(selected.layer == BattleTaskLayer::foreground ? 31U : 30U, 5);
            result.cycles -= selection.cycles;
            result.fast_rom_clocks -= selection.fast_rom_clocks;
        }
        if (dispatch.supplied_sprite_frame) {
            const auto read = work(8, 3);
            result.cycles -= read.cycles;
            result.fast_rom_clocks -= read.fast_rom_clocks;
        }
        return result;
    }
    // Initial layer selection (no command-supplied sprite frame), decoded
    // frame/termination work, and pointer retention
    // are grouped by semantic path. No instructions or program counters are
    // decoded here. See BATTLE_TASK_CLOCK.md for the source boundaries.
    if (dispatch.layer_paused)
        return dispatch.layer == BattleTaskLayer::foreground ? work(26, 3) : work(25, 3);
    switch (dispatch.layer) {
    case BattleTaskLayer::sprite:
        switch (dispatch.token) {
        case BattleTaskToken::frame: return work(152, 32);
        case BattleTaskToken::blank_frame: return work(144, 31);
        case BattleTaskToken::finish: return work(40, 7);
        case BattleTaskToken::command: return work(34, 7);
        }
        break;
    case BattleTaskLayer::foreground:
        switch (dispatch.token) {
        case BattleTaskToken::frame: return work(171, 33);
        case BattleTaskToken::blank_frame: return work(161, 32);
        case BattleTaskToken::finish: return work(99, 20);
        case BattleTaskToken::command: return work(65, 11);
        }
        break;
    case BattleTaskLayer::overlay:
        switch (dispatch.token) {
        case BattleTaskToken::frame:
        case BattleTaskToken::blank_frame: return work(149, 31);
        case BattleTaskToken::finish: return work(77, 17);
        case BattleTaskToken::command: return work(50, 10);
        }
        break;
    }
    throw std::invalid_argument("Unknown task work path");
}
BattleExecutionWork battle_task_pass_entry_work() { return work(8, 1); }
BattleExecutionWork battle_task_traversal_work(unsigned slot) {
    valid_slot(slot);
    // The final visit calls WaitFrame; all other visits take the long branch
    // back to the next slot. The two return-address bytes use slow stack data.
    return slot == 84 ? work(29, 2) : work(25, 0);
}
BattleExecutionWork battle_task_command_entry_work(bool extended) {
    return extended ? work(56, 11) : work(27, 6);
}
BattleExecutionWork battle_task_command_completion_work(BattleCommandCompletion completion) {
    switch (completion) {
    case BattleCommandCompletion::primary: return work(19, 6);
    case BattleCommandCompletion::extended: return work(30, 10);
    case BattleCommandCompletion::extended_parameter: return work(40, 14);
    case BattleCommandCompletion::narrow_gradient: return work(43, 14);
    }
    throw std::invalid_argument("Unknown animation command completion");
}
BattleExecutionWork battle_task_draw_order_entry_work(bool retained) {
    return retained ? work(7, 1) : work(12, 3);
}
BattleExecutionWork battle_task_pass_finish_work(bool active_tasks) {
    return active_tasks ? work(9, 1) : work(7, 1);
}
BattleTaskVisitTiming battle_task_visit_timing(BattleRasterTick start,
    BattleTaskVisit visit, BattleTaskDispatch dispatch,
    const std::vector<BattleRasterStall>& pauses) {
    // Validate the complete request even when the task currently skips its
    // dispatch. This keeps malformed plans independent of incidental state.
    if (dispatch.entry != BattleTaskDispatchEntry::select_layer || dispatch.supplied_sprite_frame)
        throw std::invalid_argument("A fresh task visit cannot use a command continuation");
    const auto selected = battle_task_dispatch_work(dispatch);
    BattleTaskVisitTiming result;
    result.gate = battle_task_gate_work(visit);
    result.dispatch = battle_elapsed_work(start, result.gate.work.fast_rom_clocks, pauses);
    result.finish = result.dispatch;
    if (result.gate.ready) {
        result.finish = battle_elapsed_work(result.finish, selected.fast_rom_clocks, pauses);
        result.command_pending = !dispatch.layer_paused && dispatch.token == BattleTaskToken::command;
    }
    if (!result.command_pending)
        result.finish = battle_elapsed_work(result.finish, battle_task_traversal_work(visit.slot).fast_rom_clocks,
                                            pauses);
    return result;
}
BattleExecutionWork battle_display_entry_work(bool busy) {
    // Busy entry restores the entire interrupted context and returns without
    // publishing completion. Ordinary entry ends at the first image owner.
    return busy ? work(93, 29) : work(64, 19);
}
BattleExecutionWork battle_display_return_work() { return work(38, 14); }
BattleExecutionWork battle_frame_wait_entry_work() { return work(19, 8); }
BattleExecutionWork battle_frame_wait_resume_work(BattleWaitResume resume) {
    switch (resume) {
    case BattleWaitResume::before_sample: return work(17, 5);
    case BattleWaitResume::pending_sample: return work(20, 5);
    case BattleWaitResume::released_sample: return work(14, 4);
    }
    throw std::invalid_argument("Unknown frame wait continuation");
}
} // namespace ff6
