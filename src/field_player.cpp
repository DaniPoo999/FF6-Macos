#include "ff6/field_player.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
bool event_running(const FieldEventRunner& event, std::uint32_t idle) {
    return event.continuation() != EventContinuation::script || event.position() != idle;
}
std::optional<FieldMove> cardinal_direction(Buttons buttons) {
    if (held(buttons, Button::right))
        return FieldMove::right;
    if (held(buttons, Button::left))
        return FieldMove::left;
    if (held(buttons, Button::up))
        return FieldMove::up;
    if (held(buttons, Button::down))
        return FieldMove::down;
    return {};
}
FieldTilePosition adjacent(const FieldOccupancy& grid, FieldTilePosition at, FieldMove move) {
    const auto direction = static_cast<unsigned>(move) - 1U;
    require(direction < 4, "Invalid player cardinal direction");
    constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
    return {static_cast<std::uint8_t>((at.x + dx[direction]) & (grid.width - 1U)),
            static_cast<std::uint8_t>((at.y + dy[direction]) & (grid.height - 1U))};
}
FieldStep cardinal(FieldState& state, FieldActionContext& world, const FieldMap& map,
                   const FieldLocalTiles& tiles, std::uint8_t caller, FieldMove direction) {
    auto& actor = state.field.objects[caller];
    const auto selected_id = *state.field.update_order[0];
    auto& selected = state.field.objects[selected_id];
    const bool disabled = FieldFlagAccess(state).test(0x1c1);
    const bool sprint = (state.field.equipment_effects & 32U) != 0;
    auto candidate = selected;
    auto terrain = world.player_terrain;
    auto step =
        check_field_cardinal_step(candidate, terrain, tiles, direction, false, disabled, sprint);
    // Terrain rejection precedes GetObjMapAdjacent. Evaluate on value copies
    // first so a rejected terrain direction cannot refresh the caller's cache.
    actor.speed = candidate.speed;
    if (step.kind == FieldStep::Kind::blocked)
        return step;
    actor.occupancy_tile = field_object_cell(map, actor);
    const auto destination = adjacent(world.occupancy, actor.occupancy_tile, direction);
    if (world.occupancy.at(destination.x, destination.y)) {
        candidate = selected;
        terrain = world.player_terrain;
        step =
            check_field_cardinal_step(candidate, terrain, tiles, direction, true, disabled, sprint);
        if (step.kind == FieldStep::Kind::blocked)
            return step;
    }
    if (step.reserve_destination)
        world.occupancy.cells[std::size_t{destination.y} * world.occupancy.width + destination.x] =
            selected_id;
    world.player_terrain = terrain;
    selected.upper.priority = candidate.upper.priority;
    selected.lower.priority = candidate.lower.priority;
    actor.move_direction = static_cast<std::uint8_t>(direction);
    return step;
}
FieldPlayerResult run(FieldState& state, FieldActionContext& world, FieldPlayerContext& player,
                      FieldMap& map, const FieldActionData& actions, const FieldPlayerData& data,
                      FieldEventRunner& events, std::uint8_t caller,
                      const FieldPlayerInputs& input) {
    const auto selected_id = state.field.update_order[0];
    require(caller <= FieldObjects::camera && selected_id && *selected_id <= FieldObjects::camera,
            "Player visit requires valid caller and selected actor");
    auto& actor = state.field.objects[caller];
    auto& selected = state.field.objects[*selected_id];
    require(actor.x <= 0xffffff && actor.y <= 0xffffff && actor.x % 4096 == 0 &&
                actor.y % 4096 == 0 && actor.movement.mode == 2,
            "Player visit requires an aligned user-controlled caller");
    require(world.occupancy.width == map.definition.layouts[0].width &&
                world.occupancy.height == map.definition.layouts[0].height,
            "Player occupancy and map dimensions disagree");
    (void)world.occupancy.at(0, 0);
    actor.velocity_x = actor.velocity_y = 0;
    actor.walking_animation = true;
    for (auto& layer : world.scroll.layers)
        for (auto& axis : layer)
            axis.follow_rate = 0;
    FieldPlayerResult result;
    if (input.map_loading) {
        // A departing scene owns no further navigation (UpdatePlayerMovement,
        // CheckNPCs and CheckTreasure all gate on the pending map). Scripted
        // exits may finish outside the terrain layout. Retain the last terrain
        // presentation during the fade instead of reading unrelated original
        // storage there; the destination initializes its own terrain/overlays.
        selected.move_direction = 0;
        state.field.objects[0].action_steps = 0;
        return result;
    }
    player.local = field_local_tiles(map, selected.x, selected.y);
    world.player_tile = player.local.cells[7];
    update_field_player_priority_after(selected, world.player_terrain, player.local);
    const bool ready =
        !FieldFlagAccess(state).test(0x1cf) && !input.map_loading && !input.menu_opening &&
        world.collision.stage == FieldCollision::Stage::none &&
        std::ranges::all_of(input.background_updates, [](auto v) { return v == 0 || v == 5; });
    if (ready) {
        if (const auto diagonal =
                field_player_stair_direction(player.local, world.player_terrain, input.buttons)) {
            actor.facing = held(input.buttons, Button::right) ? 1 : 3;
            actor.move_direction = static_cast<std::uint8_t>(*diagonal);
            const auto flags = world.player_tile.flags;
            if (!(flags & 4U) && (flags & 3U) != 3)
                world.player_terrain.level = flags & 3U;
            update_field_player_priority_before(selected, world.player_terrain, player.local,
                                                *diagonal);
            result.step = {FieldStep::Kind::diagonal, *diagonal};
        } else if (const auto direction = cardinal_direction(input.buttons)) {
            actor.facing = static_cast<std::uint8_t>(static_cast<unsigned>(*direction) - 1U);
            actor.pending_pose = actions.poses.standing[actor.facing];
            result.door =
                try_open_field_door(map, player.doors, data.doors, world.occupancy,
                                    {world.player_tile_position[0], world.player_tile_position[1]},
                                    player.local, world.player_terrain, actor.facing);
            if (result.door)
                result.step = {FieldStep::Kind::door};
            else
                result.step = cardinal(state, world, map, player.local, caller, *direction);
        }
    }
    const auto overlays = [&] {
        FieldOverlayView view;
        view.origin = input.overlay_origin;
        view.follow_rate = {world.scroll.layers[0][0].follow_rate,
                            world.scroll.layers[0][1].follow_rate};
        update_field_overlay(player.overlay, data.overlay, data.layouts, map, selected,
                             world.player_terrain, player.local, view);
    };
    const auto talk = [&] {
        result.talk = try_field_talk(state, world, map, actions.poses, events, caller,
                                     {input.fully_bright, input.menu_opening, input.map_loading,
                                      event_running(events, data.idle_event), input.dialogue_open,
                                      held(input.buttons, Button::a)});
    };
    if (result.step.kind == FieldStep::Kind::cardinal ||
        result.step.kind == FieldStep::Kind::diagonal) {
        if (result.step.kind == FieldStep::Kind::cardinal)
            apply_field_step_effects(state, player.effects, result.step.kind);
        // Both priority-before and the cardinal caller reload the selected
        // actor before calculating velocity. This need not be the action caller.
        const auto speed = field_move_velocity(
            actions.movement, static_cast<FieldMove>(selected.move_direction), selected.speed);
        selected.velocity_x = speed[0];
        selected.velocity_y = speed[1];
        const auto& camera = state.field.objects[FieldObjects::camera];
        update_field_scroll_rate(world.scroll, world.scroll_limits, map.parallax.multiplier,
                                 {world.camera_locked,
                                  *selected_id == FieldObjects::camera,
                                  world.player_tile_position,
                                  {selected.velocity_x, selected.velocity_y},
                                  {camera.velocity_x, camera.velocity_y}});
        if (result.step.kind == FieldStep::Kind::diagonal) {
            // Y is selected here: the priority routine reloaded it earlier.
            selected.action_steps = 1;
            overlays();
            apply_field_step_effects(state, player.effects, result.step.kind);
        } else {
            state.field.objects[0].action_steps = 1;
            overlays();
            talk();
        }
    } else {
        selected.move_direction = 0;
        state.field.objects[0].action_steps = 0;
        overlays();
        talk();
        // A conversation may just have replaced the idle cursor. Re-read real
        // event ownership before treasure, rather than reusing an input gate.
        result.treasure = try_collect_field_treasure(
            state, map, events, player.treasure, world.occupancy, world.player_tile,
            world.player_terrain, data.treasures, data.treasure,
            {input.dialogue_open, input.menu_opening, input.map_loading,
             event_running(events, data.idle_event), held(input.buttons, Button::a)});
    }
    return result;
}
} // namespace
FieldPlayerData load_field_player_data(const ContentPack& pack, std::uint16_t map) {
    const auto definition = load_field_map(pack, map).definition;
    return {load_field_door_tiles(pack),      load_field_treasure_data(pack),
            load_field_treasures(pack, map),  load_field_overlay(pack, definition.overlay),
            load_field_overlay_layouts(pack), 0};
}
FieldPlayerResult advance_field_player(FieldState& state, FieldActionContext& context,
                                       FieldPlayerContext& player, FieldMap& map,
                                       const FieldActionData& actions, const FieldPlayerData& data,
                                       FieldEventRunner& events, std::uint8_t caller,
                                       const FieldPlayerInputs& input) {
    auto next = state;
    auto world = context;
    auto retained = player;
    auto edited = map;
    auto event = events;
    const auto result = run(next, world, retained, edited, actions, data, event, caller, input);
    state = std::move(next);
    context = std::move(world);
    player = std::move(retained);
    map.tile_edits = std::move(edited.tile_edits);
    events = std::move(event);
    return result;
}
FieldPlayerResult FieldActionPass::consume_player(FieldState& state, FieldActionContext& context,
                                                  FieldPlayerContext& player, FieldMap& map,
                                                  const FieldActionData& actions,
                                                  const FieldPlayerData& data,
                                                  FieldEventRunner& events,
                                                  const FieldPlayerInputs& input) {
    require(pending_ && pending_->boundary == FieldActionBoundary::player && pending_->object,
            "No scheduled player visit to consume");
    const auto result = advance_field_player(state, context, player, map, actions, data, events,
                                             *pending_->object, input);
    event_started_ = event_started_ || event_running(events, data.idle_event);
    pending_.reset();
    ++next_slot_;
    return result;
}
} // namespace ff6
