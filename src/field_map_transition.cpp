#include "ff6/field_map_transition.hpp"
#include "rom-materials/field_transition_fade.macros.inc"
#include "rom-materials/field_transition_brightness.macros.inc"
#include "rom-materials/field_transition_step.macros.inc"

namespace ff6 {
// FadeOut's selected fields have separate scalar and packed native consumers.
static_assert(FF6_ROM_VALUE_FIELD_TRANSITION_FADE_0 ==
              (FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0 |
               (FF6_ROM_VALUE_FIELD_TRANSITION_STEP_0 << 8U)));
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
} // namespace
std::uint8_t advance_field_fade(FieldFade& fade) {
    require(fade.step < 32 && static_cast<unsigned>(fade.direction) <= 2, "Invalid field fade");
    if (fade.direction == FieldFadeDirection::darker) {
        if (fade.brightness == 0) {
            fade.direction = FieldFadeDirection::steady;
            fade.step = 0;
        } else
            fade.brightness = wrap8(unsigned{fade.brightness} + 256U - fade.step);
    } else if (fade.direction == FieldFadeDirection::brighter) {
        if ((fade.brightness & 240U) == 240) {
            fade.direction = FieldFadeDirection::steady;
            fade.step = 0;
        } else
            fade.brightness = wrap8(unsigned{fade.brightness} + fade.step);
    }
    return fade.brightness >> 4U;
}
FieldMapLoadPlan resolve_field_map_load(const FieldObjects& field, const EventMapLoad& command,
                                        std::uint32_t event_resume) {
    require(command.destination < 512 && command.facing < 4 && command.world_vehicle < 4 &&
                event_resume <= 0xffffff,
            "Invalid native map-load command");
    FieldMapLoadPlan result;
    result.characters = save_field_character_locations(field);
    result.parent = field.parent_map;
    result.run_startup = command.run_startup;
    result.fade_in = command.fade_in;
    result.keep_dimensions = command.keep_dimensions;
    if (command.set_parent) {
        result.parent.map = command.destination;
        result.parent.x = command.x;
        result.parent.y = command.y;
    }
    unsigned header = command.destination | (unsigned{command.set_parent} << 9U) |
                      (unsigned{command.lower_level} << 10U) |
                      (unsigned{command.show_name} << 11U) | (unsigned{command.facing} << 12U);
    FieldTilePosition position{command.x, command.y};
    if (command.destination >= 510) {
        if (command.destination == 511) {
            const auto direction = (result.parent.facing & 3U) ^ 2U;
            constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
            result.parent.x = wrap8(unsigned{result.parent.x} + dx[direction]);
            result.parent.y = wrap8(unsigned{result.parent.y} + dy[direction]);
            result.return_facing = static_cast<std::uint8_t>(direction | 128U);
        } else
            result.return_facing = result.parent.facing;
        header = (header & 0xfe00U) | result.parent.map;
        position = {result.parent.x, result.parent.y};
    }
    result.map = static_cast<std::uint16_t>(header & 0x1ffU);
    // FieldLoop includes set-parent in its dispatch mask. Parent-map high bits
    // can survive the OR above; the world owner reads facing from that result.
    result.dispatch_world = (header & 0x3ffU) < 3;
    if (result.map < 3)
        result.arrival = WorldMapArrival{position, static_cast<FieldFacing>((header >> 12U) & 3U),
                                         command.world_vehicle, event_resume};
    else
        result.arrival =
            FieldMapArrival{position, static_cast<FieldFacing>(command.facing), command.show_name};
    // The command's lower-level hint is not written to saved field terrain.
    // The later native scene loader must retain that separate state.
    return result;
}
bool queue_field_map_request(FieldState& state, FieldMapTransition& transition,
                             FieldEventRunner& runner) {
    require(runner.pending_request().has_value(), "No pending map-load request");
    const auto* command = std::get_if<EventMapLoad>(&runner.pending_request()->operation);
    if (!command)
        return false;
    require(!transition.pending, "A map load is already pending");
    require(runner.pending_request()->script_offset.has_value(),
            "Map command has no script offset");
    auto plan = resolve_field_map_load(state.field, *command,
                                       *runner.pending_request()->script_offset + 6U);
    auto next = transition;
    next.pending = plan;
    if (command->fade_out)
        next.fade = {FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0,
                     FF6_ROM_VALUE_FIELD_TRANSITION_STEP_0, FieldFadeDirection::darker};
    else {
        next.fade.direction = FieldFadeDirection::steady;
        next.fade.step = 0;
    }
    state.field.parent_map = plan.parent;
    transition = std::move(next);
    FieldFlagAccess flags(state);
    runner.complete_request(runner.pending_request()->identity, flags);
    return true;
}
EventYield advance_field_map_event(FieldState& state, const FieldMapTransition& transition,
                                   FieldEventRunner& runner, EventInputs inputs,
                                   std::size_t command_budget) {
    // FieldLoop continues visiting the event fade wait while fading out. Once
    // loading is eligible it must give control to the scene owner first.
    inputs.map_load_pending = transition.ready_to_load();
    inputs.fade_active = transition.fade.active();
    return advance_field_event(state, runner, inputs, command_budget);
}
} // namespace ff6
