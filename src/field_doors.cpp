#include "ff6/field_doors.hpp"
#include "rom-materials/audio_loading_choices.macros.inc"
#include "rom-materials/field_door_selectors.macros.inc"
#include "rom-materials/field_door_dimensions.macros.inc"

namespace ff6 {
namespace {
// Patch headers remain private ROM inputs, including copies used by validation.
constexpr auto first_width = FF6_ROM_VALUE_FIELD_DOOR_DIMENSIONS_0;
constexpr auto first_height = FF6_ROM_VALUE_FIELD_DOOR_DIMENSIONS_1;
constexpr auto second_width = FF6_ROM_VALUE_FIELD_DOOR_DIMENSIONS_2;
constexpr auto second_height = FF6_ROM_VALUE_FIELD_DOOR_DIMENSIONS_3;
constexpr auto third_width = FF6_ROM_VALUE_FIELD_DOOR_DIMENSIONS_4;
constexpr auto third_height = FF6_ROM_VALUE_FIELD_DOOR_DIMENSIONS_5;
constexpr auto first_cells = first_width * first_height;
constexpr auto second_cells = second_width * second_height;
constexpr auto third_cells = third_width * third_height;
constexpr auto first_bytes = first_cells + 2U;
constexpr auto second_bytes = second_cells + 2U;
constexpr auto third_bytes = third_cells + 2U;
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
void validate(const FieldDoorTiles& patches, const FieldDoorHistory& history) {
    require(history.count <= 23, "Invalid retained door count");
    for (unsigned i = 0; i < 3; ++i)
        require(patches[i].width == (i == 0 ? first_width : i == 1 ? second_width : third_width) &&
                    patches[i].height == (i == 0 ? first_height : i == 1 ? second_height : third_height) &&
                    patches[i].tiles.size() == (i == 0 ? first_cells : i == 1 ? second_cells : third_cells),
                "Invalid native door patch");
}
} // namespace
FieldDoorTiles load_field_door_tiles(const ContentPack& pack) {
    const auto data = pack.get("field_door_tiles");
    require(data.size() == first_bytes + second_bytes + third_bytes, "Invalid door tile resource extent");
    FieldDoorTiles result{decode_field_map_patch(data.subspan(0, first_bytes)),
                          decode_field_map_patch(data.subspan(first_bytes, second_bytes)),
                          decode_field_map_patch(data.subspan(first_bytes + second_bytes, third_bytes))};
    validate(result, {});
    return result;
}
void reset_field_doors(FieldDoorHistory& history) {
    history.count = 0;
}
std::optional<FieldDoorOpened>
try_open_field_door(FieldMap& map, FieldDoorHistory& history, const FieldDoorTiles& patches,
                    const FieldOccupancy& occupancy, FieldTilePosition position,
                    const FieldLocalTiles& tiles, FieldPlayerTerrain terrain, std::uint8_t facing) {
    validate(patches, history);
    require(facing < 4, "Invalid door approach facing");
    if (((tiles.cells[7].flags & 4U) && terrain.level == 2) || (facing & 1U))
        return {};
    const auto candidate = tiles.cells[facing == 0 ? 4 : 10].tile;
    const unsigned patch = candidate == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_0 ? 0 :
        candidate == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_1 ? 1 : 2;
    if (candidate != FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_0 &&
        candidate != FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_1 &&
        candidate != FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_2)
        return {};
    const auto flags = map.tile_properties[candidate][0];
    if (flags == 0xf7 || !(flags & 32U))
        return {};
    const FieldTilePosition anchor{
        static_cast<std::uint8_t>(unsigned{position.x} + (patch == 2 ? 255U : 0U)),
        static_cast<std::uint8_t>(unsigned{position.y} + (facing == 0 ? 254U : 0U))};
    const auto occupied_y = static_cast<std::uint8_t>(unsigned{anchor.y} + 1U);
    const auto layout = map.definition.layouts[0];
    require(occupancy.width == layout.width && occupancy.height == layout.height,
            "Door occupancy and map extents disagree");
    // This source lookup does not use the usual wrapping neighbor lookup.
    require(position.x < occupancy.width && occupied_y < occupancy.height,
            "Door occupancy needs unrepresented retained cells");
    if (occupancy.at(position.x, occupied_y))
        return {};
    auto next = history;
    next.positions[next.count] = anchor;
    if (next.count < 23)
        ++next.count;
    apply_field_map_patch(map, 0, anchor.x, anchor.y, patches[patch]);
    history = next;
    return FieldDoorOpened{anchor, 4, FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_8};
}
void restore_field_doors(FieldMap& map, const FieldDoorHistory& history,
                         const FieldDoorTiles& patches) {
    validate(patches, history);
    auto next = map;
    for (unsigned i = 0; i < history.count; ++i) {
        const auto at = history.positions[i];
        const auto tile = field_map_tile(next, 0, at.x, at.y);
        // Original reload selects from the closed door's upper-left tile.
        // The fallback is deliberately unconditional, including overlapping edits.
        const unsigned patch = tile == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_3 ? 0 :
            tile == FF6_ROM_VALUE_FIELD_DOOR_SELECTORS_4 ? 1 : 2;
        apply_field_map_patch(next, 0, at.x, at.y, patches[patch]);
    }
    map.tile_edits = std::move(next.tile_edits);
}
} // namespace ff6
