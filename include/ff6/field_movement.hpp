#pragma once
#include "ff6/field_map.hpp"
#include "ff6/field_state.hpp"
#include "ff6/input.hpp"

namespace ff6 {
// Direction identities are the documented object-command data language.
enum class FieldMove : std::uint8_t {
    none,
    up,
    right,
    down,
    left,
    up_right,
    down_right,
    down_left,
    up_left,
    steep_up_right,
    shallow_up_right,
    shallow_down_right,
    steep_down_right,
    steep_down_left,
    shallow_down_left,
    shallow_up_left,
    steep_up_left
};
struct FieldMoveRates {
    std::array<std::array<std::int16_t, 2>, 16> directions;
    std::array<std::uint8_t, 10> multipliers;
    std::array<std::uint8_t, 16> facing{};
};
FieldMoveRates load_field_move_rates(const ContentPack& pack);
std::array<std::int16_t, 2> field_move_velocity(const FieldMoveRates& rates, FieldMove direction,
                                                std::uint8_t speed);
struct FieldTerrainCell {
    std::uint8_t tile = 0, flags = 0, directions = 0;
    bool operator==(const FieldTerrainCell&) const = default;
};
// Object lookup wraps both coordinates at map bounds; player local-tile
// gathering has a distinct retained-row rule and remains separate below.
FieldTilePosition field_object_cell(const FieldMap& map, const FieldObject& actor);
FieldTerrainCell field_terrain_at(const FieldMap& map, FieldTilePosition cell);
void update_field_object_priority_before(FieldObject& actor, std::uint8_t flags);
void update_field_object_priority_after(FieldObject& actor, std::uint8_t flags);
struct FieldLocalTiles {
    // Row-major: x - 1..x + 1, y - 2..y + 1.
    std::array<FieldTerrainCell, 12> cells;
    bool operator==(const FieldLocalTiles&) const = default;
};
FieldLocalTiles field_local_tiles(const FieldMap& map, std::uint32_t x, std::uint32_t y);
struct FieldPlayerTerrain {
    std::uint8_t level = 0;
    bool low_priority = false;
    bool operator==(const FieldPlayerTerrain&) const = default;
};
struct FieldMoveGates {
    bool control_enabled = true, map_loading = false, menu_opening = false, collision = false;
    std::array<std::uint8_t, 3> background_updates{};
    bool sprint_disabled = false, sprint_equipped = false;
};
struct FieldStep {
    enum class Kind { stationary, blocked, cardinal, diagonal, door };
    Kind kind = Kind::stationary;
    FieldMove direction = FieldMove::none;
    bool reserve_destination = false;
    // Door tile identity; the map-edit handler must finish opening it.
    std::optional<std::uint8_t> door{};
    bool operator==(const FieldStep&) const = default;
};
void initialize_field_player_terrain(FieldObject& actor, FieldPlayerTerrain& terrain,
                                     const FieldLocalTiles& tiles, std::uint8_t saved_level);
// Shared priority/stair decisions used by the full player owner and inspector.
// The full owner distinguishes the selected actor from the scheduled caller.
void update_field_player_priority_after(FieldObject& selected, FieldPlayerTerrain& terrain,
                                        const FieldLocalTiles& tiles);
void update_field_player_priority_before(FieldObject& selected, FieldPlayerTerrain& terrain,
                                         const FieldLocalTiles& tiles, FieldMove direction);
std::optional<FieldMove> field_player_stair_direction(const FieldLocalTiles& tiles,
                                                      const FieldPlayerTerrain& terrain,
                                                      Buttons buttons);
// Isolated cardinal terrain check. Updates speed even when blocked; a successful
// result owns the before-step priority transition and destination reservation.
FieldStep check_field_cardinal_step(FieldObject& actor, FieldPlayerTerrain& terrain,
                                    const FieldLocalTiles& tiles, FieldMove direction,
                                    bool occupied, bool sprint_disabled, bool sprint_equipped);
// Called at aligned tile boundaries by the field action scheduler. This chooses
// movement, not its cadence or the game effects caused by a successful step.
// Occupied entries are up, right, down, left; the caller owns object reservations.
FieldStep plan_field_player_step(FieldObject& actor, FieldPlayerTerrain& terrain,
                                 const FieldLocalTiles& tiles, const std::array<bool, 4>& occupied,
                                 Buttons buttons, const FieldMoveGates& gates,
                                 const FieldMoveRates& rates, const FieldPoseTables& poses);
} // namespace ff6
