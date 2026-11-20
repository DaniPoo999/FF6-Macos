#include "ff6/field_map_state.hpp"
#include "rom-materials/field_player_speeds.macros.inc"

namespace ff6 {
namespace {
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
FieldObject& player(FieldObjects& field) {
    require(field.update_order[0] && *field.update_order[0] <= FieldObjects::camera,
            "Map initialization needs a valid party object");
    return field.objects[*field.update_order[0]];
}
} // namespace
FieldCharacterLocations save_field_character_locations(const FieldObjects& field) {
    FieldCharacterLocations locations;
    for (std::size_t i = 0; i < locations.size(); ++i) {
        const auto& object = field.objects[i];
        require(object.x <= 0xffffff && object.y <= 0xffffff, "Invalid saved field position");
        // Original save keeps only eight tile-coordinate bits, even for an
        // object whose full sixteen-bit pixel position lies beyond that range.
        locations[i] = {object.map, wrap8(object.x / 4096U), wrap8(object.y / 4096U)};
    }
    return locations;
}
void restore_field_character_maps(FieldObjects& field, const FieldCharacterLocations& locations) {
    for (std::size_t i = 0; i < locations.size(); ++i)
        field.objects[i].map = locations[i].map;
}
void restore_field_character_positions(FieldObjects& field,
                                       const FieldCharacterLocations& locations) {
    for (std::size_t i = 0; i < locations.size(); ++i)
        require(field.objects[i].upper.priority < 4 && field.objects[i].lower.priority < 4,
                "Invalid saved field priority");
    for (std::size_t i = 0; i < locations.size(); ++i) {
        auto& object = field.objects[i];
        object.x = unsigned{locations[i].x} * 4096U;
        object.y = unsigned{locations[i].y} * 4096U;
        object.upper.priority |= 2U;
        object.lower.priority |= 2U;
    }
}
void place_field_player(FieldObjects& field, std::uint8_t x, std::uint8_t y) {
    auto& object = player(field);
    require(object.x <= 0xffffff && object.y <= 0xffffff, "Invalid party position");
    object.x = unsigned{x} * 4096U + (object.x & 255U);
    object.y = unsigned{y} * 4096U + (object.y & 255U);
}
void initialize_field_player(FieldObjects& field, std::optional<FieldFacing> facing,
                             const FieldPoseTables& poses) {
    require(!facing || static_cast<unsigned>(*facing) < 4, "Invalid destination facing");
    auto& object = player(field);
    if (facing) {
        object.facing = static_cast<std::uint8_t>(*facing);
        object.pending_pose = object.pose = poses.standing[object.facing];
    }
    object.move_direction = object.action_steps = 0;
    object.velocity_x = object.velocity_y = 0;
    object.speed = FF6_ROM_VALUE_FIELD_PLAYER_SPEEDS_0;
}
void initialize_field_map_actors(FieldState& state, const FieldMap& map,
                                 std::span<const FieldNpcDefinition> npcs,
                                 const FieldPoseTables& poses, bool reloading_same_map,
                                 std::optional<FieldFacing> facing) {
    auto next = state;
    // InitObjGfx resets the queue before NPC/player setup. Remembered per-object
    // slot ownership persists until its initialization stage overwrites it.
    next.field.animation_queue.fill(std::nullopt);
    if (!reloading_same_map) {
        initialize_field_npcs(next, map, npcs, poses);
        initialize_field_player(next.field, facing, poses);
    }
    initialize_field_object_animation(next.field, false);
    state = std::move(next);
}
std::vector<FieldTilePosition> select_field_party_leader(FieldState& state) {
    require(state.roster.active_party < 8, "Invalid active party");
    const auto& previous = player(state.field);
    require(previous.x <= 0xffffff && previous.y <= 0xffffff, "Invalid leader position");
    require(previous.facing < 4 && previous.saved_facing < 4 && previous.layer_priority_mode < 4 &&
                static_cast<unsigned>(previous.ride) < 4 && previous.upper.priority < 4 &&
                previous.lower.priority < 4 && previous.movement.mode < 16 &&
                previous.movement.options < 16,
            "Invalid leader control state");
    const auto old_id = *state.field.update_order[0];
    auto leader = state.field.party_selectors[0];
    std::uint8_t first_position = 4;
    auto next = state;
    std::vector<FieldTilePosition> released;
    for (std::uint8_t id = 0; id < 16; ++id) {
        const auto& member = state.roster.membership[id];
        require(member.party < 8 && member.position < 4, "Invalid persistent party membership");
        if (member.party != state.roster.active_party)
            continue;
        auto& object = next.field.objects[id];
        object.x = previous.x & 0xffff00U;
        object.y = previous.y & 0xffff00U;
        // Occupancy may be temporarily unavailable during a menu/map reload.
        // Preserve the releases explicitly; the caller owns the grid's lifetime.
        released.push_back(object.occupancy_tile);
        // The original does not test the enabled bit. Equal positions retain
        // the first character identity encountered in the scan.
        if (member.position < first_position) {
            first_position = member.position;
            leader = id;
        }
    }
    require(leader && *leader <= FieldObjects::camera, "Party has no valid leader selector");
    auto& selected = next.field.objects[*leader];
    selected.membership.visible = true;
    if (*leader != old_id) {
        // Original out-of-roster writes alias unrelated persistent storage.
        // Such unsupported state must fail explicitly, never corrupt a roster.
        require(old_id < 16 && *leader < 16, "Leader transfer outside the character roster");
        selected.occupancy_tile = previous.occupancy_tile;
        selected.upper.priority = previous.upper.priority;
        selected.lower.priority = previous.lower.priority;
        selected.walking_animation = previous.walking_animation;
        selected.layer_priority_mode = previous.layer_priority_mode;
        selected.saved_facing = previous.saved_facing;
        selected.ride = previous.ride;
        selected.rider_visible = previous.rider_visible;
        selected.special_graphics = previous.special_graphics;
        // The same original mode bits encode animation speed for special poses.
        if (selected.special_pose)
            selected.special_pose->speed = static_cast<std::uint8_t>(previous.ride);
        selected.move_direction = previous.move_direction;
        selected.facing = previous.facing;
        selected.pending_pose = previous.pending_pose;
        selected.movement = selected.saved_movement = previous.movement;
        selected.scroll_with_background2 = previous.scroll_with_background2;
        auto& old = next.field.objects[old_id];
        old.movement = old.saved_movement = {};
        old.scroll_with_background2 = false;
        old.membership.visible = false;
        next.roster.membership[old_id] = old.membership;
        next.roster.membership[*leader] = selected.membership;
    }
    next.field.party_selectors = {*leader, std::nullopt, std::nullopt, std::nullopt};
    next.field.sort_pending = true;
    state = std::move(next);
    return released;
}
bool consume_field_party_request(FieldState& state, FieldOccupancy& occupancy,
                                 FieldEventRunner& runner) {
    require(runner.pending_request().has_value(), "No pending party request");
    if (!std::holds_alternative<EventUpdateParty>(runner.pending_request()->operation))
        return false;
    auto next = state;
    auto occupied = occupancy;
    release_field_occupancy(occupied, select_field_party_leader(next));
    // PushCharFlags preserves the stored party order while copying field flags.
    save_field_membership(next);
    state = std::move(next);
    occupancy = std::move(occupied);
    FieldFlagAccess flags(state);
    runner.complete_request(runner.pending_request()->identity, flags);
    return true;
}
} // namespace ff6
