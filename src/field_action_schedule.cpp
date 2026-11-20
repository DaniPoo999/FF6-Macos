#include "ff6/field_action_schedule.hpp"

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
bool aligned(const FieldObject& actor) {
    require(actor.x <= 0xffffff && actor.y <= 0xffffff, "Invalid scheduled actor position");
    return actor.x % 4096 == 0 && actor.y % 4096 == 0;
}
FieldTilePosition adjacent(const FieldOccupancy& grid, FieldTilePosition current,
                           unsigned direction) {
    constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
    require(direction < 4, "Invalid action adjacency direction");
    return {static_cast<std::uint8_t>((current.x + dx[direction]) & (grid.width - 1U)),
            static_cast<std::uint8_t>((current.y + dy[direction]) & (grid.height - 1U))};
}
void detect_contact(const FieldObject& actor, std::uint8_t identity, FieldActionContext& context,
                    FieldActionGates gates) {
    if (gates.menu_opening || gates.event_running || gates.map_loading ||
        context.collision.stage != FieldCollision::Stage::none || !(actor.movement.options & 4U) ||
        identity < 16)
        return;
    const auto& grid = context.occupancy;
    require(actor.occupancy_tile.x < grid.width && actor.occupancy_tile.y < grid.height,
            "Contact requires represented cached occupancy");
    // First character wins: up, right, down, left. NPC contact does not test
    // party membership, visibility or terrain height here.
    for (unsigned direction = 0; direction < 4; ++direction) {
        const auto target = adjacent(grid, actor.occupancy_tile, direction);
        const auto character = grid.at(target.x, target.y);
        if (character && *character < 16) {
            context.collision = {FieldCollision::Stage::detected, *character, identity,
                                 static_cast<std::uint8_t>(direction)};
            return;
        }
    }
}
void arrive(FieldState& state, FieldActionContext& context, const FieldMap& map, std::uint8_t slot,
            std::uint8_t identity, FieldActionGates gates) {
    auto& actor = state.field.objects[identity];
    auto& grid = context.occupancy;
    const bool ordinary = !(actor.movement.options & 8U) && !actor.special_graphics &&
                          actor.draw_group == FieldObject::DrawGroup::normal;
    if (ordinary) {
        if (identity == FieldObjects::camera)
            return;
        const auto old = actor.occupancy_tile;
        if (old.x < grid.width && old.y < grid.height) {
            auto& occupant = grid.cells[std::size_t{old.y} * grid.width + old.x];
            if (occupant == identity)
                occupant.reset();
        }
        if (!actor.membership.visible)
            return;
        actor.occupancy_tile = field_object_cell(map, actor);
        actor.terrain_level = field_terrain_at(map, actor.occupancy_tile).flags & 3U;
        const bool player = slot == 0 && actor.movement.mode == 2;
        if (!player || !(context.player_tile.flags & 4U) || context.player_terrain.level != 2)
            grid.cells[std::size_t{actor.occupancy_tile.y} * grid.width + actor.occupancy_tile.x] =
                identity;
        if (player)
            return;
    }
    // Excluded sprite groups deliberately retain their cached cell and level.
    // Their priorities and contact flags still run, even when hidden.
    // Fixed priority returns before the original terrain lookup; its cached
    // cell therefore need not belong to the currently represented map.
    const auto flags = actor.layer_priority_mode == 0
                           ? field_terrain_at(map, actor.occupancy_tile).flags
                           : std::uint8_t{0};
    update_field_object_priority_after(actor, flags);
    detect_contact(actor, identity, context, gates);
}
void random_action(FieldObject& actor, std::uint8_t identity, FieldActionContext& context,
                   const FieldMap& map, const FieldActionData& data) {
    context.random_index = wrap8(unsigned{context.random_index} + 1U);
    const auto direction = data.random[context.random_index] & 3U;
    actor.occupancy_tile = field_object_cell(map, actor);
    const auto destination = adjacent(context.occupancy, actor.occupancy_tile, direction);
    bool allowed = !context.occupancy.at(destination.x, destination.y);
    FieldTerrainCell tile;
    if (allowed) {
        tile = field_terrain_at(map, destination);
        const auto level = tile.flags & 7U;
        allowed = (tile.directions & 128U) &&
                  (actor.terrain_level == 1 ? (level == 1 || level == 4) : level == 2);
    }
    if (!allowed) {
        actor.velocity_x = actor.velocity_y = 0;
        return;
    }
    update_field_object_priority_before(actor, tile.flags);
    context.occupancy.cells[std::size_t{destination.y} * context.occupancy.width + destination.x] =
        identity;
    actor.move_direction = static_cast<std::uint8_t>(direction + 1U);
    actor.facing = static_cast<std::uint8_t>(direction);
    const auto speed = field_move_velocity(
        data.movement, static_cast<FieldMove>(actor.move_direction), actor.speed);
    actor.velocity_x = speed[0];
    actor.velocity_y = speed[1];
}
void active_action(FieldState& state, std::uint8_t identity, const FieldPoseTables& poses,
                   bool event_running) {
    auto& actor = state.field.objects[identity];
    actor.velocity_x = actor.velocity_y = 0;
    if (event_running)
        return;
    if (state.field.update_order[0] == identity &&
        actor.membership.party == state.roster.active_party) {
        actor.movement.mode = 2;
    } else if (actor.saved_movement.mode == 2) {
        actor.movement.mode = 0;
    } else {
        actor.movement.mode = actor.saved_movement.mode;
        if (!(actor.movement.options & 2U)) {
            require(actor.saved_facing < 4, "Invalid saved object facing");
            actor.facing = actor.saved_facing;
            actor.pending_pose = poses.standing[actor.facing];
        }
    }
}
void refresh_scroll(const FieldState& state, FieldActionContext& context, const FieldMap& map,
                    std::uint8_t identity) {
    require(state.field.update_order[0] && *state.field.update_order[0] <= FieldObjects::camera,
            "Scroll refresh requires selected object");
    const auto& party = state.field.objects[*state.field.update_order[0]];
    const auto& camera = state.field.objects[FieldObjects::camera];
    update_field_scroll_rate(context.scroll, context.scroll_limits, map.parallax.multiplier,
                             {context.camera_locked,
                              identity == FieldObjects::camera,
                              context.player_tile_position,
                              {party.velocity_x, party.velocity_y},
                              {camera.velocity_x, camera.velocity_y}});
}
} // namespace
FieldActionPass::FieldActionPass(std::uint8_t event_visits)
    : next_slot_(static_cast<std::uint8_t>((event_visits & 3U) * 6U)),
      end_slot_(static_cast<std::uint8_t>(next_slot_ + 6U)) {}
FieldActionDispatch FieldActionPass::run(FieldState& state, FieldActionContext& context,
                                         const FieldMap& map, const FieldActionData& data,
                                         FieldActionGates gates) {
    gates.event_running = gates.event_running || event_started_;
    for (; next_slot_ < end_slot_; ++next_slot_) {
        if (next_slot_ >= state.field.update_count)
            continue;
        const auto selected = state.field.update_order[next_slot_];
        require(selected && *selected <= FieldObjects::camera, "Invalid scheduled object identity");
        const auto identity = *selected;
        auto& actor = state.field.objects[identity];
        require(actor.movement.mode < 16 && actor.movement.options < 16 &&
                    actor.saved_movement.mode < 16,
                "Invalid scheduled movement mode");
        if (!aligned(actor))
            continue;
        arrive(state, context, map, next_slot_, identity, gates);
        switch (actor.movement.mode) {
        case 1: {
            const auto result = advance_field_object_action(
                state, context.occupancy, identity, map, data, context.random_index,
                {gates.event_running, context.collision.stage != FieldCollision::Stage::none,
                 context.camera_locked});
            if (result.refresh_scroll)
                refresh_scroll(state, context, map, identity);
            if (result.event_entry) {
                pending_ = {FieldActionBoundary::event, identity};
                return *pending_;
            }
            break;
        }
        case 2:
            // Suppressed user control skips the entire player routine, including
            // its local-tile refresh, velocity clear and interaction checks.
            if (!FieldFlagAccess(state).test(0x1cf)) {
                pending_ = {FieldActionBoundary::player, identity};
                return *pending_;
            }
            break;
        case 3:
            random_action(actor, identity, context, map, data);
            break;
        case 4:
            active_action(state, identity, data.poses, gates.event_running);
            break;
        default:
            if (state.field.update_order[0] == identity)
                active_action(state, identity, data.poses, gates.event_running);
            break;
        }
    }
    return {};
}
FieldActionDispatch FieldActionPass::advance(FieldState& state, FieldActionContext& context,
                                             const FieldMap& map, const FieldActionData& data,
                                             FieldActionGates gates) {
    if (pending_)
        return *pending_;
    require(state.field.update_count <= state.field.update_order.size(),
            "Invalid scheduled object count");
    (void)context.occupancy.at(0, 0);
    require(context.occupancy.width == map.definition.layouts[0].width &&
                context.occupancy.height == map.definition.layouts[0].height,
            "Scheduled map and occupancy dimensions disagree");
    auto next = state;
    auto world = context;
    auto pass = *this;
    const auto result = pass.run(next, world, map, data, gates);
    state = std::move(next);
    context = std::move(world);
    *this = pass;
    return result;
}
void FieldActionPass::consume_event(FieldState& state, const FieldActionData& data,
                                    FieldEventRunner& events) {
    require(pending_ && pending_->boundary == FieldActionBoundary::event && pending_->object,
            "No scheduled event handoff to consume");
    consume_field_object_event(state, *pending_->object, data, events);
    event_started_ = true;
    pending_.reset();
    ++next_slot_;
}
} // namespace ff6
