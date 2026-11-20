#pragma once
#include "ff6/battle_display_clock.hpp"

namespace ff6 {
// Aggregate work of reviewed native scheduling paths. FastROM instruction and
// parameter reads are six clocks; working/stack data accesses are eight. These
// counts exclude display interruptions, transfers and refresh.
struct BattleExecutionWork {
    std::uint32_t cycles = 0, fast_rom_clocks = 0;
    bool operator==(const BattleExecutionWork&) const = default;
};
struct BattleTaskVisit {
    unsigned slot = 0;
    bool active = false, sprites_paused = false;
    std::uint8_t counter = 1, rate = 1;
};
struct BattleTaskGateWork {
    BattleExecutionWork work;
    bool counted = false, ready = false;
    std::uint8_t counter = 0;
};
// The gate ends at layer selection when ready, otherwise at task traversal.
BattleTaskGateWork battle_task_gate_work(BattleTaskVisit visit);
enum class BattleTaskLayer { sprite, foreground, overlay };
enum class BattleTaskToken { frame, blank_frame, finish, command };
enum class BattleTaskDispatchEntry { select_layer, foreground_command, overlay_command };
struct BattleTaskDispatch {
    BattleTaskLayer layer = BattleTaskLayer::sprite;
    bool layer_paused = false;
    BattleTaskToken token = BattleTaskToken::frame;
    BattleTaskDispatchEntry entry = BattleTaskDispatchEntry::select_layer;
    bool supplied_sprite_frame = false;
};
// A command ends at the common command router's entry, before the handler.
// Other paths end at task traversal, after any frame/pointer publication.
// Background commands resume inside their layer's owner. A foreground task
// can become a sprite there; an overlay retains its owner until the next visit.
// Only sprite frame publication can consume a nonzero command-supplied frame.
BattleExecutionWork battle_task_dispatch_work(BattleTaskDispatch dispatch);
BattleExecutionWork battle_task_pass_entry_work();
BattleExecutionWork battle_task_traversal_work(unsigned slot);
// Common router, optionally followed by command 80's secondary router. Handler
// work and its return path are deliberately separate, never assumed free.
BattleExecutionWork battle_task_command_entry_work(bool extended);
enum class BattleCommandCompletion { primary, extended, extended_parameter, narrow_gradient };
// From the handler's final return, shared parameter cleanup, or narrow-gradient
// tail to its next dispatch. Does not repeat the eligibility/delay gate.
BattleExecutionWork battle_task_command_completion_work(BattleCommandCompletion completion);
// After all post-wait owners finish: select retained order or call preparation,
// then decide whether to start another task pass or enter final cleanup.
// Preparation work and final cleanup remain separate owners.
BattleExecutionWork battle_task_draw_order_entry_work(bool retained);
BattleExecutionWork battle_task_pass_finish_work(bool active_tasks);
struct BattleTaskVisitTiming {
    BattleTaskGateWork gate;
    BattleRasterTick dispatch = 0, finish = 0;
    bool command_pending = false;
};
// Absolute clocks allow a display pause to carry a visit into the next picture.
// Pauses are admitted intervals, not guessed interrupt/row request deadlines.
BattleTaskVisitTiming battle_task_visit_timing(BattleRasterTick start,
    BattleTaskVisit visit, BattleTaskDispatch dispatch,
    const std::vector<BattleRasterStall>& pauses);

// Work inside the display handler, from its entry to first image publication,
// or to return for an already-busy handler. Hardware interrupt admission and
// vector dispatch are outside this contract.
BattleExecutionWork battle_display_entry_work(bool busy);
BattleExecutionWork battle_display_return_work();

// WaitFrame entry through setting the request, up to its first completion read.
BattleExecutionWork battle_frame_wait_entry_work();
enum class BattleWaitResume { before_sample, pending_sample, released_sample };
// Completion has been published by the display owner. A previously sampled
// pending flag survives interruption and forces one more poll. Ends at the
// first post-wait call: windows for an animation wait, HP/status preparation
// for the main battle wait. Subsequent owner work is separate.
BattleExecutionWork battle_frame_wait_resume_work(BattleWaitResume resume);
} // namespace ff6
