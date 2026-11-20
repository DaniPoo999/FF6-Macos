#include "ff6/field_movement.hpp"
#include "rom-materials/field_player_speeds.macros.inc"
#include <algorithm>
#include "rom-materials/field_door_selectors.macros.inc"

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
unsigned direction_index(FieldMove direction, unsigned limit = 8) {
    const auto value = static_cast<unsigned>(direction);
    require(value > 0 && value <= limit, "Invalid field movement direction");
    return value;
}
const FieldTerrainCell& destination(const FieldLocalTiles& tiles, FieldMove direction,
                                    bool upper = false) {
    const auto value = direction_index(direction);
    // Offsets are topology, independent of map storage or original bus addresses.
    constexpr std::array<int, 8> dx{0, 1, 0, -1, 1, 1, -1, -1};
    constexpr std::array<int, 8> dy{-1, 0, 1, 0, -1, 1, 1, -1};
    const auto index = (2 + dy[value - 1] - (upper ? 1 : 0)) * 3 + 1 + dx[value - 1];
    return tiles.cells[static_cast<std::size_t>(index)];
}
void validate(const FieldObject& actor) {
    require(actor.upper.priority < 4 && actor.lower.priority < 4 && actor.layer_priority_mode < 4,
            "Invalid field layer priority");
}
bool fixed_priority(FieldObject& actor) {
    if (actor.layer_priority_mode == 0)
        return false;
    actor.upper.priority = actor.layer_priority_mode == 3 ? 2 : 3;
    actor.lower.priority = actor.layer_priority_mode == 2 ? 3 : 2;
    return true;
}
void after_priority(FieldObject& actor, FieldPlayerTerrain& terrain, const FieldLocalTiles& tiles) {
    const auto current = tiles.cells[7].flags;
    if (terrain.low_priority && !(current & 4U) && !(tiles.cells[4].flags & 4U))
        terrain.low_priority = false;
    if (fixed_priority(actor))
        return;
    if (current & 4U) {
        if (terrain.level != 1)
            return;
        actor.upper.priority = 3;
    } else if ((actor.upper.priority & 1U) == 0 && (current & 8U)) {
        actor.upper.priority = 3;
    }
    if (current & 16U)
        actor.lower.priority = 3;
}
void before_priority(FieldObject& actor, FieldPlayerTerrain& terrain, const FieldLocalTiles& tiles,
                     FieldMove direction) {
    const auto next = destination(tiles, direction).flags;
    if (!terrain.low_priority) {
        if (next & 4U) {
            if (terrain.level == 2)
                terrain.low_priority = true;
        } else {
            const auto upper = destination(tiles, direction, true).flags;
            if (upper != 0xf7 && (upper & 4U) && (tiles.cells[4].flags & 7U) == 2)
                terrain.low_priority = true;
        }
    }
    if (fixed_priority(actor))
        return;
    if (next & 4U) {
        if (terrain.level == 2)
            actor.upper.priority = 2;
    } else if ((actor.upper.priority & 1U) && !(next & 8U)) {
        actor.upper.priority = 2;
    }
    if (!(next & 16U))
        actor.lower.priority = 2;
}
FieldStep cardinal(FieldObject& actor, FieldPlayerTerrain& terrain, const FieldLocalTiles& tiles,
                   FieldMove direction, bool occupied, bool sprint_disabled, bool sprint_equipped) {
    const auto number = direction_index(direction, 4);
    const auto current = tiles.cells[7];
    const auto next = destination(tiles, direction).flags;
    actor.speed = !sprint_disabled && sprint_equipped ? (FF6_ROM_VALUE_FIELD_PLAYER_SPEEDS_0 + 1)
                                                       : FF6_ROM_VALUE_FIELD_PLAYER_SPEEDS_0;
    constexpr std::array<unsigned, 4> permissions{8, 1, 4, 2};
    bool allowed = (current.directions & permissions[number - 1]) != 0 && (next & 7U) != 7;
    if (current.flags & 4U) {
        allowed = allowed && !(next & ((terrain.level & 1U) ? 2U : 1U));
    } else if ((next & 3U) != 3) {
        const auto current_levels = current.flags & 3U;
        allowed = allowed && (current_levels == 3 ? !(next & 4U) : !((current_levels ^ 3U) & next));
    }
    if (occupied) {
        const auto levels = current.flags & 7U;
        allowed = allowed && (next & 4U) && levels != 1 && (levels == 2 || terrain.level == 2);
    }
    if (!allowed)
        return {FieldStep::Kind::blocked};
    // Reservation uses the old level; the following update intentionally occurs later.
    const bool reserve = !(next & 4U) || terrain.level != 2;
    if ((current.flags & 7U) < 3)
        terrain.level = current.flags & 3U;
    before_priority(actor, terrain, tiles, direction);
    actor.move_direction = static_cast<std::uint8_t>(direction);
    return {FieldStep::Kind::cardinal, direction, reserve};
}
std::optional<FieldMove> slope_direction(const FieldLocalTiles& tiles,
                                         const FieldPlayerTerrain& terrain, Buttons buttons) {
    const auto current = tiles.cells[7].flags;
    if (((current & 4U) && terrain.level == 2) || !(current & 192U))
        return {};
    const bool right = held(buttons, Button::right), left = held(buttons, Button::left);
    if (!right && !left)
        return {};
    const bool descending_right = (current & 128U) != 0;
    const auto direction = right ? (descending_right ? FieldMove::down_right : FieldMove::up_right)
                                 : (descending_right ? FieldMove::up_left : FieldMove::down_left);
    const auto next = destination(tiles, direction).flags;
    if (next == 0xf7 || !(next & (descending_right ? 128U : 64U)))
        return {};
    return direction;
}
} // namespace
FieldTilePosition field_object_cell(const FieldMap& map, const FieldObject& actor) {
    const auto layout = map.definition.layouts[0];
    const auto valid = [](unsigned n) { return n == 16 || n == 32 || n == 64 || n == 128; };
    require(valid(layout.width) && valid(layout.height), "Invalid object map extent");
    require(actor.x <= 0xffffff && actor.y <= 0xffffff, "Invalid object position");
    return {static_cast<std::uint8_t>((actor.x >> 12U) & (layout.width - 1U)),
            static_cast<std::uint8_t>((actor.y >> 12U) & (layout.height - 1U))};
}
FieldTerrainCell field_terrain_at(const FieldMap& map, FieldTilePosition cell) {
    const auto layout = map.definition.layouts[0];
    require(cell.x < layout.width && cell.y < layout.height && cell.y < 64,
            "Object terrain needs unrepresented retained/cross-layer state");
    const auto tile = field_map_tile(map, 0, cell.x, cell.y);
    return {tile, map.tile_properties[tile][0], map.tile_properties[tile][1]};
}
void update_field_object_priority_before(FieldObject& actor, std::uint8_t flags) {
    validate(actor);
    if (fixed_priority(actor))
        return;
    if (flags == 0xf7 || (flags & 4U)) {
        actor.upper.priority = actor.lower.priority = 2;
    } else {
        if (!(flags & 8U))
            actor.upper.priority = 2;
        if (!(flags & 16U))
            actor.lower.priority = 2;
    }
}
void update_field_object_priority_after(FieldObject& actor, std::uint8_t flags) {
    validate(actor);
    if (fixed_priority(actor))
        return;
    if (flags == 0xf7 || (flags & 4U)) {
        actor.upper.priority = actor.lower.priority = 2;
    } else {
        if (flags & 8U)
            actor.upper.priority = 3;
        if (flags & 16U)
            actor.lower.priority = 3;
    }
}
FieldMoveRates load_field_move_rates(const ContentPack& pack) {
    const auto data = pack.get("field_movement_rates");
    require(data.size() == 154, "Invalid field movement table size");
    FieldMoveRates rates;
    std::copy_n(data.begin(), 10, rates.multipliers.begin());
    std::copy_n(data.begin() + 138, 16, rates.facing.begin());
    for (std::size_t i = 0; i < 16; ++i)
        for (std::size_t axis = 0; axis < 2; ++axis) {
            const auto amount = read16(data, 10 + axis * 64 + i * 2);
            const auto mask = read16(data, 42 + axis * 64 + i * 2);
            require(amount <= 255 && (mask == 0 || mask == 65535), "Invalid field movement rate");
            rates.directions[i][axis] =
                static_cast<std::int16_t>(mask == 0 ? int{amount} : -int{amount});
            require(data[138 + i] < 4, "Invalid movement-facing table");
        }
    return rates;
}
std::array<std::int16_t, 2> field_move_velocity(const FieldMoveRates& rates, FieldMove direction,
                                                std::uint8_t speed) {
    const auto number = direction_index(direction, 16);
    require(speed < rates.multipliers.size(), "Undefined field movement speed");
    std::array<std::int16_t, 2> result;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto rate = rates.directions[number - 1][axis];
        require(rate >= -255 && rate <= 255, "Invalid normalized field movement rate");
        const int product = int{rate} * rates.multipliers[speed];
        require(product >= -32767 && product <= 32767, "Field movement rate exceeds signed range");
        result[axis] = static_cast<std::int16_t>(product);
    }
    return result;
}
FieldLocalTiles field_local_tiles(const FieldMap& map, std::uint32_t x, std::uint32_t y) {
    require(x <= 0xffffff && y <= 0xffffff, "Field position exceeds unsigned 16.8 range");
    const auto layout = map.definition.layouts[0];
    const auto valid_extent = [](unsigned n) { return n == 16 || n == 32 || n == 64 || n == 128; };
    require(valid_extent(layout.width) && valid_extent(layout.height),
            "Invalid terrain map extent");
    const unsigned column = (x >> 12U) & 255U, row = (y >> 12U) & 255U;
    FieldLocalTiles result;
    for (unsigned r = 0; r < 4; ++r) {
        const auto yy = r == 2 ? row : (row + 254U + r) & (layout.height - 1U);
        require(yy < 64, "Terrain row needs unresolved cross-layer state");
        for (unsigned c = 0; c < 3; ++c) {
            const auto xx = (column + 255U + c) & (layout.width - 1U);
            const auto tile = field_map_tile(map, 0, xx, yy);
            const auto properties = map.tile_properties[tile];
            result.cells[r * 3 + c] = {tile, properties[0], properties[1]};
        }
    }
    return result;
}
void initialize_field_player_terrain(FieldObject& actor, FieldPlayerTerrain& terrain,
                                     const FieldLocalTiles& tiles, std::uint8_t saved_level) {
    validate(actor);
    const auto current = tiles.cells[7].flags;
    terrain.level = (current & 4U) ? saved_level : current & 3U;
    terrain.low_priority = (current & 4U) && terrain.level == 2;
    actor.upper.priority = (current & 4U) ? (terrain.level == 2 ? 2 : 3) : (current & 8U) ? 3 : 2;
    actor.lower.priority = 2;
}
FieldStep check_field_cardinal_step(FieldObject& actor, FieldPlayerTerrain& terrain,
                                    const FieldLocalTiles& tiles, FieldMove direction,
                                    bool occupied, bool sprint_disabled, bool sprint_equipped) {
    validate(actor);
    auto next_actor = actor;
    auto next_terrain = terrain;
    auto result = cardinal(next_actor, next_terrain, tiles, direction, occupied, sprint_disabled,
                           sprint_equipped);
    actor = next_actor;
    terrain = next_terrain;
    return result;
}
void update_field_player_priority_after(FieldObject& selected, FieldPlayerTerrain& terrain,
                                        const FieldLocalTiles& tiles) {
    validate(selected);
    after_priority(selected, terrain, tiles);
}
void update_field_player_priority_before(FieldObject& selected, FieldPlayerTerrain& terrain,
                                         const FieldLocalTiles& tiles, FieldMove direction) {
    validate(selected);
    before_priority(selected, terrain, tiles, direction);
}
std::optional<FieldMove> field_player_stair_direction(const FieldLocalTiles& tiles,
                                                      const FieldPlayerTerrain& terrain,
                                                      Buttons buttons) {
    return slope_direction(tiles, terrain, buttons);
}
FieldStep plan_field_player_step(FieldObject& actor, FieldPlayerTerrain& terrain,
                                 const FieldLocalTiles& tiles, const std::array<bool, 4>& occupied,
                                 Buttons buttons, const FieldMoveGates& gates,
                                 const FieldMoveRates& rates, const FieldPoseTables& poses) {
    validate(actor);
    require(actor.x <= 0xffffff && actor.y <= 0xffffff && actor.x % 4096 == 0 &&
                actor.y % 4096 == 0,
            "Player movement decision requires aligned tile position");
    auto next = actor;
    auto ground = terrain;
    after_priority(next, ground, tiles);
    next.velocity_x = next.velocity_y = 0;
    next.walking_animation = true;
    next.move_direction = 0;
    FieldStep result;
    const bool ready =
        gates.control_enabled && !gates.map_loading && !gates.menu_opening && !gates.collision &&
        std::ranges::all_of(gates.background_updates, [](auto n) { return n == 0 || n == 5; });
    if (ready) {
        if (const auto diagonal = slope_direction(tiles, ground, buttons)) {
            result = {FieldStep::Kind::diagonal, *diagonal};
            next.facing = held(buttons, Button::right) ? 1 : 3;
            const auto current = tiles.cells[7].flags;
            if (!(current & 4U) && (current & 3U) != 3)
                ground.level = current & 3U;
            before_priority(next, ground, tiles, *diagonal);
        } else {
            std::optional<FieldMove> direction;
            if (held(buttons, Button::right))
                direction = FieldMove::right;
            else if (held(buttons, Button::left))
                direction = FieldMove::left;
            else if (held(buttons, Button::up))
                direction = FieldMove::up;
            else if (held(buttons, Button::down))
                direction = FieldMove::down;
            if (direction) {
                next.facing = static_cast<std::uint8_t>(static_cast<unsigned>(*direction) - 1U);
                next.pending_pose = poses.standing[next.facing];
                const auto target = destination(tiles, *direction);
                const bool door =
                    (!((tiles.cells[7].flags & 4U) && ground.level == 2)) &&
                    (next.facing == 0 || next.facing == 2) && !occupied[next.facing] &&
                    (target.tile == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_0 ||
                     target.tile == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_1 ||
                     target.tile == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_2) &&
                    target.flags != 0xf7 && (target.flags & 32U);
                if (door)
                    result = {FieldStep::Kind::door, FieldMove::none, false, target.tile};
                else
                    result = cardinal(next, ground, tiles, *direction, occupied[next.facing],
                                      gates.sprint_disabled, gates.sprint_equipped);
            }
        }
    }
    if (result.direction != FieldMove::none) {
        const auto velocity = field_move_velocity(rates, result.direction, next.speed);
        next.velocity_x = velocity[0];
        next.velocity_y = velocity[1];
        next.move_direction = static_cast<std::uint8_t>(result.direction);
    }
    actor = next;
    terrain = ground;
    return result;
}
} // namespace ff6
