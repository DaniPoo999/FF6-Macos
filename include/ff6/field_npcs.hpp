#pragma once
#include "ff6/field_movement.hpp"

namespace ff6 {
struct FieldNpcDefinition {
    std::uint32_t interaction_data = 0;
    std::uint16_t enable_flag = 0; // Index within the 1,024 NPC flags.
    std::uint8_t x = 0, y = 0, graphics = 0, palette = 0, speed = 0, movement = 0;
    std::uint8_t facing = 0, layer_priority = 0;
    FieldObject::DrawGroup draw_group = FieldObject::DrawGroup::normal;
    FieldRide ride = FieldRide::none;
    std::optional<FieldSpecialPose> animation;
    bool background2 = false, no_reaction = false, special_or_rider = false;
    bool operator==(const FieldNpcDefinition&) const = default;
};
FieldNpcDefinition decode_field_npc(ByteView record);
// Pointer aliases represent empty maps. Every boundary and record is validated.
std::vector<FieldNpcDefinition> decode_field_npcs(ByteView resource, std::uint16_t map);
std::vector<FieldNpcDefinition> load_field_npcs(const ContentPack& pack, std::uint16_t map);
// New-map NPC setup, followed by the existing semantic object sort. Party/player
// placement and same-map reload dispatch belong to the map transition owner.
void initialize_field_npcs(FieldState& state, const FieldMap& map,
                           std::span<const FieldNpcDefinition> definitions,
                           const FieldPoseTables& poses);
// Initialization scans enabled objects by identity, not current draw/update order.
// Reset is separate: without it the unused queue tail and object slots are retained.
void initialize_field_object_animation(FieldObjects& field, bool reset_queue);
struct FieldOccupancy {
    std::uint16_t width = 0, height = 0;
    std::vector<std::optional<std::uint8_t>> cells;
    std::optional<std::uint8_t> at(unsigned x, unsigned y) const;
    bool operator==(const FieldOccupancy&) const = default;
};
// Cached releases are unwrapped. Out-of-grid cells have no represented occupant.
void release_field_occupancy(FieldOccupancy& occupancy, std::span<const FieldTilePosition> cells);
// Shared hide behavior for event 42 and object action D1. Releases the cached
// cell, clears saved movement and mode, retains options/animation/velocity.
void hide_field_object(FieldState& state, FieldOccupancy& occupancy, std::uint8_t identity);
// Event 3E: disable/hide, release cached occupancy and the recorded animation
// slot, truncate explicit party selectors. Current movement and slot survive.
void delete_field_object(FieldState& state, FieldOccupancy& occupancy, std::uint8_t identity);
// Initial occupancy follows visible, ordinary, BG1 objects on this map, then
// the party (except beneath a bridge). Later movement reservations are separate.
// party_tile is the caller's cached tile, not an implicit fresh map lookup.
FieldOccupancy initialize_field_occupancy(const FieldMap& map, FieldObjects& field,
                                          std::uint8_t player_identity, FieldTerrainCell party_tile,
                                          FieldPlayerTerrain terrain);
std::array<bool, 4> field_adjacent_occupancy(const FieldOccupancy& occupancy,
                                             const FieldObject& actor);
} // namespace ff6
