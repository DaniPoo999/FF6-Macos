#pragma once
#include "ff6/field_action_schedule.hpp"

namespace ff6 {
struct FieldStepEffects {
    std::uint32_t steps = 0; // Serialized as an unsigned 24-bit counter.
    std::uint8_t mosaic_speed = 0;
    std::uint16_t mosaic_counter = 0;
    // Arrival sets the entrance inhibit; taking a step clears it.
    bool entrances_disabled = false, encounters_enabled = false, on_trigger = false;
    bool operator==(const FieldStepEffects&) const = default;
};
// A successful player step, not a position-integration tick. Cardinal movement
// applies HP effects and clears tile/save flags; diagonal stairs deliberately do
// neither. Overlay publication and per-object step counters belong to the caller.
void apply_field_step_effects(FieldState& state, FieldStepEffects& effects, FieldStep::Kind kind);
// World travel shares the same walking HP effects and step limit.
bool apply_world_step_effects(Roster& roster, FieldStepEffects& effects);
// One field display update. Event pulses start at 0x1e00; poison steps at
// 0x0f00. Both share the same triangular grow/shrink curve and fractional rate.
std::uint8_t advance_field_mosaic(FieldStepEffects& effects);

struct FieldTalkInputs {
    bool fully_bright = false, menu_opening = false, map_loading = false;
    bool event_running = true, dialogue_open = false, confirm_down = false;
};
// Uses the selected player's position/cache, but retains the original caller
// identity for activation. Returns the activated object, or no interaction.
// A successful caller must also close the map-title presentation before drawing.
// Actor/cache/event changes commit together. Does not execute the dialogue body.
std::optional<std::uint8_t> try_field_talk(FieldState& state, const FieldActionContext& context,
                                           const FieldMap& map, const FieldPoseTables& poses,
                                           FieldEventRunner& events, std::uint8_t caller,
                                           FieldTalkInputs inputs);
} // namespace ff6
