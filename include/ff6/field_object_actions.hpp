#pragma once
#include "ff6/field_npcs.hpp"

namespace ff6 {
struct FieldActionData {
    Bytes script;
    FieldMoveRates movement;
    FieldPoseTables poses;
    std::array<std::uint8_t, 256> random;
};
FieldActionData load_field_action_data(const ContentPack& pack);
struct FieldActionInputs {
    bool event_running = true, collision_active = false;
    bool camera_locked = false;
};
struct FieldActionResult {
    enum class Kind { waiting, moving, blocked, positioned, event_wait, event_request, finished };
    Kind kind = Kind::waiting;
    // The scene must refresh follow rates at this boundary, using its real
    // camera lock, bounds and retained center. Position commands do not do so.
    bool refresh_scroll = false;
    // An event handoff remains pending at the F9 command until consumed below.
    std::optional<std::uint32_t> event_entry;
};
// One aligned object-action visit, not one display frame. The outer scheduler
// owns six-object selection, current-cell release/arrival and non-script modes.
// Waits, command flow and movement reservations are executed here. Positions
// advance separately through step_field_objects. The shared field RNG index is
// advanced only by actual random branches. Errors leave all owned state intact.
FieldActionResult advance_field_object_action(FieldState& state, FieldOccupancy& occupancy,
                                              std::uint8_t identity, const FieldMap& map,
                                              const FieldActionData& data,
                                              std::uint8_t& random_index,
                                              FieldActionInputs inputs = {},
                                              std::size_t command_budget = 1024);
// Completes a pending F9 handoff and advances its action cursor together. The
// scene calls this only after checking that collision/event ownership is free.
void consume_field_object_event(FieldState& state, std::uint8_t identity,
                                const FieldActionData& data, FieldEventRunner& events);
} // namespace ff6
