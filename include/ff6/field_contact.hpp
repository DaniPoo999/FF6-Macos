#pragma once
#include "ff6/field_action_schedule.hpp"

namespace ff6 {
// Runs after movement and drawing, not during the six-slot contact detector.
// A detected contact waits for the selected actor's joint tile alignment. True
// means the caller must also close the map-title presentation. State, camera and
// event handoff commit together; the current event body is not executed here.
bool activate_field_contact(FieldState& state, FieldActionContext& context,
                            FieldEventRunner& events, const FieldPoseTables& poses,
                            std::uint32_t idle_entry);
bool field_contact_scroll_arrived(const FieldActionContext& context);
// Supplies camera readiness from native scroll state and consumes the runner's
// stop-scroll operation before executing its event body. Other requests remain
// pending for their usual owners. Event-visit/sort cadence is still caller-owned.
EventYield advance_field_contact_event(FieldState& state, FieldActionContext& context,
                                       FieldEventRunner& events, EventInputs input = {},
                                       std::size_t command_budget = 1024);
} // namespace ff6
