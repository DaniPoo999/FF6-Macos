#include "ff6/field_npcs.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
bool valid_extent(unsigned n) {
    return n == 16 || n == 32 || n == 64 || n == 128;
}
std::array<unsigned, 2> cell(const FieldMap& map, const FieldObject& object) {
    const auto layout = map.definition.layouts[0];
    require(valid_extent(layout.width) && valid_extent(layout.height),
            "Invalid NPC map dimensions");
    require(object.x <= 0xffffff && object.y <= 0xffffff, "Invalid NPC position");
    return {(object.x / 4096U) & (layout.width - 1U), (object.y / 4096U) & (layout.height - 1U)};
}
std::uint8_t ground(const FieldMap& map, const std::array<unsigned, 2>& xy) {
    const auto tile = field_map_tile(map, 0, xy[0], xy[1]);
    return map.tile_properties[tile][0];
}
} // namespace
FieldNpcDefinition decode_field_npc(ByteView record) {
    require(record.size() == 9, "Invalid NPC record extent");
    FieldNpcDefinition npc;
    npc.interaction_data = read24(record, 0) & 0x3ffffU;
    npc.enable_flag = read16(record, 2) >> 6U;
    npc.palette = (record[2] >> 2U) & 7U;
    npc.background2 = (record[2] & 32U) != 0;
    npc.x = record[4] & 127U;
    npc.special_or_rider = (record[4] & 128U) != 0;
    npc.y = record[5] & 63U;
    npc.speed = record[5] >> 6U;
    npc.graphics = record[6];
    npc.movement = record[7] & 15U;
    // Retain both low-priority encodings; drawing maps groups 2 and 3 alike.
    npc.draw_group = static_cast<FieldObject::DrawGroup>((record[7] >> 4U) & 3U);
    npc.ride = static_cast<FieldRide>(record[7] >> 6U);
    npc.facing = record[8] & 3U;
    npc.no_reaction = (record[8] & 4U) != 0;
    npc.layer_priority = (record[8] >> 3U) & 3U;
    const auto frame = static_cast<std::uint8_t>(record[8] >> 5U);
    if (frame != 0)
        npc.animation = FieldSpecialPose{static_cast<FieldSpecialCycle>(npc.facing), frame,
                                         static_cast<std::uint8_t>(npc.ride)};
    return npc;
}
std::vector<FieldNpcDefinition> decode_field_npcs(ByteView resource, std::uint16_t map) {
    constexpr unsigned maps = 416, header = (maps + 1) * 2;
    require(map < maps && resource.size() >= header, "Invalid NPC map or pointer table");
    require(read16(resource, 0) == header, "Invalid NPC data start");
    for (unsigned i = 0; i < maps; ++i) {
        const auto begin = read16(resource, i * 2), end = read16(resource, i * 2 + 2);
        require(begin <= end && end <= resource.size() && (end - begin) % 9 == 0 &&
                    (end - begin) / 9 <= 32,
                "Invalid NPC pointer boundary");
    }
    require(read16(resource, maps * 2) == resource.size(), "Unexpected NPC resource tail");
    std::vector<FieldNpcDefinition> result;
    const auto end = read16(resource, std::size_t{map} * 2 + 2);
    for (std::size_t at = read16(resource, std::size_t{map} * 2); at < end; at += 9)
        result.push_back(decode_field_npc(slice(resource, at, 9)));
    return result;
}
std::vector<FieldNpcDefinition> load_field_npcs(const ContentPack& pack, std::uint16_t map) {
    return decode_field_npcs(pack.get("data_c41a10_npc_properties"), map);
}
void initialize_field_npcs(FieldState& state, const FieldMap& map,
                           std::span<const FieldNpcDefinition> definitions,
                           const FieldPoseTables& poses) {
    require(definitions.size() <= 32, "Too many map NPCs");
    auto next = state;
    // Source clears exactly 24 slots. The last eight retain unrelated fields.
    for (unsigned id = 16; id < 40; ++id) {
        next.field.objects[id] = {};
        next.field.objects[id].animation_slot = 0;
    }
    for (std::size_t i = 0; i < definitions.size(); ++i) {
        const auto& npc = definitions[i];
        require(npc.enable_flag < 1024 && npc.palette < 8 && npc.x < 128 && npc.y < 64 &&
                    npc.speed < 4 && npc.movement < 16 && npc.facing < 4 &&
                    npc.layer_priority < 4 && static_cast<unsigned>(npc.ride) < 4 &&
                    static_cast<unsigned>(npc.draw_group) < 4 && npc.interaction_data <= 0x3ffff,
                "Invalid native NPC definition");
        require(!npc.animation ||
                    (static_cast<unsigned>(npc.animation->cycle) < 4 && npc.animation->offset > 0 &&
                     npc.animation->offset < 8 && npc.animation->speed < 4),
                "Invalid NPC animation definition");
        auto& object = next.field.objects[16 + i];
        object.membership = {};
        object.membership.visible = object.membership.enabled = next.flags.npc[npc.enable_flag];
        object.interaction_data = npc.interaction_data;
        object.upper = object.lower = FieldAppearance{npc.palette, 2};
        object.x = unsigned{npc.x} * 4096U;
        object.y = unsigned{npc.y} * 4096U;
        object.jump_y = 0;
        object.jump_counter = 0;
        object.speed = npc.speed;
        object.graphics = npc.graphics;
        object.scroll_with_background2 = npc.background2;
        object.movement = {npc.movement, static_cast<std::uint8_t>((npc.background2 ? 8U : 0U) |
                                                                   (npc.no_reaction ? 2U : 0U))};
        object.draw_group = npc.draw_group;
        object.ride = npc.ride;
        object.special_graphics = npc.special_or_rider && npc.ride == FieldRide::none;
        object.rider_visible = npc.special_or_rider;
        object.facing = npc.facing;
        object.pending_pose = object.pose = poses.standing[npc.facing];
        object.layer_priority_mode = npc.layer_priority;
        object.special_pose = npc.animation;
        object.map = next.field.current_map;
        object.move_direction = object.action_wait = object.action_steps = 0;
        object.walking_animation = true;
        object.saved_facing = 0;
        const auto xy = cell(map, object);
        object.occupancy_tile = {static_cast<std::uint8_t>(xy[0]),
                                 static_cast<std::uint8_t>(xy[1])};
        object.terrain_level = ground(map, xy) & 7U;
        // Initialization uses terrain even for a fixed layer-priority NPC.
        object.upper.priority = object.terrain_level >= 1 && object.terrain_level <= 3 ? 2 : 3;
    }
    for (std::size_t id = 16 + definitions.size(); id < FieldObjects::camera; ++id)
        next.field.objects[id].membership.enabled = next.field.objects[id].membership.visible =
            false;
    sort_field_objects(next);
    state = std::move(next);
}
void initialize_field_object_animation(FieldObjects& field, bool reset_queue) {
    if (reset_queue)
        field.animation_queue.fill(std::nullopt);
    std::uint8_t slot = 0;
    for (std::uint8_t id = 0; id < FieldObjects::camera && slot < 24; ++id) {
        auto& object = field.objects[id];
        if (!object.membership.enabled)
            continue;
        object.animation_slot = slot;
        field.animation_queue[slot++] = id;
    }
}
std::optional<std::uint8_t> FieldOccupancy::at(unsigned x, unsigned y) const {
    require(valid_extent(width) && valid_extent(height) &&
                cells.size() == std::size_t{width} * height,
            "Invalid occupancy dimensions");
    return cells[(y & (height - 1U)) * width + (x & (width - 1U))];
}
void release_field_occupancy(FieldOccupancy& occupancy, std::span<const FieldTilePosition> cells) {
    (void)occupancy.at(0, 0);
    for (const auto tile : cells)
        if (tile.x < occupancy.width && tile.y < occupancy.height)
            occupancy.cells[std::size_t{tile.y} * occupancy.width + tile.x].reset();
}
void hide_field_object(FieldState& state, FieldOccupancy& occupancy, std::uint8_t identity) {
    require(identity < state.field.objects.size(), "Invalid hidden object identity");
    auto& actor = state.field.objects[identity];
    // Validate/release before changing actor state. The original clears this
    // cached cell even if another identity has since occupied it.
    release_field_occupancy(occupancy, std::span(&actor.occupancy_tile, 1));
    actor.membership.visible = false;
    actor.saved_movement = {};
    actor.movement.mode = 0;
    if (identity < 16)
        state.roster.membership[identity].visible = false;
}
void delete_field_object(FieldState& state, FieldOccupancy& occupancy, std::uint8_t identity) {
    require(identity < state.field.objects.size(), "Invalid deleted object identity");
    auto& actor = state.field.objects[identity];
    if (!actor.membership.enabled)
        return;
    require(actor.animation_slot && *actor.animation_slot < state.field.animation_queue.size(),
            "Enabled deleted object has no valid animation slot");
    // Validate/release before mutation. StopObjAnim uses the retained slot even
    // if another object has overwritten it; the actor retains its slot pointer.
    release_field_occupancy(occupancy, std::span(&actor.occupancy_tile, 1));
    state.field.animation_queue[*actor.animation_slot].reset();
    actor.membership.enabled = actor.membership.visible = false;
    actor.saved_movement = {};
    const auto selector = std::ranges::find(state.field.party_selectors, identity);
    std::fill(selector, state.field.party_selectors.end(), std::nullopt);
    if (identity < 16) {
        auto& persistent = state.roster.membership[identity];
        persistent.enabled = persistent.visible = false;
    }
}
FieldOccupancy initialize_field_occupancy(const FieldMap& map, FieldObjects& field,
                                          std::uint8_t player_identity, FieldTerrainCell party_tile,
                                          FieldPlayerTerrain terrain) {
    require(player_identity <= FieldObjects::camera && terrain.level < 8,
            "Invalid occupancy player");
    const auto layout = map.definition.layouts[0];
    require(valid_extent(layout.width) && valid_extent(layout.height),
            "Invalid occupancy map size");
    auto next = field;
    FieldOccupancy result{layout.width, layout.height, {}};
    result.cells.resize(std::size_t{layout.width} * layout.height);
    const auto place = [&](std::uint8_t id) {
        const auto xy = cell(map, field.objects[id]);
        require(xy[1] < 64, "Occupancy needs unresolved cross-layer state");
        result.cells[xy[1] * layout.width + xy[0]] = id;
        next.objects[id].occupancy_tile = {static_cast<std::uint8_t>(xy[0]),
                                           static_cast<std::uint8_t>(xy[1])};
    };
    for (std::uint8_t id = 0; id < FieldObjects::camera; ++id) {
        const auto& object = field.objects[id];
        if (id != player_identity && object.map == field.current_map && object.membership.visible &&
            !object.scroll_with_background2 && !object.special_graphics &&
            object.draw_group == FieldObject::DrawGroup::normal)
            place(id);
    }
    // InitNPCMap uses the caller's cached tile flags. A fresh lookup here
    // changes map-load ordering, including cutscene maps with retained state.
    if (!(party_tile.flags & 4U) || terrain.level != 2)
        place(player_identity);
    field = std::move(next);
    return result;
}
std::array<bool, 4> field_adjacent_occupancy(const FieldOccupancy& occupancy,
                                             const FieldObject& actor) {
    require(actor.x <= 0xffffff && actor.y <= 0xffffff, "Invalid occupancy actor position");
    const auto x = actor.x / 4096U, y = actor.y / 4096U;
    return {occupancy.at(x, y + 255U).has_value(), occupancy.at(x + 1U, y).has_value(),
            occupancy.at(x, y + 1U).has_value(), occupancy.at(x + 255U, y).has_value()};
}
} // namespace ff6
