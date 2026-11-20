#pragma once
#include "ff6/field_npcs.hpp"

namespace ff6 {
struct FieldCharacterLocation {
    std::uint16_t map = 0;
    std::uint8_t x = 0, y = 0;
    bool operator==(const FieldCharacterLocation&) const = default;
};
using FieldCharacterLocations = std::array<FieldCharacterLocation, 16>;
FieldCharacterLocations save_field_character_locations(const FieldObjects& field);
void restore_field_character_maps(FieldObjects& field, const FieldCharacterLocations& locations);
void restore_field_character_positions(FieldObjects& field,
                                       const FieldCharacterLocations& locations);
// Destination placement preserves subpixel fractions and existing velocity.
void place_field_player(FieldObjects& field, std::uint8_t x, std::uint8_t y);
// Called for new maps. Absent facing preserves facing and both poses.
void initialize_field_player(FieldObjects& field, std::optional<FieldFacing> facing,
                             const FieldPoseTables& poses);
// Actor-state portion of LoadMap: reset animation queue, initialize NPC/player
// only for a new map, then assign animation slots. An explicit reload decision
// is required; equal map identifiers alone do not establish the reload path.
// Player placement, equipment, special graphics, occupancy and startup scripts
// have their own positions in the caller's load order and are not performed here.
void initialize_field_map_actors(FieldState& state, const FieldMap& map,
                                 std::span<const FieldNpcDefinition> npcs,
                                 const FieldPoseTables& poses, bool reloading_same_map,
                                 std::optional<FieldFacing> facing);
// Regroup active-party characters at the old leader, identify cached cells
// for release, select the lowest persistent party position and transfer
// control when needed. Sorting is deferred to the existing field visit owner.
// Returns ordered cached cells to release. Map loading discards the old grid;
// an active field owner must apply these releases to its live occupancy.
std::vector<FieldTilePosition> select_field_party_leader(FieldState& state);
// Event 47 additionally saves field membership while retaining persistent order.
// Other event requests remain pending for their own native owners.
bool consume_field_party_request(FieldState& state, FieldOccupancy& occupancy,
                                 FieldEventRunner& runner);
} // namespace ff6
